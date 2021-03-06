#include <iostream>

#include "ControlFlow.h"
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

ExpressionStatement::ExpressionStatement(Expression* expr_) : expr(expr_) {
	if (expr == nullptr)
		throw std::logic_error("Null pointer passed into statement.");
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
	if (!falseBlock_.empty())
		branches.push_back(IfBranch(nullptr, falseBlock_));
	branches.push_back(IfBranch(std::move(cond), trueBlock_));

	type = IF_ELSE;
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
			out << " else if (" << pBranch->condition->print() << ") {" << pBranch->lineComment() << "\n";
		} else {
			if (pBranch->block.empty())
				break;
			out << " else {" << pBranch->lineComment() << "\n";
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
	branches.push_back(IfBranch(std::move(cond), block));
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

void IfStatement::setLineNum(int num) {
	if (!branches.empty())
		branches.back().lineNum = num;
}


SwitchStatement* IfStatement::doFoldSwitch() {
	if (branches.size() < 3)
		return nullptr;

	std::vector<IfBranch> switches;


	auto pBranch = branches.rbegin();
	// original condition not moved
	Value& pOrigCond = pBranch->condition;
	if (pOrigCond == nullptr)
		return nullptr;

	if (pOrigCond->exprType != Expression::BOOL_EXPR)
		return nullptr;

	// comp goes out of scope if exit
	BinaryExpression* comp = static_cast<BinaryExpression*>(pOrigCond.get());
	if (!comp->isEquality())
		return nullptr;

	Value* pOrig = &comp->getLHS();
	std::string exprName = (*pOrig)->print();
	// cloned value will be deleted safely if exit
	// statements only have one copy in original branch if exit
	switches.push_back(IfBranch(Value(comp->getRHS()->clone()), pBranch->block));
	switches.back().lineNum = pBranch->lineNum;

	while (++pBranch != branches.rend()) {
		Value& pExpr = pBranch->condition;
		if (pExpr == nullptr) {
			switches.push_back(IfBranch(Value(), pBranch->block));
		} else if (pExpr->exprType != Expression::BOOL_EXPR) {
			return nullptr;
		} else {
			comp = static_cast<BinaryExpression*>(pExpr.get());
			if (!comp->isEquality())
				return nullptr;

			if (comp->getLHS()->print() != exprName)
				return nullptr;

			switches.push_back(IfBranch(Value(comp->getRHS()->clone()), pBranch->block));
			switches.back().lineNum = pBranch->lineNum;
		}
	}

	// "Check?" for success - skip
	// Move test expression
	SwitchStatement* pSwitch = new SwitchStatement(std::move(*pOrig), std::move(switches));
	// Detach statements from if branches
	for (auto &branch:branches) {
		branch.block.clear();
	}

	return pSwitch;
}

SwitchStatement* IfStatement::foldSwitch() {
	SwitchStatement* pSwitch = doFoldSwitch();
	if (pSwitch != nullptr)
		return pSwitch;

	for (auto& pBranch:branches) {
		for (auto& statement:pBranch.block) {
			pSwitch = statement->foldSwitch();
			if (pSwitch != nullptr) {
				delete statement;
				statement = pSwitch;
			}
		}
	}

	return nullptr;
}

SwitchStatement* WhileStatement::foldSwitch() {
	for (auto& pBlock:blocks) {
		for (auto& statement:pBlock->statements) {
			SwitchStatement* pSwitch = statement->foldSwitch();
			if (pSwitch != nullptr) {
				delete statement;
				statement = pSwitch;
			}
		}
	}
	return nullptr;
}

void WhileStatement::removeBlock(Block* pBlock) {
	blocks.erase(std::remove(blocks.begin(), blocks.end(), pBlock), blocks.end());
}


// Switches do not fall through
SwitchStatement::SwitchStatement(Value expr_, std::vector<IfBranch> switches_) : testExpr(std::move(expr_)), switches(std::move(switches_)) {
	type = SWITCH;
}

void SwitchStatement::print(std::ostream &out, int indentation) const {
	out << std::string(indentation, '\t') << "switch (" << testExpr->print() << ") {" << printLineNum();

	auto pBranch = switches.begin();
	auto pStatement = pBranch->block.begin();

	while (pBranch != switches.end()) {
		pStatement = pBranch->block.begin();
		if (pBranch->condition)
			out << std::string(indentation+1, '\t') << "case " << pBranch->condition->print() << ":\t";
		else
			out << std::string(indentation+1, '\t') << "default:\t";

		if (pStatement != pBranch->block.end()) {
			(*pStatement)->print(out);
			pStatement++;
		}
		while (pStatement != pBranch->block.end()) {
			(*pStatement)->print(out, indentation + 2);
			pStatement++;
		}
		
		
		pBranch++;
	}
	out << std::string(indentation, '\t') << "}\n";
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

	out << ") {" << printLineNum();
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
	out << std::string(indentation, '\t') << "// line " << std::to_string(lineNum) << "\n";
}

int LineNumStatement::getSize() const {
	return 0;
}

void DeclareVarStatement::print(std::ostream &out, int indentation) const {
	out << std::string(indentation, '\t') << VarType(type) << " " << name << "\n";
}


void ClearBufferStatement::print(std::ostream &out, int indentation) const {
	out << std::string(indentation, '\t') << "clearbuf" << printLineNum();
}

int ClearBufferStatement::getSize() const {
	return 0;
}


void EndScriptStatement::print(std::ostream &out, int indentation) const {
	out << std::string(indentation, '\t') << "EndScript" << printLineNum();
}


AssignStatement::AssignStatement(Value lhs_, Value rhs_) {
	if (lhs_ == nullptr || rhs_ == nullptr) {
		throw std::logic_error("Assigning null pointers.");
	}
	
	lhs = std::move(lhs_);
	rhs = std::move(rhs_);

	if (rhs->getType() == ValueType::UNDEF)
		Logger::Error() << "right operand type undefined" << std::endl;

	if (lhs->getType() == ValueType::UNDEF) {
		Logger::Warn() << lhs->print() << " type undefined.\n";
		lhs->setType(rhs->getType());
	} 
}

void AssignStatement::print(std::ostream &out, int indentation) const {
	out << std::string(indentation, '\t') << lhs->print() << " = " << rhs->print() << printLineNum();
}

BranchStatement::BranchStatement(unsigned int thenIndex_, unsigned int elseIndex_, Value cond_) : blockIndex(thenIndex_), elseIndex(elseIndex_), condition(std::move(cond_)) {
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

void ContinueStatement::print(std::ostream &out, int indentation) const {
	out << std::string(indentation, '\t') << "continue" << printLineNum();
}

void AddTextStatement::print(std::ostream &out, int indentation) const {
	out << std::string(indentation, '\t') << "AddText " << std::to_string(index) << " " << text->print() << printLineNum();
}

void SetNameStatement::print(std::ostream &out, int indentation) const {
	out << std::string(indentation, '\t') << "SetName " << name->print() << printLineNum();
}

// TODO: restructure header
WhileStatement::WhileStatement(Value cond_, std::vector<Block*> blocks_, Block* entry_) : condition(std::move(cond_)), blocks(blocks_), entryBlock(entry_) {
	for (auto& pBlock:blocks) {
		pBlock->loops.push_back(this);
	}
}

void WhileStatement::print(std::ostream &out, int indentation) const {
	out << std::string(indentation, '\t') << "while (" + condition->print() + ") {" << printLineNum();

	if (blocks.size() == 1) {
		for (auto &pStatement:blocks.back()->statements) {
			if (pStatement == this) {
				Logger::Error() << "Error: Loop\n";
				continue;
			}
			if (pStatement != blocks.back()->statements.back())
				pStatement->print(out, indentation+1);
		}
	} else {
		std::vector<Block*> toPrint, printed;
		toPrint.push_back(entryBlock);

		while (!toPrint.empty()) {
			Block* pBlock = toPrint.back();
			toPrint.pop_back();

			for (const auto &pSucc:pBlock->succ) {
				if (std::find(printed.begin(), printed.end(), pSucc) == printed.end()) {
					// Block containing while (TODO, remove that link, just replace with a continue if needed)
					// or maybe other exits
					if (std::find(blocks.begin(), blocks.end(), pSucc) != blocks.end())
						toPrint.push_back(pSucc);
				}
			}
			
			out << std::string(indentation+1, '\t') << "L" << std::to_string(pBlock->index) << "@0x" << toHex(pBlock->startAddress) << ":\n";
			for (auto &pStatement:pBlock->statements) {
				if (pStatement == this) {
					Logger::Error() << "Error: Loop\n";
					continue;
				}
				pStatement->print(out, indentation+1);
			}

			printed.push_back(pBlock);
		}
	} 

	out << std::string(indentation, '\t') << "}\n";
}

int WhileStatement::getSize() const {
	int size = 1;
	for (const auto& block:blocks) {
		for (const auto& statement:block->statements) {
			if (statement == this)
				continue;
			size += statement->getSize();
		}
	}
	return size;
}













