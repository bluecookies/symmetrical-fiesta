#ifndef STATEMENTS_H
#define STATEMENTS_H

#include <memory>

#include "Logger.h"
#include "Helper.h"

namespace ValueType {
	const unsigned int UNDEF    = 0xFFFFFFFF;
	const unsigned int VOID     = 0x0;
	const unsigned int INT      = 0xa;
	const unsigned int INTLIST  = 0xb;
	const unsigned int INTREF   = 0xd;
	const unsigned int STR      = 0x14;
	const unsigned int STRLIST  = 0x15;
	const unsigned int STRREF   = 0x17;
	const unsigned int OBJ_STR  = 0x51e;
};

inline std::string VarType(unsigned int type) {
	switch (type) {
		case ValueType::VOID: return std::string("void");
		case ValueType::INT: return std::string("int");
		case ValueType::INTLIST: return std::string("intlist");
		case ValueType::INTREF: return std::string("intref");
		case ValueType::STR: return std::string("str");
		case ValueType::STRLIST: return std::string("strlist");
		case ValueType::STRREF: return std::string("strref");
		case ValueType::OBJ_STR: return std::string("obj");	//unsure
		default:
			return "<0x" + toHex(type) + ">";
	}
}

// For ValueExpr of type INT
enum IntType {
	IntegerInvalid,
	IntegerSimple,
	IntegerLocalVar,
	IntegerFunction,
	IntegerGlobalVar,
	IntegerIndexer,
	IntegerBool,
	IntegerLocalRef,
};

class ValueExpr;
class Statement;
typedef std::unique_ptr<ValueExpr> Value;
typedef std::vector<Statement*> StatementBlock;

class ValueExpr {
	private:
		int lineNum = -1;
	protected:
		unsigned int type = ValueType::UNDEF;
		ValueExpr(unsigned int type) : type(type) {}
		ValueExpr() {}
	public:
		virtual ~ValueExpr() {}
		virtual ValueExpr* clone() const = 0;

		unsigned int getType() const { return type; }
		void setType(unsigned int type_) {
			type = type_;
		}


		// This is some fake misleading naming here
		// Not real lvalues and rvalues, more like just "assignable"
		virtual bool isLValue() { return false; }
		ValueExpr* toRValue();

		virtual bool hasSideEffect() { return false; }
		virtual std::string print(bool hex=false) const;


		int getLineNum() { return lineNum; }
		void setLineNum(int lineNum_) { lineNum = lineNum_; }

		virtual IntType getIntType() { return IntegerInvalid; }
		virtual unsigned int getIndex() { return 0xFF000000; }

		virtual void negateBool() { Logger::Warn() << "Negating non condition.\n"; }
		virtual bool isBinaryExpression() { return false; }
};

class BinaryValueExpr: public ValueExpr {
	protected:
		Value expr1 = nullptr;
		Value expr2 = nullptr;
		unsigned int op = 0;
	public:
		BinaryValueExpr(Value e1, Value e2, unsigned int op_);
		BinaryValueExpr(const BinaryValueExpr& copy) : ValueExpr(copy), expr1(copy.expr1->clone()), expr2(copy.expr2->clone()), op(copy.op) {}
		virtual BinaryValueExpr* clone() const override { return new BinaryValueExpr(*this); }

		std::string print(bool hex=false) const override;
		bool hasSideEffect() override { return expr1->hasSideEffect() || expr2->hasSideEffect(); }
		IntType getIntType() override { return (type == ValueType::INT) ? IntegerSimple : IntegerInvalid; }

		void negateBool() override;

		Value& getExpression1() { return expr1; }
		Value& getExpression2() { return expr2; }
		unsigned int getOp() { return op; }

		virtual bool isBinaryExpression() override { return true; }
};

class IndexValueExpr: public BinaryValueExpr {
	public:
		IndexValueExpr(Value e1, Value e2);
		virtual IndexValueExpr* clone() const override { return new IndexValueExpr(*this); }

		std::string print(bool hex=false) const override;

