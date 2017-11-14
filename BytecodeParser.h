#include <iostream>
#include "Helper.h"

struct Instruction {
	unsigned int address;
	unsigned char opcode;
	std::vector<unsigned int> args;
	bool nop = false;
	std::string comment;
};

typedef std::vector<Instruction> Instructions;
typedef std::vector<unsigned int> ProgStack;	// want strong typedef

class BytecodeParser {
	private:
		unsigned char* bytecode = NULL;
		unsigned int dataLength = 0;
		unsigned int currAddress = 0;
		// bastard rule of 3/5
		BytecodeParser(const BytecodeParser& src);
    BytecodeParser& operator=(const BytecodeParser& src);
		
		unsigned int readArg(Instruction &inst, unsigned int argSize);
		unsigned int readArgs(Instruction &inst, ProgStack &numStack, ProgStack &strStack, bool pop = true);
	public:
		BytecodeParser(std::ifstream &f, HeaderPair index);
		//void readBytecode(std::ifstream &f, HeaderPair index);
		void parseBytecode(Instructions &instList, const StringList &strings, const StringList &strings2);
		~BytecodeParser();
};