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

typedef struct ProgramInfo {
	// State info
	ProgStack numStack, strStack;
	ProgStack stackPointers = {0};
	
	// instruction info
	unsigned char opcode = 0;
	unsigned int address = 0;
	ProgStack args;
	std::string comment;
	
	// local info
	unsigned int numInsts = 0;
	unsigned int numNops = 0;
} ProgInfo;

void printLabels(FILE* f, std::vector<Label>::iterator &pLabel, std::vector<Label>::iterator end, unsigned int address, const char* type) {
	Label label;
	while (pLabel != end) {
		label = *pLabel;
		if (label.offset <= address) {
			pLabel++;
			if (label.offset > 0 && label.offset == address) {
				fprintf(f, "\nSet%s %x:\t; %s\n", type, label.index, label.name.c_str());
			}
		} else {
			break;
		}
	}
}


//*******
// Stuff
//*******
unsigned int readArgs(BytecodeBuffer &buf, ProgInfo &progInfo, bool pop = true) {
	unsigned int numArgs = buf.getInt();
	unsigned int arg = 0, stackCount = 0;

	for (unsigned int counter = 0; counter < numArgs; counter++) {
		arg = buf.getInt();
		progInfo.args.push_back(arg);
		if (pop) {
			if (arg == 0x0a) {
				if (!progInfo.numStack.empty()) {
					progInfo.numStack.pop_back();
					if (progInfo.stackPointers.back() > progInfo.numStack.size()) {
						Logger::Log(Logger::WARN, progInfo.address) << " Popped past frame.\n";
					}
				}
			} else if (arg == 0x0e) {
				if (!progInfo.numStack.empty()) {
					unsigned int var = progInfo.numStack.back();
					progInfo.numStack.pop_back();
					if (var >> 24 != 0x7f) {
						Logger::Log(Logger::DEBUG, progInfo.address) << " Type 0xe: " << std::hex << var << std::endl;
					}	
					if (progInfo.stackPointers.back() > progInfo.numStack.size()) {
						Logger::Log(Logger::WARN, progInfo.address) << " Popped past frame.\n";
					}
				}
			} else if (arg == 0x14) {
				if (!progInfo.strStack.empty()) {
					progInfo.strStack.pop_back();
				}
			} else if (arg == 0x51e) {
				Logger::Log(Logger::DEBUG, progInfo.address) << " Popping 0x51e - " << std::hex;
				unsigned int a1 = 0xdeadbeef, a2 = 0xdeadbeef, a3 = 0xdeadbeef;
				if (progInfo.numStack.size() >= 4) {
					progInfo.numStack.pop_back();
					a1 = progInfo.numStack.back();
						progInfo.numStack.pop_back();
					a2 = progInfo.numStack.back();
						progInfo.numStack.pop_back();
					a3 = progInfo.numStack.back();
						progInfo.numStack.pop_back();
				} else {
					throw std::exception();
				}
				if (a1 != 0xffffffff || a2 != 2)
					Logger::Log(Logger::WARN) << a3 << " " << a2 << " " << a1 << std::endl;
			} else if (arg == 0xffffffff) {
				stackCount = buf.getInt();
				for (unsigned int counter2 = 0; counter2 < stackCount; counter2++) {
					progInfo.args.push_back(buf.getInt());
				}
				progInfo.args.push_back(0xffffffff);
			} else {
				Logger::Log(Logger::DEBUG, progInfo.address) << " trying to pop stack " << arg << std::endl;
			}
		}
	}

	return numArgs;
}

