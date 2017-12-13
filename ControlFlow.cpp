#include <iostream>

#include "BytecodeParser.h"
#include "Statements.h"

#include "ControlFlow.h"
#include "Logger.h"


int Block::count;

BasicBlock::~BasicBlock() {
	for (auto s:statements)
		delete s;
}

void BasicBlock::addSuccessor(BasicBlock* pSucc) {
	succ.push_back(pSucc);
	pSucc->pred.push_back(this);
}

void BasicBlock::removeSuccessor(BasicBlock* pSucc) {
	auto pRes = std::remove(succ.begin(), succ.end(), pSucc);
	if (pRes == succ.end())
		return;

	succ.erase(pRes, succ.end());
	pSucc->pred.erase(std::remove(pSucc->pred.begin(), pSucc->pred.end(), this), pSucc->pred.end());
}

Block::Type BasicBlock::getBlockType() {
	if (statements.empty())
		return INVALID;
	
	Statement* pExit = statements.back();
	
	auto type = pExit->type;
	// Ignore fallthrough
	// Maybe don't put goto and check fallthrough here
	switch (type) {
		case Statement::RETURN:
			return RET;
		case Statement::GOTO:
			return ONEWAY;
		case Statement::JUMP_IF:{
			if (succ.size() != 2)
				throw std::logic_error("Corrupt block.");
			return TWOWAY;
		}
		default: {
			Logger::Debug() << "Invalid block type.\n";
			return INVALID;
		}
	}
}

int BasicBlock::getDrawSize() {
	if (index == 0)
		return 3;

	int size = 0;
	for (const auto& statement:statements)
		size += statement->getSize();

	return size;
}

void BasicBlock::pop() {
	if (statements.empty())
		throw std::logic_error("Popping empty block.");

	delete statements.back();
	statements.pop_back();
}

NaturalLoop::NaturalLoop(Block* header, Block* tail) : header(header) {
	blocks.push_back(header);
	merge(tail);
}

void NaturalLoop::merge(Block* tail) {
	tails.push_back(tail);
	if (std::find(blocks.begin(), blocks.end(), tail) != blocks.end()) {
		return;
	}
	Logger::Debug() << "Loop found from L" << std::to_string(tail->index) << " to L" << std::to_string(header->index) << "\n";

	// DFS stack for backwards traversal
	std::vector<Block*> toSearch;

	if (header != tail) {
		blocks.push_back(tail);
		toSearch.push_back(tail);
	}

	Block* pBlock;
	while (!toSearch.empty()) {
		pBlock = toSearch.back();
		toSearch.pop_back();

		for (const auto& pPred:pBlock->pred) {
			if (!contains(pPred)) {
				blocks.push_back(pPred);
				toSearch.push_back(pPred);
			}
		}

	}
}

bool NaturalLoop::contains(Block* pBlock) {
	return (std::find(blocks.begin(), blocks.end(), pBlock) != blocks.end());
}

// Strict weak ordering
// A < B iff A does not contain B
bool NaturalLoop::operator <(const NaturalLoop& other) const {
	// Eithre comparing with self or somehow have same header (didn't combine)
	if (header == other.header)
		return false;
	return (std::find(blocks.begin(), blocks.end(), other.header) != blocks.end());
}


// Guarantee this will be the only way new blocks are made
Block* ControlFlowGraph::getBlock(unsigned int address, bool createIfNone) {
	auto pBlockIt = std::find_if(blocks.begin(), blocks.end(), checkAddress(address));
	if (pBlockIt != blocks.end())
		return (*pBlockIt);

	if (!createIfNone)
		return nullptr;

	Block* pBlock = new Block(address);
	blocks.push_back(pBlock);
	return pBlock;
}

void ControlFlowGraph::removeBlock(Block* pBlock) {
	if (!pBlock)
		return;

	if (!pBlock->statements.empty()) {
		Logger::Warn() << "Removing non-empty block L" << std::to_string(pBlock->index) << "\n";
	}

	// Remove links up
	for (auto& pPred:pBlock->pred) {
		pPred->succ.erase(std::remove(pPred->succ.begin(), pPred->succ.end(), pBlock), pPred->succ.end());
	}
	pBlock->pred.clear();
	// Remove successors
	for (auto& pSucc:pBlock->succ) {
		pSucc->pred.erase(std::remove(pSucc->pred.begin(), pSucc->pred.end(), pBlock), pSucc->pred.end());
	}
	pBlock->succ.clear();
	// Remove from graph entirely, hope there's nothing remaining (remember to handle the calls)
	blocks.erase(std::remove(blocks.begin(), blocks.end(), pBlock), blocks.end());
	removedBlocks.push_back(pBlock);
}

