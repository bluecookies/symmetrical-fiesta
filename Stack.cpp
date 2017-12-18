#include "BytecodeParser.h"
#include "Statements.h"
#include "Logger.h"

Stack::Stack() : stackHeights({0}) {}

Stack::Stack(const Stack& copy) : stackHeights(copy.stackHeights) {
	for (const auto& value:copy.values) {
		values.emplace_back(value->clone());
	}
}


// TODO: swap, but its not like anything can go wrong here
Stack& Stack::operator=(const Stack& copy) {
	if (this == &copy)
		return *this;

	stackHeights = copy.stackHeights;
	values.clear();
	for (const auto& value:copy.values) {
		values.emplace_back(value->clone());
	}

	return *this;
}

// Takes ownership of raw pointer
void Stack::push(Expression* pValue) {
	values.emplace_back(pValue);
	height()++;

	if (back() == nullptr) {
		Logger::Error() << "Nullptr placed onto stack.\n";
		throw std::logic_error("NULL on stack.");
	}
}


Value Stack::pop() {
	if (empty())
		throw std::out_of_range("Popping empty stack.");

	if (height() == 0)
		throw std::logic_error("Popping empty frame.");

	Value pExpr = std::move(back());
	values.pop_back();
	height()--;

	return pExpr;
}

void Stack::openFrame() {
	stackHeights.push_back(0);
}

std::vector<Value>::iterator Stack::getFrame() {
	return values.end() - height();
}


void Stack::closeFrame() {
	values.erase(values.end() - height(), values.end());
	stackHeights.pop_back();
	// 
	if (stackHeights.empty())
		stackHeights.push_back(0);
}

void Stack::duplicateElement() {
	unsigned int count = stackHeights.back();
	stackHeights.push_back(0);
	for (unsigned int i = 0; i < count; i++) {
		push(values.at(values.size() - count)->clone());
	}
}

std::string Stack::print() {
	std::string str;
	for (const auto& value:values) {
		str += value->print() + " (" + VarType(value->getType()) + ") \n";
	}
	return str;
}