// TODO: move this either into a class or another file, just have the declarations together or something
void instPush(FILE* f, BytecodeBuffer &buf, SceneInfo& sceneInfo, ProgInfo& progInfo) {
	unsigned int	arg1, arg2;
		arg1 = buf.getInt();
		arg2 = buf.getInt();

	if (arg1 == 0x0a) {
		progInfo.numStack.push_back(arg2);
		fprintf(f, "%#x, %#x", arg1, arg2);
		// TODO: take it out of here and put just when called
		// maybe. maybe not
		
		// Command ~address~ index
		if (arg2 >> 24 == 0x7e) {
			unsigned int commandIndex = arg2 & 0x00ffffff;
			if (commandIndex < sceneInfo.commands.size()) {
				ScriptCommand command = sceneInfo.commands.at(commandIndex);
				progInfo.comment = command.name;
				std::stringstream stream;
				stream << std::hex << command.offset;

				if (commandIndex < sceneInfo.numGlobalCommands) {
					progInfo.comment += " (0x" + stream.str() + " in " + sceneInfo.sceneNames.at(command.file) + ")";
				} else {
					// this is a local command
					progInfo.comment += " (0x" + stream.str() + ")";
				}
			} else {
					Logger::Log(Logger::WARN, progInfo.address) << " trying to access command index " << arg2 << std::endl;
			}
		// Var name index
		} else if (arg2 >> 24 == 0x7f) {
			unsigned int varIndex = arg2 & 0x00ffffff;
			if (varIndex < sceneInfo.varNames.size()) {
				progInfo.comment = sceneInfo.varNames.at(varIndex);
			} else if (arg2 == 0x7fffffff) {
			} else {
				Logger::Log(Logger::WARN, progInfo.address) << " trying to access var index " << arg2 << std::endl;
			}
		}
	} else if (arg1 == 0x14) {
		fprintf(f, "%#x, [%#x]", arg1, arg2);
		if (arg2 <= sceneInfo.mainStrings.size()) {
			progInfo.comment = sceneInfo.mainStrings.at(arg2);
		} else {
			Logger::Log(Logger::WARN, progInfo.address) << " trying to access string index " << arg2 << std::endl;
		}
		progInfo.strStack.push_back(arg2);
	}
}

// must guarantee stackPointers will never be empty
// TODO: handle 0x7d - not sure what they are, references?
// TODO: also handle 0x7d the same things on 0x20 calls
void instDo05Thing(ProgInfo& progInfo) {
	if (progInfo.stackPointers.size() > 1) {
		unsigned int var;
		while (progInfo.stackPointers.back() < progInfo.numStack.size()) {
			if (!progInfo.numStack.empty()) {
				var = progInfo.numStack.back();
				progInfo.numStack.pop_back();
			} else {
				Logger::Log(Logger::ERROR, progInfo.address) << " Popping empty stack.\n";
			}
		}
	
		progInfo.stackPointers.pop_back();
		progInfo.numStack.push_back(0xDEADBEEF);
	} else {
		Logger::Log(Logger::ERROR, progInfo.address) << " Tried to pop base frame.\n";
	}
}

void instDo07Thing(FILE* f, BytecodeBuffer &buf, SceneInfo& sceneInfo, ProgInfo& progInfo) {
	unsigned int arg1, arg2;
		arg1 = buf.getInt();
		arg2 = buf.getInt();

	fprintf(f, "%#x, %#x", arg1, arg2);
	if (arg1 == 0x0a) {
		if (!progInfo.numStack.empty()) {
			progInfo.numStack.pop_back();
		}
	} else if (arg1 == 0x14) {
		if (!progInfo.strStack.empty()) {
			progInfo.strStack.pop_back();
		}
	}
	try {
		progInfo.comment = sceneInfo.varStrings.at(arg2);	
	} catch (std::out_of_range &e) {
		Logger::Log(Logger::WARN, progInfo.address) << "Missing string id: " << arg2 << std::endl;
	}
}

void instDo13Thing(FILE* f, BytecodeBuffer &buf, ProgInfo& progInfo) {
	fprintf(f, "%#x ", buf.getInt());
	unsigned int numArgs = readArgs(buf, progInfo);
	fprintf(f, "(");
	for (unsigned int i = 0; i < numArgs; i++) {
		fprintf(f, "%#x,", progInfo.args.back());
		progInfo.args.pop_back();
	}
	fprintf(f, ")");
}

void instCalc(FILE* f, BytecodeBuffer &buf, ProgInfo& progInfo) {	
	// calc. 0x10 is subtract?
	unsigned int arg1, arg2;
		arg1 = buf.getInt();
		arg2 = buf.getInt();
	unsigned int arg3 = buf.getChar();
	fprintf(f, "%#x, %#x, %d", arg1, arg2, arg3);
	if (arg1 == 0xa && arg2 == 0xa) {
		if (!progInfo.numStack.empty()) {
			progInfo.numStack.pop_back();
		}
		// TODO: figure out the calcs and do them
	} else if (arg1 == 0x14 && arg2 == 0x14) {
		if (!progInfo.strStack.empty())
				progInfo.strStack.pop_back();
		if (arg3 == 0x01)
			progInfo.comment = "string concatenation";
	} else {
		Logger::Log(Logger::DEBUG, progInfo.address) << " Comparing " << arg1 << " and " << arg2 << std::endl;
	}
}

