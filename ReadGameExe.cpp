#include <iostream>
#include <fstream>
#include <locale>
#include <codecvt>

#include <iomanip>
#include <unistd.h>
#include <getopt.h>

#include <limits>

#include <cassert>

#include "Helper.h"

static unsigned char XorKey[256] = {
	0xD8, 0x29, 0xB9, 0x16, 0x3D, 0x1A, 0x76, 0xD0, 0x87, 0x9B, 0x2D, 0x0C, 0x7B, 0xD1, 0xA9, 0x19,
	0x22, 0x9F, 0x91, 0x73, 0x6A, 0x35, 0xB1, 0x7E, 0xD1, 0xB5, 0xE7, 0xE6, 0xD5, 0xF5, 0x06, 0xD6,
	0xBA, 0xBF, 0xF3, 0x45, 0x3F, 0xF1, 0x61, 0xDD, 0x4C, 0x67, 0x6A, 0x6F, 0x74, 0xEC, 0x7A, 0x6F,
	0x26, 0x74, 0x0E, 0xDB, 0x27, 0x4C, 0xA5, 0xF1, 0x0E, 0x2D, 0x70, 0xC4, 0x40, 0x5D, 0x4F, 0xDA,
	0x9E, 0xC5, 0x49, 0x7B, 0xBD, 0xE8, 0xDF, 0xEE, 0xCA, 0xF4, 0x92, 0xDE, 0xE4, 0x76, 0x10, 0xDD,
	0x2A, 0x52, 0xDC, 0x73, 0x4E, 0x54, 0x8C, 0x30, 0x3D, 0x9A, 0xB2, 0x9B, 0xB8, 0x93, 0x29, 0x55,
	0xFA, 0x7A, 0xC9, 0xDA, 0x10, 0x97, 0xE5, 0xB6, 0x23, 0x02, 0xDD, 0x38, 0x4C, 0x9B, 0x1F, 0x9A,
	0xD5, 0x49, 0xE9, 0x34, 0x0F, 0x28, 0x2D, 0x1B, 0x52, 0x39, 0x5C, 0x36, 0x89, 0x56, 0xA7, 0x96,
	0x14, 0xBE, 0x2E, 0xC5, 0x3E, 0x08, 0x5F, 0x47, 0xA9, 0xDF, 0x88, 0x9F, 0xD4, 0xCC, 0x69, 0x1F,
	0x30, 0x9F, 0xE7, 0xCD, 0x80, 0x45, 0xF3, 0xE7, 0x2A, 0x1D, 0x16, 0xB2, 0xF1, 0x54, 0xC8, 0x6C,
	0x2B, 0x0D, 0xD4, 0x65, 0xF7, 0xE3, 0x36, 0xD4, 0xA5, 0x3B, 0xD1, 0x79, 0x4C, 0x54, 0xF0, 0x2A,
	0xB4, 0xB2, 0x56, 0x45, 0x2E, 0xAB, 0x7B, 0x88, 0xC5, 0xFA, 0x74, 0xAD, 0x03, 0xB8, 0x9E, 0xD5,
	0xF5, 0x6F, 0xDC, 0xFA, 0x44, 0x49, 0x31, 0xF6, 0x83, 0x32, 0xFF, 0xC2, 0xB1, 0xE9, 0xE1, 0x98,
	0x3D, 0x6F, 0x31, 0x0D, 0xAC, 0xB1, 0x08, 0x83, 0x9D, 0x0D, 0x10, 0xD1, 0x41, 0xF9, 0x00, 0xBA,
	0x1A, 0xCF, 0x13, 0x71, 0xE4, 0x86, 0x21, 0x2F, 0x23, 0x65, 0xC3, 0x45, 0xA0, 0xC3, 0x92, 0x48,
	0x9D, 0xEA, 0xDD, 0x31, 0x2C, 0xE9, 0xE2, 0x10, 0x22, 0xAA, 0xE1, 0xAD, 0x2C, 0xC4, 0x2D, 0x7F
};

int main(int argc, char* argv[]) {
	extern char *optarg;
	extern int optind;
	
	std::string outFilename;
	static char usageString[] = "Usage: readgameexe [-o outfile] [-v] [-d] <Gameexe.dat>";
	
	bool dumpEncoded = false;

	// Handle options
	int option = 0;
	while ((option = getopt(argc, argv, "o:vd")) != -1) {
		switch (option) {
		case 'v':
			Logger::increaseVerbosity();
			break;
		case 'o':
			outFilename = std::string(optarg);
		break;
		case 'd':
			dumpEncoded = true;
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

	if (outFilename.empty())
		outFilename = filename + std::string(".txt");


	std::fstream fileStream(filename, std::ifstream::in | std::ifstream::binary);
	
	fileStream.ignore(std::numeric_limits<std::streamsize>::max());
	unsigned int length = fileStream.gcount() - 8;
	fileStream.clear();
	fileStream.seekg(8, std::ios_base::beg);
	unsigned char *buffer = new unsigned char[length];
	fileStream.read((char*) buffer, length);
	fileStream.close();
	
	// xor decode
	for (unsigned int i = 0; i < length; i++) {
		buffer[i] ^= XorKey[i & 0xFF];
	}
	unsigned int compressedSize = readUInt32(buffer);
	unsigned int decompressedSize = readUInt32(buffer + 4);

	if (compressedSize != length) {
		Logger::Log(Logger::ERROR) << "Expected " << std::hex << length << ", got " << compressedSize << std::endl;
		unsigned int possibleKey = (length ^ compressedSize);
		Logger::Log(Logger::INFO) << "Possibly requiring key starting with " << std::hex << std::setfill('0');
		for (unsigned int k = 0; k < 4; k++) {
			Logger::Log(Logger::INFO) << std::setw(2) << (possibleKey & 0xFF) << " ";
			possibleKey >>= 8;
		}
		Logger::Log(Logger::INFO) << std::endl;

		if (dumpEncoded) {
			fileStream.open(outFilename, std::ios::out | std::ios::binary);
			fileStream.write((char*) buffer, length);
			fileStream.flush();
		}


		exit(1);
	}
	
	unsigned char *decompressed = new unsigned char[decompressedSize];
	decompressData(buffer + 8, decompressed, decompressedSize);
	
	std::wstring_convert<std::codecvt_utf8<char16_t>, char16_t> ucs2conv;
	std::u16string gameExe16((char16_t*) decompressed, decompressedSize >> 1);
		
	fileStream.open(outFilename, std::ios::out);
	fileStream << ucs2conv.to_bytes(gameExe16);
	fileStream.close();

	delete[] buffer;
	delete[] decompressed;
	
	return 0;
}