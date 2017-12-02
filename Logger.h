#ifndef LOGGER_H
#define LOGGER_H

#include <fstream>

#define ANSI_RESET "\x1b[0m"
#define ANSI_RED "\x1b[31m"
#define ANSI_YELLOW "\x1b[33m"

namespace Logger {
	class NullBuffer : public std::streambuf {
		public:
  	int overflow(int c) {
  		return c;
  	}
	};
	static NullBuffer nullBuf;
	static std::ostream nout(&nullBuf);

	const int LEVEL_NONE = 0;
	const int LEVEL_ERROR = 1;
	const int LEVEL_WARN = 2;
	const int LEVEL_INFO = 3;
	const int LEVEL_DEBUG = 4;
	const int LEVEL_VERBOSE_DEBUG = 5;
	const int LEVEL_VERBOSE_VERBOSE_DEBUG = 6;

	extern int LogLevel;
	
	inline std::ostream& Log(int level, unsigned int address, std::ostream& stream = std::cout) {
		if (level <= LogLevel) {
			if (address != 0xFFFFFFFF)
				return stream << "0x" << std::hex << address << ": ";
			else
				return stream;
		} else {
			return nout;
		}
	}

	// Maybe not functions, but just references that change statically
	inline std::ostream& Error(unsigned int address = 0xFFFFFFFF) {
		return Log(LEVEL_ERROR, address) << ANSI_RED << "Error" << ANSI_RESET << ": ";
	}
	inline std::ostream& Warn(unsigned int address = 0xFFFFFFFF) {
		return Log(LEVEL_WARN, address) << ANSI_YELLOW << "Warning" << ANSI_RESET << ": ";
	}
	inline std::ostream& Error_(unsigned int address = 0xFFFFFFFF) {
		return Log(LEVEL_ERROR, address);
	}
	inline std::ostream& Warn_(unsigned int address = 0xFFFFFFFF) {
		return Log(LEVEL_WARN, address);
	}
	inline std::ostream& Info(unsigned int address = 0xFFFFFFFF) {
		return Log(LEVEL_INFO, address);
	}
	inline std::ostream& Debug(unsigned int address = 0xFFFFFFFF) {
		return Log(LEVEL_DEBUG, address);
	}

	inline std::ostream& VDebug(unsigned int address = 0xFFFFFFFF) {
		return Log(LEVEL_VERBOSE_DEBUG, address);
	}

	inline std::ostream& VVDebug(unsigned int address = 0xFFFFFFFF) {
		return Log(LEVEL_VERBOSE_VERBOSE_DEBUG, address);
	}

	inline void increaseVerbosity() {
		//if (LogLevel < LEVEL_VERBOSE_VERBOSE_DEBUG)
		LogLevel++;
	}
}

#endif