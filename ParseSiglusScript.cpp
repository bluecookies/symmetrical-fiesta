// TODO (but in the future): recognize control structure:
// also for loops are pretty obvious 

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

#include "Helper.h"
#include "Logger.h"
#include "Structs.h"
#include "BytecodeParser.h"

//if (!stream.is_open()) {
//		Logger::Log(Logger::WARN) << "Could not open file " << filename << std::endl;
//		return;
//	}

void readScriptHeader(std::ifstream &f, ScriptHeader &header) {
	f.read((char*) &header.headerSize, 4);
	if (header.headerSize != 0x84) {
		Logger::Error() << "Expected script header size 0x84, got 0x" << std::hex << header.headerSize << std::endl;
		throw std::exception();
	}
	readHeaderPair(f, header.bytecode);
	readHeaderPair(f, header.stringIndex);
	readHeaderPair(f, header.stringData);
	
	readHeaderPair(f, header.labels);
	readHeaderPair(f, header.entrypoints);
	
	readHeaderPair(f, header.localCommandIndex);    
	readHeaderPair(f, header.unknown1);
	readHeaderPair(f, header.staticVarIndex);    
	readHeaderPair(f, header.staticVarNames);
	
	readHeaderPair(f, header.functions);
	readHeaderPair(f, header.functionNameIndex);
	readHeaderPair(f, header.functionNames);
	assert(header.functionNameIndex.count == header.functions.count);		// get rid of the asserts
	
	readHeaderPair(f, header.localVarIndex);
	readHeaderPair(f, header.localVarNames);
	
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

void readGlobalInfo(SceneInfo &info, std::string filename, int fileIndex, const ScriptHeader &header) {
	// Read scene names, commands and variables
	// TODO: make safe
	// including verifying everything in bounds
	ScriptCommand command;
	std::ifstream globalInfoFile("SceneInfo.dat", std::ios::in | std::ios::binary);
	if (!globalInfoFile.is_open()) {
		//TODO: make it so it's not important
		Logger::Error() << "Could not open global scene info.\n";
	} else {
		std::string name;
		unsigned int count;
		
		// Scene names
		info.thisFile = fileIndex;
		std::string basename = filename.substr(filename.find_last_of("/\\") + 1);
		basename = basename.substr(0, basename.find_last_of('.'));
		
		
		globalInfoFile.read((char*) &count, 4);		// unsafe
		for (unsigned int i = 0; i < count; i++) {
			std::getline(globalInfoFile, name, '\0');
			info.sceneNames.push_back(name);
			if (fileIndex < 0) {
				if (basename.compare(name) == 0) {
					fileIndex = info.thisFile = i;
					Logger::Info() << "Determined file index: " << fileIndex << std::endl;
				}
			}
		}
		
		// Vars
		// TODO: fix notation, so global/local refers to scope, find another name for where its defined
		globalInfoFile.read((char*) &count, 4);
		info.numGlobalVars = count;
		info.globalVars.reserve(count + header.staticVarIndex.count);
		StackValue var(0, 0);
		for (unsigned int i = 0; i < count; i++) {
			globalInfoFile.read((char*) &var.type, 4);
			globalInfoFile.read((char*) &var.length, 4);
			std::getline(globalInfoFile, var.name, '\0');

			info.globalVars.push_back(var);
		}

		
		// Commands
		globalInfoFile.read((char*) &count, 4);
		info.numGlobalCommands = count;
		info.commands.resize(count + header.localCommandIndex.count);
		for (unsigned int i = 0; i < count; i++) {
			globalInfoFile.read((char*) &command.offset, 4);
			globalInfoFile.read((char*) &command.file, 4);
			if (command.file >= info.sceneNames.size()) {
				Logger::Error() << "Command " << std::to_string(i) << " referencing non-existent file index " << std::to_string(command.file) << std::endl;
				throw std::exception();
			}
			command.index = i;
			std::getline(globalInfoFile, command.name, '\0');
			if (command.name.empty())
				command.name = "fun_" + std::to_string(i);
			info.commands[i] = command;
		}
		Logger::Info() << "Read " << std::dec << std::to_string(count) << " global commands.\n";
		
	}
}

void readSceneInfo(SceneInfo &info, std::ifstream &stream, const ScriptHeader &header, std::string filename, int fileIndex) {
	// Read scene pack globals
	readGlobalInfo(info, filename, fileIndex, header);
	// Read markers
	readLabels(stream, info.labels, header.labels);
	readLabels(stream, info.markers, header.entrypoints);
	readLabels(stream, info.functions, header.functions);
	Logger::Info() << "Read " << std::dec << header.functions.count << " functions.\n";

	
	// Read function names
	StringList functionNames;
	readStrings(stream, functionNames, header.functionNameIndex, header.functionNames);
	for (auto it = info.functions.begin(); it != info.functions.end(); it++) {
		it->name = functionNames.at(it - info.functions.begin());
	}
	
	// Read static vars
	// TODO: not just the names
	if (header.staticVarNames.count > 0) {
		StringList staticVarNames;
		readStrings(stream, staticVarNames, header.staticVarIndex, header.staticVarNames);
		// TODO: try to get static var info
		ProgStack staticVars;
		for (auto varName : staticVarNames) {
			staticVars.push_back(StackValue(0xDEADBEEF, 0xa));
			staticVars.back().name = varName;
		}
		info.globalVars.insert(info.globalVars.end(), staticVars.begin(), staticVars.end());
		Logger::Info() << "Read " << std::to_string(header.staticVarNames.count) << " static variables.\n";
	}
	

	// Read local commands
	ScriptCommand command;
	stream.seekg(header.localCommandIndex.offset, std::ios::beg);
	for (unsigned int i = 0; i < header.localCommandIndex.count; i++) {
		stream.read((char*) &command.index, 4);
		stream.read((char*) &command.offset, 4);
		command.name.clear();
		command.file = info.thisFile;
		if (command.index < info.commands.size()) {
			// Local command
			if (command.index >= info.numGlobalCommands) {
			
				auto predAtOffset = [command](Label function) {
					return (function.offset == command.offset);
				};
				
				auto funcIt = std::find_if(info.functions.begin(), info.functions.end(), predAtOffset);
				if (funcIt != info.functions.end()) {
					command.name = funcIt->name;
					Logger::VDebug() << "Command " << command.name << " (" << std::hex << command.index << ") at " << command.offset << std::endl;
				} else {
					Logger::Warn(command.offset) << "Warning: Local command name " << std::hex << command.index << " not found.\n";
				}
				info.commands[command.index] = command;
			// Global command defined in this file
			} else if (command.index == 0) {
				Logger::Debug(command.offset) << "Command index 0\n";
			}
		} else {
			Logger::Error(command.offset) << "Local command " << std::hex << command.index << " has too high index.\n";
		}
	}
	Logger::Info() << "Read " << std::dec << header.localCommandIndex.count << " local commands.\n";
}

int Logger::LogLevel = Logger::LEVEL_INFO;
int main(int argc, char* argv[]) {
	extern char *optarg;
	extern int optind;
	
	std::string outFilename;
	static char usageString[] = "Usage: parsess [-o outfile] [-v] [-i file index] <input.ss>";
	
	int fileIndex = -1;
	// Handle options
	int option = 0;
	while ((option = getopt(argc, argv, "o:vi:")) != -1) {
		switch (option) {
		case 'v':
			Logger::increaseVerbosity();
			break;
		case 'o':
			outFilename = std::string(optarg);
		break;
		case 'i':
			fileIndex = std::stoi(optarg);
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
		Logger::Error() << "Could not open file " << filename;
		return 1;
	}
	
	if (outFilename.empty())
		outFilename = filename + std::string(".asm");
	
	ScriptHeader header;
	readScriptHeader(fileStream, header);
	
	// Read labels, markers, functions, commands
	// and global scene pack stuff
	SceneInfo sceneInfo;
	try {
		readSceneInfo(sceneInfo, fileStream, header, filename, fileIndex);
	} catch (std::exception &e) {
		std::cerr << e.what() << std::endl;
		return 1;
	}
	
	
	readStrings(fileStream, sceneInfo.mainStrings, header.stringIndex, header.stringData, true);
	readStrings(fileStream, sceneInfo.localVarNames, header.localVarIndex, header.localVarNames);
	
	
	Logger::VDebug() << sceneInfo.mainStrings;
	Logger::VDebug() << sceneInfo.localVarNames;
	
	BytecodeParser parser(fileStream, header.bytecode, sceneInfo);
	fileStream.close();
	
	try {
		parser.parseBytecode();
	} catch (std::out_of_range &e) {
		std::cerr << e.what() << std::endl;
	}

	parser.printInstructions(outFilename);
	parser.dumpCFG(filename + ".gv");
		
	// TODO: handle these
	if (header.unknown6.count != 0) {
		Logger::Warn() << "Unknown6 has " << header.unknown6.count << " elements.\n";
		
	}
	if (header.unknown7.count != 0) {
		Logger::Warn() << "Unknown7 has " << header.unknown7.count << " elements.\n";
	}
			
	return 0;
}