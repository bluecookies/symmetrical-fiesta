#include <iostream>
#include <vector>
#include <memory>

#include "Helper.h"
#include "Logger.h"

#include "Statements.h"

// TODO: some renaming, maybe have expression inherit statement


void negateCondition(Value& cond) {
	if (cond->exprType == ValueExpr::BOOL_EXPR) {
		static_cast<BinaryValueExpr*>(cond.get())->negateBool();
	} else {
		// check if its a not, if so remove the not
		cond = make_unique<NotExpr>(std::move(cond));
	}
}

// Expressions

std::string ValueExpr::print(bool) const {
	return std::string("(value)");
}

ValueExpr* ValueExpr::toRValue() {
	if (type == ValueType::INTREF)
		type = ValueType::INT;
	else if (type == ValueType::STRREF)
		type = ValueType::STR;

	return this;
}


BinaryValueExpr::BinaryValueExpr(Value e1, Value e2, unsigned int op_) {
	if (e1 == nullptr || e2 == nullptr) {
		throw std::logic_error("Assigning null pointers.");
	}

	// Take ownership (if I had rewritten this instead of trying to edit it, I wouldn't have tried to use e1 and e2 again later)
	expr1 = std::move(e1);
	expr2 = std::move(e2);
	op = op_;

	if (op == 0xFFFFFFFF)
		return;

	if (expr1->getType() != expr2->getType())
		Logger::Warn() << "Comparing different types: " << VarType(expr1->getType()) << ", " << VarType(expr2->getType()) << std::endl;

	if (expr1->getType() == ValueType::INT)
		type = ValueType::INT;
	else if (expr1->getType() == ValueType::STR && expr2->getType() == ValueType::STR) {
		if (op == 1)
			type = ValueType::STR;
		else
			type = ValueType::INT;
	}

	switch(op) {
		case 0x10:
		case 0x11:
		case 0x12:
		case 0x13:
		case 0x14:
		case 0x15:
		case 0x20:
		case 0x21:
			exprType = BOOL_EXPR;
	}
}

std::string BinaryValueExpr::print(bool hex) const {
	std::string opRep;
	switch (op) {
		case 0x01: opRep = " + "; break;
		case 0x02: opRep = " - "; break;
		case 0x03: opRep = " / "; break;
		case 0x04: opRep = " * "; break;
		case 0x10: opRep = " != "; break;
		case 0x11: opRep = " == "; break;
		case 0x12: opRep = " <= "; break;
		case 0x13: opRep = " < "; break;
		case 0x14: opRep = " >= "; break;
		case 0x15: opRep = " > (check this) "; break;
		case 0x20: opRep = " & "; break;
		case 0x21: opRep = " | (check this) "; break;
		default: 
			opRep = " <op" + (hex ? ("0x" + toHex(op)) : std::to_string(op)) + "> ";
			break;
	}

	return expr1->print(hex) + opRep + expr2->print(hex);
}

void BinaryValueExpr::negateBool() {
	switch (op) {
		case 0x10: op = 0x11; break;
		case 0x11: op = 0x10; break;
		case 0x12: op = 0x15; break;
		case 0x13: op = 0x14; break;
		case 0x14: op = 0x13; break;
		case 0x15: op = 0x12; break;

		// DeMorgan's laws
		case 0x20:
			op = 0x21;
			negateCondition(expr1);
			negateCondition(expr2);
		break;
		case 0x21:
			op = 0x20;
			negateCondition(expr1);
			negateCondition(expr2);
		break;
	}
}

// this looks weird, check the expiration date
IndexValueExpr::IndexValueExpr(Value e1, Value e2) : BinaryValueExpr(std::move(e1), std::move(e2), 0xFFFFFFFF) {
	if (expr2->getType() != ValueType::INT) {
		Logger::Error() << "Indexing with non-int value.\n";
		throw std::logic_error("Bad index");
	}

	Logger::VVDebug() << expr1->print() << " is array of type " << VarType(expr1->getType()) << "\n";

	// Set type of value
	unsigned int arrType = expr1->getType() - 3;
	if (arrType == ValueType::INTLIST)
		type = ValueType::INTREF;
	else if (arrType == ValueType::STRLIST)
		type = ValueType::STRREF;
	else if (arrType == ValueType::OBJLIST)
		type = ValueType::OBJ_STR;
	else
		type = ValueType::INTREF;
}

std::string IndexValueExpr::print(bool hex) const {
	return expr1->print(hex) + "[" + expr2->print(hex) + "]";
}

