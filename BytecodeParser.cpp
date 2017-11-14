#include <iostream>
#include <iomanip>

#include "Helper.h"
#include "Structs.h"

#include "BytecodeParser.h"

// Parsing the bytecode
BytecodeParser::BytecodeParser(std::ifstream &f, HeaderPair index) {
	f.seekg(index.offset, std::ios_base::beg);
	
	dataLength = index.count;
	bytecode = new unsigned char[dataLength + 16];
	
	f.read((char*) bytecode, dataLength);
	if (f.fail()) {
		Logger::Log(Logger::ERROR) << "Tried to read " << dataLength << " bytes, got" << f.gcount() << std::endl;
		throw std::exception();
	}
	Logger::Log(Logger::DEBUG) << "Read " << f.gcount() << " bytes of bytecode." << std::endl;
}

unsigned int BytecodeParser::readArg(Instruction &inst, unsigned int argSize = 4) {
	unsigned int arg = 0;
	if (argSize == 4) {
		arg = readUInt32(bytecode + currAddress);
		currAddress += 4;
	} else if (argSize == 1) {
		arg = bytecode[currAddress++];
	}
	inst.args.push_back(arg);
	return arg;
}

// WARNING: pop_back is no throw
unsigned int BytecodeParser::readArgs(Instruction &inst, ProgStack &numStack, ProgStack &strStack, bool pop) {
	unsigned int numArg = readArg(inst), arg = 0;

	unsigned int opcode = inst.opcode;
	
	if (numArg > 2) {
		Logger::Log(Logger::VERBOSE_DEBUG) << "Address 0x" << std::hex << std::setw(4) << +inst.address;
		Logger::Log(Logger::VERBOSE_DEBUG) << ":  Reading: " << std::dec << numArg << " arguments." << std::endl;
		
		if (dataLength - currAddress < 4 * numArg) {
			Logger::Log(Logger::ERROR) << "Going to overflow." << std::endl;
			throw std::exception();
		}
		
	}

	for (unsigned int counter = 0; counter < numArg; counter++) {
		arg = readArg(inst);
		if (pop) {
			if (arg == 0x0a) {
				if (!numStack.empty())
					numStack.pop_back();
			} else if (arg == 0x14) {
				if (!strStack.empty())
					strStack.pop_back();
			} else if (arg == 0x051e) {
			} else if (arg == 0x0514) {
			} else {
				Logger::Log(Logger::WARN) << "Instruction " << opcode << " at address 0x" << std::hex << inst.address;
				Logger::Log(Logger::WARN) << " trying to pop stack " << arg << std::endl;
			}
		}
	}

	return numArg;
}

