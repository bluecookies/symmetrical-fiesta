#include <memory>

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
	const unsigned int OBJECT   = 0x51e;
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
		case ValueType::OBJECT: return std::string("obj");	//unsure
		default:
			return "<0x" + toHex(type) + ">";
	}
}

class Expression {
	public:
		//virtual Expression* clone() { return new Expression(*this); }

		virtual ~Expression() {}
		virtual void print(std::ostream& out);

		
};

class LineExpr: public Expression {
	unsigned int lineNum = 0;
	public:
		LineExpr(unsigned int line) : lineNum(line) {}
		//virtual Expression* clone() override  { return new LineExpr(*this); }

		void print(std::ostream &out) override;
};

class ValueExpr: public Expression {
	protected:
		unsigned int type = ValueType::UNDEF;
		ValueExpr(unsigned int type) : type(type) {}
		ValueExpr() {}
	public:
		virtual std::shared_ptr<ValueExpr> clone()  { return std::make_shared<ValueExpr>(*this); }

		unsigned int getType() { return type; }
		void setType(unsigned int type_) {
			type = type_;
		}

		// This is some fake misleading naming here
		// Not real lvalues and rvalues, more like just "assignable"
		virtual bool isLValue() { return false; }

		virtual bool isIndexer() { return false; }
		virtual unsigned int getVarIndex() { return 0xFFFFFFFF; }

		// to exterminate
		virtual bool isSpecialCall() { return false; }
		virtual bool is0x54() { return false; }
};

typedef std::vector<std::shared_ptr<ValueExpr>> ProgStack;


class BinaryValueExpr: public ValueExpr {
	protected:
		std::shared_ptr<ValueExpr> expr1 = nullptr;
		std::shared_ptr<ValueExpr> expr2 = nullptr;
		unsigned int op = 0;
	public:
		BinaryValueExpr(std::shared_ptr<ValueExpr> e1, std::shared_ptr<ValueExpr> e2, unsigned int op_);
		virtual std::shared_ptr<ValueExpr> clone() override  { return std::make_shared<BinaryValueExpr>(*this); }

		void print(std::ostream &out) override;
};

class IndexValueExpr: public BinaryValueExpr {
	public:
		IndexValueExpr(std::shared_ptr<ValueExpr> e1, std::shared_ptr<ValueExpr> e2);
		virtual std::shared_ptr<ValueExpr> clone() override  { return std::make_shared<IndexValueExpr>(*this); }

		void print(std::ostream &out) override;

		virtual bool isLValue() override { return true; }
};

// Can't think of any more unary expressions
class NotExpr: public ValueExpr {
	std::shared_ptr<ValueExpr> cond;
	public:
		NotExpr(std::shared_ptr<ValueExpr> cond) : cond(cond) {}

		void print(std::ostream &out) override;
};

class RawValueExpr: public ValueExpr {
	unsigned int value = 0;
	std::string str;
	public:
		RawValueExpr(unsigned int type, unsigned int value) : ValueExpr(type), value(value) {}
		RawValueExpr(std::string str, unsigned int value) : ValueExpr(ValueType::STR), value(value), str(str) {}

		virtual std::shared_ptr<ValueExpr> clone() override  { return std::make_shared<RawValueExpr>(*this); }

		bool isIndexer() override;
		unsigned int getVarIndex() override;

		bool isSpecialCall() override;
		bool is0x54() override { return value == 0x54; }


		void print(std::ostream &out) override;
};

class VarValueExpr: public ValueExpr {
	std::string name;
	unsigned int length = 0;
	VarValueExpr() {}
	public:
		VarValueExpr(std::string name, unsigned int type, unsigned int length);
		static VarValueExpr* stackLoc(std::vector<unsigned int> stackHeights);

		virtual std::shared_ptr<ValueExpr> clone() override  { return std::make_shared<VarValueExpr>(*this); }

		void print(std::ostream &out) override;

		virtual bool isLValue() override { return true; }
};

class ErrValueExpr: public ValueExpr {
	public:
		void print(std::ostream &out) override;
};

// Represents the target of the call (loosely)
class FnCall {
	ProgStack tempList;
	public:
		FnCall() {}
		FnCall(ProgStack vals);
		bool needExtra();
		bool pushRet();

		void print(std::ostream &out);
};


// Far absolute call/system call
class CallExpr: public ValueExpr {
	FnCall callFunc;
	unsigned int extraCallThing = 0;
	unsigned int fnOption = 0;
	std::vector<unsigned int> fnExtra;
	protected:
		ProgStack fnArgs;
		CallExpr() {}
	public:
		CallExpr(ProgStack fnCall, unsigned int option, ProgStack args, std::vector<unsigned int> extraList, unsigned int returnType);

		bool needExtra() { return callFunc.needExtra(); }
		bool pushRet() { return callFunc.pushRet(); }
		void extraThing(unsigned int extra) { extraCallThing = extra; }

		void print(std::ostream &out) override;
};

// Near absolute call
class ShortCallExpr: public CallExpr {
	unsigned int blockIndex = 0;
	public:
		ShortCallExpr(unsigned int blockIndex, ProgStack args);

		void print(std::ostream &out) override;
};

class AssignExpr: public Expression {
	std::shared_ptr<ValueExpr> lhs = nullptr;
	std::shared_ptr<ValueExpr> rhs = nullptr;
	public:
		AssignExpr(std::shared_ptr<ValueExpr> lhs, std::shared_ptr<ValueExpr> rhs);
		//virtual Expression* clone() override  { return new AssignExpr(*this); }

		void print(std::ostream &out) override;
};


class JumpExpr: public Expression {
	unsigned int blockIndex, elseIndex;
	std::shared_ptr<ValueExpr> cond;
	public:
		JumpExpr(unsigned int blockIndex, unsigned int elseIndex = 0, std::shared_ptr<ValueExpr> cond = nullptr) : blockIndex(blockIndex), elseIndex(elseIndex), cond(cond) {}

		void print(std::ostream &out) override;
};

class RetExpr: public Expression {
	ProgStack values;
	public:
		RetExpr(ProgStack ret) : values(ret) {}

		void print(std::ostream &out) override;
};

class AddTextExpr: public Expression {
	std::shared_ptr<ValueExpr> text;
	unsigned int index = 0;
	public:
		AddTextExpr(std::shared_ptr<ValueExpr> text, unsigned int index) : text(text), index(index) {}

		void print(std::ostream &out) override;
};

class SetNameExpr: public Expression {
	std::shared_ptr<ValueExpr> name;
	public:
		SetNameExpr(std::shared_ptr<ValueExpr> name) : name(name) {}

		void print(std::ostream &out) override;
};

/* class CallExpr: public ValueExpr {
	
}; */

class Statement {
	unsigned int lineNum = 0;
	Expression* expr = nullptr;
	public:
		Statement(Expression* expr);
		virtual ~Statement();

		virtual void print(std::ostream &out);
};











