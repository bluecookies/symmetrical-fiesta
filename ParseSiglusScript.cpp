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

//if (!stream.is_open()) {
//		Logger::Log(Logger::WARN) << "Could not open file " << filename << std::endl;
//		return;
//	}

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
	readHeaderPair(f, header.functionNameIndex);		// TODO: local command index
	readHeaderPair(f, header.functionName);
	
	readHeaderPair(f, header.varStringIndex);
	readHeaderPair(f, header.varStringData);
	
	readHeaderPair(f, header.unknown6);
	readHeaderPair(f, header.unknown7);
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

LabelData readLabelData(std::ifstream &stream, ScriptHeader header) {
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
			
	SceneInfo sceneInfo;
	// Read scene names, commands and variables
	// TODO: make safe
	// including verifying everything in bounds
	std::ifstream globalInfoFile("SceneInfo.dat", std::ios::in | std::ios::binary);
	if (!globalInfoFile.is_open()) {
		Logger::Log(Logger::INFO) << "Could not open scene info.\n";
	} else {
		std::string name;
		unsigned int count, temp;
		ScriptCommand command;
		
		// Scene names
		globalInfoFile.read((char*) &count, 4);		// unsafe
		for (unsigned int i = 0; i < count; i++) {
			std::getline(globalInfoFile, name, '\0');
			sceneInfo.sceneNames.push_back(name);
			// weird - should override
			if (filename.compare(name) >= 0)
				sceneInfo.thisFile = i;
		}
		
		// Vars
		globalInfoFile.read((char*) &count, 4);
		for (unsigned int i = 0; i < count; i++) {
			globalInfoFile.read((char*) &temp, 4);
			globalInfoFile.read((char*) &temp, 4);
			std::getline(globalInfoFile, name, '\0');
		}
		
		// Commands
		globalInfoFile.read((char*) &count, 4);
		for (unsigned int i = 0; i < count; i++) {
			globalInfoFile.read((char*) &command.offset, 4);
			globalInfoFile.read((char*) &command.file, 4);
			command.index = i;
			std::getline(globalInfoFile, command.name, '\0');
			sceneInfo.commands.push_back(command);
		}
	}
	globalInfoFile.close();
	
		// Read labels, markers, functions, commands
	LabelData controlInfo = readLabelData(fileStream, header);
	
	// Sort label info
	std::sort(controlInfo.labels.begin(),        controlInfo.labels.end(), compOffset);
	std::sort(controlInfo.markers.begin(),       controlInfo.markers.end(), compOffset);
	std::sort(controlInfo.functions.begin(),     controlInfo.functions.end(), compOffset);
	std::sort(controlInfo.functionTable.begin(), controlInfo.functionTable.end(), compOffset);	//TODO : local commands

	// go through functions and see what matches local commands
	std::vector<ScriptCommand> localCommands;
	ScriptCommand command;
	auto localComIt = controlInfo.functionTable.begin();
	for (auto funcIt = controlInfo.functions.begin(); funcIt != controlInfo.functions.end(); funcIt++) {
		while (localComIt != controlInfo.functionTable.end() && funcIt->offset > localComIt->offset) {
			localComIt++;
		}
		if (funcIt->offset == localComIt->offset) {
			command.offset = funcIt->offset;
			command.file = sceneInfo.thisFile;
			command.index = localComIt->index;
			command.name = funcIt->name;
			localCommands.push_back(command);
		}
	}
	
	Instructions instList;
	
	BytecodeParser parser(fileStream, header.bytecode);
	// TODO: look into merging command table and handling that at a different place
	parser.parseBytecode(instList, mainStrings, varStrings, sceneInfo, localCommands);
	
	// TODO: handle these
	if (header.unknown6.count != 0) {
		Logger::Log(Logger::WARN) << "Unknown6 is not empty.\n";
	} else if (header.unknown7.count != 0) {
		Logger::Log(Logger::WARN) << "Unknown7 is not empty.\n";
	} else if (header.unknown6.offset != header.unknown7.offset) {
		Logger::Log(Logger::WARN) << "Check this file out, something's weird\n";
	}
	fileStream.close();
	
	parser.printInstructions(instList, outFilename, controlInfo, sceneInfo);
	
	return 0;
}