#ifndef HELPER_H
#define HELPER_H

#include <iostream>
#include <fstream>
#include <vector>

#include "Structs.h"

// not sure how this is meant to be done
// hope i don't get bitten

// mingw actually, i'm sure visual studio would be better
#ifdef XXX_WIN32
	#include <locale>
	#include <codecvt>
	#include <windows.h>
	#ifdef ERROR
		// I could change, but this is just annoying
		#undef ERROR
	#endif
	static std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> g_UTF16Conv;
	// typedef
	// broken
	#define OPEN_OFSTREAM(filename) _wfopen(g_UTF16Conv.from_bytes((filename)).data(), L"wb")
	#define WRITE_FILE(handle, buf, length) WriteFile(handle, buf, length, NULL, NULL)
	#define PUTC_FILE(handle, c) putc(c, handle)
	// CloseHandle
	#define CLOSE_OFSTREAM(handle) fclose(handle) 
#else
	#define OPEN_OFSTREAM(filename) std::ofstream(filename, std::ios::out | std::ios::binary)
	#define WRITE_FILE(stream, buf, length) stream.write((char*) buf, length)
	#define PUTC_FILE(stream, c) stream.put(c)
	#define CLOSE_OFSTREAM(stream) stream.close()
#endif
// i'm getting bitten, help
// this is not the way its mean't to be done


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


	// add a level prefix or something
	//enum LogLevel {
		const int NONE = 0;
		const int LEVEL_ERROR = 1;
		const int LEVEL_WARN = 2;
		const int LEVEL_INFO = 3;
		const int LEVEL_DEBUG = 4;
		const int VERBOSE_DEBUG = 5;
	//};
	extern int LogLevel;
	
	inline std::ostream& Log(int level, unsigned int address = 0, std::ostream& stream = std::cout) {
		if (level <= LogLevel) {
			if (address > 0)
				return stream << "0x" << std::hex << address << ": ";
			else
				return stream;
		} else {
			return nout;
		}
	}

	// Maybe not functions, but just references that change statically
	inline std::ostream& Error(unsigned int address = 0) {
		return Log(LEVEL_ERROR, address) << "Error: ";
	}
	inline std::ostream& Warn(unsigned int address = 0) {
		return Log(LEVEL_WARN, address) << "Warning: ";
	}
	inline std::ostream& Info(unsigned int address = 0) {
		return Log(LEVEL_INFO, address);
	}
	inline std::ostream& Debug(unsigned int address = 0) {
		return Log(LEVEL_DEBUG, address);
	}

	// Yes, yes, verbosity and level are different
	inline void increaseVerbosity() {
		if (LogLevel < VERBOSE_DEBUG)
			LogLevel++;
	}
}

#endif