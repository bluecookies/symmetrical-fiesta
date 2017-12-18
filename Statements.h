#ifndef STATEMENTS_H
#define STATEMENTS_H

#include <memory>

#include "Logger.h"
#include "Helper.h"

namespace ValueType {
	const unsigned int UNDEF               = 0xFFFFFFFF;
	const unsigned int VOID                = 0x0;
	const unsigned int INT                 = 0xA;
	const unsigned int INT_LIST            = 0xB;
	const unsigned int STR                 = 0x14;
	const unsigned int STR_LIST            = 0x15;

	const unsigned int CALL_ELEMENT        = 0x3FC;
	const unsigned int DATABASE_ELEMENT    = 0x46A;
	const unsigned int COUNTER_ELEMENT     = 0x4B0;
	const unsigned int COUNTER_LIST        = 0x4B1;
	const unsigned int FRAMEACTION_ELEMENT = 0x4BA;
	const unsigned int FRAMEACTION_LIST    = 0x4BB;

	const unsigned int STAGE_ELEMENT       = 0x514;
	const unsigned int STAGE_LIST          = 0x515;
	const unsigned int OBJECT              = 0x51E;
	const unsigned int OBJECT_LIST         = 0x51F;

	const unsigned int SCREEN_ELEMENT      = 0x546;
	const unsigned int QUAKE_ELEMENT       = 0x550;
	const unsigned int QUAKE_LIST          = 0x551;
	const unsigned int SNDBGM_ELEMENT      = 0x578;
	const unsigned int SNDKOE_ELEMENT      = 0x582;
	const unsigned int SNDPCMES_ELEMENT    = 0x58C;
	const unsigned int SNDPCMCH_ELEMENT    = 0x58D;
	const unsigned int SNDPCMCH_LIST       = 0x58E;
	const unsigned int SNDSE_ELEMENT       = 0x596;
};

inline std::string VarType(unsigned int type) {
	switch (type) {
		case ValueType::VOID: return std::string("void");
		case ValueType::INT: return std::string("int");
		case ValueType::INT_LIST: return std::string("int_list");
		case ValueType::STR: return std::string("str");
		case ValueType::STR_LIST: return std::string("str_list");
		case ValueType::OBJECT: return std::string("object");
		case ValueType::OBJECT_LIST: return std::string("object_list");
		case ValueType::STAGE_ELEMENT: return std::string("stage_element");
		case ValueType::STAGE_LIST: return std::string("stage_list");

		default:
			return "<0x" + toHex(type) + ">";
	}
}

// For Expression of type INT
enum IntType {
	IntegerInvalid,
	IntegerSimple,
	IntegerLocalVar,
	IntegerFunction,
	IntegerGlobalVar,
	IntegerIndexer,
	IntegerBool,
};

class Expression;
class Statement;
typedef std::unique_ptr<Expression> Value;
typedef std::vector<Statement*> StatementBlock;


void negateCondition(Value& cond);

class Expression {
	private:
		int lineNum = -1;
	protected:
		unsigned int type = ValueType::UNDEF;
		int listLength = 0;
		Expression(unsigned int type_) : type(type_) {}
		Expression() {}
	public:
		virtual ~Expression() {}
		virtual Expression* clone() const = 0;

		unsigned int getType() const { return type; }
		void setType(unsigned int type_) {
			type = type_;
		}

		virtual bool hasSideEffect() { return false; }
		virtual std::string print(bool hex=false) const;


		//virtual int getAddress() const { return address; }
		//int getLineNum() const { return lineNum; }
		//void setLineNum(int lineNum_) { lineNum = lineNum_; }

		virtual IntType getIntType() { return IntegerInvalid; }
		virtual unsigned int getIndex() { return 0xFF000000; }

		virtual bool isBinaryExpression() { return false; }


		enum ExpressionType {
			INVALID, BOOL_EXPR
		};

		ExpressionType exprType = INVALID;

		virtual int getPrecedence() const { return 0xFF; };
};

class BinaryExpression: public Expression {
	protected:
		Value expr1 = nullptr;
		Value expr2 = nullptr;
		unsigned char op = 0;
	public:
		BinaryExpression(Value e1, Value e2, unsigned char op_);
		BinaryExpression(const BinaryExpression& copy) : Expression(copy), expr1(copy.expr1->clone()), expr2(copy.expr2->clone()), op(copy.op) {}
		virtual BinaryExpression* clone() const override { return new BinaryExpression(*this); }

