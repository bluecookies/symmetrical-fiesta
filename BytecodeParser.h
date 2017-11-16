#ifndef BYTECODE_H
#define BYTECODE_H

#include <iostream>
#include "Helper.h"


struct Label {
	unsigned int offset;
	unsigned int index;
	std::string name;
};


struct ScriptCommand {
	// Offset of instruction in script
	unsigned int offset;
	// Index of the file it appears in
	unsigned int file;
	// Index of command - might not need
	unsigned int index;
	// Command name
	std::string name;
};

struct SceneInfo {
	StringList sceneNames;
	// TODO: if no global info available, infer from local
	// TODO: handle things when no global available
	std::vector<ScriptCommand> commands;	// global + local
	unsigned int numGlobalCommands = 0;
	unsigned int thisFile = 0xFFFFFFFF;
	
	std::vector<Label> labels, markers, functions;
};

class BytecodeBuffer {
	private:
		unsigned char* bytecode = NULL;
		unsigned int dataLength = 0;
		unsigned int currAddress = 0;
	public:
		BytecodeBuffer(std::ifstream &f, HeaderPair index);
		~BytecodeBuffer();
		// TODO: fill in the assign and move
		
		unsigned int size();
		// more like a reader than a buffer now
		unsigned int getInt();
		unsigned char getChar();
		unsigned int getAddress();
		bool done();
};


// TODO: strings go into scene info too
// why are they outside
void parseBytecode(BytecodeBuffer &buf, std::string filename, SceneInfo sceneInfo,
	const StringList &strings, const StringList &strings2);

#endif