#ifndef CONTROLFLOW_H
#define CONTROLFLOW_H

#include <vector>

#include "Bitset.h"

class Statement;
typedef std::vector<Statement*> StatementBlock;

typedef struct BasicBlock {
	int index = -1;
	enum Type {
		ONEWAY, TWOWAY,
		RET, FALL,
		INVALID
	};

	bool parsed = false;
	bool isFunction = false;

	std::vector<Statement*> loops;

	unsigned int startAddress;
	unsigned int nextAddress;

	StatementBlock statements;

	std::vector<BasicBlock*> pred, succ, calls;

	BasicBlock(unsigned int address, int index_) : index(index_), startAddress(address){}
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
struct Function;

class ControlFlowGraph {
	private:
		int blockIndex = 0;
		std::vector<Block*> blocks, removedBlocks;
		struct checkAddress {
			unsigned int address;
		    checkAddress(int address_) : address(address_) {}

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
		ControlFlowGraph(const ControlFlowGraph& other) = delete;
		ControlFlowGraph& operator=(ControlFlowGraph const&) = delete;	// TODO: fix this
		~ControlFlowGraph();

		Block* getBlock(unsigned int address, bool create = true);

		void structureStatements();

		void printBlocks(std::ofstream& out, unsigned int indentation = 0);
		void dumpGraph(std::string filename);
};

#endif