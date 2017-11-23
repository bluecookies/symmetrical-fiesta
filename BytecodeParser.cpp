// Parse the bytecode
// A lot of this is stolen from tanuki
// https://github.com/bitprime/vn_translation_tools/blob/master/rewrite/
// also Inori

#include <cstdio>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <exception>
#include <algorithm>

#include "Helper.h"
#include "Structs.h"

#include "BytecodeParser.h"

/*unsigned int BytecodeParser::readArgs(bool pop) {
	unsigned int numArgs = getInt();
	unsigned int arg = 0, stackCount = 0;
	StackValue var;

	for (unsigned int counter = 0; counter < numArgs; counter++) {
		arg = getInt();
		progInfo.args.push_back(arg);
		if (pop) {
			if (arg == 0x0a) {
				if (!progInfo.stack.empty()) {
					var = progInfo.stack.back();
					progInfo.stack.pop_back();
					if (var.type != STACK_NUM && var.type != STACK_REF) {
						Logger::Log(Logger::WARN, progInfo.address) << "Non-num type read.\n";
					}
					
					if (progInfo.stackPointers.back() > progInfo.stack.size()) {
						Logger::Log(Logger::WARN, progInfo.address) << " Popped past frame.\n";
					}
				}
			} else if (arg == 0x0e) {
				if (!progInfo.stack.empty()) {
					var = progInfo.stack.back();
					progInfo.stack.pop_back();
					// TODO: handle this type
					
					if (var.value >> 24 != 0x7f) {
						Logger::Log(Logger::DEBUG, progInfo.address) << " Type 0xe: " << std::hex << var.value << std::endl;
					}	
					if (progInfo.stackPointers.back() > progInfo.stack.size()) {
						Logger::Log(Logger::WARN, progInfo.address) << " Popped past frame.\n";
					}
				}
			} else if (arg == 0x14) {
				if (!progInfo.stack.empty()) {
					var = progInfo.stack.back();
					progInfo.stack.pop_back();
					
					if (var.type != STACK_STR && var.type != STACK_REF) {
						Logger::Log(Logger::WARN, progInfo.address) << "Non-string type read.\n";
					}
				}
			} else if (arg == 0x51e) {
				// TODO: HANDLE REF LATER
				unsigned int a1 = 0xdeadbeef, a2 = 0xdeadbeef, a3 = 0xdeadbeef;
				if (progInfo.stack.size() >= 4) {
					progInfo.stack.pop_back();
					a1 = progInfo.stack.back().value;
						progInfo.stack.pop_back();
					a2 = progInfo.stack.back().value;
						progInfo.stack.pop_back();
					a3 = progInfo.stack.back().value;
						progInfo.stack.pop_back();
				} else {
					
					throw std::exception();
				}
				if (a1 != 0xffffffff || a2 != 2) {
					Logger::Log(Logger::WARN, progInfo.address) << " Popping 0x51e - " << std::hex;
					Logger::Log(Logger::WARN) << a3 << " " << a2 << " " << a1 << std::endl;
				}
			} else if (arg == 0xffffffff) {
				stackCount = getInt();
				for (unsigned int counter2 = 0; counter2 < stackCount; counter2++) {
					progInfo.args.push_back(getInt());
				}
				progInfo.args.push_back(0xffffffff);
			} else {
				Logger::Log(Logger::DEBUG, progInfo.address) << " trying to pop stack " << arg << std::endl;
			}
		}
	}

	return numArgs;
}*/
/*
void instDo07Thing(FILE* f, BytecodeBuffer &buf, SceneInfo& sceneInfo, ProgInfo& progInfo) {
	unsigned int arg1, arg2;
		arg1 = getInt();
		arg2 = getInt();
	
	StackValue var;

	fprintf(f, "%#x, %#x", arg1, arg2);
	if (arg1 == 0x0a) {
		if (!progInfo.stack.empty()) {
			var = progInfo.stack.back();
			progInfo.stack.pop_back();
			
			if (var.type != STACK_NUM && var.type != STACK_REF)
				Logger::Log(Logger::WARN, progInfo.address) << "Non-num type read.\n";
		}
	} else if (arg1 == 0x14) {
		if (!progInfo.stack.empty()) {
			var = progInfo.stack.back();
			progInfo.stack.pop_back();
			
			if (var.type != STACK_STR)
				Logger::Log(Logger::WARN, progInfo.address) << "Non-num type read.\n";
		}
	}
	try {
		progInfo.comment = sceneInfo.varStrings.at(arg2);	
	} catch (std::out_of_range &e) {
		Logger::Log(Logger::WARN, progInfo.address) << "Missing string id: " << arg2 << std::endl;
	}
}

void instDo13Thing(FILE* f, BytecodeBuffer &buf, ProgInfo& progInfo) {
	fprintf(f, "%#x ", getInt());
	unsigned int numArgs = readArgs(buf, progInfo);
	fprintf(f, "(");
	for (unsigned int i = 0; i < numArgs; i++) {
		fprintf(f, "%#x,", progInfo.args.back());
		progInfo.args.pop_back();
	}
	fprintf(f, ")");
}


void instDo15Thing(FILE* f, BytecodeBuffer &buf, ProgInfo& progInfo) {
	unsigned int numArgs = readArgs(buf, progInfo);
	fprintf(f, "(");
	for (unsigned int i = 0; i < numArgs; i++) {
		fprintf(f, "%#x,", progInfo.args.back());
		progInfo.args.pop_back();
	}
	fprintf(f, ")");
} */


