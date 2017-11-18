// Parse the bytecode
// A lot of this is stolen from tanuki
// https://github.com/bitprime/vn_translation_tools/blob/master/rewrite/
// also Inori

#include <cstdio>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <exception>
#include <algorithm>

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

void printLabels(FILE* f, std::vector<Label>::iterator &pLabel, std::vector<Label>::iterator end, unsigned int address, const char* type) {
	Label label;
	while (pLabel != end) {
		label = *pLabel;
		if (label.offset <= address) {
			pLabel++;
		}
		if (label.offset > 0 && label.offset == address) {
			fprintf(f, "\nSet%s %x:\t; %s\n", type, label.index, label.name.c_str());
		} else {
			//if (verbose) {
			//	Logger::Log(Logger::DEBUG) << type << " " << labe.offset << " > " << std::hex << address << ", index ";
			//	Logger::Log(Logger::DEBUG) << label.index << ": " << label.name.c_str() << std::endl;
			//}
			break;
		}
	}
}

unsigned int readArgs(BytecodeBuffer &buf, std::vector<unsigned int> &argList, ProgStack &numStack, ProgStack &strStack, bool pop = true) {
	unsigned int numArg = buf.getInt();
	unsigned int arg = 0, stackCount = 0;

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
			} else if (arg == 0xffffffff) {
				stackCount = buf.getInt();
				for (unsigned int counter2 = 0; counter2 < stackCount; counter2++) {
					arg = buf.getInt();
					argList.push_back(arg);
				}
				argList.push_back(0xffffffff);
			} else {
				// actually wrong address, but figure it out
				Logger::Log(Logger::DEBUG, buf.getAddress()) << " trying to pop stack " << arg << std::endl;
			}
		}
	}

	return numArg;
}

// TODO: flag to print raw (might be long)

