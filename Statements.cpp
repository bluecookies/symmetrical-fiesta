#include <iostream>
#include <vector>
#include <memory>

#include "Helper.h"
#include "Logger.h"
#include "Statements.h"

// Expressions

std::string Expression::print(bool) {
	return std::string("Expression here\n");
}

std::string LineExpr::print(bool hex) {
	if (hex)
		return "line 0x" + toHex(lineNum);
	else
		return "line " + std::to_string(lineNum);
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

std::string BinaryValueExpr::print(bool hex) {
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
		default: 
			opRep = " <op" + (hex ? ("0x" + toHex(op)) : std::to_string(op)) + "> ";
			break;
	}

	return expr1->print(hex) + opRep + expr2->print(hex);
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

std::string IndexValueExpr::print(bool hex) {
	return expr1->print(hex) + "[" + expr2->print(hex) + "]";
}

// when fixing, make sure type is integer and integer bool, and cond is also integer and integer bool
NotExpr::NotExpr(Value cond) : ValueExpr(ValueType::INT), condition(std::move(cond)) {
	if (condition->getType() != ValueType::INT) {
		Logger::Error() << "Not cannot be applied to value of type " << VarType(condition->getType()) << std::endl;
	}
}


std::string NotExpr::print(bool hex) {
	return "!(" + condition->print(hex) + ")";
}

std::string RawValueExpr::print(bool hex) {
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


std::string VarValueExpr::print(bool hex) {
	if (!name.empty())
		return name;
	else
		return ValueExpr::print(hex);
}

std::string ErrValueExpr::print(bool) {
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


std::string CallExpr::print(bool hex) {
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

std::string Function::print() {
	return name;
}

ShortCallExpr::ShortCallExpr(unsigned int index, std::vector<Value> args) {
	blockIndex = index;
	fnArgs = std::move(args);

	type = ValueType::INT;
}


std::string ShortCallExpr::print(bool hex) {
	std::string str = "call@L" + std::to_string(blockIndex) + "(";
	for (auto it = fnArgs.rbegin(); it != fnArgs.rend(); it++) {
		if (it != fnArgs.rbegin())
			str += ", ";

		str += (*it)->print(hex);
	}
	str += ")";
	return str;
}


AssignExpr::AssignExpr(Value lhs_, Value rhs_) {
	if (lhs_ == nullptr || rhs_ == nullptr) {
		throw std::logic_error("Assigning null pointers.");
	}
	
	lhs = std::move(lhs_);
	rhs = std::move(rhs_);

	if (!lhs->isLValue())
		throw std::logic_error("error: lvalue required as left operand of assignment");

	if (rhs->getType() == ValueType::UNDEF)
		throw std::logic_error("right operand type undefined");

	if (lhs->getType() == ValueType::UNDEF) {
		lhs->setType(rhs->getType());
	} 
}

std::string AssignExpr::print(bool hex) {
	return lhs->print(hex) + " = " + rhs->print(hex) + "\t(" + VarType(lhs->getType()) + ")";
}

JumpExpr::JumpExpr(unsigned int blockIndex, unsigned int elseIndex, Value cond) : blockIndex(blockIndex), elseIndex(elseIndex), condition(std::move(cond)) {

}


std::string JumpExpr::print(bool hex) {
	if (condition != nullptr) {
		return "if (" + condition->print(hex) +")  jump L" + std::to_string(blockIndex) + " else L" + std::to_string(elseIndex);
	} else {
		return "jump L" + std::to_string(blockIndex);
	}
}


std::string RetExpr::print(bool hex) {
	std::string str("return");
	if (!values.empty()) {
		str += " (";
		for (auto it = values.rbegin(); it != values.rend(); it++) {
			if (it != values.rbegin())
				str += ", ";

			str += (*it)->print(hex);
		}
		str += ")";
	}
	return str;
}

std::string AddTextExpr::print(bool hex) {
	return "addtext " + (hex ? ("0x" + toHex(index)) : std::to_string(index)) + " " + text->print(hex);
}

std::string SetNameExpr::print(bool hex) {
	return "setname " +	name->print(hex);
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