// unsafe
void BytecodeParser::handleAssign() {
	unsigned int type = getInt();
	unsigned int inType = getInt();
	unsigned int unknown = getInt();
}

void BytecodeParser::handleCall() {
	StackValue arg;
	unsigned int type;

	unsigned int arg1 = getInt();
	
	// Arguments to pass to function to be called
	unsigned int numArgs1 = getInt();
	for (unsigned int i = 0; i < numArgs1; i++) {
		type = getInt();
		//arg = progInfo.stack.rbegin()[i];
	}

	// don't know
	unsigned int numArgs2 = getInt();
	for (unsigned int i = 0; i < numArgs2; i++) {
		type = getInt();
	}
	
	// Return type
	unsigned int retType = getInt();

	// Function to call

	// might not be right, since some take up more than one slot
	StackValue stackTop = progInfo.stack.rbegin()[numArgs1];
	if (stackTop.type != STACK_NUM)
		Logger::Log(Logger::WARN) << "Call address not of type 0x0a.\n";
	
	Logger::Log(Logger::INFO) << "Calling function " << std::hex << stackTop.value;

	if (stackTop.value >> 24 == 0x7e) {
		unsigned int commandIndex = stackTop.value & 0x00FFFFFF;

		if (commandIndex < sceneInfo.commands.size()) {
			ScriptCommand command = sceneInfo.commands.at(commandIndex);
			std::stringstream stream;
			stream << std::hex << command.offset;
			try {
				Logger::Log(Logger::INFO) << command.name << " -  (0x" + stream.str() + " in " + sceneInfo.sceneNames.at(command.file) + ")";
			} catch(std::exception &e) {
				Logger::Log(Logger::ERROR) << "Scene index " << std::dec << command.file << " out of bounds\n";
			}
		} else {
			Logger::Log(Logger::WARN) << "\nTrying to access command index " << commandIndex;
		}

	}
	Logger::Log(Logger::INFO) << std::endl;

	/* switch (stackTop.value) {
		case 0x0c:
		case 0x12:
		case 0x13:
		case 0x4c:
		case 0x4d:
		case 0x5a:
		case 0x5b:
		case 0x64:
		case 0x7f:
			//if (progInfo.stackPointers.back() == progInfo.stack.size()) {
				//fprintf(f, ", %#x", getInt());
				//progInfo.stackPointers.pop_back();
			//}
		break;
		case 0xffffffff:
			//Logger::Log(Logger::WARN, progInfo.address) << " Calling function 0xffffffff.\n";
		break;
		case 0xDEADBEEF:
			//Logger::Log(Logger::WARN, progInfo.address) << " Calling deadbeef\n";
	} */
	
	
	progInfo.stack.push_back(StackValue(0xdeadbeef, retType));
}


