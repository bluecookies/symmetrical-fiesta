#ifndef PARSER_H
#define PARSER_H

#include <vector>
#include <map>

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
	void closeFrame();


	Value pop();
	void push(ValueExpr* pValue);
};

struct BasicBlock;
struct ProgBranch;

struct Function {
	std::string name = "FN_ERROR";
	unsigned int address = 0xFFFFFFFF;
	unsigned int index = 0xFFFFFFFF;

	Function() {}
	Function(std::string name_, unsigned int address_, int index_) : name(name_), address(address_), index(index_) {}
};

typedef class BytecodeParser Parser;
class BytecodeBuffer;
class ScriptInfo;
class ControlFlowGraph;

class BytecodeParser {
	private:
		BytecodeBuffer* buf;

		unsigned int numParams = 0;	// part of state
		std::vector<Value> localVars; // please no corrupt
		Value getLocalVar(unsigned int index);
	public:
		std::vector<ProgBranch> toTraverse;
		unsigned int instAddress = 0;
		// operand stack
		Stack stack;

		//std::vector<CallRet> callStack;

		unsigned int getInt();
		unsigned char getChar();
		Value getArg(unsigned int type, const ScriptInfo& info);
		ValueExpr* getLValue(const ScriptInfo &info);

		FunctionExpr* getCallFunction(const ScriptInfo& info);
	public:
		BytecodeParser(std::ifstream &f, HeaderPair index);
		~BytecodeParser();

		void addBranch(BasicBlock* pBlock, Stack* saveStack = nullptr);
		void parse(ControlFlowGraph& cfg, ScriptInfo& info, std::map<unsigned int, std::string> *pAsmLines = nullptr);

		unsigned char addressWidth;

		std::string getFunctionSignature();
};

#endif