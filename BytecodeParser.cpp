// Parse the bytecode
// A lot of this is stolen from tanuki
// https://github.com/bitprime/vn_translation_tools/blob/master/rewrite/
// also Inori

#include <cstdio>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <exception>

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

static StringList s_mnemonics = populateMnemonics();

typedef std::vector<unsigned int> ProgStack;


// TODO: trying to hide ugliness (make it not ugly)
// TODO: rewrite this whole file
// TODO: not just file, the whole thing should be refactored actually
/*void printLabels(FILE* f, const LabelData &info, const std::vector<ScriptCommand> commands, unsigned int address) {
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

}*/

unsigned int readArgs(BytecodeBuffer &buf, std::vector<unsigned int> &argList, ProgStack &numStack, ProgStack &strStack, bool pop = true) {
	unsigned int numArg = buf.getInt();
	unsigned int arg = 0;

	/*if (dataLength - currAddress < 4 * numArg) {
		Logger::Log(Logger::ERROR) << "Going to overflow." << std::endl;
		throw std::exception();
	}*/

	for (unsigned int counter = 0; counter < numArg; counter++) {
		arg = buf.getInt();
		argList.push_back(arg);
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
				Logger::Log(Logger::WARN, buf.getAddress()) << " trying to pop stack " << arg << std::endl;
			}
		}
	}

	return numArg;
}