		virtual bool isLValue() override { return (type == ValueType::INTREF || type == ValueType::STRREF); }
};

class RawValueExpr: public ValueExpr {
	unsigned int value = 0;
	std::string str;
	public:
		RawValueExpr(unsigned int type, unsigned int value) : ValueExpr(type), value(value) {}
		RawValueExpr(std::string str, unsigned int value) : ValueExpr(ValueType::STR), value(value), str(str) {}

		virtual RawValueExpr* clone() const override { return new RawValueExpr(*this); }

		std::string print(bool hex=false) const override;
		IntType getIntType() override;
		unsigned int getIndex() override { return (value & 0x00FFFFFF); };
};

class VarValueExpr: public ValueExpr {
	private:
		static unsigned int varCount;

		std::string name;
		unsigned int length = 0;
		VarValueExpr() {}
	public:
		VarValueExpr(std::string name, unsigned int type, unsigned int length);
		VarValueExpr(unsigned int type);

		virtual VarValueExpr* clone() const override { return new VarValueExpr(*this); }

		std::string print(bool hex=false) const override;
		bool hasSideEffect() override { return false; }
		IntType getIntType() override { return (type == ValueType::INT) ? IntegerSimple : IntegerInvalid; }

		virtual bool isLValue() override { return (type == ValueType::INTREF || type == ValueType::STRREF); }
};

class ErrValueExpr: public ValueExpr {
	public:
		std::string print(bool hex=false) const override;
		IntType getIntType() override { return IntegerInvalid; }

		virtual ErrValueExpr* clone() const override { return new ErrValueExpr(); }

};

// Represents the target of the call (loosely)
class Function {
	std::string name;
	// if it is a script command
	int fileIndex = -1;
	unsigned int address = 0;
	public:
		Function(std::string name) : name(name) {}
		Function(std::string name, int index, unsigned int address) : name(name), fileIndex(index), address(address) {}

		void extraThing(unsigned int extra) { extraCall = extra; }

		std::string print() const;

		// if it is special
		bool hasExtra = false;
		unsigned int extraCall = 0;
		bool pushRet = true;
};


// Far absolute call/system call
class CallExpr: public ValueExpr {
	Function callFunc;
	unsigned int fnOption = 0;
	std::vector<unsigned int> fnExtra;
	protected:
		std::vector<Value> fnArgs;
		CallExpr() : callFunc("undefined") {}
	public:
		CallExpr(Function callFunc, unsigned int option, std::vector<Value> args, std::vector<unsigned int> extraList, unsigned int returnType);
		CallExpr(const CallExpr& copy);
		virtual CallExpr* clone() const override { return new CallExpr(*this); }

		std::string print(bool hex=false) const override;
		bool hasSideEffect() override { return true; }
};

// Near absolute call
class ShortCallExpr: public CallExpr {
	unsigned int blockIndex = 0;
	public:
		ShortCallExpr(unsigned int blockIndex, std::vector<Value> args);
		virtual ShortCallExpr* clone() const override { return new ShortCallExpr(*this); }

		std::string print(bool hex=false) const override;
};


// Statements

class Statement;
class IfStatement;

typedef struct BasicBlock Block;


class Statement {
	protected:
		int lineNum = -1;
		Statement() {}
	public:
		virtual ~Statement() {};

		virtual void print(std::ostream &out, int indentation = 0) const = 0;

		virtual IfStatement* makeIf(Value cond, StatementBlock block);

		virtual int getSize() const { return 1; }

		enum StatementType {
			INVALID,
			ASSIGN, RETURN,
			CLEAR_BUFFER, LINE_NUM,
			JUMP_IF, GOTO
		};

		StatementType type = INVALID;

		int getLineNum() const { return lineNum; }
		void setLineNum(int num) { lineNum = num; }
		std::string printLineNum() const { return (lineNum >= 0) ? ("\t// line " + std::to_string(lineNum) + "\n") : "\n"; }
};

class ExpressionStatement : public Statement {
	ValueExpr* expr = nullptr;
	public:
		ExpressionStatement(ValueExpr* expr_);
		~ExpressionStatement();

