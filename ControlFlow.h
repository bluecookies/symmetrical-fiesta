#ifndef CONTROLFLOW_H
#define CONTROLFLOW_H

#include <vector>

#include "Bitset.h"

class Statement;
typedef std::vector<Statement*> StatementBlock;

typedef struct BasicBlock {
	static int count;
	int index = 0;
	enum Type {
		ONEWAY, TWOWAY,
		RET, FALL,
		INVALID
	};

	bool parsed = false;
	bool isFunction = false;
	bool isEntrypoint = false;

	std::vector<Statement*> loops;

	unsigned int startAddress;
	unsigned int nextAddress;

	StatementBlock statements;

	std::vector<BasicBlock*> pred, succ, calls;

	BasicBlock(unsigned int address) : index(count++), startAddress(address){}
	~BasicBlock();

	void addSuccessor(BasicBlock*);
	void removeSuccessor(BasicBlock*);
	int getDrawSize();
	Type getBlockType();

	void pop();
} Block;


typedef struct NaturalLoop {
	Block* header;
	std::vector<Block*> tails;
	std::vector<Block*> blocks;

	NaturalLoop(Block* header, Block* tail);
	void merge(Block* tail);
	bool operator <(const NaturalLoop& other) const;
	bool contains(Block* pBlock);
} Loop;

class BytecodeParser;

class ControlFlowGraph {
	private:
		std::vector<Block*> blocks, removedBlocks;
		struct checkAddress {
			unsigned int address;
		    checkAddress(int address) : address(address) {}

		    bool operator ()(Block* const& pBlock) const {
		    	return pBlock->startAddress == address;
		   	}
		};

		std::vector<Bitset> findDominators();
		std::vector<Loop> findLoops();
		bool StructureIf(Block* pBlock);
		bool StructureMerge(Block* pBlock);
		void StructureLoops();

		void removeBlock(Block* pBlock);
		void mergeBlocks(Block* pBlock, Block* pSucc);

	public:
		ControlFlowGraph(BytecodeParser &parser, std::vector<unsigned int> entrypoints);
		~ControlFlowGraph();

		Block* getBlock(unsigned int address, bool create = true);

		void structureStatements();

		void printBlocks(std::string filename);
		void dumpGraph(std::string filename);
};

#endif