#include <iostream>
#include <vector>
#include <memory>

#include "Helper.h"
#include "Logger.h"

#include "Statements.h"


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
		case 0x13: opRep = " < (maybe, check) "; break;
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
			expr1->negateBool();
			expr2->negateBool();
		break;
		case 0x21:
			op = 0x20;
			expr1->negateBool();
			expr2->negateBool();
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
	else if (arrType == ValueType::OBJ_STR - 3)
		type = ValueType::OBJ_STR;
	else
		type = ValueType::INTREF;
}

std::string IndexValueExpr::print(bool hex) const {
	return expr1->print(hex) + "[" + expr2->print(hex) + "]";
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

CallExpr::CallExpr(Function fnCall_, unsigned int option_, std::vector<Value> args_, std::vector<unsigned int> extraList_, unsigned int returnType_) : callFunc(fnCall_) {
	type = returnType_;
	fnOption = option_;
	fnArgs = std::move(args_);
	fnExtra = extraList_;
}

CallExpr::CallExpr(const CallExpr& copy) : ValueExpr(copy), callFunc(copy.callFunc), fnOption(copy.fnOption), fnExtra(copy.fnExtra) {
	for (const auto& arg:copy.fnArgs) {
		fnArgs.emplace_back(arg->clone());
	}
}


std::string CallExpr::print(bool hex) const {
	std::string str = callFunc.print();
	str += "-" + std::to_string(fnOption) + "(";
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
	if (callFunc.hasExtra)
		str += "<" + std::to_string(callFunc.extraCall) + ">";

	return str;
}

std::string Function::print() const {
	return name;
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
