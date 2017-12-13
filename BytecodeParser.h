#ifndef PARSER_H
#define PARSER_H

#include <vector>

#include "Statements.h"



struct Label {
	unsigned int address;
	Label(unsigned int offset) : address(offset) {}
};

struct Stack {
	std::vector<Value> values;
	std::vector<unsigned int> stackHeights;
	Stack() : stackHeights({0}) {}

	unsigned int size() { return values.size(); }
	bool empty() { return size() == 0; }

	// Caller needs to check for emptiness
	Value& back() { return values.back(); }

	unsigned int& height() { return stackHeights.back(); }
	void openFrame() { stackHeights.push_back(0); }
	void closeFrame() { 
		values.erase(values.end() - height(), values.end());
		stackHeights.pop_back();
	}


	Value pop();
	void push(ValueExpr* pValue);
};

struct BasicBlock;
struct ProgBranch;

typedef class BytecodeParser Parser;
class BytecodeBuffer;
class ScriptInfo;
class ControlFlowGraph;

class BytecodeParser {
	private:
		BytecodeBuffer* buf;

	public:
		std::vector<ProgBranch> toTraverse;
		unsigned int instAddress = 0;
		// operand stack
		Stack stack;

		//std::vector<CallRet> callStack;

		unsigned int getInt();
		unsigned char getChar();
		Value getArg(unsigned int type);
		ValueExpr* getLValue(const ScriptInfo &info);

		Function getCallFunction(const ScriptInfo& info);
	public:
		BytecodeParser(std::ifstream &f, HeaderPair index);
		~BytecodeParser();

		void addBranch(BasicBlock* pBlock, Stack* saveStack = nullptr);
		void parse(ScriptInfo& info, ControlFlowGraph& cfg);
};

#endif