void instDo08Thing(ProgInfo& progInfo) {
	progInfo.stackPointers.push_back(progInfo.numStack.size());
}

void instDo15Thing(FILE* f, BytecodeBuffer &buf, ProgInfo& progInfo) {
	unsigned int numArgs = readArgs(buf, progInfo);
	fprintf(f, "(");
	for (unsigned int i = 0; i < numArgs; i++) {
		fprintf(f, "%#x,", progInfo.args.back());
		progInfo.args.pop_back();
	}
	fprintf(f, ")");
}

// TODO: is 20 a store?


// TODO: pop stack pointer if reached
void instCall(FILE* f, BytecodeBuffer &buf, ProgInfo& progInfo) {
	unsigned int arg;
	unsigned int arg1 = buf.getInt();
	fprintf(f, "%#x, (", arg1);
	
	// Arguments to pass to function to be called
	unsigned int numArgs1 = readArgs(buf, progInfo);
	for (unsigned int i = 0; i < numArgs1; i++) {
		arg = progInfo.args.back();
		progInfo.args.pop_back();
		
		if (arg != 0xffffffff) {
			fprintf(f, "%#x,", arg);
		} else {
			fprintf(f, "{");
			while ((arg = progInfo.args.back()) != 0xffffffff) {
				fprintf(f, "%#x,", arg);
				progInfo.args.pop_back();
			}
			progInfo.args.pop_back();	// pop off the first 0xffffffff
			fprintf(f, "}, ");
		}
	}
	fprintf(f, "), (");
	// I don't know what these are
	unsigned int numArgs2 = readArgs(buf, progInfo, false);
	for (unsigned int i = 0; i < numArgs2; i++) {
		fprintf(f, "%#x,", progInfo.args.back());
		progInfo.args.pop_back();
	}
	
	// Return type
	unsigned int arg2 = buf.getInt();
	fprintf(f, "), %#x", arg2);

	// Function to call
	unsigned int stackTop = progInfo.numStack.back();
	progInfo.numStack.pop_back();
	fprintf(f, " [%#x] ", stackTop);
	
	switch (stackTop) {
		// TODO: "understand" what these mean
		case 0x0c:
		//Yes- 0, (a), (), 0
		//No - 0, ( ), (), a
		//No - 1, (a), (), 0
		//No - 0, ( ), (), 0
			if (arg1 == 0 && arg2 == 0x00 && numArgs1 == 1) {	// or at least not 0xa
				fprintf(f, ", %#x", buf.getInt());
			}
		break;
		case 0x12:
			if (arg2 == 0x00) {
				fprintf(f, ", %#x", buf.getInt());
			}
		break;
		case 0x4c:
			//No - 0, (a       ), (), a
			//Yes- 1, (a,14,14,), (), a
			//Yes- 0, ( ,14,14,), (), a
			if (numArgs1 > 0)
				fprintf(f, ", %#x", buf.getInt());
		break;
		case 0x4d:
			//No - 0, (a), (), a
			if (arg2 == 0)
				fprintf(f, ", %#x", buf.getInt());
		break;
		case 0x5a:
		case 0x5b:
			if (numArgs1 > 0)
				fprintf(f, ", %#x", buf.getInt());
		break;
		// TODO: see if this is true for all
		case 0x13:
		case 0x64:
		case 0x7f:
			if (progInfo.stackPointers.back() == progInfo.numStack.size()) {
				fprintf(f, ", %#x", buf.getInt());
				progInfo.stackPointers.pop_back();
			}
		break;
		case 0xffffffff:
			Logger::Log(Logger::WARN, progInfo.address) << " Calling function 0xffffffff.\n";
		break;
		case 0xDEADBEEF:
			Logger::Log(Logger::WARN, progInfo.address) << " Calling deadbeef\n";
	}
	
	// TEMP?
	while (progInfo.stackPointers.back() < progInfo.numStack.size()) {
		progInfo.numStack.pop_back();
	}
	if (progInfo.stackPointers.size() > 1) {
		progInfo.stackPointers.pop_back();
	} else {
		Logger::Log(Logger::WARN, progInfo.address) << " Popped past.\n";
	}
	
	if (arg2 == 0x0a) {
		progInfo.numStack.push_back(3735928559);
	} else if (arg2 == 0x14) {
		progInfo.strStack.push_back(3735928559);
	}
}


