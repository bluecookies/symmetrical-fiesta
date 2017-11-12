#include <iostream>
#include <fstream>
#include <locale>
#include <codecvt>
#include <cassert>

#include "Helper.h"

struct PCKHEADER {
	uint32_t v1;
	uint32_t numFiles;
	uint32_t fileInfoOffset1;
	uint32_t fileInfoOffset2;
	uint32_t a1;
	uint32_t a2;
	uint32_t a3;
	uint32_t a4;
};

struct FILE_INFO {
	uint64_t offset;
	uint64_t length;
};


void readFileInfo(std::ifstream &stream, FILE_INFO &pair) {
	stream.read((char*) &pair.offset, sizeof(uint64_t));
	stream.read((char*) &pair.length, sizeof(uint64_t));
}


int main(int argc, char* argv[]) {
	if (argc < 2) {
		std::cout << "Usage: extractpck <input.pck> [outfile]" << std::endl;
		return 1;
	}

	unsigned int fileInfoOffset;
	unsigned int numFiles;

	std::ifstream fileStream(argv[1], std::ifstream::in | std::ifstream::binary);
	fileStream.seekg(4, std::ios_base::beg);
	fileStream.read((char*) &numFiles, 4);
	fileStream.seekg(12, std::ios_base::beg);
	fileStream.read((char*) &fileInfoOffset, 4);
	fileInfoOffset += 0x20;
	
	fileStream.seekg(fileInfoOffset, std::ios_base::beg);
	FILE_INFO fileInfo[numFiles];
	for (unsigned int i = 0; i < numFiles; i++) {
		readFileInfo(fileStream, fileInfo[i]);
	}
	
	unsigned int fileNameLength[numFiles];	// in bytes
	fileStream.seekg(0x20, std::ios_base::beg);
	for (unsigned int i = 0; i < numFiles; i++) {
		fileStream.read((char*) &fileNameLength[i], 4);
	}
	
	unsigned int pos = fileStream.tellg();
	std::wstring_convert<std::codecvt_utf8<char16_t>, char16_t> ucs2conv;
	std::ofstream outStream;
	for (unsigned int i = 0; i < numFiles; i++) {
		fileStream.seekg(pos, std::ios_base::beg);
		
		unsigned char fileName[fileNameLength[i]];
		fileStream.read((char*) fileName, fileNameLength[i]);
		
		pos = fileStream.tellg();
		
		std::u16string fileName16((char16_t*) fileName, fileNameLength[i] >> 1);
		outStream.open(std::string("Pack/") + ucs2conv.to_bytes(fileName16), std::ofstream::out | std::ofstream::binary);
		
		fileStream.seekg(fileInfo[i].offset, std::ios_base::beg);
		char file[fileInfo[i].length];
		fileStream.read(file, fileInfo[i].length);
		outStream.write(file, fileInfo[i].length);
		outStream.close();
	}
	fileStream.close();

	return 0;
}