void ControlFlowGraph::mergeBlocks(Block* pBlock, Block* pSucc) {
	Logger::VDebug() << "Merging L" << std::to_string(pBlock->index) << " and L" << std::to_string(pSucc->index) << ".\n";

	// Update successors
	pBlock->succ.insert(pBlock->succ.end(), pSucc->succ.begin(), pSucc->succ.end());
	for (auto& pSuccSucc:pSucc->succ) {
		std::replace(pSuccSucc->pred.begin(), pSuccSucc->pred.end(), pSucc, pBlock);
	}

	// Move statements
	pBlock->statements.reserve(pBlock->statements.size() + pSucc->statements.size());
	pBlock->statements.insert(pBlock->statements.end(), pSucc->statements.begin(), pSucc->statements.end());
	pSucc->statements.clear();

	// Move calls
	pBlock->calls.insert(pBlock->calls.end(), pSucc->calls.begin(), pSucc->calls.end());
	pSucc->calls.clear();

	removeBlock(pSucc);
}

// For now, no blocks are allowed to have been removed yet
std::vector<Bitset> ControlFlowGraph::findDominators() {
	int numBlocks = blocks.size();
  	std::vector<Bitset> dominators(numBlocks, Bitset(numBlocks, true));
  	if (blocks[0]->index != 0) {
  		Logger::Error() << "Blocks corrupted.\n";
  		return dominators;
  	}
  	// Set entry to have itself
  	dominators[0].clear();
  	dominators[0].set(0);
 	
 	bool changed = true;
 	Bitset temp;
 	while (changed) {
 		changed = false;

 		for (const auto& pBlock:blocks) {
 			if (pBlock->index == 0)
 				continue;
 			
 			Bitset& currDoms = dominators.at(pBlock->index);
 			for (const auto& pPred:pBlock->pred) {
 				temp = currDoms;
 				currDoms &= dominators.at(pPred->index);
 				currDoms.set(pBlock->index);
 				if (temp != currDoms)
 					changed = true;
 			}
 		}
 	}
 	return dominators;
}

std::vector<Loop> ControlFlowGraph::findLoops() {
	std::vector<Loop> loops;
	std::vector<Bitset> dominators = findDominators();
	for (const auto& pBlock:blocks) {
		Bitset currDom = dominators.at(pBlock->index);
		for (const auto& pSucc:pBlock->succ) {
			// If this is a back edge
			if (currDom.get(pSucc->index)) {
				// Check if existing loop with same header
				auto loopIt = std::find_if(loops.begin(), loops.end(), [pSucc](const Loop& loop){ return loop.header == pSucc; });
				if (loopIt == loops.end())
					loops.emplace_back(pSucc, pBlock);
				else
					loopIt->merge(pBlock);
			}
		}
	}
	return loops;
}

ControlFlowGraph::ControlFlowGraph(Parser &parser, std::vector<unsigned int> entrypoints) {
	Block* entry = getBlock(0xFFFFFFFF);
	Block* pBlock;
	for (const auto& address:entrypoints) {
		if (address == 0x0)
			continue;
		Logger::VDebug() << "Entrypoint added at 0x" << toHex(address) << std::endl;
		pBlock = getBlock(address);
		pBlock->isEntrypoint = true;
		parser.addBranch(pBlock);

		entry->addSuccessor(pBlock);
	}
}

ControlFlowGraph::~ControlFlowGraph() {
	for (auto &pBlock:blocks) {
		delete pBlock;
	}
	for (auto &pBlock:removedBlocks) {
		delete pBlock;
	}
}