void parseBytecode(BytecodeBuffer &buf, std::string filename, SceneInfo sceneInfo) {
	FILE* f = fopen(filename.c_str(), "wb");

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

	Logger::Log(Logger::DEBUG) << "Parsing " << std::to_string(buf.size()) << " bytes.\n";
	
	ProgramInfo progInfo;
	
	while (!buf.done()) {
		// Get address before incrementing
		progInfo.address = buf.getAddress();
		progInfo.opcode = buf.getChar();
		progInfo.numInsts++;
		
		//DEBUG
		/*if (instAddress >= 0x30196 && instAddress <= 0x3021c){
			std::cout << std::hex << instAddress << std::dec << std::endl;
			std::cout << "CommandStack: " << commandStacks.size() << " " << commandStacks.back() << std::endl;
			std::cout << "NumStack: " << numStack.size() << " " << numStack.back() << std::endl;
		}*/
		//END_DEBUG
		
		// Print labels and commands
		printLabels(f, labelIt, sceneInfo.labels.end(), progInfo.address, "Label");
		printLabels(f, markerIt, sceneInfo.markers.end(), progInfo.address, "Marker");
		printLabels(f, functionIt, sceneInfo.functions.end(), progInfo.address, "Function");
		printLabels(f, commandIt, localCommands.end(), progInfo.address, "Command");
		
		fprintf(f, "%#06x>\t%02x| %s ", progInfo.address, progInfo.opcode, s_mnemonics[progInfo.opcode].c_str());
		
		switch (progInfo.opcode) {
			case 0x02:	instPush(f, buf, sceneInfo, progInfo);	break;
			
			case 0x01:
			case 0x03:
			case 0x04:	// load value > ??
			case 0x10:
			case 0x11:
			case 0x12:
			case 0x31:	fprintf(f, "%#x", buf.getInt());		break;
			
			case 0x05:	instDo05Thing(progInfo);						break;
			
			case 0x06:
			case 0x09:
			case 0x32:
			case 0x33:
			case 0x34:																			break;
			
			case 0x16:	Logger::Log(Logger::INFO, progInfo.address) << "End of script reached.\n";
				break;
			case 0x07:	instDo07Thing(f, buf, sceneInfo, progInfo);		break;
			case 0x08:	instDo08Thing(progInfo);						break;

			case 0x13:	instDo13Thing(f, buf, progInfo);		break;
			case 0x14:
			case 0x20:	fprintf(f, "%#x, %#x, %#x", buf.getInt(), buf.getInt(), buf.getInt());
				break;
			case 0x21:	fprintf(f, "%#x, %d", buf.getInt(), buf.getChar());
				break;
			case 0x22:	instCalc(f, buf, progInfo);					break;

			case 0x15:	instDo15Thing(f, buf, progInfo);		break;

			case 0x30:	instCall(f, buf, progInfo);					break;

			case 0x00:
			default:
				progInfo.numNops++;
				Logger::Log(Logger::INFO) << "Address 0x" << std::hex << std::setw(4) << progInfo.address;
				Logger::Log(Logger::INFO) << ":  NOP encountered: 0x" << std::setw(2) << +progInfo.opcode << std::dec << std::endl;
		}
		
		if (!progInfo.comment.empty()) {
			fprintf(f, "\t; %s", progInfo.comment.c_str());
			progInfo.comment.clear();
		}
		
		fprintf(f, "\n");
	}
	
	Logger::Log(Logger::INFO) << "Parsed " << progInfo.numInsts << " instructions.\n";
	if (progInfo.numNops > 0) {
		Logger::Log(Logger::WARN) << "Warning: " << progInfo.numNops << " NOP instructions skipped.\n";
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