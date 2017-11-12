// TODO: fix size tracking
//#if defined(_WIN32)
//_mkdir(strPath.c_str());
//#else 
//mkdir(strPath.c_str(), 0777);
//#endif
// TODO: get rid of the weird arrays
// TODO: input file, output dir
// TODO: var info and cmd info


#include <cstdlib>
#include <iostream>
#include <fstream>
#include <cstdint>

#include <cassert>
#include <unistd.h>
#include <getopt.h>

#include "Helper.h"
#include "Structs.h"

void readScenePackHeader(std::ifstream &f, ScenePackHeader &header) {
	f.read((char*) &header.headerSize, 4);
	assert(header.headerSize == 0x5C);
	
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
	
	static char usageString[] = "Usage: readscene [-d outdir] [-v] [Scene.pck]";
	
	int option = 0;
	while ((option = getopt(argc, argv, "d:v")) != -1) {
		switch (option) {
		case 'v':
			Logger::increaseVerbosity();
			break;
		case 'd':
			std::cout << "Wait, don't actually use this yet" << std::endl;
			return 0;
		break;
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
	
	// Read strings
	StringList varNames = readStrings(fileStream, header.varNameIndex, header.varName);
	StringList cmdNames = readStrings(fileStream, header.cmdNameIndex, header.cmdName);
	StringList sceneNames = readStrings(fileStream, header.sceneNameIndex, header.sceneName);
	
	std::ofstream outStream;
	// Print strings
	outStream.open(outdir + "/VarNames.txt");
		outStream << varNames;
	outStream.close();
	outStream.open(outdir + "/CmdNames.txt");
		outStream << cmdNames;
	outStream.close();
	
	// Dump scene scripts
	for (unsigned int i = 0; i < header.sceneNameIndex.count; i++) {
		fileStream.seekg(header.sceneData.offset + sceneDataInfo.at(i).offset);
		unsigned char buffer[sceneDataInfo.at(i).count];
		fileStream.read((char*) buffer, sceneDataInfo.at(i).count);
		
		decodeData(buffer, sceneDataInfo.at(i).count);
		
		// Decompress
		unsigned int compressedSize = readUInt32(buffer);
		unsigned int decompressedSize = readUInt32(buffer + 4);
		
		assert(sizeof(buffer) == compressedSize);
		
		unsigned char decompressed[decompressedSize];
		
		decompressData(buffer + 8, decompressed, decompressedSize);
		
		// Dump decompressed
		std::string outfile = outdir + "/" + sceneNames.at(i) + ".ss";
		outStream.open(outfile, std::ofstream::out | std::ofstream::binary);
		outStream.write((char*) decompressed, decompressedSize);
		outStream.close();
	}
	fileStream.close();

	return 0;
}
