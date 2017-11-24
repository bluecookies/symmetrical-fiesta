#ifndef BYTECODE_H
#define BYTECODE_H

#include <iostream>
#include "Helper.h"

struct ScriptCommand {
	// Offset of instruction in script
	unsigned int offset = 0;
	// Index of the file it appears in
	unsigned int file = 0xFFFFFFFF;
	// Index of command - might not need
	unsigned int index = 0;
	// Command name
	std::string name;
};

struct Label {
	unsigned int offset = 0;
	unsigned int index = 0;
	std::string name;
	
	Label& operator=(const ScriptCommand &in) {
		offset = in.offset;
		index = in.index;
		name = in.name;
		
		return *this;
	}
	
	bool operator<(const Label& b) const {
		return offset < b.offset;
	}
	
	Label() {
		offset = 0;
		index = 0;
	}
	Label(const ScriptCommand &in) {
		offset = in.offset;
		index = in.index;
		name = in.name;
	}
};

#define	STACK_VOID	0x00
#define	STACK_NUM	0x0a
#define STACK_STR	0x14
#define	STACK_OBJ	0x51e

struct StackValue {
	bool endFrame = false;
	unsigned int value = 0xDEADBEEF;
	unsigned int type = STACK_VOID;
	std::string name;

	unsigned int length = 0;

	// TODO: name this properly
	StackValue() {
		endFrame = true;
	};
	StackValue(unsigned int val_, unsigned int type_) : value(val_), type(type_) {
		if (type_ == STACK_NUM)
			name = std::to_string(val_);
	};
};

typedef std::vector<StackValue> ProgStack;

struct SceneInfo {
	StringList sceneNames;
	// Global and local refer to the variable scope
	ProgStack globalVars;
	StringList localVarNames;
	StringList mainStrings;
	// TODO: if no global info available, infer from local
	// TODO: handle things when no global available
	std::vector<ScriptCommand> commands;
	unsigned int numGlobalCommands = 0;
	unsigned int numGlobalVars = 0;
	unsigned int thisFile = 0xFFFFFFFF;
	
	std::vector<Label> labels, markers, functions;
};


class BytecodeParser {
	private:
		unsigned char* bytecode = NULL;
		unsigned int dataLength = 0;
		unsigned int currAddress = 0;

		ProgStack stack;
		SceneInfo sceneInfo;

		unsigned int getInt();
		unsigned char getChar();

		unsigned int parseFunction(const Label &function, ProgStack &localVars, std::string &outputString);

	public:
		BytecodeParser(std::ifstream &f, HeaderPair index, SceneInfo info);
		~BytecodeParser();
		// TODO: fill in the assign and move
		
		void parseBytecode(std::string filename);
};
#endif