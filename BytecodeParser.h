#ifndef BYTECODE_H
#define BYTECODE_H

#include <iostream>
#include <vector>
#include <set>

#include "Helper.h"

std::string VarType(unsigned int type);

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

#define	STACK_VOID	0x00
#define	STACK_NUM	0x0a
#define STACK_STR	0x14
#define	STACK_OBJ	0x51e

class StackValue {
	bool isEndFrame = false;	

	public:
		StackValue() {}

		unsigned int value = 0xDEADBEEF;
		unsigned int type = STACK_VOID;
		unsigned int length = 0;
		std::string name;

		static StackValue Sentinel() {
			StackValue val = StackValue();
			val.isEndFrame = true;
			return val;
		};

		bool endFrame() {
			return isEndFrame;
		}

		bool isIndex() {
			return (type == STACK_NUM && value == 0xFFFFFFFF);
		}

		StackValue(unsigned int val_, unsigned int type_) : value(val_), type(type_) {
			//if (type == STACK_NUM) {
			//	name = std::to_string(value);
			//} else {
				// int for intermediate even though its not
				name = "int" + std::to_string(value);
			//}
		};

		StackValue(unsigned int type_, std::string name_) : type(type_), name(name_) {

		};
};

typedef std::vector<StackValue> ProgStack;

class BytecodeBuffer {
	private:
		unsigned char* bytecode = NULL;
		unsigned int dataLength = 0;
		unsigned int currAddress = 0;

	public:
		unsigned int getInt();
		unsigned char getChar();
		bool done() {
			return currAddress == dataLength;
		};
		unsigned int getAddress() {
			return currAddress;
		};

		void setAddress(unsigned int address) {
			currAddress = address;
		}

		BytecodeBuffer(std::ifstream& f, unsigned int length);
		~BytecodeBuffer();
};

typedef class BytecodeParser Parser;
typedef class BasicBlock Block;
class Instruction {
	protected:
		unsigned char opcode = 0;
		static unsigned char width;
		Instruction(Parser *parser, unsigned char opcode);
	public:	
		unsigned int address = 0;
		bool toPrint = true;

		static bool expandGlobalVars;
		static bool expandCommandNames;
		static bool expandStrings;

		static Instruction* newInst(Parser *parser, unsigned char opcode); 

		virtual void setTarget(Block*) { };

		virtual ~Instruction() {};
		static void setWidth(unsigned int maxAddress);

		virtual void print(Parser *parser, std::ofstream &stream) const;
};

class BasicBlock {
	private:
		static unsigned int count;

		std::vector<Instruction*> instructions;
	public:
		enum Type {
			ONEWAY, TWOWAY, SWITCH,
			CALL, RET, FALL,
			INVALID
		};
	public:
		unsigned int startAddress = 0;
		unsigned int index;
		static std::set<unsigned int> blockAddresses;

		std::set<BasicBlock*> prec;
		std::set<BasicBlock*> succ;

		std::set<unsigned int> labels;

		Type blockType = INVALID;

		bool isFunction = false;

		BasicBlock(unsigned int address) : startAddress(address) {
			index = count;
			count++;

			blockAddresses.insert(address);
			// could use return to see if succeeded instead of checking outside

			Logger::VVDebug(address) << "Block created.\n";
		};
		~BasicBlock();

		void pushInst(Instruction* pInst) {
			instructions.push_back(pInst);
		};

		void simplifyInstructions(Parser* parser);

		void printInstructions(Parser* parser, std::ofstream &out);

		unsigned int size() {
			return instructions.size();
		}
};


struct Label {
	unsigned int offset = 0;
	unsigned int index = 0;
	BasicBlock* pBlock = nullptr;
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

class ProgBranch {
	public:
		unsigned int address = 0;
		ProgStack stack;
		BasicBlock* prev = nullptr;
		Instruction* jump = nullptr;
		ProgBranch(unsigned int address_) : address(address_) {};
		ProgBranch(unsigned int address_, ProgStack stack_) : address(address_), stack(stack_) {};
};

struct SceneInfo {
	StringList sceneNames;
	// Global and local refer to the variable scope
	// Global variables include global and static
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

class CallRet {
	public:
		unsigned int address;
		BasicBlock* pBlock;
		unsigned int stackPointer;
		CallRet(unsigned int ret, BasicBlock* retBlock, unsigned int retSP) :
			address(ret), pBlock(retBlock), stackPointer(retSP) {};
};

class BytecodeParser {
	private:
		BytecodeBuffer* buf;

		std::vector<BasicBlock*> blocks;

		bool isParsed(unsigned int address);
	public:
		unsigned int instAddress = 0;
		SceneInfo sceneInfo;
		ProgStack stack;

		std::vector<CallRet> callStack;

		unsigned int getInt() {
			return buf->getInt();
		};
		unsigned char getChar() {
			return buf->getChar();
		};
		unsigned int readArgs(std::vector<unsigned int> &typeList);

		BytecodeParser(std::ifstream &f, HeaderPair index, SceneInfo info);
		~BytecodeParser();

	public:
		void parseBytecode();
		void printInstructions(std::string filename, bool sorted = false);
		void dumpCFG(std::string filename);

		// wait fix this
		BasicBlock* addBlock(BasicBlock* pBlock, unsigned int address, Instruction* inst = nullptr);
};
#endif