// TODO: function backup? offset? table index thing
// TODO: check the ranges are right

// TODO: show raw bytes

// TODO (but in the future): recognize control structure: switch is
// [05]
// dup stack, push val
// cmp stack, je
// pop stack

#define __USE_MINGW_ANSI_STDIO 0
//#define _GLIBCXX_DEBUG

#include <cstdio>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>

#include <cassert>
#include <unistd.h>
#include <getopt.h>

// maybe put logger by itself
#include "Helper.h"
#include "Structs.h"

struct Instruction {
	unsigned int address;
	unsigned char opcode;
	std::vector<unsigned int> args;
	bool nop = false;
	std::string comment;
};

struct Label {
	unsigned int offset;
	unsigned int index;
	std::string name;
};

struct LabelData {
	std::vector<Label> labels, markers;
	std::vector<Label> functions, commands;
	std::vector<Label> functionTable;
};

inline bool compOffset(Label a, Label b) { return (a.offset < b.offset); }

typedef std::vector<Instruction> Instructions;
typedef std::vector<unsigned int> ProgStack;	// want strong typedef

void readScriptHeader(std::ifstream &f, ScriptHeader &header) {
	f.read((char*) &header.headerSize, 4);
	if (header.headerSize != 0x84) {
		Logger::Log(Logger::ERROR) << "Error: Expected script header size 0x84, got 0x" << std::hex << header.headerSize << std::endl;
		throw std::exception();
	}
	readHeaderPair(f, header.bytecode);	// be consistent with naming byteCode vs bytecode
	readHeaderPair(f, header.stringIndex);
	readHeaderPair(f, header.stringData);
	
	readHeaderPair(f, header.labels);
	readHeaderPair(f, header.markers);
	
	readHeaderPair(f, header.functionIndex);    
	readHeaderPair(f, header.unknown1);
	readHeaderPair(f, header.stringsIndex1);    
	readHeaderPair(f, header.strings1);
	
	readHeaderPair(f, header.functions);
	readHeaderPair(f, header.functionNameIndex);
	readHeaderPair(f, header.functionName);
	
	readHeaderPair(f, header.varStringIndex);
	readHeaderPair(f, header.varStringData);
	
	readHeaderPair(f, header.unknown6);
	readHeaderPair(f, header.unknown7);
}


class BytecodeParser {
	private:
		unsigned char* bytecode = NULL;
		unsigned int dataLength = 0;
		unsigned int currAddress = 0;
		// bastard rule of 3/5
		BytecodeParser(const BytecodeParser& src);
    BytecodeParser& operator=(const BytecodeParser& src);
		
		unsigned int readArg(Instruction &inst, unsigned int argSize);
		unsigned int readArgs(Instruction &inst, ProgStack &numStack, ProgStack &strStack, bool pop = true);
	public:
		BytecodeParser(std::ifstream &f, HeaderPair index);
		//void readBytecode(std::ifstream &f, HeaderPair index);
		void parseBytecode(Instructions &instList, const StringList &strings, const StringList &strings2);
		~BytecodeParser();
};


