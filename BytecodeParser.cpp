#include <cstdio>
#include <iostream>
#include <iomanip>
#include <sstream>

#include "Helper.h"
#include "Structs.h"

#include "BytecodeParser.h"

StringList populateMnemonics() {
	std::stringstream hexStream;
	hexStream << std::hex << std::setfill('0');
	
	StringList mnemonics;
	mnemonics.resize(0x100);
	// Temporary
	mnemonics[0x00] = "nop";
	for (unsigned short i = 1; i < 256; i++) {
		hexStream << std::setw(2) << +i;
		mnemonics[i] = "[" + hexStream.str() + "]";
		hexStream.str("");
	}
	mnemonics[0x02] = "push";		// value onto target stack
	mnemonics[0x03] = "pop";		// and discard
	//mnemonics[0x04] = "dup";
	mnemonics[0x07] = "pop";		// value from target stack into target var
	mnemonics[0x08] = "[08]";		// does something with [05] to make it so there's 1 value left
															// long address maybe? since its followed by a call
	mnemonics[0x10] = "jmp";
	mnemonics[0x11] = "je";
	mnemonics[0x12] = "jne";
	mnemonics[0x30] = "call";
	
	return mnemonics;
}

StringList BytecodeParser::mnemonics = populateMnemonics();


// TODO: trying to hide ugliness (make it not ugly)
// TODO: rewrite this whole file
// TODO: not just file, the whole thing should be refactored actually
void printLabels(FILE* f, const LabelData &info, const std::vector<ScriptCommand> commands, unsigned int address) {
	static std::vector<Label>::const_iterator 
		labelIt = info.labels.begin(),
		markerIt = info.markers.begin(),
		functionIt = info.functions.begin(),
		functionTableIt = info.functionTable.begin();
	
	static std::vector<ScriptCommand>::const_iterator commandIt = commands.begin();
	
	Label label;
	ScriptCommand command;
	
	while (labelIt != info.labels.end()) {
		label = *labelIt;
		if (label.offset <= address) {
			labelIt++;
		}
		if (label.offset == address) {
			fprintf(f, "\nSetLabel %x:\n", label.index);
		} else {
			break;
		}
	}
	while (markerIt != info.markers.end()) {
		label = *markerIt;
		if (label.offset <= address) {
			markerIt++;
		} 
		if (label.offset > 0 && label.offset == address) {
			fprintf(f, "\nSetMarker %x:\n", label.index);
		} else {
			break;
		}
	}
	while (functionIt != info.functions.end()) {
		label = *functionIt;
		if (label.offset <= address) {
			functionIt++;
		}
		if (label.offset == address) {
			fprintf(f, "\nSetFunction %x:\t; %s\n", label.index, label.name.c_str());
		} else {
			break;
		}
	}
	while (commandIt != commands.end()) {
		command = *commandIt;
		if (command.offset <= address) {
			commandIt++;
		}
		if (command.offset == address) {
			fprintf(f, "\nSetCommand %x:\t; %s\n", command.index, command.name.c_str());
		} else {
			break;
		}
	}
	while (functionTableIt != info.functionTable.end()) {
		label = *functionTableIt;
		if (label.offset <= address) {
			functionTableIt++;
		} 
		if (label.offset == address) {
			fprintf(f, "\nSetCommand (short) %x:\n", label.index);
		} else {
			break;
		}
	}

}


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
void BytecodeParser::parseBytecode(
	Instructions &instList, 
	const StringList &strings, const StringList &strings2, 
	const SceneInfo &sceneInfo, const std::vector<ScriptCommand> &localCommands
) {
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
	
	ScriptCommand command;
	
//commandNames.at(arg2 & 0xffffff) + 

	Logger::Log(Logger::INFO) << "Parsing " << std::to_string(dataLength) << " bytes.\n";
	
	while (currAddress < dataLength) {
		Instruction inst;
		inst.address = currAddress;
		opcode = bytecode[currAddress++];
		inst.opcode = opcode;

		switch (opcode) {
			case 0x01:
				readArg(inst);
			break;
			case 0x02:
				arg1 = readArg(inst);
				arg2 = readArg(inst);
				if (arg1 == 0x0a) {
					numStack.push_back(arg2);
					commandStacks.back()++;
					if (arg2 >> 24 == 0x7e) {
						unsigned int commandIndex = arg2 & 0x00ffffff;
						if (commandIndex < sceneInfo.commands.size()) {
							command = sceneInfo.commands.at(commandIndex);
							// TODO: check when loading that sceneNames is big enough
							try {
								inst.comment = command.name + " (" + sceneInfo.sceneNames.at(command.file) + ")";
							} catch(std::exception &e) {
								Logger::Log(Logger::ERROR, inst.address) << ", scene index " << std::dec << command.file << " out of bounds\n";
							}
						} else {
							std::vector<ScriptCommand>::const_iterator found = std::find_if(
								localCommands.begin(), localCommands.end(), 
								[commandIndex](ScriptCommand command){
									return (command.index == commandIndex);
								});
							if (found != localCommands.end()) {
								inst.comment = found->name;
							} else {
								Logger::Log(Logger::WARN, inst.address) << " trying to access command index "<< arg2 << std::endl;
							}
						}
					}
				} else if (arg1 == 0x14) {
					if (arg2 <= strings.size()) {
						inst.comment = strings.at(arg2);
					} else {
						Logger::Log(Logger::WARN, inst.address) << " trying to access string index "<< arg2 << std::endl;
					}
					strStack.push_back(arg2);
				}
			break;
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

// TODO: flag to print raw

// TODO: restructure so that the parsed one reads, and then just prints what is known
void BytecodeParser::printInstructions(Instructions &instList, std::string filename, LabelData &info, const SceneInfo &sceneInfo) {
	FILE* f = fopen(filename.c_str(), "wb");
	
	std::vector<ScriptCommand> commands;
	std::copy_if(
		sceneInfo.commands.begin(), sceneInfo.commands.end(), 
		std::back_inserter(commands), 
		[sceneInfo](ScriptCommand c){
			return c.file == sceneInfo.thisFile;
		});
	std::sort(commands.begin(), commands.end(), compOffsetArgh);
	
	Instructions::iterator it;
	
	Logger::Log(Logger::DEBUG) << "Printing " << instList.size() << " instructions.\n";
	
	Instruction instruction;
	unsigned int numNops = 0;
	for (it = instList.begin(); it != instList.end(); it++) {
		instruction = *it;
		// Check for labels, markers and functions
		printLabels(f, info, commands, instruction.address);
		
		if (instruction.nop) {
			numNops++;
		}		
		
		fprintf(f, "%#06x>\t%02x| %s\t", instruction.address, instruction.opcode, mnemonics[instruction.opcode].c_str());
		
		// Don't know if I should be going through again, after already parsing once
		// maybe second parse should actually do something useful
		unsigned int counter = 0;
		switch (instruction.opcode) {
			case 0x02:
			case 0x07:
				if (instruction.args.at(0) == 0x14) {
					fprintf(f, "0x14, [%#06x]", instruction.args.at(1));
				} else {
					fprintf(f, "%#x, %#x", instruction.args.at(0), instruction.args.at(1));
				}
			break;
			case 0x30: {	// figure out a nicer way of printing arg lists
				auto argIt = instruction.args.begin();
				fprintf(f, "%#x, (", *argIt); 
				counter = *(++argIt);
				for (unsigned int i = 0; i < counter; i++) {
					fprintf(f, "%#x,", *(++argIt));
				}
				fprintf(f, ") (");
				counter = *(++argIt);
				for (unsigned int i = 0; i < counter; i++) {
					fprintf(f, "%#x,", *(++argIt));
				}
				fprintf(f, ") %#x", *(++argIt));
				if (++argIt != instruction.args.end()) {
					fprintf(f, ", %#x", *argIt);	// shouldn't be any more
				}
			} break;
			default:
				for (auto argIt = instruction.args.begin(); argIt != instruction.args.end(); argIt++) {
					if (argIt != instruction.args.begin()) fprintf(f, ", ");
					fprintf(f, "%#x", *argIt);
				}
		}

		
	
		if (!instruction.comment.empty())
			fprintf(f, "\t; %s", instruction.comment.c_str());
		
		fprintf(f, "\n");
	}
	
	if (numNops > 0) {
		Logger::Log(Logger::WARN) << numNops << " NOP instructions skipped.\n";
	}
	fclose(f);
}

BytecodeParser::~BytecodeParser() {
	if (bytecode)	// don't need check
		delete[] bytecode;
}