bool IndexValueExpr::isLValue() {
	return (type == ValueType::INTREF || type == ValueType::STRREF || type == ValueType::OBJ_STR);
}


MemberExpr::MemberExpr(Value e1, Value e2) : BinaryValueExpr(std::move(e1), std::move(e2), 0xFFFFFFFF) {
	if (expr2->getType() != ValueType::INT) {
		Logger::Warn() << "Accessing with non-int value (is that bad?).\n";
	}

	// Temporary
	type = ValueType::INTREF;
}

std::string MemberExpr::print(bool hex) const {
	return expr1->print(hex) + "." + expr2->print(hex);
}


std::string RawValueExpr::print(bool hex) const {
	if (type == ValueType::STR) {
		return "\"" + str + "\"";
	} else if (hex) {
		return "0x" + toHex(value);
	} else {
		return std::to_string(value);
	}
}

IntType RawValueExpr::getIntType() {
	if (type != ValueType::INT)
		return IntegerInvalid;

	if (value == 0xFFFFFFFF)
		return IntegerIndexer;

	unsigned char intType = value >> 24;
	switch (intType) {
		case 0x00:
			if (value == 0x53 || value == 0x25 || value == 0x26)
				return IntegerLocalRef;
			else
				return IntegerSimple;
		case 0x7d:	return IntegerLocalVar;
		case 0x7e:	return IntegerFunction;
		case 0x7f:	return IntegerGlobalVar;
		default:
			return IntegerInvalid;
	}
}

std::string NotExpr::print(bool hex) const {
	return "!(" + cond->print(hex) + ")";
}


// fix
unsigned int VarValueExpr::varCount = 0;
VarValueExpr::VarValueExpr(std::string name, unsigned int type, unsigned int length) : ValueExpr(type + 3), name(name), length(length) {
	if (type == ValueType::INTLIST || type == ValueType::STRLIST) {
		if (length == 0)
			Logger::Error() << VarType(type) << " must have positive length.\n";
	} else if (type == ValueType::INT || type == ValueType::STR) {
		if (length > 0)
			Logger::Error() << VarType(type) << " cannot have positive length.\n";
	}
}

VarValueExpr::VarValueExpr(unsigned int type) : ValueExpr(type) {
	name = "var" + std::to_string(varCount++);
}


std::string VarValueExpr::print(bool hex) const {
	if (!name.empty())
		return name;
	else
		return ValueExpr::print(hex);
}

std::string ErrValueExpr::print(bool) const {
	return std::string("(ERROR)");
}

CallExpr::CallExpr(FunctionExpr* fnCall_, unsigned int option_, std::vector<Value> args_, std::vector<unsigned int> extraList_, unsigned int returnType_) : callFunc(fnCall_) {
	type = returnType_;
	fnOption = option_;
	fnArgs = std::move(args_);
	fnExtra = extraList_;
}

CallExpr::CallExpr(const CallExpr& copy) : ValueExpr(copy), callFunc(copy.callFunc->clone()), fnOption(copy.fnOption), fnExtra(copy.fnExtra) {
	for (const auto& arg:copy.fnArgs) {
		fnArgs.emplace_back(arg->clone());
	}
}

CallExpr::~CallExpr() {
	delete callFunc;
}


std::string CallExpr::print(bool hex) const {
	std::string str = callFunc->print(true);
	if (fnOption != 0)
		str += "-" + std::to_string(fnOption);
	str += "(";
	for (auto it = fnArgs.rbegin(); it != fnArgs.rend(); it++) {
		if (it != fnArgs.rbegin())
			str += ", ";

		str += (*it)->print(hex);
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

	return str;
}

std::string FunctionExpr::print(bool hex) const {
	std::string ret;
	if (callValue != nullptr)
		ret = callValue->print(hex);
	else
		ret = name;

	if (hasExtra)
		ret += "<" + std::to_string(extraCall) + ">";
	return ret;
}

ShortCallExpr::ShortCallExpr(unsigned int index, std::vector<Value> args) {
	blockIndex = index;
	fnArgs = std::move(args);

	type = ValueType::INT;
}


std::string ShortCallExpr::print(bool hex) const {
	std::string str = "call@L" + std::to_string(blockIndex) + "(";
	for (auto it = fnArgs.rbegin(); it != fnArgs.rend(); it++) {
		if (it != fnArgs.rbegin())
			str += ", ";

		str += (*it)->print(hex);
	}
	str += ")";
	return str;
}
