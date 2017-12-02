// TODO: handle dir properly

#include <iostream>
#include <fstream>
#include <locale>
#include <codecvt>
#include <cassert>

#include "Helper.h"
#include "Logger.h"
#include "Structs.h"

void readFileInfo(std::ifstream &stream, FileInfo &pair) {
	stream.read((char*) &pair.offset, sizeof(uint64_t));
	stream.read((char*) &pair.length, sizeof(uint64_t));
}

int Logger::LogLevel = Logger::LEVEL_INFO;
int main(int argc, char* argv[]) {
	static char usageString[] = "Usage: extractpck <input.pck> [outfile]";
	if (argc < 2) {
		std::cout << usageString << std::endl;
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
	std::vector<FileInfo> fileInfo;
	fileInfo.reserve(numFiles);
	FileInfo fInfo;
	unsigned int maxFileLength = 0;
	for (unsigned int i = 0; i < numFiles; i++) {
		readFileInfo(fileStream, fInfo);
		fileInfo.push_back(fInfo);
		if (fInfo.length > maxFileLength)
			maxFileLength = fInfo.length;
	}
	
	fileStream.seekg(0x20, std::ios_base::beg);
	StringList filenames = readFilenames(fileStream, numFiles);
	
	fileStream.seekg(fileInfoOffset, std::ios_base::beg);
	std::ofstream outStream;
	char *fileBuffer = new char[maxFileLength];
	for (unsigned int i = 0; i < numFiles; i++) {
		outStream.open(std::string("Pack/") + filenames.at(i), std::ofstream::out | std::ofstream::binary);
		
		fileStream.seekg(fileInfo.at(i).offset, std::ios_base::beg);
		fileStream.read(fileBuffer, fileInfo.at(i).length);
		outStream.write(fileBuffer, fileInfo.at(i).length);
		outStream.close();
	}
	delete[] fileBuffer;
	fileStream.close();

	return 0;
}