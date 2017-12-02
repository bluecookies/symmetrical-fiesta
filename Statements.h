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
		std::string name;
		ValueExpr(unsigned int type) : type(type) {}
		ValueExpr() {}
	public:
		//virtual Expression* clone() override  { return new ValueExpr(*this); }

		unsigned int getType() { return type; }
		void setType(unsigned int type_) {
			type = type_;
		}

		virtual bool isIndexer() { return false; }
};

typedef std::vector<std::shared_ptr<ValueExpr>> ProgStack;


class BinaryValueExpr: public ValueExpr {
	protected:
		std::shared_ptr<ValueExpr> expr1 = nullptr;
		std::shared_ptr<ValueExpr> expr2 = nullptr;
		unsigned int op = 0;
	public:
		BinaryValueExpr(std::shared_ptr<ValueExpr> e1, std::shared_ptr<ValueExpr> e2, unsigned int op_);
		//virtual Expression* clone() override  { return new BinaryValueExpr(*this); }

		void print(std::ostream &out) override;
};

class IndexValueExpr: public BinaryValueExpr {
	public:
		IndexValueExpr(std::shared_ptr<ValueExpr> e1, std::shared_ptr<ValueExpr> e2);
		//virtual Expression* clone() override  { return new IndexValueExpr(*this); }

		void print(std::ostream &out) override;
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
	public:
		RawValueExpr(unsigned int type, unsigned int value) : ValueExpr(type), value(value) {}
		//virtual Expression* clone() override  { return new RawValueExpr(*this); }

		bool isIndexer() override;

		void print(std::ostream &out) override;
};

class LValueExpr: public ValueExpr {
	LValueExpr() {}
	public:
		static LValueExpr* stackLoc(std::vector<unsigned int> stackHeights);
		//virtual Expression* clone() override  { return new LValueExpr(*this); }

		void print(std::ostream &out) override;
};

class ErrValueExpr: public ValueExpr {

};

class CallExpr: public ValueExpr {
	ProgStack fnCall, fnArgs;
	unsigned int fnOption;
	std::vector<unsigned int> fnExtra;
	public:
		CallExpr(ProgStack fnCall, unsigned int option, ProgStack args, std::vector<unsigned int> extraList, unsigned int returnType);

		void print(std::ostream &out) override;
};

class AssignExpr: public Expression {
	std::shared_ptr<LValueExpr> lhs = nullptr;
	std::shared_ptr<ValueExpr> rhs = nullptr;
	public:
		AssignExpr(std::shared_ptr<LValueExpr> lhs, std::shared_ptr<ValueExpr> rhs);
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











