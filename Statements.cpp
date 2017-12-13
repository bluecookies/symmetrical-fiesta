#include <iostream>

#include "Statements.h"

// Statements

/*
Statement* Statement::makeStatement(Expression* expr) {
	if (expr == nullptr)
		throw std::logic_error("Null pointer passed into statement.");

	Statement* pStatement;
	if (expr->getType() == Expression::UNCOND_JUMP) {
		pStatement = new GotoStatement(expr->getIndex());
	} else {
		pStatement = new Statement();

		pStatement->expr = expr;
	}

	if (expr->getLineNum() >= 0)
		pStatement->lineNum = expr->getLineNum();

	return pStatement;
} */

ExpressionStatement::ExpressionStatement(ValueExpr* expr_) : expr(expr_) {
	if (expr == nullptr)
		throw std::logic_error("Null pointer passed into statement.");

	if (expr->getLineNum() >= 0)
		lineNum = expr->getLineNum();
}


ExpressionStatement::~ExpressionStatement() {
	delete expr;
};

void ExpressionStatement::print(std::ostream &out, int indentation) const {
	out << std::string(indentation, '\t') << expr->print() << printLineNum();
}

// Puts this statement into the else block
IfStatement* Statement::makeIf(Value cond, StatementBlock block) {
	return new IfStatement(std::move(cond), block, {this});
}



IfStatement::IfStatement(Value cond, StatementBlock trueBlock_, StatementBlock falseBlock_) {
	int lineNum = cond->getLineNum();
	branches.push_back(IfBranch(nullptr, falseBlock_));
	branches.push_back(IfBranch(std::move(cond), trueBlock_, lineNum));
}

IfStatement::~IfStatement() {
	for (const auto& branch:branches) {
		for (const auto& statement:branch.block) {
			delete statement;
		}
		// destroy condition
	}
}

void IfStatement::print(std::ostream &out, int indentation) const {
	auto pBranch = branches.rbegin();
	if (pBranch == branches.rend())
		return;

	out << std::string(indentation, '\t') << "if (" << pBranch->condition->print() << ") {" << pBranch->lineComment() << "\n";
	for (const auto& statement:pBranch->block) {
		statement->print(out, indentation + 1);
	}
	out << std::string(indentation, '\t') << "}";
	pBranch++;
	while (pBranch != branches.rend()) {
		if (pBranch->condition) {
			out << std::string(indentation, '\t') << " else if (" << pBranch->condition->print() << ") {" << pBranch->lineComment() << "\n";
		} else {
			if (pBranch->block.empty())
				break;
			out << std::string(indentation, '\t') << " else {" << pBranch->lineComment() << "\n";
			if (pBranch + 1 != branches.rend())
				Logger::Error() << "More conditions after else block.\n";
		}

		for (const auto& statement:pBranch->block) {
			statement->print(out, indentation + 1);
		}
		pBranch++;
		out << std::string(indentation, '\t') << "}";
	}
	out << "\n";
}

IfStatement* IfStatement::makeIf(Value cond, StatementBlock block) {
	int lineNum = cond->getLineNum();
	branches.push_back(IfBranch(std::move(cond), block, lineNum));
	return this;
}

int IfStatement::getSize() const {
	int size = 1 + branches.size();
	for (const auto& branch:branches) {
		for (const auto& statement:branch.block) {
			size += statement->getSize();
		}
	}
	return size;
}

// For statement - contains.. blocks?

ForStatement::ForStatement(Value cond_, StatementBlock inc) : condition(std::move(cond_)), increment(inc) {
}

ForStatement::~ForStatement() {
	for (const auto& statement:increment)
		delete statement;
}

