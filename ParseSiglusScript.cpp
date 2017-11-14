// TODO: function backup? offset? table index thing
// TODO: check the ranges are right

// TODO: show raw bytes

// TODO (but in the future): recognize control structure: switch is
// [05]
// dup stack, push val
// cmp stack, je
// pop stack

#define __USE_MINGW_ANSI_STDIO 0

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
		void readArgs(Instruction &inst);
	public:
		BytecodeParser(std::ifstream &f, HeaderPair index);
		//void readBytecode(std::ifstream &f, HeaderPair index);
		Instructions parseBytecode(const StringList &strings, const StringList &strings2);
		~BytecodeParser();
};


void populateMnemonics(StringList &mnemonics) {
	std::stringstream hexStream;
	hexStream << std::hex << std::setfill('0');
	
	mnemonics.resize(256);
	// Temporary
	mnemonics[0] = "nop";
	for (unsigned char i = 1; i != 0; i++) {	//probs undefined behaviour, but temporary
		hexStream << std::setw(2) << +i;
		mnemonics[i] = "[" + hexStream.str() + "]";
		hexStream.str("");
	}
	mnemonics[0x02] = "push";		// value onto target stack
	mnemonics[0x03] = "pop";		// and discard
	//mnemonics[0x04] = "dup";
	mnemonics[0x07] = "pop";		// value from target stack into target var
	mnemonics[0x10] = "jmp";
	mnemonics[0x11] = "je";
	mnemonics[0x12] = "jne";
	
}

void readCommands(const std::string filename, std::vector<Label> &labels) {
	std::ifstream stream(filename, std::ifstream::in | std::ifstream::binary);	// this is much longer than "rb"
	
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


void printInstructions(Instructions instList, StringList mnemonics, std::string filename, LabelData info) {
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
		
		// TODO: this would violate all bytes
		if (instruction.nop) {
			numNops++;
			continue;
		}
		// TODO: print these to another file
		
		
		fprintf(f, "%#06x>\t%02x| %s\t", instruction.address, instruction.opcode, mnemonics[instruction.opcode].c_str());
		// String pointer stack
		if ((instruction.opcode == 0x02 || instruction.opcode == 0x07) && (instruction.args[0] == 0x14)) {
			fprintf(f, "%#x, [%#06x]", instruction.args[0], instruction.args[1]);
		} else {
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
	
	
	BytecodeParser parser(fileStream, header.bytecode);
	Instructions instList = parser.parseBytecode(mainStrings, varStrings);
	
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
	
	printInstructions(instList, mnemonics, outFilename, controlInfo);
	
	return 0;
}


// Parsing the bytecode
BytecodeParser::BytecodeParser(std::ifstream &f, HeaderPair index) {
	f.seekg(index.offset, std::ios_base::beg);
	
	dataLength = index.count;
	bytecode = new unsigned char[dataLength];
	
	f.read((char*) bytecode, dataLength);
	if (f.fail()) {
		Logger::Log(Logger::ERROR) << "Tried to read " << dataLength << " bytes, got" << f.gcount() << std::endl;
		throw std::exception();
	}
	Logger::Log(Logger::INFO) << "Read " << f.gcount() << " bytes of bytecode." << std::endl;
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

void BytecodeParser::readArgs(Instruction &inst) {
	unsigned int numArg = readArg(inst);

	if (numArg > 1) {
		Logger::Log(Logger::DEBUG) << "Address 0x" << std::hex << std::setw(4) << +inst.address;
		Logger::Log(Logger::DEBUG) << ":  Reading: " << std::dec << numArg << " arguments." << std::endl;
		
		if (dataLength - currAddress < 4 * numArg) {
			Logger::Log(Logger::ERROR) << "Going to overflow." << std::endl;
			throw std::exception();
		}
		
	}

	for (unsigned int counter = 0; counter < numArg; counter++) {
		readArg(inst);
	}
}

// Stolen from tanuki
// https://github.com/bitprime/vn_translation_tools/blob/master/rewrite/
Instructions BytecodeParser::parseBytecode(const StringList &strings, const StringList &strings2) {
	unsigned int arg;
	unsigned char opcode;
	Instructions instList;
	unsigned int stackTop = 0;	// 0xa stack
	
	Logger::Log(Logger::DEBUG) << "Parsing " << std::to_string(dataLength) << " bytes.\n";
	
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
				try {
					inst.comment = strings2.at(arg);
				} catch (std::out_of_range &e) {
					Logger::Log(Logger::WARN) << "Missing string id " << arg << " at 0x" << std::hex << inst.address << std::dec << std::endl;
				}
			break;
			case 0x13:
			case 0x14:
			case 0x20:
				readArg(inst);
				readArg(inst);
				readArg(inst);
			break;
			case 0x21:
				readArg(inst);
				readArg(inst, 1);
			break;
			case 0x22:	// calc. 0x10 is subtract?
				readArg(inst);
				readArg(inst);
				readArg(inst, 1);
			break;
			case 0x02:
				arg = readArg(inst);
				if (arg == 0x0a) {
					arg = readArg(inst);
					stackTop = arg;
				} else if (arg == 0x14) {
					arg = readArg(inst);
					inst.comment = strings.at(arg);	// Check in range?
				}
			break;
			case 0x08:
			break;
			case 0x15:
				readArgs(inst);
			break;
			case 0x30:
				readArg(inst);
				readArgs(inst);
				readArgs(inst);
								
				readArg(inst);		// something about return type?
				if (stackTop == 0x02 || stackTop == 0x08 ||
						stackTop == 0x0a || stackTop == 0x0c || stackTop == 0x0d || 
						stackTop == 0x4c || stackTop == 0x4d
				) {	// check if right.. somehow
					readArg(inst);
				}
			break;
			case 0x00:
			default:
				inst.nop = true;
				Logger::Log(Logger::DEBUG) << "Address 0x" << std::hex << std::setw(4) << +inst.address;
				Logger::Log(Logger::DEBUG) << ":  NOP encountered: 0x" << std::setw(2) << +opcode << std::dec << std::endl;
		}
		instList.push_back(inst);
	}
	return instList;
}

BytecodeParser::~BytecodeParser() {
	if (bytecode)	// don't need check
		delete[] bytecode;
}