bool ControlFlowGraph::StructureIf(Block* pBlock) {
	if (pBlock->getBlockType() != Block::TWOWAY) {
		return false;
	}
	Logger::VVDebug() << std::to_string(pBlock->index) << " has " << std::to_string(pBlock->succ.size()) << " successors.\n";

	// Might be a little confusing - jump location was added first when hitting a conditional jump
	Block* trueBlock = pBlock->succ.at(0);
	Block* falseBlock = pBlock->succ.at(1);
	if (trueBlock->pred.size() != 1 || falseBlock->pred.size() != 1) {
		return false;
	}
	if (trueBlock->succ.size() != 1 || falseBlock->succ.size() != 1) {
		return false;
	}
	if (trueBlock->succ.at(0) != falseBlock->succ.at(0)) {
		return false;
	}
	Block* finalBlock = trueBlock->succ.at(0);

	// Remove the last (jump) statements
	trueBlock->pop();
	falseBlock->pop();

	BranchStatement* pBranch = static_cast<BranchStatement*>(pBlock->statements.back());

	IfStatement* pIf;
	// If false block only has one statement (not including the jump)
	if (falseBlock->statements.size() == 1) {
		pIf = falseBlock->statements.back()->makeIf(std::move(pBranch->getCondition()), trueBlock->statements);
		trueBlock->statements.clear();
		falseBlock->statements.pop_back();
	} else {
		pIf = new IfStatement(std::move(pBranch->getCondition()), trueBlock->statements, falseBlock->statements);
		trueBlock->statements.clear();
		falseBlock->statements.clear();
	}
	/* Ahem
	 The last statement (which should contain the last expression which is a conditional jump) is destroyed,
	 destroying the jump expression, which by now has its condition moved out into the if statement.
	 This condition will be destroyed by the if statement which is destroyed when the vector is cleared. */

	// Remove the if-goto
	pBlock->pop();
	// Push the if statement
	pBlock->statements.push_back(pIf);
	/* Every statement in both blocks have been moved out, and they are not referenced anywhere else 
	   except maybe as a subroutine, so should probably watch out for that. */

	// Transfer calls
	pBlock->calls.insert(pBlock->calls.end(), trueBlock->calls.begin(), trueBlock->calls.end());
	pBlock->calls.insert(pBlock->calls.end(), falseBlock->calls.begin(), falseBlock->calls.end());
	trueBlock->calls.clear();
	falseBlock->calls.clear();

	// Remove true/false blocks from control flow
	removeBlock(trueBlock);
	removeBlock(falseBlock);

	// Replace successor with single target
	pBlock->addSuccessor(finalBlock);

	// Add jump instruction to target
	pBlock->statements.push_back(new GotoStatement(finalBlock->index));

	// delete true and false blocks
	// but not yet
	return true;
}

void ControlFlowGraph::StructureLoops() {
	std::vector<Loop> loops = findLoops();
	std::sort(loops.begin(), loops.end());

	// While - Two way header, one way latching node
	// Do While - One way header, two way latching node

	// TODO: (maybe) rewrite the matching/recognition to be nicer
	for (auto& loop:loops) {
		if (loop.tails.size() != 1)
			continue;	// Deal with this if turns out to be needed
		if (loop.header->getBlockType() != Block::TWOWAY)
			continue;
		if (loop.header->statements.size() != 1)
			continue;
		if (loop.tails[0]->getBlockType() != Block::ONEWAY)
			continue;

		Block* breakNode;
		Block* loopStart;
		if (!loop.contains(loop.header->succ[0])) {
			breakNode = loop.header->succ[0];
			loopStart = loop.header->succ[1];
		} else {
			loopStart = loop.header->succ[0];
			breakNode = loop.header->succ[1];
			if (loop.contains(breakNode))
				continue;	// Maybe deal with this;
		}

		BranchStatement* pBranch = static_cast<BranchStatement*>(loop.header->statements.back());
		Value& cond = pBranch->getCondition();
		// Check which branch leads into the loop
		// if (condition) jump breaknode => while (!condition)
		// if (condition) jump loopnode => while (condition)
		if (loop.header->succ[0] == breakNode)
			cond->negateBool();

		// Remove header - only the while statement in it
		// not in general - restructure
		loop.blocks.erase(std::remove(loop.blocks.begin(), loop.blocks.end(), loop.header), loop.blocks.end());
		WhileStatement* pWhile = new WhileStatement(std::move(cond), loop.blocks, loopStart);	// check condition
	
		loop.header->pop();
		loop.header->statements.push_back(pWhile);
		loop.header->statements.push_back(new GotoStatement(breakNode->index));


		loop.header->removeSuccessor(loopStart);

		// Remove the link from tail to head
		loop.tails[0]->removeSuccessor(loop.header);
		loop.tails[0]->pop();
		loop.tails[0]->statements.push_back(new ContinueStatement());

		// Transfer calls
		for (const auto& block:loop.blocks) {
			loop.header->calls.insert(loop.header->calls.end(), block->calls.begin(), block->calls.end());
		}
	}
}

