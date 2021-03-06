#include <locale>
#include <codecvt>

#include <cassert>
#include "Helper.h"
#include "Structs.h"
#include "Logger.h"

std::wstring_convert<std::codecvt_utf8<char16_t>, char16_t> g_UCS2Conv;

unsigned int readUInt32(unsigned char* buf) {
	return buf[0] + (buf[1] << 8) + (buf[2] << 16) + (buf[3] << 24);
}

/*
unsigned int readUInt32(char* buf) {
	return readUInt32((unsigned char*) buf);
}
*/

void readHeaderPair(std::ifstream &stream, HeaderPair &pair) {
	stream.read(reinterpret_cast<char*>(&pair.offset), sizeof(uint32_t));
	stream.read(reinterpret_cast<char*>(&pair.count), sizeof(uint32_t));
}

void readHeaderPair(unsigned char* buf, HeaderPair &pair) {
	pair.offset = readUInt32(buf);
	pair.count = readUInt32(buf+4);
}

void readStrings(std::ifstream &f, StringList &strings, HeaderPair index, HeaderPair data, bool decode) {
	assert(index.count == data.count);

	if (index.count == 0)
		return;
	
	strings.reserve(index.count);
		
	f.seekg(index.offset, std::ios_base::beg);
		
	// Read offsets and lengths of strings
	// offset and lengths are in wide chars (2 bytes)
	HeaderPair* stringIndices = new HeaderPair[index.count];
	for (unsigned int i = 0; i < index.count; i++) {
		readHeaderPair(f, stringIndices[i]);
	}
		
	// Read data
	std::vector<char16_t> strBuf;
	for (unsigned int i = 0; i < index.count; i++) { 
		f.seekg(data.offset + 2 * stringIndices[i].offset, std::ios_base::beg);
		
		strBuf.resize(stringIndices[i].count);
		f.read(reinterpret_cast<char*>(&strBuf[0]), 2 * stringIndices[i].count);
		
		if (decode) {
			for (unsigned int j = 0; j < stringIndices[i].count; j++) {
				strBuf[j] ^= (i * 0x7087);
			}
		}
		
		std::u16string string16(strBuf.begin(), strBuf.end());
		try {
			strings.push_back(g_UCS2Conv.to_bytes(string16));
		} catch (std::exception &e) {
			std::cout << "Exception: " << e.what() << std::endl;
  	}
	}

	delete[] stringIndices;
	
	Logger::Debug() << "Read " << strings.size() << " strings from 0x" << std::hex << data.offset << std::dec << std::endl;
}

// Requires stream pointer to be at beginning of table
StringList readFilenames(std::ifstream &f, unsigned int numFiles) {
	uint32_t *filenameLengths = new uint32_t[numFiles];
	f.read(reinterpret_cast<char*>(filenameLengths), numFiles * 4);
	
	StringList filenames;
	filenames.reserve(numFiles);
	
	char16_t* strBuf = new char16_t[512];
	for (unsigned int i = 0; i < numFiles; i++) {
		f.read(reinterpret_cast<char*>(strBuf), filenameLengths[i]);

		std::u16string string16(strBuf, filenameLengths[i] >> 1);
		try {
			filenames.push_back(g_UCS2Conv.to_bytes(string16));
		} catch (std::exception &e) {
			std::cout << "Exception: " << e.what() << std::endl;
  	}
	}
	
	delete[] strBuf;
	delete[] filenameLengths;
	
	return filenames;
}

