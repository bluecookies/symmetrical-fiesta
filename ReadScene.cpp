// TODO: fix size tracking
//#if defined(_WIN32)
//_mkdir(strPath.c_str());
//#else 
//mkdir(strPath.c_str(), 0777);
//#endif
// TODO: input file, output dir
// TODO: var info and cmd info

// TODO: auto key if same name
// or read key from keys.dat if recognize initial trying

#define __USE_MINGW_ANSI_STDIO 0

#include <iostream>
#include <fstream>
#include <iomanip>
#include <cstdint>

#include <cassert>
#include <unistd.h>
#include <getopt.h>

#include "Helper.h"
#include "Structs.h"

void readScenePackHeader(std::ifstream &f, ScenePackHeader &header) {
	f.read((char*) &header.headerSize, 4);
	if (header.headerSize != 0x5C) {
		Logger::Log(Logger::ERROR) << "Error: Expected scene pack header size 0x5C, got 0x" << std::hex << header.headerSize << std::endl;
		throw std::exception();
	}
	
	readHeaderPair(f, header.varInfo);
	readHeaderPair(f, header.varNameIndex);
	readHeaderPair(f, header.varName);
	
	readHeaderPair(f, header.cmdInfo);
	readHeaderPair(f, header.cmdNameIndex);
	readHeaderPair(f, header.cmdName);
	
	readHeaderPair(f, header.sceneNameIndex);
	readHeaderPair(f, header.sceneName);
	readHeaderPair(f, header.sceneInfo);
	readHeaderPair(f, header.sceneData);
	
	f.read((char*) &header.extraKeyUse, 4);
	f.read((char*) &header.sourceHeaderLength, 4);	// Figure this out. It's at the end of the data, taunting me
}