// TODO: there's a bug here, but it's very random
// appears once every few times
// find it and squash it
void parseBytecode(BytecodeBuffer &buf, 
	std::string filename, SceneInfo sceneInfo,
	const StringList &strings, const StringList &varStrings
) {
	FILE* f = fopen(filename.c_str(), "wb");
	
	unsigned int arg, arg1, arg2;
	unsigned int numInsts = 0, numNops = 0;
	unsigned char opcode;
	unsigned int instAddress;
	ProgStack numStack, strStack;
	unsigned int stackTop;
	std::vector<unsigned int> commandStacks;
	commandStacks.push_back(0);
	
	std::string comment;
	
	std::vector<unsigned int> args;
	
	std::vector<Label> localCommands;
	auto predCommandFile = [sceneInfo](ScriptCommand c) {
		return c.file == sceneInfo.thisFile;
	};
	std::copy_if(sceneInfo.commands.begin(), sceneInfo.commands.end(), 
		std::back_inserter(localCommands), predCommandFile);
	
	std::sort(sceneInfo.labels.begin(),		sceneInfo.labels.end());
	std::sort(sceneInfo.markers.begin(),	sceneInfo.markers.end());
	std::sort(sceneInfo.functions.begin(),sceneInfo.functions.end());
	std::sort(localCommands.begin(),	localCommands.end());
	
	// TODO: pretty up, everything
	auto labelIt = sceneInfo.labels.begin(),
		markerIt = sceneInfo.markers.begin(),
		functionIt = sceneInfo.functions.begin(),
		commandIt = localCommands.begin();

	ScriptCommand command;
	Logger::Log(Logger::INFO) << "Parsing " << std::to_string(buf.size()) << " bytes.\n";
	
	while (!buf.done()) {
		// Get address before incrementing
		instAddress = buf.getAddress();
		opcode = buf.getChar();
		numInsts++;
		
		// Print labels and commands
		printLabels(f, labelIt, sceneInfo.labels.end(), instAddress, "Label");
		printLabels(f, markerIt, sceneInfo.markers.end(), instAddress, "Marker");
		printLabels(f, functionIt, sceneInfo.functions.end(), instAddress, "Function");
		printLabels(f, commandIt, localCommands.end(), instAddress, "Command");
		
		fprintf(f, "%#06x>\t%02x| %s ", instAddress, opcode, s_mnemonics[opcode].c_str());
		
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
							std::stringstream stream;
							stream << std::hex << command.offset;
							// TODO: check when loading that sceneNames is big enough
							if (commandIndex < sceneInfo.numGlobalCommands) {
								try {
									comment += " (0x" + stream.str() + " in " + sceneInfo.sceneNames.at(command.file) + ")";
								} catch(std::exception &e) {
									Logger::Log(Logger::ERROR, instAddress) << ", scene index " << std::dec << command.file << " out of bounds\n";
								}
							} else {
								// this is a local command
								comment += " (0x" + stream.str() + " in " + sceneInfo.sceneNames.at(command.file) + ")";
							}
						} else {
								Logger::Log(Logger::WARN, instAddress) << " trying to access command index " << arg2 << std::endl;
						}
					} else if (arg2 >> 24 == 0x7f) {
						unsigned int varIndex = arg2 & 0x00ffffff;
						if (varIndex < sceneInfo.varNames.size()) {
							comment = sceneInfo.varNames.at(varIndex);
						} else {
								Logger::Log(Logger::WARN, instAddress) << " trying to access var index " << arg2 << std::endl;
						}
					}
				} else if (arg1 == 0x14) {
					fprintf(f, "[%#x]", arg2);
					if (arg2 <= strings.size()) {
						comment = strings.at(arg2);
					} else {
						Logger::Log(Logger::WARN, instAddress) << " trying to access string index " << arg2 << std::endl;
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
				if (commandStacks.size() > 1) {
					commandStacks.pop_back();
					commandStacks.back()++;	// collapse into lower frame
				} else {
					Logger::Log(Logger::ERROR, instAddress) << " Tried to pop base frame.\n";
				}
			break;
			case 0x06:
			case 0x09:
			case 0x32:
			break;
			case 0x16:
				Logger::Log(Logger::INFO, instAddress) << "End of script reached.\n";
			break;
			case 0x07:
				arg1 = buf.getInt();
				arg = buf.getInt();
				fprintf(f, "%#x, %#x", arg1, arg);
				if (arg1 == 0x0a) {
					if (!numStack.empty())
						numStack.pop_back();
					//Logger::Log(Logger::DEBUG, instAddress) << "Popping 0xa\n";
				} else if (arg1 == 0x14) {
					if (!strStack.empty())
							strStack.pop_back();
				}
				try {
					comment = varStrings.at(arg);	
				} catch (std::out_of_range &e) {
					Logger::Log(Logger::WARN) << "Missing string id " << arg << " at 0x" << std::hex << instAddress << std::dec << std::endl;
				}
			break;
			case 0x13:
				fprintf(f, "%#x ", buf.getInt());
				arg = readArgs(buf, args, numStack, strStack);
				fprintf(f, "(");
				for (unsigned int i = 0; i < arg; i++) {
					fprintf(f, "%#x,", args.back());
					args.pop_back();
				}
				fprintf(f, ")");
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
				fprintf(f, "(");
				for (unsigned int i = 0; i < arg; i++) {
					fprintf(f, "%#x,", args.back());
					args.pop_back();
				}
				fprintf(f, ")");
			break;
			case 0x30: {
				arg1 = buf.getInt();
				fprintf(f, "%#x, (", arg1);
				unsigned int count = readArgs(buf, args, numStack, strStack);
				for (unsigned int i = 0; i < count; i++) {
					arg = args.back();
					args.pop_back();
					if (arg != 0xffffffff) {
						fprintf(f, "%#x,", arg);
					} else {
						fprintf(f, "{");
						while ((arg = args.back()) != 0xffffffff) {
							fprintf(f, "%#x,", arg);
							args.pop_back();
						}
						args.pop_back();	// pop off the first 0xffffffff
						fprintf(f, "}, ");
					}
				}
				fprintf(f, "), (");
				arg = readArgs(buf, args, numStack, strStack, false);
				for (unsigned int i = 0; i < arg; i++) {
					fprintf(f, "%#x,", args.back());
					args.pop_back();
				}
				arg2 = buf.getInt();
				fprintf(f, "), %#x", arg2);

				stackTop = numStack.back();
				// wary about determining function call address
				// maybe later
				fprintf(f, " [%#x] ", stackTop);
				
				switch (stackTop) {
					// TODO: "understand" what these mean
					case 0x0c:
					//Yes- 0, (a), (), 0
					//No - 0, ( ), (), a
					//No - 1, (a), (), 0
					//No - 0, ( ), (), 0
						if (arg1 == 0 && arg2 == 0x00 && count == 1) {	// or at least not 0xa
							fprintf(f, ", %#x", buf.getInt());
						}
					break;
					case 0x12:
						if (arg2 == 0x00) {	// or at least not 0xa
							fprintf(f, ", %#x", buf.getInt());
						}
					break;
					case 0x4c:
						//No - 0, (a       ), (), a
						//Yes- 1, (a,14,14,), (), a
						if (arg1 == 0x01)
							fprintf(f, ", %#x", buf.getInt());
					break;
					case 0x4d:
						//No - 0, (a), (), a
						if (arg2 == 0)
							fprintf(f, ", %#x", buf.getInt());
					break;
					case 0x5a:
						//Yes- 0, (a), (), 0
						//No - 0, ( ), (), 0
						if (count > 0)
							fprintf(f, ", %#x", buf.getInt());
					break;
					case 0x5b:
						//Yes-1, (a,a), (), a
						//Yes-0, (a  ), (), a
						//No -0, (   ), (), a 
						if (count > 0)
							fprintf(f, ", %#x", buf.getInt());
					break;
					case 0xffffffff:	// i dunno about this
						//Yes- 0, (0xa,), (), 0
						//No - 0, (0xa,0xa,0xa,0xa,), (), 0 
						//No - 0, (0xa,0xa,), (), 0
						if (count == 1)	
							fprintf(f, ", %#x", buf.getInt());
					break;
				}
				
				if (commandStacks.size() > 1) {
					commandStacks.pop_back();
				}
				
				// something, don't know yet
				if (arg == 0x0a) {
					numStack.push_back(0);
					commandStacks.back()++;
				} else if (arg == 0x14) {
					strStack.push_back(0);
				}
			} break;
			case 0x00:
			default:
				numNops++;
				Logger::Log(Logger::INFO) << "Address 0x" << std::hex << std::setw(4) << instAddress;
				Logger::Log(Logger::INFO) << ":  NOP encountered: 0x" << std::setw(2) << +opcode << std::dec << std::endl;
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