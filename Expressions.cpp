#include <iostream>
#include <vector>
#include <memory>

#include "Helper.h"
#include "Logger.h"

#include "Statements.h"

// TODO: some renaming, maybe have expression inherit statement

void negateCondition(Value& cond) {
	if (cond->exprType == Expression::BOOL_EXPR) {
		static_cast<BinaryExpression*>(cond.get())->negateBool();
	} else {
		// check if its a not, if so remove the not
		cond = make_unique<NotExpr>(std::move(cond));
	}
}

// Expressions

std::string Expression::print(bool) const {
	return std::string("(value)");
}


//	Precedence table
//  [-] [~] [!]
//	* / %
//  + -
//  << >> >>>
//  < <= > >=
//  == !=
//  &
//  ^
//  |
//  &&
//  ||
//

BinaryExpression::BinaryExpression(Value e1, Value e2, unsigned char op_) {
	if (e1 == nullptr || e2 == nullptr) {
		throw std::logic_error("Assigning null pointers.");
	}

	// Take ownership (if I had rewritten this instead of trying to edit it, I wouldn't have tried to use e1 and e2 again later)
	expr1 = std::move(e1);
	expr2 = std::move(e2);
	op = op_;

	if (op == 0xFF)
		return;

	unsigned int type1 = expr1->getType();
	unsigned int type2 = expr2->getType();

	if (type1 == ValueType::INT && type2 == ValueType::INT) {
		type = ValueType::INT;
	} else if (type1 == ValueType::STR && type2 == ValueType::INT) {
		if (op == 0x03)
			type = ValueType::STR;
	} else if (type1 == ValueType::STR && type2 == ValueType::STR) {
		if (op == 0x01)
			type = ValueType::STR;
		else if (0x10 <= op && op <= 0x15)
			type = ValueType::INT;
	}

	if (type == ValueType::UNDEF)
		Logger::Warn() << expr1->print() << "(" << VarType(type1) << "), " << expr2->print() << "(" << VarType(type2) << "), 0x" << toHex(op, 2) << std::endl;

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

std::string BinaryExpression::print(bool hex) const {
	std::string opRep;
	switch (op) {
		case 0x01: opRep = " + "; break;
		case 0x02: opRep = " - "; break;
		case 0x03: opRep = "*"; break;	
		case 0x04: opRep = "/"; break;	// safe division
		case 0x05: opRep = " % "; break;	// safe modulo
		case 0x10: opRep = " == "; break;
		case 0x11: opRep = " != "; break;
		case 0x12: opRep = " > "; break;
		case 0x13: opRep = " >= "; break;
		case 0x14: opRep = " < "; break;
		case 0x15: opRep = " <=  "; break;
		case 0x20: opRep = " && "; break; // does not short circuit
		case 0x21: opRep = " || "; break;
		case 0x31: opRep = " & "; break; // bitwise
		case 0x32: opRep = " | "; break;
		case 0x33: opRep = " ^ "; break;
		case 0x34: opRep = " << "; break;
		case 0x35: opRep = " >> "; break;
		case 0x36: opRep = " >>> "; break; // unsigned
		default: 
			opRep = " <op" + (hex ? ("0x" + toHex(op)) : std::to_string(op)) + "> ";
			break;
	}
	int leftPrecedence = expr1->getPrecedence();
	int rightPrecedence = expr2->getPrecedence();
	int precedence = getPrecedence();

	std::string left = (leftPrecedence < precedence) ? ("(" + expr1->print(hex) + ")") : expr1->print(hex);
	std::string right;
	if (rightPrecedence < precedence) {
		right = "(" + expr2->print(hex) + ")";
	} else if (rightPrecedence > precedence) {
		right = expr2->print(hex);
	} else {
		if (op == 0x02 || op == 0x04)
			right = "(" + expr2->print(hex) + ")";
		else
			right = expr2->print(hex);
	}


	return left + opRep + right; 
}

void BinaryExpression::negateBool() {
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


UnaryExpression::UnaryExpression(Value expr_, unsigned char op_) {
	if (expr_ == nullptr) {
		throw std::logic_error("Assigning null pointers.");
	}

	expr = std::move(expr_);
	op = op_;

	if (op == 0xFF)
		return;

	type = expr->getType();


	if (type != ValueType::INT) {
		Logger::Warn() << expr->print() << "(" << VarType(type) << "), 0x" << toHex(op, 2) << std::endl;
	}
}

std::string UnaryExpression::print(bool hex) const {
	std::string opRep;
	std::string node = expr->print(hex);
	if (expr->getPrecedence() < getPrecedence() || op == 0x01)
		node = "(" + node + ")";
	switch (op) {
		case 0x01: return "I" + node;	// identity
		case 0x02: return "-" + node + "";
		case 0x30: return "~" + node;	// bitwise NOT
		default: // should actually pop the value
			return "<op" + (hex ? ("0x" + toHex(op)) : std::to_string(op)) + ">" + node;
	}
}

// TODO: make a lookup or something
int BinaryExpression::getPrecedence() const {
	if (op == 0x01 || op == 0x02)
		return 20;

	if (op == 0x03 || op == 0x04 || op == 0x05)
		return 22;

	if (op == 0x20)
		return 6;
	if (op == 0x21)
		return 4;

	if (op == 0x34 || op == 0x35 || op == 0x36)
		return 18;

	if (op >= 0x12 && op <= 0x15)
		return 16;

	if (op == 0x10 || op == 0x11)
		return 14;

	if (op == 0x31)
		return 12;
	if (op == 0x32)
		return 10;
	if (op == 0x33)
		return 8;

	return 0xFF;
}

int UnaryExpression::getPrecedence() const {
	return 24;
}

int NotExpr::getPrecedence() const {
	return 24;
}

// this looks weird, check the expiration date
IndexValueExpr::IndexValueExpr(Value e1, Value e2) : BinaryExpression(std::move(e1), std::move(e2), 0xFF) {
	if (expr2->getType() != ValueType::INT) {
		Logger::Error() << "Indexing with non-int value.\n";
		throw std::logic_error("Bad index");
	}

	Logger::VVDebug() << expr1->print() << " is array of type " << VarType(expr1->getType()) << "\n";

	// Set type of value
	unsigned int arrType = expr1->getType();
	if (arrType == ValueType::INT_LIST)
		type = ValueType::INT;
	else if (arrType == ValueType::STR_LIST)
		type = ValueType::STR;
	else if (arrType == ValueType::OBJECT_LIST)
		type = ValueType::OBJECT;
	else
		type = ValueType::INT;
}

std::string IndexValueExpr::print(bool hex) const {
	return expr1->print(hex) + "[" + expr2->print(hex) + "]";
}

MemberExpr::MemberExpr(Value e1, Value e2) : BinaryExpression(std::move(e1), std::move(e2), 0xFF) {
	if (expr2->getType() != ValueType::INT) {
		Logger::Warn() << "Accessing with non-int value (is that bad?).\n";
	}

	// Temporary
	type = ValueType::INT;
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
		case 0x01:	// to remove and replace with kinetic handling
			return IntegerSimple;
		case 0x7d:	return IntegerLocalVar;
		case 0x7e:	return IntegerFunction;
		case 0x7f:	return IntegerGlobalVar;
		default:
			return IntegerInvalid;
	}
}

IntType CallExpr::getIntType() {
	if (type != ValueType::INT)
		return IntegerInvalid;

	return IntegerSimple;
}

std::string NotExpr::print(bool hex) const {
	if (cond->getPrecedence() < getPrecedence())
		return "!(" + cond->print(hex) + ")";
	else
		return "!" + cond->print(hex);
}


// fix
unsigned int VariableExpression::varCount = 0;
VariableExpression::VariableExpression(std::string name_, unsigned int type_, unsigned int length_) : Expression(type_), name(name_) {
	listLength = length_;
	if (type == ValueType::INT || type == ValueType::STR) {
		if (listLength > 0)
			Logger::Error() << VarType(type) << " cannot have positive length.\n";
	}

	Logger::VDebug() << "Created var " << name << " (" << VarType(type) << ")\n";
}

VariableExpression::VariableExpression(unsigned int type) : Expression(type) {
	name = "var" + std::to_string(varCount++);
}


std::string VariableExpression::print(bool hex) const {
	if (!name.empty())
		return name;
	else
		return Expression::print(hex);
}

ListExpression::ListExpression(std::vector<Value> elements_) : elements(std::move(elements_)) {
	listLength = elements.size();
	type = ValueType::INT_LIST;	// check
}

ListExpression::ListExpression(const ListExpression& copy) : Expression(copy) {
	for (const auto& elem:copy.elements) {
		elements.emplace_back(elem->clone());
	}
}

std::string ListExpression::print(bool hex) const  {
	std::string str = "{";
	for (auto it = elements.begin(); it != elements.end(); it++) {
		if (it != elements.begin()) str += ", ";
		str += (*it)->print(hex);
	}
	str += "}";
	return str;
}

ErrValueExpr::ErrValueExpr(std::string err, unsigned int address) {
	Logger::Error(address) << "Error value: " << err << std::endl;
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

CallExpr::CallExpr(const CallExpr& copy) : Expression(copy), callFunc(copy.callFunc->clone()), fnOption(copy.fnOption), fnExtra(copy.fnExtra) {
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

FunctionExpr::FunctionExpr(const FunctionExpr& copy) : name(copy.name) {
	if (copy.callValue != nullptr)
		callValue = Value(copy.callValue->clone());
	else
		callValue = nullptr;
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