// TODO: flag to print raw (might be long)
void parseBytecode(BytecodeBuffer &buf, 
	std::string filename, SceneInfo sceneInfo,
	const StringList &strings, const StringList &strings2
) {
	FILE* f = fopen(filename.c_str(), "wb");
	
	unsigned int arg, arg1, arg2;
	unsigned int numInsts = 0, numNops = 0;
	unsigned char opcode;
	ProgStack numStack, strStack;
	unsigned int stackTop;
	std::vector<unsigned int> commandStacks;
	commandStacks.push_back(0);
	
	std::string comment;
	
	std::vector<unsigned int> args;
	
	unsigned int stackArr[] = {
		0x02, //0x08,
		0x0c, //0x0d
		0x12, //0x1e,
		0x4c, // making a selection (choice)?
		0x4d 
	};
	
	std::vector<ScriptCommand> localCommands;
	auto predCommandFile = [sceneInfo](ScriptCommand c) {
		return c.file == sceneInfo.thisFile;
	};
	std::copy_if(sceneInfo.commands.begin(), sceneInfo.commands.end(), 
		std::back_inserter(sceneInfo.commands), predCommandFile);
	
	
	ScriptCommand command;
	Logger::Log(Logger::INFO) << "Parsing " << std::to_string(buf.size()) << " bytes.\n";
	
	while (!buf.done()) {
		opcode = buf.getChar();
		numInsts++;
		
		// Print labels and commands
		
		
		fprintf(f, "%#06x>\t%02x| %s\t", buf.getAddress(), opcode, s_mnemonics[opcode].c_str());
		
		switch (opcode) {
			case 0x01:
				fprintf(f, "%#x", buf.getInt());
			break;
			case 0x02:
				arg1 = buf.getInt();
				fprintf(f, "%#x, ", arg1);
				arg2 = buf.getInt();
				if (arg1 == 0x0a) {
					numStack.push_back(arg2);
					fprintf(f, "%#x", arg2);
					commandStacks.back()++;
					if (arg2 >> 24 == 0x7e) {
						unsigned int commandIndex = arg2 & 0x00ffffff;
						if (commandIndex < sceneInfo.commands.size()) {
							command = sceneInfo.commands.at(commandIndex);
							comment = command.name;
							// TODO: check when loading that sceneNames is big enough
							if (commandIndex < sceneInfo.numGlobalCommands) {
								try {
									comment += " (" + sceneInfo.sceneNames.at(command.file) + ")";
								} catch(std::exception &e) {
									Logger::Log(Logger::ERROR, buf.getAddress()) << ", scene index " << std::dec << command.file << " out of bounds\n";
								}
							} else {
							}
						} else {
								Logger::Log(Logger::WARN, buf.getAddress()) << " trying to access command index " << arg2 << std::endl;
						}
					}
				} else if (arg1 == 0x14) {
					fprintf(f, "[%#x]", arg2);
					if (arg2 <= strings.size()) {
						comment = strings.at(arg2);
					} else {
						Logger::Log(Logger::WARN, buf.getAddress()) << " trying to access string index "<< arg2 << std::endl;
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
				fprintf(f, "%#x", buf.getInt());
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
				Logger::Log(Logger::INFO, buf.getAddress()) << "End of script reached.\n";
			break;
			case 0x07:
				buf.getInt();
				arg = buf.getInt();
				fprintf(f, "%#x", arg);
				if (arg == 0x0a) {
					if (!numStack.empty())
						numStack.pop_back();
					Logger::Log(Logger::DEBUG, buf.getAddress()) << "Popping 0xa\n";
				} else if (arg == 0x14) {
					try {
						comment = strings2.at(arg);
						if (!strStack.empty())
							strStack.pop_back();
					} catch (std::out_of_range &e) {
						Logger::Log(Logger::WARN) << "Missing string id " << arg << " at 0x" << std::hex << buf.getAddress() << std::dec << std::endl;
					}
				}
			break;
			case 0x13:
				fprintf(f, "%#x, %#x", buf.getInt(), buf.getInt());
			break;
			case 0x14:
				fprintf(f, "%#x, %#x, %#x", buf.getInt(), buf.getInt(), buf.getInt());
			break;
			case 0x20:
				fprintf(f, "%#x, %#x, %#x", buf.getInt(), buf.getInt(), buf.getInt());
				if (commandStacks.size() > 1) {	
					commandStacks.pop_back();	// not sure if should close all
				}
			break;
			case 0x21:
				fprintf(f, "%#x, %d", buf.getInt(), buf.getChar());
			break;
			case 0x22:	// calc. 0x10 is subtract?
				arg1 = buf.getInt();
				arg2 = buf.getInt();
				fprintf(f, "%#x, %#x, %d", arg1, arg2, buf.getChar());
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
				arg = readArgs(buf, args, numStack, strStack);
				for (unsigned int i = 0; i < arg; i++) {
					fprintf(f, "%#x,", args.back());
					args.pop_back();
				}
			break;
			case 0x30:
				arg1 = buf.getInt();
				fprintf(f, "%#x, (", arg1);
				arg = readArgs(buf, args, numStack, strStack);
				for (unsigned int i = 0; i < arg; i++) {
					fprintf(f, "%#x,", args.back());
					args.pop_back();
				}
				fprintf(f, "), (");
				arg = readArgs(buf, args, numStack, strStack, false);
				for (unsigned int i = 0; i < arg; i++) {
					fprintf(f, "%#x,", args.back());
					args.pop_back();
				}
				fprintf(f, "), %#x", buf.getInt());

				stackTop = numStack.back();
				//if ((numStack.top() & 0x7e000000) == 0) {
				if (arg1 == 0x01) {
					if (std::find(std::begin(stackArr), std::end(stackArr), stackTop) != std::end(stackArr)){
						fprintf(f, "), %#x", buf.getInt());
					} else {
						Logger::Log(Logger::DEBUG, buf.getAddress()) << " stacktop is " << stackTop << std::endl;
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
				numNops++;
				Logger::Log(Logger::INFO) << "Address 0x" << std::hex << std::setw(4) << buf.getAddress();
				Logger::Log(Logger::INFO) << ":  NOP encountered: 0x" << std::setw(2) << opcode << std::dec << std::endl;
		}
		
		if (!comment.empty()) {
			fprintf(f, "\t; %s", comment.c_str());
			comment.clear();
		}
		
		fprintf(f, "\n");
	}
	
	Logger::Log(Logger::INFO) << "Parsed " << numInsts << " instructions.\n";
	if (numNops > 0) {
		Logger::Log(Logger::WARN) << "Warning: " << numNops << " NOP instructions skipped.\n";
	}

	fclose(f);
}

// Handle the reading and ownership of the bytecode buffer
BytecodeBuffer::BytecodeBuffer(std::ifstream &f, HeaderPair index) {
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

BytecodeBuffer::~BytecodeBuffer() {
	delete[] bytecode;
}

unsigned int BytecodeBuffer::size() {
	return dataLength;
}
// buffer shouldn't be handling address?
unsigned int BytecodeBuffer::getInt() {
	unsigned int value = 0;
	if (currAddress + 4 <= dataLength) {
		value = readUInt32(bytecode + currAddress);
		currAddress += 4;
	} else {
		throw std::out_of_range("Buffer out of data");
	}
	return value;
}
unsigned char BytecodeBuffer::getChar() {
	unsigned char value = 0;
	if (currAddress < dataLength) {
		value = bytecode[currAddress++];
	} else {
		throw std::out_of_range("Buffer out of data");
	}
	return value;
}

unsigned int BytecodeBuffer::getAddress() {
	return currAddress;
}

bool BytecodeBuffer::done() {
	return (currAddress >= dataLength);
}