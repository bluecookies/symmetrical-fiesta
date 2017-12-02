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

	e1->print(Logger::VVDebug());
	Logger::VVDebug() << " is array of type " << VarType(e1->getType()) << "\n";

	// Set type of value
	unsigned int arrType = e1->getType() - 3;
	if (arrType == ValueType::INTLIST)
		type = ValueType::INTREF;
	else if (arrType == ValueType::STRLIST)
		type = ValueType::STRREF;
	else
		type = ValueType::INTREF;
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

void RawValueExpr::print(std::ostream &out) {
	if (!str.empty()) {
		out << "\"" << str << "\"";
	} else {
		out << std::to_string(value);
	}
}

bool RawValueExpr::isIndexer() {
	if (type != ValueType::INT)
		return false;

	return (value == 0xFFFFFFFF);
}

unsigned int RawValueExpr::getVarIndex() {
	if (type != ValueType::INT)
		return 0xFFFFFFFF;

	if (value >> 24 == 0x7f)
		return (value & 0x00FFFFFF);

	return 0xFFFFFFFF;
}

bool RawValueExpr::isSpecialCall() {
	if (type != ValueType::INT)
		return false;

	if (value == 0x12)
		return true;

	return false;
}

VarValueExpr::VarValueExpr(std::string name, unsigned int type, unsigned int length) : ValueExpr(type + 3), name(name), length(length) {
	if (type == ValueType::INTLIST || type == ValueType::STRLIST) {
		if (length == 0)
			Logger::Error() << VarType(type) << " must have positive length.\n";
	} else {
		if (length > 0)
			Logger::Error() << VarType(type) << " cannot have positive length.\n";
	}
}

VarValueExpr* VarValueExpr::stackLoc(std::vector<unsigned int> stackHeights) {
	unsigned int height = stackHeights.back();
	unsigned int depth =  stackHeights.size() - 1;

	VarValueExpr* pValue = new VarValueExpr();

	if (depth > 0)
		pValue->name = "stack_" + std::to_string(depth) + "_" + std::to_string(height);
	else
		pValue->name = "stack" + std::to_string(height);

	return pValue;
}

void VarValueExpr::print(std::ostream &out) {
	if (!name.empty())
		out << name;
	else
		ValueExpr::print(out);
}

void ErrValueExpr::print(std::ostream &out) {
	out << "(ERROR)";
}

CallExpr::CallExpr(ProgStack fnCall_, unsigned int option_, ProgStack args_, std::vector<unsigned int> extraList_, unsigned int returnType_) {
	type = returnType_;
	callFunc = FnCall(fnCall_);
	fnOption = option_;
	fnArgs = args_;
	fnExtra = extraList_;
}

void CallExpr::print(std::ostream& out) {
	callFunc.print(out);
	out <<"-" << std::to_string(fnOption) << "(";
	for (auto it = fnArgs.rbegin(); it != fnArgs.rend(); it++) {
		if (it != fnArgs.rbegin())
			out << ", ";

		(*it)->print(out);
	}
	out << ")";
	if (!fnExtra.empty()) {
		out << "_{";
		for (auto it = fnExtra.rbegin(); it != fnExtra.rend(); it++) {
			if (it != fnExtra.rbegin())
				out << ", ";

			out << std::to_string(*it);
		}
		out << "}";
	}
	if (needExtra())
		out << "<" << std::to_string(extraCallThing) << ">";
}

FnCall::FnCall(ProgStack vals) : tempList(vals) {
	if (vals.empty()) {
		Logger::Error() << "Function call is empty!\n";
		return;
	}
}

void FnCall::print(std::ostream& out) {
	if (tempList.empty()) {
		Logger::Error() << "Function call is empty!\n";
		return;
	}
	for (auto it = tempList.rbegin(); it != tempList.rend(); it++) {
		if (it != tempList.rbegin())
			out << "_";;

		(*it)->print(out);
	}
}

bool FnCall::needExtra() {
	if (tempList.size() != 1)
		return false;
	// Aaargh gross
	return tempList.back()->isSpecialCall();
}

bool FnCall::pushRet() {
	if (tempList.size() != 1)
		return true;
	return !tempList.back()->is0x54();
}

ShortCallExpr::ShortCallExpr(unsigned int index, ProgStack args) {
	blockIndex = index;
	fnArgs = args;

	type = ValueType::INT;
}


void ShortCallExpr::print(std::ostream& out) {
	out << "call@L" << std::to_string(blockIndex);
	out << "(";
	for (auto it = fnArgs.rbegin(); it != fnArgs.rend(); it++) {
		if (it != fnArgs.rbegin())
			out << ", ";

		(*it)->print(out);
	}
	out << ")";
}


AssignExpr::AssignExpr(std::shared_ptr<ValueExpr> lhs, std::shared_ptr<ValueExpr> rhs):lhs(lhs), rhs(rhs) {
	if (lhs == nullptr || rhs == nullptr) {
		throw std::logic_error("Assigning null pointers.");
	}

	if (!lhs->isLValue())
		throw std::logic_error("error: lvalue required as left operand of assignment");

	if (rhs->getType() == ValueType::UNDEF)
		throw std::logic_error("right operand type undefined");

	if (lhs->getType() == ValueType::UNDEF) {
		lhs->setType(rhs->getType());
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

void AddTextExpr::print(std::ostream& out) {
	out << "addtext " << std::to_string(index) << " ";
	text->print(out);
	out << "\n";
}

void SetNameExpr::print(std::ostream& out) {
	out << "setname ";
	name->print(out);
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