void BytecodeParser::parseBytecode() {
	ProgramInfo progInfo;
	
	unsigned char opcode;
	bool endScript = false;
	
	unsigned int temp = 0;
	Logger::Log(Logger::INFO) << "\n****************************\n*Program execution starting*\n****************************\n";

	while (!endScript && temp++ < 200) {
		opcode = getChar();

		switch (opcode) {
			case 0x01:	handle01();					break;
			case 0x02:	handlePush();				break;
			case 0x03:	handlePop();				break;
			case 0x04:	handleDup();				break;
			case 0x05:	handleEval();				break;
			case 0x08:	handle08();					break;
			case 0x10:	
			case 0x11:	
			case 0x12:	handleJump(opcode); 		break;
			case 0x16:	endScript = true;			break;
			case 0x20:	handleAssign();			break;
			case 0x22:	handleCalc(); 				break;
			case 0x30:	handleCall();				break;

			//case 0x31:	fprintf(f, "%#x", getInt());		break;
			
			case 0x06:
			case 0x09:
			case 0x32:
			case 0x33:
			case 0x34:									break;
			
			//case 0x07:	instDo07Thing(f, buf, sceneInfo, progInfo);		break;

			//case 0x13:	instDo13Thing(f, buf, progInfo);		break;
			//case 0x14:	break;
			//case 0x21:	fprintf(f, "%#x, %d", getInt(), getChar());
			

			//case 0x15:	instDo15Thing(f, buf, progInfo);		break;

			case 0x00:
			default:
				Logger::Log(Logger::INFO) << "NOP encountered: 0x" << std::setw(2) << +opcode << std::endl;
		}
	}
}


BytecodeParser::BytecodeParser(std::ifstream &f, HeaderPair index, SceneInfo info) {
	sceneInfo = info;

	f.seekg(index.offset, std::ios_base::beg);
	
	dataLength = index.count;
	bytecode = new unsigned char[dataLength];
	
	f.read((char*) bytecode, dataLength);
	if (f.fail()) {
		Logger::Log(Logger::ERROR) << "Tried to read " << dataLength << " bytes, got" << f.gcount() << std::endl;
		throw std::exception();
	}
	Logger::Log(Logger::DEBUG) << "Read " << f.gcount() << " bytes of bytecode." << std::endl;
}

BytecodeParser::~BytecodeParser() {
	delete[] bytecode;
}

unsigned int BytecodeParser::getInt() {
	unsigned int value = 0;
	if (progInfo.programCounter + 4 <= dataLength) {
		value = readUInt32(bytecode + progInfo.programCounter);
		progInfo.programCounter += 4;
	} else {
		throw std::out_of_range("Buffer out of data");
	}
	return value;
}
unsigned char BytecodeParser::getChar() {
	unsigned char value = 0;
	if (progInfo.programCounter < dataLength) {
		value = bytecode[progInfo.programCounter++];
	} else {
		throw std::out_of_range("Buffer out of data");
	}
	return value;
}


// Handle instructions
void BytecodeParser::handle01() {
	Logger::Log(Logger::DEBUG) << "Stack? Index? " << std::hex << getInt() << std::endl;
}

void BytecodeParser::handleEval() {
	StackValue output, var;
	unsigned int arrIndex = 0;
	while (!progInfo.stack.empty()) {
		var = progInfo.stack.back();
		progInfo.stack.pop_back();

		if (var.type == STACK_FRAME) {
			break;
		}

		switch (var.value >> 24) {
			case 0x00: {
				arrIndex = var.value;
			} break;
			case 0x7f: {
				// Variable
				unsigned int varIndex = var.value & 0x00FFFFFF;
				if (varIndex >= sceneInfo.scriptVars.size())
					throw std::logic_error("Variable index out of bound");

				ScriptVar &variable = sceneInfo.scriptVars[varIndex];

				Logger::Log(Logger::INFO) << "Loading value of " << variable.name << ", type: " << variable.type << std::endl;

				output = StackValue(0xdeadbeef, variable.type);
			} break;

		}
	}
	progInfo.stack.push_back(output);
	//Logger::Log(Logger::ERROR) << var.value << ", " << var.type << " is not supported yet.\n";
}

