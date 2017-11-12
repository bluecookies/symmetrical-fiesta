// TODO: function backup? offset? table index thing
// TODO: check the ranges are right

#define __USE_MINGW_ANSI_STDIO 0

#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <vector>

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
typedef std::vector<Instruction> Instructions;

void readScriptHeader(std::ifstream &f, ScriptHeader &header) {
	f.read((char*) &header.headerSize, 4);
	assert(header.headerSize == 0x84);
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
	
	readHeaderPair(f, header.stringsIndex2);
	readHeaderPair(f, header.strings2);
	
	readHeaderPair(f, header.unknown6);
	readHeaderPair(f, header.unknown7);
}

char* readBytecode(std::ifstream &f, HeaderPair index) {
	f.seekg(index.offset, std::ios_base::beg);
	char* bytecode = new char[index.count];
	f.read(bytecode, index.count);
	return bytecode;
}

// Stolen from tanuki
// https://github.com/bitprime/vn_translation_tools/blob/master/rewrite/
Instructions parseBytecode(char* bytecode, unsigned int length, 
	const StringList &strings, const StringList &strings2
) {
	
	unsigned int curr = 0, arg, numArg;
	unsigned char opcode;
	Instructions instList;
	unsigned int stackTop = 0;	// unsure if actual stack. to ask
	
	while (curr < length) {
		Instruction inst;
		inst.address = curr;
		opcode = bytecode[curr++];
		inst.opcode = opcode;
		switch (opcode) {
			case 0x01:
			case 0x03:
			case 0x04:
			case 0x10:
			case 0x11:
			case 0x12:
			case 0x31:
				arg = readUInt32(bytecode + curr);
				curr += 4;
				inst.args.push_back(arg);
			break;
			case 0x05:
			case 0x06:
			case 0x09:
			case 0x16:
			case 0x32:
			break;
			case 0x07:
				arg = readUInt32(bytecode + curr);	curr += 4;
					inst.args.push_back(arg);
				arg = readUInt32(bytecode + curr);	curr += 4;
					inst.args.push_back(arg);
					inst.comment = strings2.at(arg);
			break;
			case 0x13:
			case 0x14:
			case 0x20:
				arg = readUInt32(bytecode + curr);	curr += 4;
					inst.args.push_back(arg);
				arg = readUInt32(bytecode + curr);	curr += 4;
					inst.args.push_back(arg);
				arg = readUInt32(bytecode + curr);	curr += 4;
					inst.args.push_back(arg);
			break;
			case 0x21:
				arg = readUInt32(bytecode + curr);	curr += 4;
					inst.args.push_back(arg);
				arg = bytecode[curr++];
					inst.args.push_back(arg);
			break;
			case 0x22:
				arg = readUInt32(bytecode + curr);	curr += 4;
					inst.args.push_back(arg);
				arg = readUInt32(bytecode + curr);	curr += 4;
					inst.args.push_back(arg);
				arg = bytecode[curr++];
					inst.args.push_back(arg);
			break;
			case 0x02:
				arg = readUInt32(bytecode + curr);	curr += 4;
					inst.args.push_back(arg);
				if (arg == 0x0a) {
					arg = readUInt32(bytecode + curr);	curr += 4;
						inst.args.push_back(arg);
					stackTop = arg;
				} else if (arg == 0x14) {
					arg = readUInt32(bytecode + curr);	curr += 4;
						inst.args.push_back(arg);
						inst.comment = strings.at(arg);	// Check in range?
				}
			break;
			case 0x08:
			break;
			case 0x15:
				numArg = arg = readUInt32(bytecode + curr);	curr += 4;
					inst.args.push_back(arg);
				for (unsigned int counter = 0; counter < numArg; counter++) {
					arg = readUInt32(bytecode + curr);	curr += 4;
					inst.args.push_back(arg);
				}
			break;
			case 0x30:
				arg = readUInt32(bytecode + curr);	curr += 4;
					inst.args.push_back(arg);
				numArg = arg = readUInt32(bytecode + curr);	curr += 4;
					inst.args.push_back(arg);
				Logger::Log(Logger::DEBUG) << "Address 0x" << std::hex << std::setw(4) << +inst.address << std::dec;
				Logger::Log(Logger::DEBUG) << ":  arg count: " << numArg;
				
				for (unsigned int counter = 0; counter < numArg; counter++) {
					arg = readUInt32(bytecode + curr);	curr += 4;
					inst.args.push_back(arg);
				}
				numArg = arg = readUInt32(bytecode + curr);	curr += 4;
					inst.args.push_back(arg);
				for (unsigned int counter = 0; counter < numArg; counter++) {
					arg = readUInt32(bytecode + curr);	curr += 4;
					inst.args.push_back(arg);
				}
				
				Logger::Log(Logger::DEBUG) << " " << numArg << std::endl;
				
				arg = readUInt32(bytecode + curr);	curr += 4;
					inst.args.push_back(arg);		// something about return type?
				if (stackTop == 0x0c || stackTop == 0x4c || stackTop == 0x4d) {	// check if right.. somehow
					arg = readUInt32(bytecode + curr);	curr += 4;
					inst.args.push_back(arg);
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

void populateMnemonics(StringList &mnemonics) {
	std::stringstream hexStream;
	hexStream << std::hex << std::setfill('0');
	
	mnemonics.resize(256);
	// Temporary
	mnemonics[0] = "nop";
	for (unsigned char i = 1; i != 0; i++) {	//probs undefined behaviour, but temporary
		hexStream << std::setw(2) << std::to_string(i);
		mnemonics[i] = "[" + hexStream.str() + "]";
		hexStream.str("");
	}
	mnemonics[0x10] = "jmp";
	mnemonics[0x11] = "je";
	mnemonics[0x12] = "jne";
	
}

void readCommands(const std::string filename, std::vector<unsigned int> &offsets, StringList &names) {
	std::ifstream stream(filename, std::ifstream::in | std::ifstream::binary);	// this is much longer than "rb"
	std::string cmdName;
	unsigned int offset;
	stream.read((char*) &offset, 4);
	while (!stream.eof()) {
		offsets.push_back(offset);
		std::getline(stream, cmdName, '\0');
		names.push_back(cmdName);
		stream.read((char*) &offset, 4);
	}
	stream.close();
}

// TODO: this is getting kind of unwieldy
void printInstructions(Instructions instList, StringList mnemonics, std::string filename, 
	const std::vector<unsigned int> &labels,
	const std::vector<unsigned int> &markers,
	const std::vector<unsigned int> &functions,
	const std::vector<unsigned int> &commands,
	StringList functionNames, StringList cmdNames
){
	std::ofstream stream(filename);
	stream << std::setfill('0');
	
	Instructions::iterator it;
	std::vector<unsigned int>::const_iterator labelIt;
	Instruction instruction;
	for (it = instList.begin(); it != instList.end(); it++) {
		instruction = *it;
		// Check for labels, markers and functions
		for (labelIt = labels.begin(); labelIt != labels.end(); labelIt++) {
			if (*labelIt == instruction.address) {
				stream << "\nSetLabel " << (labelIt - labels.begin()) << ":" << std::endl;
			}
		}
		for (labelIt = markers.begin(); labelIt != markers.end(); labelIt++) {
			if (*labelIt == 0) continue;
			if (*labelIt == instruction.address) {
				stream << "\nSetMarker " << (labelIt - markers.begin()) << ":" << std::endl;
			}
		}
		for (labelIt = functions.begin(); labelIt != functions.end(); labelIt++) {
			if (*labelIt == instruction.address) {
				stream << "\nSetFunction " << (labelIt - functions.begin()) << ":";
				// Check if function names is empty (or at least exists the name)
				stream << "\t; " << functionNames.at(labelIt - functions.begin());
				stream << std::endl;
			}
		}
		for (labelIt = commands.begin(); labelIt != commands.end(); labelIt++) {
			if (*labelIt == instruction.address) {
				stream << "\nSetCommand " << cmdNames.at(labelIt - commands.begin()) << std::endl;
			}
		}
		
		if (instruction.nop)
			continue;
		stream << "0x" << std::setw(4) << std::hex << instruction.address << ">\t";
		stream << std::setw(2) << +instruction.opcode << "| ";	// +instruction promotes to printable number
		stream << mnemonics[instruction.opcode] << "\t";
		for (auto argIt = instruction.args.begin(); argIt != instruction.args.end(); argIt++) {
			if (argIt != instruction.args.begin()) stream << ", ";
			stream << "0x" << *argIt;
		}
		if (!instruction.comment.empty())
			stream << "\t; " << instruction.comment;
		
		stream << std::endl;
	}
	
	stream.close();
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
	
	if (outFilename.empty())
		outFilename = filename + std::string(".asm");
	
	ScriptHeader header;
	readScriptHeader(fileStream, header);
	
	StringList mainStrings = readStrings(fileStream, header.stringIndex, header.stringData, true);
	StringList strings1 = readStrings(fileStream, header.stringsIndex1, header.strings1);
	StringList strings2 = readStrings(fileStream, header.stringsIndex2, header.strings2);
	StringList functionNames = readStrings(fileStream, header.functionNameIndex, header.functionName);
	
	Logger::Log(Logger::INFO) << strings1;
	
	// Read labels - TODO: check out of range
	unsigned int offset;
	
	std::vector<unsigned int> labels;
	labels.reserve(header.labels.count);
	fileStream.seekg(header.labels.offset, std::ios_base::beg);
	for (unsigned int i = 0; i < header.labels.count; i++) {
		fileStream.read((char*) &offset, 4);
		labels.push_back(offset);
	}
	
	std::vector<unsigned int> markers;
	markers.reserve(header.markers.count);
	fileStream.seekg(header.markers.offset, std::ios_base::beg);
	for (unsigned int i = 0; i < header.markers.count; i++) {
		fileStream.read((char*) &offset, 4);
		markers.push_back(offset);
	}
	
	std::vector<unsigned int> functions;
	functions.reserve(header.functions.count);
	fileStream.seekg(header.functions.offset, std::ios_base::beg);
	for (unsigned int i = 0; i < header.functions.count; i++) {
		fileStream.read((char*) &offset, 4);
		functions.push_back(offset);
	}
	
	char* bytecode = readBytecode(fileStream, header.bytecode);
	Instructions instList = parseBytecode(bytecode, header.bytecode.count, mainStrings, strings2);
	
	// TODO: handle these
	if (header.unknown6.count != 0) {
		Logger::Log(Logger::WARN) << "Unknown6 is not empty.\n";
	} else if (header.unknown7.count != 0) {
		Logger::Log(Logger::WARN) << "Unknown7 is not empty.\n";
	} else if (header.unknown6.offset != header.unknown7.offset) {
		Logger::Log(Logger::WARN) << "Check this file out, something's weird\n";
	}
	fileStream.close();
	
	// Read commands
	std::vector<unsigned int> commands;
	StringList commandNames;
	readCommands(filename + ".commands", commands, commandNames);
	
	StringList mnemonics(0x100);
	populateMnemonics(mnemonics);
	
	printInstructions(instList, mnemonics, outFilename, labels, markers, functions, commands, functionNames, commandNames);

	delete[] bytecode;
	
	return 0;
}