// Stolen from tanuki
// https://github.com/bitprime/vn_translation_tools/blob/master/rewrite/
// todo: separate this into its own thing
void BytecodeParser::parseBytecode(Instructions &instList, const StringList &strings, const StringList &strings2) {
	unsigned int arg, arg1, arg2;
	unsigned char opcode;
	ProgStack numStack, strStack;
	unsigned int stackTop;
	std::vector<unsigned int> commandStacks;
	commandStacks.push_back(0);
	
	unsigned int stackArr[] = {
		0x02, //0x08,
		0x0c, 
		//0x0d
		0x12, //0x1e,
		0x4c, // making a selection (choice)?
		0x4d 
	};
	
	
	Logger::Log(Logger::INFO) << "Parsing " << std::to_string(dataLength) << " bytes.\n";
	
	while (currAddress < dataLength) {
		Instruction inst;
		inst.address = currAddress;
		opcode = bytecode[currAddress++];
		inst.opcode = opcode;

		switch (opcode) {
			case 0x01:
			case 0x03:
			case 0x04:	// load value > ??
			case 0x10:
			case 0x11:
			case 0x12:
			case 0x31:
				readArg(inst);
			break;
			case 0x05:
				while (commandStacks.back() --> 1) {
					if (!numStack.empty())
						numStack.pop_back();
				}
				commandStacks.pop_back();
				commandStacks.back()++;	// collapse into lower frame
			break;
			case 0x06:
			case 0x09:
			case 0x32:
			break;
			case 0x16:
				Logger::Log(Logger::INFO) << "End of script reached at address 0x" << std::hex << inst.address << std::dec << std::endl;
			break;
			case 0x07:
				readArg(inst);
				arg = readArg(inst);
				if (arg == 0x0a) {
					if (!numStack.empty())
						numStack.pop_back();
					Logger::Log(Logger::WARN) << "Not sure if this should happen." << std::endl;
				} else if (arg == 0x14) {
					try {
						inst.comment = strings2.at(arg);
						if (!strStack.empty())
							strStack.pop_back();
					} catch (std::out_of_range &e) {
						Logger::Log(Logger::WARN) << "Missing string id " << arg << " at 0x" << std::hex << inst.address << std::dec << std::endl;
					}
				}
			break;
			case 0x13:
				readArg(inst);
				readArg(inst);
			break;
			case 0x14:
				readArg(inst);
				readArg(inst);
				readArg(inst);
			break;
			case 0x20:
				readArg(inst);
				readArg(inst);
				readArg(inst);
				if (commandStacks.size() > 1) {	
					commandStacks.pop_back();	// not sure if should close all
				}
			break;
			case 0x21:
				readArg(inst);
				readArg(inst, 1);
			break;
			case 0x22:	// calc. 0x10 is subtract?
				arg1 = readArg(inst);
				arg2 = readArg(inst);
				readArg(inst, 1);
				if (arg1 == 0xa && arg2 == 0xa) {
					if (!numStack.empty())
						numStack.pop_back();
					//if (!numStack.empty())
					//	numStack.pop_back();
					// do the calc
					// numStack.push_back(0);
				}
			break;
			case 0x02:
				arg1 = readArg(inst);
				arg2 = readArg(inst);
				if (arg1 == 0x0a) {
					numStack.push_back(arg2);
					commandStacks.back()++;
				} else if (arg1 == 0x14) {
					inst.comment = strings.at(arg2);	// Check in range?
					strStack.push_back(arg2);
				}
			break;
			case 0x08:
				commandStacks.push_back(0);
			break;
			case 0x15:
				readArgs(inst, numStack, strStack);
			break;
			case 0x30:
				arg = readArg(inst);
				readArgs(inst, numStack, strStack);
				readArgs(inst, numStack, strStack, false);
								
				readArg(inst);

				stackTop = numStack.back();
				//if ((numStack.top() & 0x7e000000) == 0) {
				if (arg == 0x01) {
					if (std::find(std::begin(stackArr), std::end(stackArr), stackTop) != std::end(stackArr)){
						readArg(inst);
					} else {
						Logger::Log(Logger::DEBUG) << "Address 0x" << std::hex << std::setw(4) << +inst.address;
						Logger::Log(Logger::DEBUG) << ", stacktop is " << stackTop << std::endl;
					
						Logger::Log(Logger::DEBUG) << "Command stack layers: " << commandStacks.size() << std::endl;
					}
				}
				//if (arg == 0x0a)
					//numStack.push_back(0x02);
				if (commandStacks.size() > 1)
					commandStacks.pop_back();
			break;
			case 0x00:
			default:
				inst.nop = true;
				Logger::Log(Logger::INFO) << "Address 0x" << std::hex << std::setw(4) << +inst.address;
				Logger::Log(Logger::INFO) << ":  NOP encountered: 0x" << std::setw(2) << +opcode << std::dec << std::endl;
		}
		instList.push_back(inst);
	}
	
	Logger::Log(Logger::INFO) << "Read " << instList.size() << " instructions.\n";
}

BytecodeParser::~BytecodeParser() {
	if (bytecode)	// don't need check
		delete[] bytecode;
}