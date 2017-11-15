#ifndef BYTECODE_H
#define BYTECODE_H

#include <iostream>
#include "Helper.h"


struct Label {
	unsigned int offset;
	unsigned int index;
	std::string name;
};

struct LabelData {
	std::vector<Label> labels, markers;
	std::vector<Label> functions;
	std::vector<Label> functionTable;
};

struct ScriptCommand {
	// Offset of instruction in script
	unsigned int offset;
	// Index of the file it appears in
	unsigned int file;
	// Index of command
	unsigned int index;
	// Command name
	std::string name;
};

struct SceneInfo {
	StringList sceneNames;
	std::vector<ScriptCommand> commands;
	unsigned int thisFile;
};


// change later
inline bool compOffset(Label a, Label b) { return (a.offset < b.offset); }
inline bool compOffsetArgh(ScriptCommand a, ScriptCommand b) { return (a.offset < b.offset); }


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
		static StringList mnemonics;
		// bastard rule of 3/5
		BytecodeParser(const BytecodeParser& src);
    BytecodeParser& operator=(const BytecodeParser& src);
		
		unsigned int readArg(Instruction &inst, unsigned int argSize);
		unsigned int readArgs(Instruction &inst, ProgStack &numStack, ProgStack &strStack, bool pop = true);
	public:
		BytecodeParser(std::ifstream &f, HeaderPair index);
		//void readBytecode(std::ifstream &f, HeaderPair index);
		void parseBytecode(Instructions &instList, const StringList &strings, const StringList &strings2, const SceneInfo &sceneInfo);
		void printInstructions(Instructions &instList, std::string filename, LabelData &info, const SceneInfo &sceneInfo);
		~BytecodeParser();
};

#endif