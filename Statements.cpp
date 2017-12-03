#include <iostream>
#include <vector>
#include <memory>

#include "Helper.h"
#include "Logger.h"
#include "Statements.h"

// Expressions

std::string Expression::print() {
	return std::string("Expression here\n");
}

std::string LineExpr::print() {
	return "line " + std::to_string(lineNum);
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

std::string BinaryValueExpr::print() {
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

	return expr1->print() + opRep + expr2->print();
}

IndexValueExpr::IndexValueExpr(std::shared_ptr<ValueExpr> e1, std::shared_ptr<ValueExpr> e2) : BinaryValueExpr(e1, e2, 0xFFFFFFFF) {
	if (e2->getType() != ValueType::INT) {
		Logger::Error() << "Indexing with non-int value.\n";
		throw std::logic_error("Bad index");
	}

	Logger::VVDebug() << e1->print() << " is array of type " << VarType(e1->getType()) << "\n";

	// Set type of value
	unsigned int arrType = e1->getType() - 3;
	if (arrType == ValueType::INTLIST)
		type = ValueType::INTREF;
	else if (arrType == ValueType::STRLIST)
		type = ValueType::STRREF;
	else if (arrType == ValueType::OBJ_STR - 3)
		type = ValueType::OBJ_STR;
	else
		type = ValueType::INTREF;
}

std::string IndexValueExpr::print() {
	return expr1->print() + "[" + expr2->print() + "]";
}

std::string NotExpr::print() {
	return "!(" + cond->print() + ")";
}

std::string RawValueExpr::print() {
	if (!str.empty()) {
		return "\"" + str + "\"";
	} else {
		return std::to_string(value);
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

unsigned int RawValueExpr::getCommandIndex() {
	if (type != ValueType::INT)
		return 0xFFFFFFFF;

	if (value >> 24 == 0x7e)
		return (value & 0x00FFFFFF);

	return 0xFFFFFFFF;
}

VarValueExpr::VarValueExpr(std::string name, unsigned int type, unsigned int length) : ValueExpr(type + 3), name(name), length(length) {
	if (type == ValueType::INTLIST || type == ValueType::STRLIST) {
		if (length == 0)
			Logger::Error() << VarType(type) << " must have positive length.\n";
	} else if (type == ValueType::INT || type == ValueType::STR) {
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

std::string VarValueExpr::print() {
	if (!name.empty())
		return name;
	else
		return ValueExpr::print();
}

std::string ErrValueExpr::print() {
	return std::string("(ERROR)");
}

CallExpr::CallExpr(Function fnCall_, unsigned int option_, ProgStack args_, std::vector<unsigned int> extraList_, unsigned int returnType_) : callFunc(fnCall_) {
	type = returnType_;
	fnOption = option_;
	fnArgs = args_;
	fnExtra = extraList_;
}

std::string CallExpr::print() {
	std::string str = callFunc.print();
	str += "-" + std::to_string(fnOption) + "(";
	for (auto it = fnArgs.rbegin(); it != fnArgs.rend(); it++) {
		if (it != fnArgs.rbegin())
			str += ", ";

		str += (*it)->print();
	}
	str += ")";
	if (!fnExtra.empty()) {
		str += "_{";
		for (auto it = fnExtra.rbegin(); it != fnExtra.rend(); it++) {
			if (it != fnExtra.rbegin())
				str += ", ";

			str += std::to_string(*it);
		}
		str += "}";
	}
	if (callFunc.hasExtra)
		str += "<" + std::to_string(callFunc.extraCall) + ">";

	return str;
}

std::string Function::print() {
	return name;
}

ShortCallExpr::ShortCallExpr(unsigned int index, ProgStack args) {
	blockIndex = index;
	fnArgs = args;

	type = ValueType::INT;
}


std::string ShortCallExpr::print() {
	std::string str = "call@L" + std::to_string(blockIndex) + "(";
	for (auto it = fnArgs.rbegin(); it != fnArgs.rend(); it++) {
		if (it != fnArgs.rbegin())
			str += ", ";

		str += (*it)->print();
	}
	str += ")";
	return str;
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

std::string AssignExpr::print() {
	return lhs->print() + " = " + rhs->print() + "\t(" + VarType(lhs->getType()) + ")";
}

std::string JumpExpr::print() {
	if (cond != nullptr) {
		return "if (" + cond->print() +")  jump L" + std::to_string(blockIndex) + " else L" + std::to_string(elseIndex);
	} else {
		return "jump L" + std::to_string(blockIndex);
	}
}


std::string RetExpr::print() {
	std::string str("return");
	if (!values.empty()) {
		str += " (";
		for (auto it = values.rbegin(); it != values.rend(); it++) {
			if (it != values.rbegin())
				str += ", ";

			str += (*it)->print();
		}
		str += ")";
	}
	return str;
}

std::string AddTextExpr::print() {
	return "addtext " + std::to_string(index) + " " + text->print();
}

std::string SetNameExpr::print() {
	return "setname " +	name->print();
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

	out << expr->print() << "\n";
}



/* class CallStatement:Statement {
	ValueExpr* function = nullptr;
	std::vector<ValueExpr*> params;
}; */

/*class AssignStatement:Statement {
	AssignExpr* expr;
};*/