int main(int argc, char* argv[]) {
	extern char *optarg;
	extern int optind;
	
	static char usageString[] = "Usage: readscene [-d outdir] [-v] [-k xorkey] [Scene.pck]";
	
	bool keyProvided = false;
	unsigned char extraKey[16];
	
	int option = 0;
	while ((option = getopt(argc, argv, "d:vk:")) != -1) {
		switch (option) {
		case 'v':
			Logger::increaseVerbosity();
			break;
		case 'd':
			std::cout << "Wait, don't actually use this yet: " << optarg << std::endl;
			return 0;
		break;
		case 'k': {
			keyProvided = true;
			std::ifstream keyfile(optarg, std::ifstream::in | std::ifstream::binary);
			keyfile.read((char*) extraKey, 16);
			keyfile.close();
		} break;
		default:
			std::cout << usageString << std::endl;
			return 1;
		}
	}
	
	std::string filename("Scene.pck");
	if (optind < argc) {
		filename = argv[optind];
	}
	
	
	std::ifstream fileStream(filename, std::ifstream::in | std::ifstream::binary);
	
	// TODO: Check Scene.pck.hash
	
	std::string outdir("Scene");
	
	// Read pack header
	ScenePackHeader header;
	readScenePackHeader(fileStream, header);
	
	// Read file table
	assert(header.sceneNameIndex.count == header.sceneInfo.count);
	std::vector<HeaderPair> sceneDataInfo;
	sceneDataInfo.reserve(header.sceneInfo.count);

	fileStream.seekg(header.sceneInfo.offset);
	HeaderPair pair;
	for (unsigned int i = 0; i < header.sceneInfo.count; i++) {
		readHeaderPair(fileStream, pair);
		sceneDataInfo.push_back(pair);
	}
	
	// Read var and cmd info
	assert(header.varInfo.count == header.varNameIndex.count);
	assert(header.cmdInfo.count == header.cmdNameIndex.count);
	std::vector<HeaderPair> varInfo, cmdInfo;
	varInfo.reserve(header.varInfo.count);
	cmdInfo.reserve(header.cmdInfo.count);
	fileStream.seekg(header.varInfo.offset);
	for (unsigned int i = 0; i < header.varInfo.count; i++) {
		readHeaderPair(fileStream, pair);
		varInfo.push_back(pair);
	}
	fileStream.seekg(header.cmdInfo.offset);
	for (unsigned int i = 0; i < header.cmdInfo.count; i++) {
		readHeaderPair(fileStream, pair);
		cmdInfo.push_back(pair);
	}
	// Read the strings
	StringList varNames = readStrings(fileStream, header.varNameIndex, header.varName, false);
	StringList cmdNames = readStrings(fileStream, header.cmdNameIndex, header.cmdName, false);
	StringList sceneNames = readStrings(fileStream, header.sceneNameIndex, header.sceneName);
	
	// Write the global info
	std::string name;
	unsigned int count;
	std::ofstream outStream("SceneInfo.dat", std::ios::out | std::ios::binary);
		// scene names
		count = sceneNames.size();
		outStream.write((char*) &count, 4);
		for (auto it = sceneNames.begin(); it != sceneNames.end(); it++) {
			name = *it;
			outStream.write(name.c_str(), name.length());
			outStream.put('\0');
		}
		
		// var info table
		// {uint32_t unknown, uint32_t unknown}
		count = varInfo.size();
		outStream.write((char*) &count, 4);
		for (auto it = varInfo.begin(); it != varInfo.end(); it++) {
			outStream.write((char*) &(it->count), 4);
			outStream.write((char*) &(it->offset), 4);
			name = varNames.at(it - varInfo.begin());
			outStream.write(name.c_str(), name.length());
			outStream.put('\0');
		}
		
		// cmd info table
		// {uint32_t fileIndex, uint32_t cmdStringIndex}
		count = cmdInfo.size();
		outStream.write((char*) &count, 4);
		for (auto it = cmdInfo.begin(); it != cmdInfo.end(); it++) {
			outStream.write((char*) &(it->count), 4);		// instruction address
			outStream.write((char*) &(it->offset), 4);	// file it is in
			name = cmdNames.at(it - cmdInfo.begin());
			outStream.write(name.c_str(), name.length());
			outStream.put('\0');
		}
	outStream.close();
	
	// Dump scene scripts
	std::ofstream cmdFile;
	unsigned int offset;
	for (unsigned int i = 0; i < header.sceneNameIndex.count; i++) {
		offset = header.sceneData.offset + sceneDataInfo.at(i).offset;
		fileStream.seekg(offset);
		unsigned char* buffer = new unsigned char[sceneDataInfo.at(i).count];
		fileStream.read((char*) buffer, sceneDataInfo.at(i).count);
		
		if (header.extraKeyUse) {
			if (keyProvided)
				decodeExtra(buffer, sceneDataInfo.at(i).count, extraKey);
			else
				std::cout << "Warning: extra xor key required (probably)." << std::endl;
		}
		
		decodeData(buffer, sceneDataInfo.at(i).count);
		
		// Decompress
		unsigned int compressedSize = readUInt32(buffer);
		unsigned int decompressedSize = readUInt32(buffer + 4);
		
		if (sceneDataInfo.at(i).count != compressedSize) {
			Logger::Log(Logger::ERROR) << "Error at pack " << +i << ": " << sceneNames.at(i) << std::endl;
			Logger::Log(Logger::ERROR) << "Expected " << std::hex << sceneDataInfo.at(i).count << " at address 0x";
			Logger::Log(Logger::ERROR) << offset << ", got " << compressedSize << ".\n";
			if (header.extraKeyUse && !keyProvided) {
				unsigned int possibleKey = (sceneDataInfo.at(i).count ^ compressedSize);
				std::cout << "Possibly requiring key starting with " << std::hex << std::setfill('0');
				for (unsigned int k = 0; k < 4; k++) {
					std::cout << std::setw(2) << (possibleKey & 0xFF) << " ";
					possibleKey >>= 8;
				}
				std::cout << std::endl;
			}
			exit(1);
		}
		unsigned char* decompressed = new unsigned char[decompressedSize];
		
		decompressData(buffer + 8, decompressed, decompressedSize);
		
		// Dump decompressed
		std::string outfile = outdir + "/" + sceneNames.at(i) + ".ss";
		auto outFile = OPEN_OFSTREAM(outfile);
		WRITE_FILE(outFile, decompressed, decompressedSize);
		
		delete[] buffer;
		delete[] decompressed;
		CLOSE_OFSTREAM(outFile);
	}
	fileStream.close();

	return 0;
}