		std::string print(bool hex=false) const override;
		bool hasSideEffect() override { return expr1->hasSideEffect() || expr2->hasSideEffect(); }
		IntType getIntType() override { return (type == ValueType::INT) ? IntegerSimple : IntegerInvalid; }

	public:
		void negateBool();
		bool isEquality() { return op == 0x11; }
		Value& getLHS() { return expr1; }
		Value& getRHS() { return expr2; }

		virtual int getPrecedence() const override;

};

class UnaryExpression: public Expression {
	protected:
		Value expr = nullptr;
		unsigned char op = 0;
	public:
		UnaryExpression(Value expr_, unsigned char op_);
		UnaryExpression(const UnaryExpression& copy) : Expression(copy), expr(copy.expr->clone()), op(copy.op) {}
		virtual UnaryExpression* clone() const override { return new UnaryExpression(*this); }

		std::string print(bool hex=false) const override;
		bool hasSideEffect() override { return expr->hasSideEffect(); }
		IntType getIntType() override { return (type == ValueType::INT) ? IntegerSimple : IntegerInvalid; }

		virtual int getPrecedence() const override;

};

class IndexValueExpr: public BinaryExpression {
	public:
		IndexValueExpr(Value e1, Value e2);
		virtual IndexValueExpr* clone() const override { return new IndexValueExpr(*this); }

		std::string print(bool hex=false) const override;
};

class MemberExpr: public BinaryExpression {
	public:
		MemberExpr(Value e1, Value e2);
		virtual MemberExpr* clone() const override { return new MemberExpr(*this); }

		std::string print(bool hex=false) const override;
};


class RawValueExpr: public Expression {
	unsigned int value = 0;
	std::string str;
	public:
		RawValueExpr(unsigned int type_, unsigned int value_) : Expression(type_), value(value_) {}
		RawValueExpr(std::string str_, unsigned int value_) : Expression(ValueType::STR), value(value_), str(str_) {}

		virtual RawValueExpr* clone() const override { return new RawValueExpr(*this); }

		std::string print(bool hex=false) const override;
		IntType getIntType() override;
		unsigned int getIndex() override { return (value & 0x00FFFFFF); };
};

class VariableExpression: public Expression {
	private:
		static unsigned int varCount;

		std::string name;
		VariableExpression() {}
	public:
		VariableExpression(std::string name, unsigned int type, unsigned int length = 0);
		VariableExpression(unsigned int type);

		virtual VariableExpression* clone() const override { return new VariableExpression(*this); }

		std::string print(bool hex=false) const override;
		bool hasSideEffect() override { return false; }
		IntType getIntType() override { return (type == ValueType::INT) ? IntegerSimple : IntegerInvalid; }
};

class ListExpression: public Expression {
	private:
		std::vector<Value> elements;
	public:
		ListExpression(std::vector<Value> elements_);
		ListExpression(const ListExpression& copy);
		virtual ListExpression* clone() const override { return new ListExpression(*this); }

		std::string print(bool hex=false) const override;
		// not really, have to check, but I don't want this to silently fail
		bool hasSideEffect() override { return true; }


};

class ErrValueExpr: public Expression {
	public:
		ErrValueExpr(std::string err="", unsigned int address=0xFFFFFFFF);
		std::string print(bool hex=false) const override;
		IntType getIntType() override { return IntegerInvalid; }

		virtual ErrValueExpr* clone() const override { return new ErrValueExpr(); }

};

// This might only come up with weird debugging conditions
class NotExpr: public Expression {
	Value cond;
	public:
		NotExpr(Value cond_) : cond(std::move(cond_)) {}
		NotExpr(const NotExpr& copy) : Expression(copy), cond(copy.cond->clone()) {}
		virtual NotExpr* clone() const override { return new NotExpr(*this); }

		std::string print(bool hex=false) const override;
		IntType getIntType() override { return IntegerBool; }

		virtual int getPrecedence() const override;
};

