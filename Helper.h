#ifndef HELPER_H
#define HELPER_H

#include <fstream>
#include <iostream>
#include <vector>
#include <iomanip>
#include <sstream>

#include "Structs.h"

// C++11
template<typename T, typename ...Args>
std::unique_ptr<T> make_unique( Args&& ...args ) {
    return std::unique_ptr<T>( new T( std::forward<Args>(args)... ) );
}


static inline std::string toHex(unsigned int c, unsigned char width = 0) {
	std::stringstream ss;
	if (width > 0)
		ss << std::setfill('0') << std::setw(width);
	ss << std::hex << c;
	return ss.str();
}


typedef std::vector<std::string> StringList;
 
unsigned int readUInt32(unsigned char* buf);
unsigned int readUInt32(char* buf);
void readHeaderPair(std::ifstream &stream, HeaderPair &pair);
void readHeaderPair(unsigned char* buf, HeaderPair &pair);

void readStrings(std::ifstream &f, StringList &strings, HeaderPair index, HeaderPair data, bool decode = false);
//void printStrings(StringList strings, std::ostream &f = std::cout);
inline std::ostream& operator << (std::ostream& stream, const StringList &strings) {
	for (auto &string:strings) {
    stream << string << std::endl;
	}
	return stream;
}
inline std::string operator + (HeaderPair pair, const std::string &string) {
	return std::to_string(pair.offset) + ", " + std::to_string(pair.count) + "\t" + string;
}

inline StringList operator + (std::vector<HeaderPair> pairs, const StringList &strings) {
	StringList result; result.reserve(strings.size());
	unsigned int numPairs = pairs.size();
	for (StringList::const_iterator it = strings.begin(); it != strings.end(); it++) {
		result.push_back(pairs.at((it - strings.begin()) % numPairs) + *it);
	}
	return result;
}

StringList readFilenames(std::ifstream &f, unsigned int numFiles);

void decodeExtra(unsigned char* debuf, unsigned int desize, unsigned char* key);
void decodeData(unsigned char* debuf, unsigned int desize);
void decompressData(unsigned char* compData, unsigned char* decompBegin, unsigned int decompSize);

#endif