void ForStatement::print(std::ostream &out, int indentation) const {
	out << std::string(indentation, '\t') << "for (;" << condition->print() << ";";

	auto pStatement = increment.begin();
	if (pStatement != increment.end()) {
		(*pStatement)->print(out);
		pStatement++;
	}
	while (pStatement != increment.end()) {
		out << ", ";
		(*pStatement)->print(out);
		pStatement++;
	}

	std::string lineComment = (condition->getLineNum() < 0) ? "\n" : ("\t// line " + std::to_string(condition->getLineNum()) + "\n");

	out << ") {" << lineComment;
	out << std::string(indentation, '\t') << "}\n";
}

GotoStatement::GotoStatement(unsigned int index) : blockIndex(index) {
	type = GOTO;
}

void GotoStatement::print(std::ostream &out, int indentation) const {
	out << std::string(indentation, '\t') << "goto L" << std::to_string(blockIndex) << printLineNum();
}

int GotoStatement::getSize() const {
	return 0;
}

LineNumStatement::LineNumStatement(int lineNum_) {
	lineNum = lineNum_;
	type = LINE_NUM;
}

void LineNumStatement::print(std::ostream &out, int indentation) const {
	out << std::string(indentation, '\t') << "line " << std::to_string(lineNum) << "\n";
}

int LineNumStatement::getSize() const {
	return 0;
}

void ClearBufferStatement::print(std::ostream &out, int indentation) const {
	out << std::string(indentation, '\t') << "clearbuf" << printLineNum();
}

int ClearBufferStatement::getSize() const {
	return 0;
}

AssignStatement::AssignStatement(Value lhs_, Value rhs_) {
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

void AssignStatement::print(std::ostream &out, int indentation) const {
	out << std::string(indentation, '\t') << lhs->print() << " = " << rhs->print() << "\t(" << VarType(lhs->getType()) << ")" << printLineNum();
}

BranchStatement::BranchStatement(unsigned int thenIndex, unsigned int elseIndex, Value cond) : blockIndex(thenIndex), elseIndex(elseIndex), condition(std::move(cond)) {
	if (condition == nullptr)
		throw std::logic_error("Jump with null condition");
	type = JUMP_IF;
}


void BranchStatement::print(std::ostream &out, int indentation) const {
	out << std::string(indentation, '\t') << "if (" << condition->print() << ")  jump L" << std::to_string(blockIndex) << " else L" + std::to_string(elseIndex) << printLineNum();
}


void ReturnStatement::print(std::ostream &out, int indentation) const {
	out << std::string(indentation, '\t') << "return";
	if (!values.empty()) {
		out << " (";
		for (auto it = values.rbegin(); it != values.rend(); it++) {
			if (it != values.rbegin())
				out << ", ";

			out << (*it)->print();
		}
		out << ")";
	}
	out << printLineNum();
}

void AddTextStatement::print(std::ostream &out, int indentation) const {
	out << std::string(indentation, '\t') << "AddText " << std::to_string(index) << " " << text->print() << printLineNum();
}

void SetNameStatement::print(std::ostream &out, int indentation) const {
	out << std::string(indentation, '\t') << "SetName " << name->print() << printLineNum();
}


WhileStatement::WhileStatement(Value cond_, std::vector<Block*> blocks) : condition(std::move(cond_)), blocks(blocks) {
	if (condition->getLineNum() >= 0)
		lineNum = condition->getLineNum();
}

void WhileStatement::print(std::ostream &out, int indentation) const {
	out << std::string(indentation, '\t') << "while () " << printLineNum();
}


/*
Statement* IfStatement::foldSwitch() {
	if (!condition->isBinaryExpression())
		return nullptr;


	BinaryValueExpr* comp = static_cast<BinaryValueExpr*>(condition.get());
	std::string hopefullyTemp = comp->getExpression1()->print();

	if (comp->getOp() != 0x11)
		return nullptr;

	unsigned int value = comp->getExpression2()->getIndex();
	if (value & 0xFF000000)
		return nullptr;

	if (falseBlock.size() > 1)
		return nullptr;
	else if (falseBlock.empty())

	//falseBlock.back
		return nullptr;

	return nullptr;
} */















