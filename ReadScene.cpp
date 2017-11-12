// TODO: fix size tracking

#include <cstdlib>
#include <iostream>
#include <fstream>
#include <locale>
#include <codecvt>
#include <cstdint>

#include <cassert>

#include "Helper.h"
#include "Structs.h"

bool g_Verbose = false;

void readFileInfo(unsigned char* buf, FileInfo &fileInfo) {
	fileInfo.offset = readUInt32(buf);
	fileInfo.length = readUInt32(buf+4);
}

int main() {

	std::ifstream fileStream("Scene.pck", std::ifstream::in | std::ifstream::binary);
	
	// TODO: Check Scene.pck.hash
	
	
	// Read pack header
	PackHeader packHeader;
	fileStream.read((char*) &packHeader.headerLength, 4);
	assert(packHeader.headerLength == 92);
	
	readHeaderPair(fileStream, packHeader.VarInfo);
	readHeaderPair(fileStream, packHeader.VarNameIndex);
	readHeaderPair(fileStream, packHeader.VarName);
	
	readHeaderPair(fileStream, packHeader.CmdInfo);
	readHeaderPair(fileStream, packHeader.CmdNameIndex);
	readHeaderPair(fileStream, packHeader.CmdName);
	
	readHeaderPair(fileStream, packHeader.SceneNameIndex);
	readHeaderPair(fileStream, packHeader.SceneName);
	readHeaderPair(fileStream, packHeader.SceneInfo);
	readHeaderPair(fileStream, packHeader.SceneData);
	
	fileStream.read((char*) &packHeader.ExtraKeyUse, 4);
	fileStream.read((char*) &packHeader.SourceHeaderLength, 4);

	// Read var info
	HeaderPair VarNameLength[packHeader.VarNameIndex.count];
	fileStream.seekg(packHeader.VarNameIndex.offset);
	for (unsigned int i = 0; i < packHeader.VarNameIndex.count; i++) {
		readHeaderPair(fileStream, VarNameLength[i]);
	}
	
	// Read cmd info
	HeaderPair CmdNameLength[packHeader.CmdNameIndex.count];
	fileStream.seekg(packHeader.CmdNameIndex.offset);
	for (unsigned int i = 0; i < packHeader.CmdNameIndex.count; i++) {
		readHeaderPair(fileStream, CmdNameLength[i]);
	}
	
	// Read scene info
	HeaderPair SceneNameLength[packHeader.SceneNameIndex.count];	// Should be using vectors. VLAs aren't even allowed.
	fileStream.seekg(packHeader.SceneNameIndex.offset);
	for (unsigned int i = 0; i < packHeader.SceneNameIndex.count; i++) {
		readHeaderPair(fileStream, SceneNameLength[i]);
	}
	
	HeaderPair SceneDataInfo[packHeader.SceneInfo.count];
	fileStream.seekg(packHeader.SceneInfo.offset);
	for (unsigned int i = 0; i < packHeader.SceneInfo.count; i++) {
		readHeaderPair(fileStream, SceneDataInfo[i]);
	}

	unsigned int offset;
	std::wstring_convert<std::codecvt_utf8<char16_t>, char16_t> ucs2conv;
	std::ofstream outStream;
	
	// Var names
	outStream.open("Extracted/VarNames.txt");
	for (unsigned int i = 0; i < packHeader.VarNameIndex.count; i++) {
		offset = packHeader.VarName.offset + VarNameLength[i].offset * 2;
		fileStream.seekg(offset);
		char16_t rawVarName[VarNameLength[i].count];
		fileStream.read((char*) rawVarName, VarNameLength[i].count * 2);
		std::u16string varName(rawVarName, VarNameLength[i].count);
		outStream << ucs2conv.to_bytes(varName) << std::endl;
	}
	outStream.close();
	
	// Cmd names
	outStream.open("Extracted/CmdNames.txt");
	for (unsigned int i = 0; i < packHeader.CmdNameIndex.count; i++) {
		offset = packHeader.CmdName.offset + CmdNameLength[i].offset * 2;
		fileStream.seekg(offset);
		char16_t rawCmdName[CmdNameLength[i].count];
		fileStream.read((char*) rawCmdName, CmdNameLength[i].count * 2);
		std::u16string cmdName(rawCmdName, CmdNameLength[i].count);
		outStream << ucs2conv.to_bytes(cmdName) << std::endl;
	}
	outStream.close();
	
	
	// Scene name
	for (unsigned int i = 0; i < packHeader.SceneNameIndex.count; i++) {
		// Consider trying to load string into buffer and doing string operations
		offset = packHeader.SceneName.offset + SceneNameLength[i].offset * 2; // Offset is by 2 byte wchar_t
		fileStream.seekg(offset);
		char16_t rawSceneName[SceneNameLength[i].count];
		fileStream.read((char*) rawSceneName, SceneNameLength[i].count*2);
		std::u16string sceneName(rawSceneName, SceneNameLength[i].count);
			
		offset = packHeader.SceneData.offset + SceneDataInfo[i].offset;
		fileStream.seekg(offset);
		unsigned char buffer[SceneDataInfo[i].count];
		fileStream.read((char*) buffer, SceneDataInfo[i].count);

		//std::cout << SceneDataInfo[i].offset << ", " << SceneDataInfo[i].count << std::endl;
		
		decrypt2(buffer, SceneDataInfo[i].count);
		
		// Decompress
		unsigned int compressedSize = readUInt32(buffer);
		unsigned int decompressedSize = readUInt32(buffer + 4);
		
		assert(sizeof(buffer) == compressedSize);
		
		unsigned char decompressed[decompressedSize];
		
		decompressData(buffer + 8, decompressed, decompressedSize);
		
		//std::ofstream out("decompressed", std::ofstream::out | std::ofstream::binary);
		//out.write((char*) decompressed, decompressedSize);
		
		
		// Dump decompressed
		outStream.open(ucs2conv.to_bytes(sceneName) + std::string(".dump"), std::ofstream::out | std::ofstream::binary);
		outStream.write((char*) decompressed, decompressedSize);
		outStream.close();
		
		// Read decompressed scene
		/* See ParseSiglusScript
		SCENEHEADERV2 sceneHeader;
		sceneHeader.headerLength = readUInt32(decompressed);
		assert(sceneHeader.headerLength == 132);
		
		readHeaderPair(decompressed + 12, sceneHeader.StringIndexPair);
		readHeaderPair(decompressed + 20, sceneHeader.StringDataPair);
		
		FILEINFO StringIndex[sceneHeader.StringIndexPair.count];
		offset = sceneHeader.StringIndexPair.offset;
		for (unsigned int i = 0; i < sceneHeader.StringIndexPair.count; i++) {
			readFileInfo(decompressed + offset, StringIndex[i]);
			offset += 8;
		}
		
		// Write strings
		outStream.open(std::string("Extracted/") + ucs2conv.to_bytes(sceneName));
		for (unsigned int i = 0; i < sceneHeader.StringIndexPair.count; i++) {
			offset = sceneHeader.StringDataPair.offset + StringIndex[i].offset * 2;
		
			char16_t* rawString = (char16_t*) (decompressed + offset);
			for (int j = 0; j < StringIndex[i].length; j++) {
				rawString[j] ^= (i * 0x7087);
			}
			std::u16string newString(rawString, StringIndex[i].length);
			outStream << ucs2conv.to_bytes(newString) << std::endl;
		}
		outStream.close();
		if (g_Verbose)
			std::cout << "Wrote " << sceneHeader.StringIndexPair.count << " lines to " << ucs2conv.to_bytes(sceneName) << std::endl;
		*/
	}
	fileStream.close();

	return 0;
}
