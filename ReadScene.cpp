// TODO: fix size tracking
//#if defined(_WIN32)
//_mkdir(strPath.c_str());
//#else 
//mkdir(strPath.c_str(), 0777);
//#endif
// TODO: get rid of the weird arrays
// TODO: input file, output dir
// TODO: proper logging


#include <cstdlib>
#include <iostream>
#include <fstream>
#include <cstdint>

#include <cassert>

#include "Helper.h"
#include "Structs.h"

bool g_Verbose = false;

void readPackHeader(std::ifstream &f, PackHeader &header) {
	f.read((char*) &header.headerLength, 4);
	assert(header.headerLength == 0x5C);
	
	readHeaderPair(f, header.VarInfo);
	readHeaderPair(f, header.VarNameIndex);
	readHeaderPair(f, header.VarName);
	
	readHeaderPair(f, header.CmdInfo);
	readHeaderPair(f, header.CmdNameIndex);
	readHeaderPair(f, header.CmdName);
	
	readHeaderPair(f, header.SceneNameIndex);
	readHeaderPair(f, header.SceneName);
	readHeaderPair(f, header.SceneInfo);
	readHeaderPair(f, header.SceneData);
	
	f.read((char*) &header.ExtraKeyUse, 4);
	f.read((char*) &header.SourceHeaderLength, 4);	// Figure this out. It's at the end of the data, taunting me
}


void readFileInfo(unsigned char* buf, FileInfo &fileInfo) {
	fileInfo.offset = readUInt32(buf);
	fileInfo.length = readUInt32(buf+4);
}

int main(int argc, char* argv[]) {
	std::string filename("Scene.pck");
	if (argc > 1)
		filename = argv[1];
	
	std::ifstream fileStream(filename, std::ifstream::in | std::ifstream::binary);
	
	// TODO: Check Scene.pck.hash
	
	std::string outdir("Scene");
	
	// Read pack header
	PackHeader packHeader;
	readPackHeader(fileStream, packHeader);
	
	assert(packHeader.SceneNameIndex.count == packHeader.SceneInfo.count);
	HeaderPair SceneDataInfo[packHeader.SceneInfo.count];	// Should be using vectors. VLAs aren't even allowed.
	fileStream.seekg(packHeader.SceneInfo.offset);
	for (unsigned int i = 0; i < packHeader.SceneInfo.count; i++) {
		readHeaderPair(fileStream, SceneDataInfo[i]);
	}
	
	// Read var names
	StringList varNames = readStrings(fileStream, packHeader.VarNameIndex, packHeader.VarName);
	// Read cmd names
	StringList cmdNames = readStrings(fileStream, packHeader.CmdNameIndex, packHeader.CmdName);
	
	StringList sceneNames = readStrings(fileStream, packHeader.SceneNameIndex, packHeader.SceneName);
	
	std::ofstream outStream;
	// Print strings
	outStream.open(outdir + "/VarNames.txt");
	printStrings(varNames, outStream);
	outStream.close();
	
	outStream.open(outdir + "/VarNames.txt");
	printStrings(cmdNames, outStream);
	outStream.close();
	
	unsigned int offset;
	// Dump scene scripts
	for (unsigned int i = 0; i < packHeader.SceneNameIndex.count; i++) {
		offset = packHeader.SceneData.offset + SceneDataInfo[i].offset;
		fileStream.seekg(offset);
		unsigned char buffer[SceneDataInfo[i].count];
		fileStream.read((char*) buffer, SceneDataInfo[i].count);
		
		decodeData(buffer, SceneDataInfo[i].count);
		
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
