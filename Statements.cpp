#include <iostream>
#include <vector>
#include <memory>

#include "Helper.h"
#include "Logger.h"
#include "Statements.h"

// Expressions

void Expression::print(std::ostream& out) {
	out << "Expression here\n";
}

void LineExpr::print(std::ostream &out) {
	out << "line " << std::to_string(lineNum) << std::endl; 
}

BinaryValueExpr::BinaryValueExpr(std::shared_ptr<ValueExpr> e1, std::shared_ptr<ValueExpr> e2, unsigned int op_) {
	if (e1 == nullptr || e2 == nullptr) {
		throw std::logic_error("Assigning null pointers.");
	}

	expr1 = e1;
	expr2 = e2;
	op = op_;

	if (op == 0xFFFFFFFF)
		return;

	if (e1->getType() != e2->getType())
		Logger::Warn() << "Comparing different types: " << VarType(expr1->getType()) << ", " << VarType(expr2->getType()) << std::endl;

	if (e1->getType() == ValueType::INT)
		type = ValueType::INT;
	else if (e1->getType() == ValueType::STR && e2->getType() == ValueType::STR) {
		if (op == 1)
			type = ValueType::STR;
		else
			type = ValueType::INT;
	}
}

void BinaryValueExpr::print(std::ostream &out) {
	std::string opRep;
	switch (op) {
		case 0x01: opRep = " + "; break;
		case 0x02: opRep = " - "; break;
		case 0x03: opRep = " / "; break;
		case 0x04: opRep = " * "; break;
		case 0x10: opRep = " != "; break;
		case 0x11: opRep = " < "; break;
		case 0x12: opRep = " <= "; break;
		case 0x13: opRep = " > "; break;
		case 0x14: opRep = " >= "; break;
		case 0x20: opRep = " && "; break;
		default: opRep = " <op" + std::to_string(op) + "> "; break;
	}

	expr1->print(out);
	out << opRep;
	expr2->print(out);
}

IndexValueExpr::IndexValueExpr(std::shared_ptr<ValueExpr> e1, std::shared_ptr<ValueExpr> e2) : BinaryValueExpr(e1, e2, 0xFFFFFFFF) {
	if (e2->getType() != ValueType::INT) {
		Logger::Error() << "Indexing with non-int value.\n";
		throw std::logic_error("Bad index");
	}

	// Set type of value
	type = ValueType::INT;
}

void IndexValueExpr::print(std::ostream &out) {
	expr1->print(out);
	out << "[";
		expr2->print(out);
	out << "]";
}

void NotExpr::print(std::ostream &out) {
	out << "!(";
	cond->print(out);
	out << ")";
}


// TODO: Add strings here
void RawValueExpr::print(std::ostream &out) {
	out << std::to_string(value);
}

bool RawValueExpr::isIndexer() {
	if (type != ValueType::INT)
		return false;

	return (value == 0xFFFFFFFF);
}

LValueExpr* LValueExpr::stackLoc(std::vector<unsigned int> stackHeights) {
	unsigned int height = stackHeights.back();
	unsigned int depth =  stackHeights.size() - 1;

	LValueExpr* pLValue = new LValueExpr();

	if (depth > 0)
		pLValue->name = "stack_" + std::to_string(depth) + "_" + std::to_string(height);
	else
		pLValue->name = "stack" + std::to_string(height);

	return pLValue;
}

void LValueExpr::print(std::ostream &out) {
	if (!name.empty())
		out << name;
	else
		ValueExpr::print(out);
}

// TODO: handle fnCall
CallExpr::CallExpr(ProgStack fnCall_, unsigned int option_, ProgStack args_, std::vector<unsigned int> extraList_, unsigned int returnType_) {
	type = returnType_;
	if (fnCall_.empty()) {
		Logger::Error() << "Function call is empty!\n";
	}
	fnCall = fnCall_;
	fnOption = option_;
	fnArgs = args_;
	fnExtra = extraList_;
}

void CallExpr::print(std::ostream& out) {
	if (fnCall.empty()) {
		out << "(ERROR)";
		return;
	}

	out << "FN_";
	fnCall.back()->print(out);
	 out <<"-" << std::to_string(fnOption) << "(";
	for (auto it = fnArgs.rbegin(); it != fnArgs.rend(); it++) {
		if (it != fnArgs.rbegin())
			out << ", ";

		(*it)->print(out);
	}
	out << ")";
	if (!fnExtra.empty()) {
		out << " {";
		for (auto it = fnExtra.rbegin(); it != fnExtra.rend(); it++) {
			if (it != fnExtra.rbegin())
				out << ", ";

			out << std::to_string(*it);
		}
		out << "} ";
	}
}


AssignExpr::AssignExpr(std::shared_ptr<LValueExpr> lhs, std::shared_ptr<ValueExpr> rhs):lhs(lhs), rhs(rhs) {
	if (lhs == nullptr || rhs == nullptr) {
		throw std::logic_error("Assigning null pointers.");
	}

	if (rhs->getType() == ValueType::UNDEF)
		throw std::logic_error("Rvalue type undefined");

	if (lhs->getType() == ValueType::UNDEF) {
		lhs->setType(rhs->getType());
	} else if (lhs->getType() != rhs->getType()) {
		throw std::logic_error("Type mismatch!");
	}
}

void AssignExpr::print(std::ostream& out) {
	lhs->print(out);
	out << " = ";
	rhs->print(out);
	out << "\t(" << VarType(lhs->getType()) << ")\n";
}

void JumpExpr::print(std::ostream& out) {
	if (cond != nullptr) {
		out << "if (";
		cond->print(out);
		out << ")  jump L" << std::to_string(blockIndex) << " else L" << std::to_string(elseIndex) << "\n";
	} else {
		out << "jump L" << std::to_string(blockIndex) << "\n";
	}
}


void RetExpr::print(std::ostream& out) {
	out << "return";
	if (!values.empty()) {
		out << " (";
		for (auto it = values.rbegin(); it != values.rend(); it++) {
			if (it != values.rbegin())
				out << ", ";

			(*it)->print(out);
		}
		out << ")";
	}
	out << "\n";
}


// Statements

Statement::Statement(Expression* expr_) {
	if (expr_ == nullptr)
		throw std::logic_error("Null pointer passed into statement.");

	expr = expr_;
}

Statement::~Statement() {
	delete expr;
};

void Statement::print(std::ostream &out) {
	if (lineNum != 0)
		out << std::to_string(lineNum) << ": ";

	expr->print(out);
}



/* class CallStatement:Statement {
	ValueExpr* function = nullptr;
	std::vector<ValueExpr*> params;
}; */

/*class AssignStatement:Statement {
	AssignExpr* expr;
};*/
