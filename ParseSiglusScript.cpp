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
	readHeaderPair(f, header.bytecode);
	readHeaderPair(f, header.stringIndex);
	readHeaderPair(f, header.stringData);
	
	readHeaderPair(f, header.labels);
	readHeaderPair(f, header.markers);
	
	readHeaderPair(f, header.localCommandIndex);    
	readHeaderPair(f, header.unknown1);
	readHeaderPair(f, header.stringsIndex1);    
	readHeaderPair(f, header.strings1);
	
	readHeaderPair(f, header.functions);
	readHeaderPair(f, header.functionNameIndex);
	readHeaderPair(f, header.functionName);
	assert(header.functionNameIndex.count == header.functions.count);		// get rid of the asserts
	
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

SceneInfo readSceneInfo(std::ifstream &stream, ScriptHeader header, std::string filename) {
	SceneInfo info;
	
	// Read scene pack globals
	// Read scene names, commands and variables
	// TODO: make safe
	// including verifying everything in bounds
	ScriptCommand command;
	std::ifstream globalInfoFile("SceneInfo.dat", std::ios::in | std::ios::binary);
	if (!globalInfoFile.is_open()) {
		Logger::Log(Logger::INFO) << "Could not open global scene info.\n";
	} else {
		std::string name;
		unsigned int count, temp;
		
		// Scene names
		globalInfoFile.read((char*) &count, 4);		// unsafe
		for (unsigned int i = 0; i < count; i++) {
			std::getline(globalInfoFile, name, '\0');
			info.sceneNames.push_back(name);
			// weird - should override
			if (filename.compare(name) >= 0)
				info.thisFile = i;
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
		info.numGlobalCommands = count;
		info.commands.resize(count + header.localCommandIndex.count);
		for (unsigned int i = 0; i < count; i++) {
			globalInfoFile.read((char*) &command.offset, 4);
			globalInfoFile.read((char*) &command.file, 4);
			command.index = i;
			std::getline(globalInfoFile, command.name, '\0');
			info.commands[i] = command;
		}
		Logger::Log(Logger::INFO) << std::to_string(count) << " global commands read\n";
		
	}
	globalInfoFile.close();
	
	// Read markers
	readLabels(stream, info.labels, header.labels);
	readLabels(stream, info.markers, header.markers);
	readLabels(stream, info.functions, header.functions);
	
	// Read function names
	StringList functionNames = readStrings(stream, header.functionNameIndex, header.functionName);
	for (auto it = info.functions.begin(); it != info.functions.end(); it++) {
		it->name = functionNames.at(it - info.functions.begin());
	}
	
	// Read local commands
	stream.seekg(header.localCommandIndex.offset, std::ios::beg);
	for (unsigned int i = 0; i < header.localCommandIndex.count; i++) {
		stream.read((char*) &command.index, 4);
		stream.read((char*) &command.offset, 4);
		command.name.clear();
		command.file = info.thisFile;
		if (command.index < info.commands.size() && command.index >= info.numGlobalCommands) {
			auto predAtOffset = [command](Label function) {
				return (function.offset == command.offset);
			};
			auto funcIt = std::find_if(info.functions.begin(), info.functions.end(), predAtOffset);
			if (funcIt != info.functions.end()) {
				command.name = funcIt->name;
			} else {
				Logger::Log(Logger::WARN) << "Warning: Local command name " << std::hex << command.index;
				Logger::Log(Logger::WARN) << " at offset 0x" << command.offset << " not found.\n";
			}
			info.commands[command.index] = command;
		} else {
			Logger::Log(Logger::ERROR) << "Error: Local command " << std::hex << command.index;
			Logger::Log(Logger::ERROR) << " at offset 0x" << command.offset << " has too high index.\n";
		}
	}
	
	return info;
}

int main(int argc, char* argv[]) {
	extern char *optarg;
	extern int optind;
	
	std::string outFilename;
	static char usageString[] = "Usage: parsess [-o outfile] [-v] <input.ss>";
	
	// Handle options
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
	// and global scene pack stuff
	SceneInfo sceneInfo = readSceneInfo(fileStream, header, filename);
	
	BytecodeBuffer bytecode(fileStream, header.bytecode);
	fileStream.close();
	
	parseBytecode(bytecode, outFilename, sceneInfo, mainStrings, varStrings);
	
	// TODO: handle these
	if (header.unknown6.count != 0) {
		Logger::Log(Logger::WARN) << "Unknown6 is not empty.\n";
	} else if (header.unknown7.count != 0) {
		Logger::Log(Logger::WARN) << "Unknown7 is not empty.\n";
	} else if (header.unknown6.offset != header.unknown7.offset) {
		Logger::Log(Logger::WARN) << "Check this file out, something's weird\n";
	}
		
	return 0;
}