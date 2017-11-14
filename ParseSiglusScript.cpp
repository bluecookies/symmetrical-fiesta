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
#include "BytecodeParser.h"

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