bool ControlFlowGraph::StructureMerge(Block* pBlock) {
	if (pBlock->succ.size() == 1) {
		Block* pSucc = pBlock->succ[0];
		if (pSucc->pred.size() == 1) {
			// Delete jump statement
			pBlock->pop();
			// Merge blocks
			mergeBlocks(pBlock, pSucc);
			return true;
		}
	}
	return false;
}

void ControlFlowGraph::structureStatements() {
	Logger::Info() << "Structuring loops.\n";
	StructureLoops();


	Logger::Info() << "Structuring if else statements.\n";
	// Structure ifs and simplify blocks
	bool changed = true;
	while (changed) {
		changed = false;
		for (auto &pBlock:blocks) {
			if (StructureIf(pBlock)) {
				changed = true;
				break;
			} else if (pBlock->index != 0) {
				changed |= StructureMerge(pBlock);
			}
		}
	}

	// Turn ifs into switches
	//for (auto &pBlock:blocks) {
	//	if (!pBlock->statements.empty()) {
	//		pBlock->statements.back()->foldSwitch();
	//	}
	//}

	
}

void ControlFlowGraph::printBlocks(std::string filename) {
	std::vector<Block*> toPrint, printed;
	toPrint.push_back(blocks.at(0));

	std::ofstream out(filename);
	while (!toPrint.empty()) {
		Block* pBlock = toPrint.back();
		toPrint.pop_back();

		for (const auto &pSucc:pBlock->succ) {
			if (std::find(printed.begin(), printed.end(), pSucc) == printed.end()) {
				toPrint.push_back(pSucc);
			}
		}

		for (const auto &pCall:pBlock->calls) {
			if (std::find(printed.begin(), printed.end(), pCall) == printed.end()) {
				toPrint.push_back(pCall);
			}
		}

		if (pBlock->index > 0) {
			out << "\nL" << std::to_string(pBlock->index) << "@0x" << toHex(pBlock->startAddress) << ":\n";
			for (auto &pStatement:pBlock->statements) {
				pStatement->print(out);
			}
		}

		printed.push_back(pBlock);
	}
	out.close();
}

void ControlFlowGraph::dumpGraph(std::string filename) {
	std::vector<Block*> toPrint, printed;
	printed.push_back(blocks.at(0));

	std::ofstream out(filename);
	// should be same - check
	out << "strict digraph " << "CFG" << " {\n";
	out << "\tnode [shape=box, fixedsize=true, fontsize=8, labelloc=\"t\"]\n";

	std::string props;

	Block* pBlock = blocks.at(0);
	props += "penwidth=2,color=blue,";
	props += "height=" + std::to_string(pBlock->getDrawSize()*0.05);
	out << "\tEntry" << " [" << props << "]\n";

	out << "\tEntry -> {";
	for (auto p = pBlock->succ.begin(); p != pBlock->succ.end(); p++) {
		if (p != pBlock->succ.begin())
			out << "; ";
		out << "Block" << std::to_string((*p)->index);

		toPrint.push_back(*p);
	}
	out << "}\n";

	while (!toPrint.empty()) {
		pBlock = toPrint.back();
		toPrint.pop_back();

		props = "";
		if (pBlock->isFunction)
			props += "color=red,";
		props += "height=" + std::to_string(pBlock->getDrawSize()*0.05);

		out << "\tBlock" << std::to_string(pBlock->index) << " [" << props << "]\n";

		// Successors
		if (pBlock->succ.empty()) {
			if (!pBlock->isFunction)
				out << "\tBlock" << std::to_string(pBlock->index) << " -> Exit\n";
		} else {
			out << "\tBlock" << std::to_string(pBlock->index) << " -> {";
			for (auto p = pBlock->succ.begin(); p != pBlock->succ.end(); p++) {
				if (p != pBlock->succ.begin())
					out << "; ";
				out << "Block" << std::to_string((*p)->index);

				if (std::find(printed.begin(), printed.end(), *p) == printed.end())
					toPrint.push_back(*p);
			}
			out << "}\n";
		}

		if (!pBlock->calls.empty()) {
			out << "\tBlock" << std::to_string(pBlock->index) << " -> {";
			for (auto p = pBlock->calls.begin(); p != pBlock->calls.end(); p++) {
				if (p != pBlock->calls.begin())
					out << "; ";
				out << "Block" << std::to_string((*p)->index);

				if (std::find(printed.begin(), printed.end(), *p) == printed.end())
					toPrint.push_back(*p);
			}
			out << "} [color=red]\n";
		}

		printed.push_back(pBlock);
	}


	out << "\tExit [penwidth=2, height=0.15]\n";

	out << "}\n";

	out.close();
}