void BytecodeParser::handle08() {
	progInfo.stack.push_back(StackValue(progInfo.programCounter, STACK_FRAME));
}

void BytecodeParser::handlePush() {
	unsigned int type = getInt();
	unsigned int value = getInt();

	progInfo.stack.push_back(StackValue(value, type));
}

void BytecodeParser::handlePop() {
	if (progInfo.stack.empty())
		throw std::logic_error("Popping empty stack");

	unsigned int type = getInt();

	if (progInfo.stack.back().type != type) {
		Logger::Log(Logger::WARN) << "Stack top has type " << std::hex << progInfo.stack.back().type << ", expected " << type << std::endl;
	}

	progInfo.stack.pop_back();
}

void BytecodeParser::handleDup() {
	if (progInfo.stack.empty())
		throw std::logic_error("Duplicating empty stack");

	StackValue stackTop = progInfo.stack.back();
	unsigned int type = getInt();
	if (stackTop.type != type) {
		Logger::Log(Logger::WARN) << "Stack top has type " << std::hex << stackTop.type << ", expected " << type << std::endl;
	}

	progInfo.stack.push_back(stackTop);
}

void BytecodeParser::handleJump(unsigned char condition) {
	if (condition == 0x11 || condition == 0x12) {
		if (progInfo.stack.empty())
			throw std::logic_error("Empty stack to test jump");

		StackValue testVal = progInfo.stack.back();
		progInfo.stack.pop_back();

		if (testVal.type != STACK_NUM) {
			Logger::Log(Logger::WARN) << "Testing against type " << testVal.type << std::endl;
		}

		if (condition == 0x11 && testVal.value == 0) {
			Logger::Log(Logger::DEBUG) << "Jumping because equal - ";
		} else if (condition == 0x12 && testVal.value != 0) {
			Logger::Log(Logger::DEBUG) << "Jumping because not equal - ";
		} else {
			return;
		}
	}


	unsigned int labelIndex = getInt();
	auto labelIt = std::find_if(sceneInfo.labels.begin(), sceneInfo.labels.end(), [labelIndex](Label l) {
		return l.index == labelIndex;
	});

	if (labelIt == sceneInfo.labels.end()) {
		Logger::Log(Logger::ERROR) << "Could not find label " << std::hex << labelIndex << std::endl;
		return;
	}

	progInfo.programCounter = labelIt->offset;
	Logger::Log(Logger::INFO) << "Jumping to label " << labelIndex << std::endl;
}

void BytecodeParser::handleCalc() {	
	unsigned int type1 = getInt();
	unsigned int type2 = getInt();
	unsigned char calc = getChar();

	if (type1 != type2) {
		Logger::Log(Logger::WARN) << "Comparing " << type1 << " with " << type2 << " with operationg " << calc << std::endl;
	}

	if (progInfo.stack.size() < 2)
		throw std::logic_error("Calculating with not enough values");

	StackValue operand1 = progInfo.stack.back();
		progInfo.stack.pop_back();
	StackValue operand2 = progInfo.stack.back();
		progInfo.stack.pop_back();

	if (operand1.type != type1)
		Logger::Log(Logger::WARN) << "Operand1 has type " << std::hex << operand1.type << ", expected " << type1 << std::endl;
	if (operand2.type != type2)
		Logger::Log(Logger::WARN) << "Operand2 has type " << std::hex << operand2.type << ", expected " << type2 << std::endl;

	switch (calc) {
		case 0x10:
			progInfo.stack.push_back(StackValue(operand1.value - operand2.value, STACK_NUM));
		break;
		default:
			Logger::Log(Logger::DEBUG) << "Performing operation " << calc << " on (" << operand1.value << " , " << operand2.value << " )\n";
			progInfo.stack.push_back(StackValue(0xDEADBEEF, type1));
	}
}

