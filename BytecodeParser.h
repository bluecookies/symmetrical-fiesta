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

#define	STACK_UNDEF		0x0000
#define	STACK_NUM		0x000a
#define STACK_NUM_ARR	0x000b
#define	STACK_NUM_REF	0x000d
#define STACK_COMMAND	0x000e
#define	STACK_STR		0x0014
#define STACK_STR_ARR	0x0015
#define STACK_STR_REF	0x0017
#define	STACK_OBJ		0x051e
#define	STACK_FRAME		0xFFFF


struct ScriptVar {
	unsigned int type = STACK_UNDEF;
	unsigned int length = 0;
	//
	std::string name;

};


// TODO: make names more indicative
// varNames -> globalVars + localVars
// mainStrings -> localStrings
// varStrings -> paramNames
// maybe still separate out commands
struct SceneInfo {
	StringList sceneNames;
	std::vector<ScriptVar> scriptVars;
	StringList mainStrings, varStrings;
	// TODO: if no global info available, infer from local
	// TODO: handle things when no global available
	std::vector<ScriptCommand> commands;	// global + local
	unsigned int numGlobalCommands = 0;
	unsigned int numGlobalVars = 0;
	unsigned int thisFile = 0xFFFFFFFF;
	
	std::vector<Label> labels, markers, functions;
};


struct StackValue {
	unsigned int value = 0;
	unsigned int type = STACK_UNDEF;

	StackValue() {};
	StackValue(unsigned int val_, unsigned int type_) : value(val_), type(type_) {};
};

typedef std::vector<StackValue> ProgStack;

typedef struct ProgramInfo {
	// State info
	ProgStack stack;
	
	// instruction info
	unsigned int programCounter = 0;

	std::vector<unsigned int> args;
} ProgInfo;

class BytecodeParser {
	private:
		unsigned char* bytecode = NULL;
		unsigned int dataLength = 0;
		ProgInfo progInfo;
		SceneInfo sceneInfo;

		void handle01();
		void handleEval();
		void handle08();

		void handlePush();
		void handlePop();
		void handleDup();
		void handleJump(unsigned char condition);
		void handleCalc();
		void handleAssign();
		void handleCall();

		unsigned int readArgs(bool pop = true);

	public:
		BytecodeParser(std::ifstream &f, HeaderPair index, SceneInfo sceneInfo);
		~BytecodeParser();
		// TODO: fill in the assign and move

		unsigned int getInt();
		unsigned char getChar();
		
		void parseBytecode();
};
#endif