void decodeExtra(unsigned char* debuf, unsigned int desize, unsigned char* key) {
	unsigned int key_idx = 0;
	unsigned int xor_idx = 0;

	for (xor_idx = 0; xor_idx < desize; xor_idx++, key_idx++, key_idx = key_idx & 0xF) {
		debuf[xor_idx] ^= key[key_idx];
	}
}
void decodeData(unsigned char* debuf, unsigned int desize) {
	unsigned int key_idx = 0;
	unsigned int xor_idx = 0;

	static unsigned char key[] = {
		0x70, 0xF8, 0xA6, 0xB0, 0xA1, 0xA5, 0x28, 0x4F, 0xB5, 0x2F, 0x48, 0xFA, 0xE1, 0xE9, 0x4B, 0xDE,
		0xB7, 0x4F, 0x62, 0x95, 0x8B, 0xE0, 0x03, 0x80, 0xE7, 0xCF, 0x0F, 0x6B, 0x92, 0x01, 0xEB, 0xF8,
		0xA2, 0x88, 0xCE, 0x63, 0x04, 0x38, 0xD2, 0x6D, 0x8C, 0xD2, 0x88, 0x76, 0xA7, 0x92, 0x71, 0x8F,
		0x4E, 0xB6, 0x8D, 0x01, 0x79, 0x88, 0x83, 0x0A, 0xF9, 0xE9, 0x2C, 0xDB, 0x67, 0xDB, 0x91, 0x14,
		0xD5, 0x9A, 0x4E, 0x79, 0x17, 0x23, 0x08, 0x96, 0x0E, 0x1D, 0x15, 0xF9, 0xA5, 0xA0, 0x6F, 0x58,
		0x17, 0xC8, 0xA9, 0x46, 0xDA, 0x22, 0xFF, 0xFD, 0x87, 0x12, 0x42, 0xFB, 0xA9, 0xB8, 0x67, 0x6C,
		0x91, 0x67, 0x64, 0xF9, 0xD1, 0x1E, 0xE4, 0x50, 0x64, 0x6F, 0xF2, 0x0B, 0xDE, 0x40, 0xE7, 0x47,
		0xF1, 0x03, 0xCC, 0x2A, 0xAD, 0x7F, 0x34, 0x21, 0xA0, 0x64, 0x26, 0x98, 0x6C, 0xED, 0x69, 0xF4,
		0xB5, 0x23, 0x08, 0x6E, 0x7D, 0x92, 0xF6, 0xEB, 0x93, 0xF0, 0x7A, 0x89, 0x5E, 0xF9, 0xF8, 0x7A,
		0xAF, 0xE8, 0xA9, 0x48, 0xC2, 0xAC, 0x11, 0x6B, 0x2B, 0x33, 0xA7, 0x40, 0x0D, 0xDC, 0x7D, 0xA7,
		0x5B, 0xCF, 0xC8, 0x31, 0xD1, 0x77, 0x52, 0x8D, 0x82, 0xAC, 0x41, 0xB8, 0x73, 0xA5, 0x4F, 0x26,
		0x7C, 0x0F, 0x39, 0xDA, 0x5B, 0x37, 0x4A, 0xDE, 0xA4, 0x49, 0x0B, 0x7C, 0x17, 0xA3, 0x43, 0xAE,
		0x77, 0x06, 0x64, 0x73, 0xC0, 0x43, 0xA3, 0x18, 0x5A, 0x0F, 0x9F, 0x02, 0x4C, 0x7E, 0x8B, 0x01,
		0x9F, 0x2D, 0xAE, 0x72, 0x54, 0x13, 0xFF, 0x96, 0xAE, 0x0B, 0x34, 0x58, 0xCF, 0xE3, 0x00, 0x78,
		0xBE, 0xE3, 0xF5, 0x61, 0xE4, 0x87, 0x7C, 0xFC, 0x80, 0xAF, 0xC4, 0x8D, 0x46, 0x3A, 0x5D, 0xD0,
		0x36, 0xBC, 0xE5, 0x60, 0x77, 0x68, 0x08, 0x4F, 0xBB, 0xAB, 0xE2, 0x78, 0x07, 0xE8, 0x73, 0xBF
	};
	for (xor_idx = 0; xor_idx < desize; xor_idx++, key_idx++, key_idx = key_idx & 0x800000FF) {
		debuf[xor_idx] ^= key[key_idx];
	}
}

// LZSS variant - similar to Nintendo's Yaz0 format
void decompressLZSS(unsigned char* compData, unsigned char* decompBegin, unsigned int decompSize) {
	unsigned char* from = compData;
	unsigned char* to = decompBegin;
	unsigned char* decompEnd = decompBegin + decompSize;
	unsigned int EAX;
	while (true) {
		unsigned char marker = *from;
		from++;
		for (int i = 0; i < 8; i++) {	// Iterate over marker's bits
				if (to == decompEnd)
					return;
				if ((marker & 1) == 0) {
					EAX = from[0] + (from[1] << 8);	// Load word from source
					from += 2;
					unsigned int counter = (EAX & 0xF) + 2;
					EAX >>= 4;
					while (counter > 0) {
						*to = *(to - EAX);
						to++; counter--;
					}
				} else {
					*to = *from;
					to++; from++;
				}
				marker >>= 1;
		}
	}
}