// Represents the target of the call (loosely)
class FunctionExpr: public Expression {
	std::string name;
	Value callValue;
	public:
		FunctionExpr(std::string name_) : name(name_) {}
		FunctionExpr(Value val_) : callValue(std::move(val_)) {}
		FunctionExpr(const FunctionExpr& copy);
		virtual FunctionExpr* clone() const override { return new FunctionExpr(*this); }


		void extraThing(unsigned int extra) { extraCall = extra; }

		std::string print(bool hex=false) const override;

		// if it is special
		bool hasExtra = false;
		unsigned int extraCall = 0;
};


// Far absolute call/system call
class CallExpr: public Expression {
	FunctionExpr* callFunc = nullptr;
	unsigned int fnOption = 0;
	std::vector<unsigned int> fnExtra;
	protected:
		std::vector<Value> fnArgs;
		CallExpr() : callFunc(new FunctionExpr("undefined")) {}
	public:
		CallExpr(FunctionExpr* callFunc, unsigned int option, std::vector<Value> args, std::vector<unsigned int> extraList, unsigned int returnType);
		CallExpr(const CallExpr& copy);
		~CallExpr();
		virtual CallExpr* clone() const override { return new CallExpr(*this); }

		std::string print(bool hex=false) const override;
		bool hasSideEffect() override { return true; }
		IntType getIntType() override;
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
class SwitchStatement;

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
			JUMP_IF, GOTO,
			CONTINUE, SWITCH,
			WHILE, IF_ELSE,
			END_SCRIPT
		};

		StatementType type = INVALID;

		int getLineNum() const { return lineNum; }
		virtual void setLineNum(int num) { lineNum = num; }
		std::string printLineNum() const { return (lineNum >= 0) ? ("\t// line " + std::to_string(lineNum) + "\n") : "\n"; }

		virtual SwitchStatement* foldSwitch() { return nullptr; }
		virtual void removeBlock(Block*) { }
};

class ExpressionStatement : public Statement {
	Expression* expr = nullptr;
	public:
		ExpressionStatement(Expression* expr_);
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

	IfBranch(Value cond_, StatementBlock block_) : condition(std::move(cond_)), block(block_) {}

	std::string lineComment() const { return (lineNum >= 0) ? ("\t// line " + std::to_string(lineNum)) : ""; }
};

class IfStatement: public Statement {
	// Branches in reverse order
	std::vector<IfBranch> branches;
	public:
		IfStatement(Value cond_, StatementBlock trueBlock_, StatementBlock falseBlock_);
		~IfStatement();

		virtual void print(std::ostream &out, int indentation = 0) const override;
		virtual void setLineNum(int num) override;

		virtual IfStatement* makeIf(Value cond, StatementBlock block) override;

		virtual int getSize() const override;

		virtual SwitchStatement* foldSwitch() override;
		SwitchStatement* doFoldSwitch();
};


class SwitchStatement: public Statement {
	Value testExpr;
	std::vector<IfBranch> switches;
	public:
		SwitchStatement(Value expr, std::vector<IfBranch> switches);

		virtual void print(std::ostream &out, int indentation = 0) const override;
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
		virtual SwitchStatement* foldSwitch() override;
		virtual void removeBlock(Block* pBlock) override;


};

class ContinueStatement: public Statement {
	public:
		ContinueStatement() { type = CONTINUE; }
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


class EndScriptStatement: public Statement {
	public:
		EndScriptStatement() { type = END_SCRIPT; }

		virtual void print(std::ostream &out, int indentation = 0) const override;
};


class AddTextStatement: public Statement {
	Value text;
	unsigned int index = 0;
	public:
		AddTextStatement(Value text_, unsigned int index_) : text(std::move(text_)), index(index_) {}

		virtual void print(std::ostream &out, int indentation = 0) const override;
};

class SetNameStatement: public Statement {
	Value name;
	public:
		SetNameStatement(Value name_) : name(std::move(name_)) {}

		virtual void print(std::ostream &out, int indentation = 0) const override;
};

class DeclareVarStatement: public Statement {
	unsigned int type;
	std::string name;
	public:
		DeclareVarStatement(unsigned int type_, std::string name_) : type(type_), name(name_) {}
		virtual void print(std::ostream &out, int indentation = 0) const override;

};


#endif