		virtual void print(std::ostream &out, int indentation = 0) const override;
};

//===============================================================
// Control flow statements
//    some can be structures as well
//		infinite descent
//===============================================================

struct IfBranch {
	Value condition;
	StatementBlock block;
	int lineNum = -1;

	IfBranch(Value cond_, StatementBlock block_, int lineNum_ = -1) : condition(std::move(cond_)), block(block_), lineNum(lineNum_) {}

	std::string lineComment() const { return (lineNum >= 0) ? ("\t// line " + std::to_string(lineNum)) : ""; }
};

class IfStatement: public Statement {
	// Branches in reverse order
	std::vector<IfBranch> branches;
	public:
		IfStatement(Value cond_, StatementBlock trueBlock_, StatementBlock falseBlock_);
		~IfStatement();

		virtual void print(std::ostream &out, int indentation = 0) const override;

		virtual IfStatement* makeIf(Value cond, StatementBlock block) override;

		virtual int getSize() const override;
};


class SwitchStatement: public Statement {

};

class ForStatement: public Statement {
	Value condition;
	StatementBlock increment;
	public:
		ForStatement(Value cond_, StatementBlock inc);
		~ForStatement();

		virtual void print(std::ostream &out, int indentation = 0) const override;
};

class GotoStatement: public Statement {
	unsigned int blockIndex = 0;
	public:
		GotoStatement(unsigned int index);
		virtual void print(std::ostream &out, int indentation = 0) const override;
		virtual int getSize() const override;
};

class BranchStatement: public Statement {
	unsigned int blockIndex, elseIndex;
	Value condition;
	public:
		BranchStatement(unsigned int blockIndex, unsigned int elseIndex = 0, Value cond = nullptr);

		virtual void print(std::ostream &out, int indentation = 0) const override;
		Value& getCondition() { return condition; }
};

class WhileStatement: public Statement {
	Value condition;
	std::vector<Block*> blocks;
	Block* entryBlock;
	public:
		WhileStatement(Value cond_, std::vector<Block*> blocks, Block* entry);
		//~WhileStatement();

		virtual void print(std::ostream &out, int indentation = 0) const override;
		virtual int getSize() const override;
};

class ContinueStatement: public Statement {
	public:
		virtual void print(std::ostream &out, int indentation = 0) const override;
};

class ReturnStatement: public Statement {
	std::vector<Value> values;
	public:
		ReturnStatement(std::vector<Value> ret) : values(std::move(ret)) { type = RETURN; }

		virtual void print(std::ostream &out, int indentation = 0) const override;
};

//================================================================

class AssignStatement: public Statement {
	Value lhs = nullptr;
	Value rhs = nullptr;
	public:
		AssignStatement(Value lhs, Value rhs);

		virtual void print(std::ostream &out, int indentation = 0) const override;
};


class LineNumStatement: public Statement {
	public:
		LineNumStatement(int lineNum_);

		virtual void print(std::ostream &out, int indentation = 0) const override;
		virtual int getSize() const override;
};

class ClearBufferStatement: public Statement {
	public:
		ClearBufferStatement() { type = CLEAR_BUFFER; }

		virtual void print(std::ostream &out, int indentation = 0) const override;
		virtual int getSize() const override;
};

class AddTextStatement: public Statement {
	Value text;
	unsigned int index = 0;
	public:
		AddTextStatement(Value text, unsigned int index) : text(std::move(text)), index(index) {}

		virtual void print(std::ostream &out, int indentation = 0) const override;
};

class SetNameStatement: public Statement {
	Value name;
	public:
		SetNameStatement(Value name) : name(std::move(name)) {}

		virtual void print(std::ostream &out, int indentation = 0) const override;
};

class Op21Statement: public Statement {
	unsigned int u1;
	unsigned char u2;
	public:
		Op21Statement(unsigned int u1, unsigned char u2) : u1(u1), u2(u2) {}
		virtual void print(std::ostream &out, int indentation = 0) const override;

};

#endif






