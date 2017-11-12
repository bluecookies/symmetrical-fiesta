#ifndef HELPER_H
#define HELPER_H

#include <iostream>
#include <fstream>
#include <vector>

#include "Structs.h"

typedef std::vector<std::string> StringList;

unsigned int readUInt32(unsigned char* buf);
unsigned int readUInt32(char* buf);
void readHeaderPair(std::ifstream &stream, HeaderPair &pair);
void readHeaderPair(unsigned char* buf, HeaderPair &pair);

StringList readStrings(std::ifstream &f, HeaderPair index, HeaderPair data, bool decode = false);
//void printStrings(StringList strings, std::ostream &f = std::cout);
inline std::ostream& operator << (std::ostream& stream, const StringList strings) {
	for (auto &string:strings) {
    stream << string << std::endl;
	}
	return stream;
}
inline std::string operator + (HeaderPair pair, std::string string) {
	return std::to_string(pair.offset) + ", " + std::to_string(pair.count) + "\t" + string;
}

inline StringList operator + (std::vector<HeaderPair> pairs, StringList strings) {
	StringList result; result.reserve(strings.size());
	int numPairs = pairs.size();
	for (StringList::iterator it = strings.begin(); it != strings.end(); it++) {
		result.push_back(pairs.at((it - strings.begin()) % numPairs) + *it);
	}
	return result;
}

StringList readFilenames(std::ifstream &f, unsigned int numFiles);

void decodeData(unsigned char* debuf, unsigned int desize);
void decompressData(unsigned char* compData, unsigned char* decompBegin, unsigned int decompSize);


// Logging
// This might actually be my first time writing c++, not c
// no Logger class. I wonder if that's okay
namespace Logger {
	class NullBuffer : public std::streambuf {
		public:
  	int overflow(int c) {
  		return c;
  	}
	};
	static NullBuffer nullBuf;
	static std::ostream nout(&nullBuf);

	//enum LogLevel {
		const int NONE = 0;
		const int ERROR = 1;
		const int WARN = 2;
		const int INFO = 3;
		const int DEBUG = 4;
	//};
	extern int LogLevel;
	
	inline std::ostream& Log(int level) {
		if (level <= LogLevel) {
			return std::cout;
		} else {
			return nout;
		}
	}
	// Yes, yes, verbosity and level are different
	inline void increaseVerbosity() {
		if (LogLevel < DEBUG)
			LogLevel++;
	}
}

#endif