void populateMnemonics(StringList &mnemonics) {
	std::stringstream hexStream;
	hexStream << std::hex << std::setfill('0');
	
	mnemonics.resize(256);
	// Temporary
	mnemonics[0x00] = "nop";
	for (unsigned char i = 1; i != 0; i++) {	//probs undefined behaviour, but temporary
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
	
}

void readCommands(const std::string filename, std::vector<Label> &labels) {
	std::ifstream stream(filename, std::ifstream::in | std::ifstream::binary);	// this is much longer than "rb"
	
	if (!stream.is_open()) {
		Logger::Log(Logger::WARN) << "Could not open file " << filename << std::endl;
		return;
	}
	
	
	Label label;
	stream.read((char*) &label.offset, 4);
	stream.read((char*) &label.index, 4);
	while (!stream.eof()) {
		std::getline(stream, label.name, '\0');
		labels.push_back(label);
		stream.read((char*) &label.offset, 4);
		stream.read((char*) &label.index, 4);
	}
	stream.close();
}

// TODO: trying to hide ugliness (make it not ugly)
void printLabels(FILE* f, const LabelData &info, unsigned int address) {
	static std::vector<Label>::const_iterator 
		labelIt = info.labels.begin(),
		markerIt = info.markers.begin(),
		functionIt = info.functions.begin(),
		commandIt = info.commands.begin(),
		functionTableIt = info.functionTable.begin();
	
	Label label;
	
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
	while (commandIt != info.commands.end()) {
		label = *commandIt;
		if (label.offset <= address) {
			commandIt++;
		}
		if (label.offset == address) {
			fprintf(f, "\nSetCommand %x:\t; %s\n", label.index, label.name.c_str());
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
			fprintf(f, "\nSetFunction %x:\n", label.index);
		} else {
			break;
		}
	}

}


void printInstructions(Instructions &instList, StringList mnemonics, std::string filename, LabelData info) {
	// Drop some safety, not sure where the slowness is (should check)
	FILE* f = fopen(filename.c_str(), "wb");
	
	// Sort label info
	std::sort(info.labels.begin(), info.labels.end(), compOffset);
	std::sort(info.markers.begin(), info.markers.end(), compOffset);
	std::sort(info.functions.begin(), info.functions.end(), compOffset);
	std::sort(info.commands.begin(), info.commands.end(), compOffset);
	std::sort(info.functionTable.begin(), info.functionTable.end(), compOffset);
	
	Instructions::iterator it;
	
	Logger::Log(Logger::INFO) << "Printing " << instList.size() << " instructions.\n";
	
	Instruction instruction;
	unsigned int numNops = 0;
	for (it = instList.begin(); it != instList.end(); it++) {
		instruction = *it;
		// Check for labels, markers and functions
		printLabels(f, info, instruction.address);
		
		if (instruction.nop) {
			numNops++;
		}
		// TODO: print these to another file
		
		
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
	
	Logger::Log(Logger::INFO) << numNops << " NOP instructions skipped.\n";	
	fclose(f);
}

void readLabels(std::ifstream &stream, std::vector<Label> &labels, HeaderPair index) {
	Label label;
	
	labels.reserve(index.count);
	stream.seekg(index.offset, std::ios_base::beg);
	for (unsigned int i = 0; i < index.count; i++) {
		label.index = i;
		stream.read((char*) &label.offset, 4);
		labels.push_back(label);
	}
}

LabelData readLabelData(std::ifstream &stream, ScriptHeader header, const std::string filename) {
	LabelData info;
	
	// Read markers
	readLabels(stream, info.labels, header.labels);
	readLabels(stream, info.markers, header.markers);
	readLabels(stream, info.functions, header.functions);
	
	// Read function names
	StringList functionNames = readStrings(stream, header.functionNameIndex, header.functionName);
	
	// TODO: I could do these when reading the header
	assert(header.functionNameIndex.count == header.functions.count);		// get rid of the asserts too
	for (auto it = info.functions.begin(); it != info.functions.end(); it++) {
		(*it).name = functionNames.at(it - info.functions.begin());	// string manipulation scares me
	}
	
	// Read function index
	info.functionTable.reserve(header.functionIndex.count);
	stream.seekg(header.functionIndex.offset, std::ios_base::beg);
	Label label;
	for (unsigned int i = 0; i < header.functionIndex.count; i++) {
		stream.read((char*) &label.index, 4);
		stream.read((char*) &label.offset, 4);
		info.functionTable.push_back(label);
	}
	
	// Read commands
	readCommands(filename + ".commands", info.commands);
	
	return info;
}

int main(int argc, char* argv[]) {
	extern char *optarg;
	extern int optind;
	
	std::string outFilename;
	static char usageString[] = "Usage: parsess [-o outfile] [-v] <input.ss>";
	
	int option = 0;
	while ((option = getopt(argc, argv, "o:v")) != -1) {
		switch (option) {
		case 'v':
			Logger::increaseVerbosity();
			break;
		case 'o':
			outFilename = std::string(optarg);
		break;
		default:
			std::cout << usageString << std::endl;
			return 1;
		}
	}
	
	if (optind >= argc) {
		std::cout << usageString << std::endl;
		return 1;
	}
	std::cout << std::setfill('0');
	
	std::string filename(argv[optind]);
	std::ifstream fileStream(filename, std::ifstream::in | std::ifstream::binary);
	if (!fileStream.is_open()) {
		Logger::Log(Logger::ERROR) << "Could not open file " << filename;
		return 1;
	}
	
	if (outFilename.empty())
		outFilename = filename + std::string(".asm");
	
	ScriptHeader header;
	readScriptHeader(fileStream, header);
	
	StringList mainStrings = readStrings(fileStream, header.stringIndex, header.stringData, true);
	StringList strings1 = readStrings(fileStream, header.stringsIndex1, header.strings1);
	StringList varStrings = readStrings(fileStream, header.varStringIndex, header.varStringData);
	
	Logger::Log(Logger::INFO) << strings1;
	
	// Read labels, markers, functions, commands
	LabelData controlInfo = readLabelData(fileStream, header, filename);
	
	Instructions instList;
	
	BytecodeParser parser(fileStream, header.bytecode);
	parser.parseBytecode(instList, mainStrings, varStrings);
	
	// TODO: handle these
	if (header.unknown6.count != 0) {
		Logger::Log(Logger::WARN) << "Unknown6 is not empty.\n";
	} else if (header.unknown7.count != 0) {
		Logger::Log(Logger::WARN) << "Unknown7 is not empty.\n";
	} else if (header.unknown6.offset != header.unknown7.offset) {
		Logger::Log(Logger::WARN) << "Check this file out, something's weird\n";
	}
	fileStream.close();
	
	
	StringList mnemonics(0x100);
	populateMnemonics(mnemonics);
	
	try {
		printInstructions(instList, mnemonics, outFilename, controlInfo);
	} catch(std::exception &e) {
		std::cerr << e.what() << std::endl;
	}
	
	return 0;
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
	unsigned int arg;
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
void BytecodeParser::parseBytecode(Instructions &instList, const StringList &strings, const StringList &strings2) {
	unsigned int arg, arg1, arg2;
	unsigned char opcode;
	ProgStack numStack, strStack;
	unsigned int stackTop;
	bool commandStacking = false;
	unsigned int commandStackSize = 0;
	
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
				commandStacking = false;
				while (commandStackSize --> 1) {
					if (!numStack.empty())
						numStack.pop_back();
				}
				commandStackSize = 0;
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
				commandStacking = false;
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
					if (commandStacking)
						commandStackSize++;
				} else if (arg1 == 0x14) {
					inst.comment = strings.at(arg2);	// Check in range?
					strStack.push_back(arg2);
				}
			break;
			case 0x08:
				commandStacking = true;
				commandStackSize = 0;
			break;
			case 0x15:
				readArgs(inst, numStack, strStack);
			break;
			case 0x30:
				arg = readArg(inst);
				readArgs(inst, numStack, strStack);
				readArgs(inst, numStack, strStack, false);
								
				readArg(inst);
				// hacky
				stackTop = numStack.back();
				//if ((numStack.top() & 0x7e000000) == 0) {
				if (arg == 0x01) {
					if (std::find(std::begin(stackArr), std::end(stackArr), stackTop) != std::end(stackArr)){
						readArg(inst);
					}
				}
				//if (arg == 0x0a)
					//numStack.push_back(0x02);
				commandStacking = false;
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