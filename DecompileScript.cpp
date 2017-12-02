#define __USE_MINGW_ANSI_STDIO 0

#include <iomanip>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <memory>

#include <cassert>
#include <unistd.h>
#include <getopt.h>

#include "Helper.h"
#include "Logger.h"
#include "Structs.h"


#include "Statements.h"

// todo: make progstack an actual struct that contains a vector and the frames

typedef struct BasicBlock {
	static int count;
	int index = 0;
	enum Type {
		ONEWAY, TWOWAY, SWITCH,
		CALL, RET, FALL,
		INVALID
	};

	Type blockType = INVALID;
	bool parsed = false;
	bool isFunction = false;
	bool isEntrypoint = false;

	unsigned int startAddress;
	unsigned int nextAddress;

	std::vector<Expression*> expressions;
	std::vector<Statement*> statements;

	std::vector<BasicBlock*> pred, succ, calls;

	BasicBlock(unsigned int address) : index(count++), startAddress(address){}
	~BasicBlock();


} Block;
int Block::count;

BasicBlock::~BasicBlock() {
	for (auto s:statements)
		delete s;
}

struct Label {
	unsigned int address;
	Block* pBlock = nullptr;
	Label(unsigned int offset) : address(offset) {}
};

/* class CallRet {
	public:
		unsigned int address;
		BasicBlock* pBlock;
		unsigned int stackPointer;
		CallRet(unsigned int ret, BasicBlock* retBlock, unsigned int retSP) :
			address(ret), pBlock(retBlock), stackPointer(retSP) {};
}; */

class BytecodeBuffer {
	private:
		unsigned char* bytecode = NULL;
		unsigned int dataLength = 0;
		unsigned int currAddress = 0;

	public:
		unsigned int getInt();
		unsigned char getChar();
		bool done() {
			return currAddress == dataLength;
		};
		unsigned int getAddress() {
			return currAddress;
		};

		void setAddress(unsigned int address) {
			currAddress = address;
		}

		BytecodeBuffer(std::ifstream& f, unsigned int length);
		~BytecodeBuffer();
};

struct ProgBranch {
	Block* pBlock;
	ProgStack stack;
	std::vector<unsigned int> stackHeights;
	ProgBranch(Block* pBlock, ProgStack saveStack, std::vector<unsigned int> stackHeights) : pBlock(pBlock), stack(saveStack), stackHeights(stackHeights) {
		//Deepcopy stack - actually maybe not
	}
};

typedef class BytecodeParser Parser;
class ScriptInfo;

class BytecodeParser {
	private:
		BytecodeBuffer* buf;

		std::shared_ptr<ValueExpr> stackPop();
		void stackPush(std::shared_ptr<ValueExpr> pValue);

	public:
		std::vector<ProgBranch> toTraverse;
		unsigned int instAddress = 0;
		// operand stack
		ProgStack stack;
		std::vector<unsigned int> stackHeights;

		//std::vector<CallRet> callStack;

		unsigned int getInt() {
			return buf->getInt();
		};
		unsigned char getChar() {
			return buf->getChar();
		};
		std::shared_ptr<ValueExpr> getArg(unsigned int type);
		std::shared_ptr<ValueExpr> getLValue(const ScriptInfo &info);
	public:
		BytecodeParser(std::ifstream &f, HeaderPair index);
		~BytecodeParser();

		void addBranch(Block* pBlock);
		void addBranch(Block* pBlock, ProgStack saveStack, std::vector<unsigned int> stackHeights);
		void parse(ScriptInfo& info);
};

class ScriptInfo {
	private:
		std::vector<Label> labels, entrypoints, functions;
		std::vector<Block*> blocks;

		std::vector<Block*> entryBlocks;

		struct checkAddress {
			unsigned int address;
		    checkAddress(int address) : address(address) {}

		    bool operator ()(Block* const& pBlock) const {
		    	return pBlock->startAddress == address;
		   	}
		};
	private:
		int fileIndex;
		StringList sceneNames;

		StringList stringData;
		ProgStack globalVars;

	public:
		ScriptInfo(std::ifstream &f, const ScriptHeader& header, int fileIndex);
		~ScriptInfo();

		std::string getString(unsigned int index);
		std::shared_ptr<ValueExpr> getGlobalVar (unsigned int index) const;

		Block* getBlock(unsigned int address);
		Block* getBlockAtLabel(unsigned int labelIndex);

		bool checkBlock(unsigned int address);

		void readGlobalInfo(std::string filename);

		void initEntrypoints(Parser& parser);
		void generateStatements();

		void printBlocks(std::string filename);
		void dumpCFG(std::string filename);
};

	



void readScriptHeader(std::ifstream &f, ScriptHeader &header) {
	f.read((char*) &header.headerSize, 4);
	if (header.headerSize != 0x84) {
		Logger::Error() << "Expected script header size 0x84, got 0x" << std::hex << header.headerSize << std::endl;
		throw std::exception();
	}
	readHeaderPair(f, header.bytecode);
	readHeaderPair(f, header.stringIndex);
	readHeaderPair(f, header.stringData);
	
	readHeaderPair(f, header.labels);
	readHeaderPair(f, header.entrypoints);
	
	readHeaderPair(f, header.localCommandIndex);    
	readHeaderPair(f, header.unknown1);
	readHeaderPair(f, header.staticVarIndex);    
	readHeaderPair(f, header.staticVarNames);
	
	readHeaderPair(f, header.functions);
	readHeaderPair(f, header.functionNameIndex);
	readHeaderPair(f, header.functionNames);
	
	readHeaderPair(f, header.localVarIndex);
	readHeaderPair(f, header.localVarNames);
	
	readHeaderPair(f, header.unknown6);
	readHeaderPair(f, header.unknown7);

	assert(header.functions.count == header.functionNameIndex.count);
}

void readLabels(std::ifstream &stream, std::vector<Label> &labels, HeaderPair index) {
	labels.reserve(index.count);

	unsigned int offset;
	stream.seekg(index.offset, std::ios_base::beg);
	for (unsigned int i = 0; i < index.count; i++) {
		stream.read((char*) &offset, 4);
		labels.push_back(Label(offset));
	}
}


std::string ScriptInfo::getString(unsigned int index) {
	return (index >= stringData.size()) ? "" : stringData[index];
}

std::shared_ptr<ValueExpr> ScriptInfo::getGlobalVar(unsigned int index) const {
	if (index & 0xFF000000)
		return nullptr;

	if (index >= globalVars.size())
		return nullptr;

	return globalVars[index];

}


// Guarantee this will be the only way new blocks are made
Block* ScriptInfo::getBlock(unsigned int address) {
	auto pBlockIt = std::find_if(blocks.begin(), blocks.end(), checkAddress(address));
	if (pBlockIt != blocks.end())
		return (*pBlockIt);

	Block* pBlock = new Block(address);
	blocks.push_back(pBlock);
	return pBlock;
}

Block* ScriptInfo::getBlockAtLabel(unsigned int labelIndex) {
	if (labelIndex >= labels.size()) {
		throw std::out_of_range("Label index out of range.");
	}
	if (labels[labelIndex].pBlock == nullptr)
		labels[labelIndex].pBlock = getBlock(labels[labelIndex].address);

	return labels[labelIndex].pBlock;
}

bool ScriptInfo::checkBlock(unsigned int address) {
	auto pBlockIt = std::find_if(blocks.begin(), blocks.end(), checkAddress(address));
	return (pBlockIt != blocks.end());
}

ScriptInfo::ScriptInfo(std::ifstream &stream, const ScriptHeader &header, int index) : fileIndex(index) {
	readLabels(stream, labels, header.labels);
	readLabels(stream, entrypoints, header.entrypoints);
	readLabels(stream, functions, header.functions);

	readStrings(stream, stringData, header.stringIndex, header.stringData, true);
}

ScriptInfo::~ScriptInfo() {
	for (auto &pBlock:blocks) {
		delete pBlock;
	}
}

void ScriptInfo::readGlobalInfo(std::string filename) {
	std::ifstream globalInfoFile("SceneInfo.dat", std::ios::in | std::ios::binary);
	if (!globalInfoFile.is_open()) {
		Logger::Error() << "Could not open global scene info.\n";
		return;
	}
	std::string name;
	unsigned int count;
	
	// Scene names
	std::string basename = filename.substr(filename.find_last_of("/\\") + 1);
	basename = basename.substr(0, basename.find_last_of('.'));
	
	globalInfoFile.read((char*) &count, 4);	
	for (unsigned int i = 0; i < count; i++) {
		std::getline(globalInfoFile, name, '\0');
		sceneNames.push_back(name);
		if (fileIndex < 0) {
			if (basename.compare(name) == 0) {
				Logger::Info() << "Determined file index: " << i << "(" << name << ")\n";
			}
		}
	}
	
	// Global vars
	globalInfoFile.read((char*) &count, 4);
	globalVars.reserve(count);
	unsigned int type, length;
	for (unsigned int i = 0; i < count; i++) {
		globalInfoFile.read((char*) &type, 4);
		globalInfoFile.read((char*) &length, 4);
		std::getline(globalInfoFile, name, '\0');

		globalVars.push_back(std::make_shared<VarValueExpr>(name, type, length));
	}
	Logger::Info() << "Read " << std::to_string(count) << " global variables.\n";
	globalInfoFile.close();
}


void ScriptInfo::initEntrypoints(Parser &parser) {
	Block* pBlock;
	for (const auto& entrypoint:entrypoints) {
		if (entrypoint.address == 0x0)
			continue;
		Logger::VDebug() << "Entrypoint added at 0x" << toHex(entrypoint.address) << std::endl;
		pBlock = getBlock(entrypoint.address);
		pBlock->isEntrypoint = true;
		parser.addBranch(pBlock);

		entryBlocks.push_back(pBlock);
	}
}

void ScriptInfo::generateStatements() {
	for (auto &pBlock:blocks) {
		for (auto &pExpr:pBlock->expressions){
			pBlock->statements.push_back(new Statement(pExpr));
		}
	}
}

void ScriptInfo::printBlocks(std::string filename) {
	std::ofstream out(filename);
	for (auto &pBlock:blocks) {
		out << "\nL" << std::to_string(pBlock->index) << "@0x" << toHex(pBlock->startAddress) << "\n";
		out << "==========================================================================\n";
		for (auto &pStatement:pBlock->statements){
			pStatement->print(out);
		}
		out << "\n==========================================================================\n";
	}
	out.close();
}

void ScriptInfo::dumpCFG(std::string filename) {
	std::ofstream out(filename);
	// should be same - check
	out << "strict digraph " << "CFG" << " {\n";
	out << "\tnode [shape=box]\n";
	for (const auto &block:blocks) {
		std::string props;
		if (block->isEntrypoint)
			props += "penwidth=2,";
		if (block->isFunction)
			props += "color=red,";

		if (!props.empty())
			out << "\tBlock" << std::to_string(block->index) << " [" << props << "]\n";

		// Successors
		if (block->succ.empty()) {
			if (!block->isFunction)
				out << "\tBlock" << std::to_string(block->index) << " -> Exit\n";
		} else {
			out << "\tBlock" << std::to_string(block->index) << " -> {";
			for (auto p = block->succ.begin(); p != block->succ.end(); p++) {
				if (p != block->succ.begin())
					out << "; ";
				out << "Block" << std::to_string((*p)->index);
			}
			out << "}\n";
		}

		if (!block->calls.empty()) {
			out << "\tBlock" << std::to_string(block->index) << " -> {";
			for (auto p = block->calls.begin(); p != block->calls.end(); p++) {
				if (p != block->calls.begin())
					out << "; ";
				out << "Block" << std::to_string((*p)->index);
			}
			out << "} [color=red]\n";
		}
	}

	out << "Exit [penwidth=2]\n";

	out << "}\n";

	out.close();
}


int Logger::LogLevel = Logger::LEVEL_INFO;
int main(int argc, char* argv[]) {
	extern char *optarg;
	extern int optind;
	
	std::string outFilename;
	static char usageString[] = "Usage: decompiless [-o outfile] [-v] [-i file index] <input.ss>";

	int fileIndex = -1;
	// Handle options
	int option = 0;
	while ((option = getopt(argc, argv, "o:vi:")) != -1) {
		switch (option) {
		case 'v':
			Logger::increaseVerbosity();
			break;
		case 'o':
			outFilename = std::string(optarg);
		break;
		case 'i':
			fileIndex = std::stoi(optarg);
		break;
		default:
			std::cout << usageString << std::endl;
			return 1;
		}
	}
	
	if (optind >= argc) {
		std::cout << usageString << std::endl;
		return 1;
	}
	std::cout << std::setfill('0');
	
	std::string filename(argv[optind]);
	std::ifstream fileStream(filename, std::ifstream::in | std::ifstream::binary);
	if (!fileStream.is_open()) {
		Logger::Error() << "Could not open file " << filename;
		return 1;
	}
	
	if (outFilename.empty())
		outFilename = filename + std::string(".asm");
	
	// Start reading stuff

	// Read script header
	ScriptHeader header;
	readScriptHeader(fileStream, header);

	BytecodeParser parser(fileStream, header.bytecode);

	ScriptInfo info(fileStream, header, fileIndex);
	fileStream.close();

	info.readGlobalInfo(filename);

	info.initEntrypoints(parser);
	
	try {
		parser.parse(info);
	} catch(std::logic_error &e) {
		std::cerr << e.what() << std::endl;
	}

	info.generateStatements();

	info.printBlocks(outFilename);

	info.dumpCFG(filename + ".gv");
		
	// TODO: handle these
	if (header.unknown1.count != 0) {
		Logger::Warn() << "Unknown1 has " << header.unknown1.count << " elements.\n";
		
	}
	if (header.unknown6.count != 0) {
		Logger::Warn() << "Unknown6 has " << header.unknown6.count << " elements.\n";
		
	}
	if (header.unknown7.count != 0) {
		Logger::Warn() << "Unknown7 has " << header.unknown7.count << " elements.\n";
	}
			
	return 0;
}






BytecodeParser::BytecodeParser(std::ifstream &f, HeaderPair index) {
	f.seekg(index.offset, std::ios_base::beg);
	buf = new BytecodeBuffer(f, index.count);

	stackHeights.push_back(0);
}

BytecodeParser::~BytecodeParser() {
	delete buf;
}


void BytecodeParser::addBranch(Block* pBlock) {
	std::vector<unsigned int> heights;
	heights.push_back(0);
	addBranch(pBlock, ProgStack(), heights);
}

void BytecodeParser::addBranch(Block* pBlock, ProgStack saveStack, std::vector<unsigned int> stackHeights) {
	if (pBlock == nullptr)
		throw std::logic_error("Null block");
	if (pBlock->parsed)
		return;
	for (const auto &branch:toTraverse) {
		// Operate under assumption block pointers will be unique
		if (branch.pBlock == pBlock)
			return;
	}

	toTraverse.push_back(ProgBranch(pBlock, saveStack, stackHeights));
}


std::shared_ptr<ValueExpr> BytecodeParser::stackPop() {
	if (stack.empty())
		throw std::out_of_range("Popping empty stack.");

	std::shared_ptr<ValueExpr> pExpr = stack.back();
	stack.pop_back();
	stackHeights.back()--;

	return pExpr;
}
void BytecodeParser::stackPush(std::shared_ptr<ValueExpr> pValue) {
	stack.push_back(pValue);
	stackHeights.back()++;
}

std::shared_ptr<ValueExpr> BytecodeParser::getArg(unsigned int type) {
	if (stack.empty())
		throw std::out_of_range("Popping arguments off an empty stack.");

	unsigned int actualType = stack.back()->getType();
	if (type == ValueType::INT) {
		if (actualType != ValueType::INT) {
			Logger::Error(instAddress) << "Cannot get argument of type int with type " << VarType(actualType) << std::endl;
		}

		return stackPop();
	} else if (type == ValueType::STR) {
		if (actualType != ValueType::STR) {
			Logger::Error(instAddress) << "Cannot get argument of type str with type " << VarType(actualType) << std::endl;
		}

		return stackPop();
	}

	return std::make_shared<ErrValueExpr>();
}

std::shared_ptr<ValueExpr> BytecodeParser::getLValue(const ScriptInfo &info) {
	std::shared_ptr<ValueExpr> pCurr, pLast, pIndex;
	while (stackHeights.back() > 0) {
		pCurr = stackPop();
		// handle pCurr first (eg 0x7f)

		if (pIndex != nullptr) {
			std::shared_ptr<ValueExpr> pVar = info.getGlobalVar(pCurr->getVarIndex());
			
			if (pVar)
				pCurr = std::make_shared<IndexValueExpr>(pVar, pIndex);
			else // not quite right yet, transform this into a proper intlist/strlist first
				pCurr = std::make_shared<IndexValueExpr>(pCurr, pIndex);

			pIndex = nullptr;
		} else if (pCurr->isIndexer()) {
			if (pLast == nullptr)
				throw std::logic_error("Null index.");

			pIndex = pLast;
		} else if (pCurr->getType() == ValueType::INT) {
			// If value is a variable, load it
			auto pVar = info.getGlobalVar(pCurr->getVarIndex());
			if (pVar) {
				pCurr = pVar;
			}
		} else {
			Logger::Error(instAddress) << "Don't know what to do with ";
			pCurr->print(Logger::Error_());
			Logger::Error_() << " (" << VarType(pCurr->getType()) << ")\n";
		}
		pLast = pCurr;
	}


	if (!pCurr->isLValue())
		Logger::Error(instAddress) << "Could not get lvalue!\n";

	// Close the 0x08
	stackHeights.pop_back();
	return pCurr;
}


void BytecodeParser::parse(ScriptInfo& info) {
	while (!toTraverse.empty()) {
		ProgBranch branch = toTraverse.back();
		toTraverse.pop_back();

		Block* pBlock = branch.pBlock;
		buf->setAddress(pBlock->startAddress);
		stack = branch.stack;
		stackHeights = branch.stackHeights;
		pBlock->parsed = true;

		Logger::Debug(instAddress) << "Parsing new branch - starting at block " << std::to_string(pBlock->index) << std::endl;

		unsigned char opcode;
		Expression* pExpr;
		bool newBlock = false;
		while (!buf->done()) {
			instAddress = buf->getAddress();
			opcode = getChar();

			pExpr = nullptr;
			switch (opcode) {
				case 0x01: pExpr = new LineExpr(getInt()); break;
				case 0x02: {
					unsigned int type = getInt();
					unsigned int value = getInt();
					std::shared_ptr<ValueExpr> rhs;
					if (type == ValueType::STR) {
						std::string str = info.getString(value);
						if (str.empty())
							rhs = std::make_shared<RawValueExpr>(type, value);
						else
							rhs = std::make_shared<RawValueExpr>(str, value);
					} else {
						rhs = std::make_shared<RawValueExpr>(type, value);
					}

					std::shared_ptr<VarValueExpr> lhs(VarValueExpr::stackLoc(stackHeights));
					pExpr = new AssignExpr(lhs, rhs);
					stackPush(rhs);
				} break;
				case 0x03: {
					unsigned int type = getInt();
					std::shared_ptr<ValueExpr> back = stackPop();
					if (back->getType() != type) {
						Logger::Error(instAddress) << "Expected type " << VarType(type) << ", got type " << VarType(back->getType()) << std::endl;
					}
				} break;
				case 0x04: {
					unsigned int type = getInt();
					std::shared_ptr<ValueExpr> rhs = stack.back();
					if (rhs->getType() != type) {
						Logger::Error(instAddress) << "Dup - Expected type " << VarType(type) << ", got type " << VarType(rhs->getType()) << std::endl;
						break;
					}

					std::shared_ptr<VarValueExpr> lhs(VarValueExpr::stackLoc(stackHeights));
					pExpr = new AssignExpr(lhs, rhs);
					stackPush(rhs);
				} break;
				case 0x05: {
					std::shared_ptr<ValueExpr> rhs = getLValue(info);
					if (stackHeights.empty())
						throw std::logic_error("No frames to close.");

					// Convert lvalue to rvalue (not really)
					// these aren't even real lvalues
					rhs = rhs->clone();
					// Crude but seems to work so far
					rhs->setType(rhs->getType() - 3);

					std::shared_ptr<VarValueExpr> lhs(VarValueExpr::stackLoc(stackHeights));
					pExpr = new AssignExpr(lhs, rhs);
					stackPush(rhs);
				} break;
				case 0x08: {
					stackHeights.push_back(0);
				} break;
				case 0x10: {
					unsigned int labelIndex = getInt();
					Block* pJumpBlock = info.getBlockAtLabel(labelIndex);

					pExpr = new JumpExpr(pJumpBlock->index);

					pBlock->nextAddress = buf->getAddress();
					buf->setAddress(pJumpBlock->startAddress);
					newBlock = true;
				} break;
				case 0x11: 
				case 0x12: {
					unsigned int labelIndex = getInt();
					Block* pJumpBlock = info.getBlockAtLabel(labelIndex);

					// Pop the condition
					std::shared_ptr<ValueExpr> condition = stackPop();

					// Store stack after condition has been popped
					addBranch(pJumpBlock, stack, stackHeights);
					pBlock->succ.push_back(pJumpBlock);
					pJumpBlock->pred.push_back(pBlock);


					Block* pNextBlock = info.getBlock(buf->getAddress());
					if (opcode == 0x11)
						pExpr = new JumpExpr(pJumpBlock->index, pNextBlock->index, std::make_shared<NotExpr>(condition));
					else
						pExpr = new JumpExpr(pJumpBlock->index, pNextBlock->index, condition);

					pBlock->nextAddress = buf->getAddress();
					newBlock = true;
				} break;
				case 0x13: {
					unsigned int labelIndex = getInt();
					Block* pCallBlock = info.getBlockAtLabel(labelIndex);
					pCallBlock->isFunction = true;
					pBlock->calls.push_back(pCallBlock);

					unsigned int numArgs = getInt();
					std::vector<unsigned int> argTypes;
					for (unsigned int i = 0; i < numArgs; i++) {
						argTypes.push_back(getInt());
					}

					ProgStack args;
					for (auto &type:argTypes) {
						args.push_back(getArg(type));
					}

					addBranch(pCallBlock, stack, stackHeights);

					std::shared_ptr<ValueExpr> pCall = std::make_shared<ShortCallExpr>(pCallBlock->index, args);

					std::shared_ptr<VarValueExpr> lhs(VarValueExpr::stackLoc(stackHeights));
					pExpr = new AssignExpr(lhs, pCall);
					stackPush(pCall);
				} break;
				case 0x15: {
					unsigned int numArgs = getInt();
					std::vector<unsigned int> retTypes;
					for (unsigned int i = 0; i < numArgs; i++) {
						retTypes.push_back(getInt());
					}

					ProgStack ret;
					for (auto &type:retTypes) {
						ret.push_back(getArg(type));
					}

					pExpr = new RetExpr(ret);

					pBlock->nextAddress = buf->getAddress();
					newBlock = true;

					if (stack.size() != branch.stack.size()) {
						Logger::Error(instAddress) << "Stack height changed from " << std::to_string(branch.stack.size()) << " to " << std::to_string(stack.size()) << std::endl;
						for (auto &value:stack) {
							value->print(Logger::Debug());
							Logger::Debug() << "\n";
						}
					}

				} break;
				case 0x20: {
					unsigned int lType = getInt();
					unsigned int rType = getInt();
					unsigned int unknown = getInt();
					if (unknown != 1) {
						Logger::Warn(instAddress) << "Assigning with " << std::to_string(unknown) << std::endl;
					}

					std::shared_ptr<ValueExpr> rhs = stackPop();
					std::shared_ptr<ValueExpr> lhs = getLValue(info);

					if (lhs->getType() != lType) {
						Logger::Error(instAddress) << "Assign - Expected type " << VarType(lType) << ", got type " << VarType(lhs->getType()) << std::endl;
						lhs = std::make_shared<ErrValueExpr>();
					}
					if (rhs->getType() != rType) {
						Logger::Error(instAddress) << "Assign - Expected type " << VarType(rType) << ", got type " << VarType(rhs->getType()) << std::endl;
						rhs = std::make_shared<ErrValueExpr>();
					}

					Logger::VVDebug(instAddress) << "Assign: " << VarType(lType) << " <- " << VarType(rType) << std::endl;

					pExpr = new AssignExpr(lhs, rhs);
				} break;
				case 0x22: {
					unsigned int lhsType = getInt();
					unsigned int rhsType = getInt();
					unsigned int op = getChar();

					std::shared_ptr<ValueExpr> lhs, rhs;
					rhs = stackPop();
					lhs = stackPop();
					if (lhs->getType() != lhsType) {
						Logger::Error(instAddress) << "Calc - Expected type " << VarType(lhsType) << ", got type " << VarType(lhs->getType()) << std::endl;
						lhs = std::make_shared<ErrValueExpr>();
					}
					if (rhs->getType() != rhsType) {
						Logger::Error(instAddress) << "Calc - Expected type " << VarType(rhsType) << ", got type " << VarType(rhs->getType()) << std::endl;
						rhs = std::make_shared<ErrValueExpr>();
					}

					std::shared_ptr<VarValueExpr> resultLoc(VarValueExpr::stackLoc(stackHeights));
					std::shared_ptr<ValueExpr> result = std::make_shared<BinaryValueExpr>(lhs, rhs, op);

					pExpr = new AssignExpr(resultLoc, result);
					stackPush(result);
				} break;
				case 0x30: {
					unsigned int option = getInt();

					unsigned int numArgs = getInt();
					std::vector<unsigned int> argTypes;
					for (unsigned int i = 0; i < numArgs; i++) {
						argTypes.push_back(getInt());
					}

					unsigned int numExtra = getInt();
					std::vector<unsigned int> extraList;
					for (unsigned int i = 0; i < numExtra; i++) {
						extraList.push_back(getInt());
					}

					unsigned int returnType = getInt();

					ProgStack args;
					for (auto &type:argTypes) {
						args.push_back(getArg(type));
					}

					// Reversed though
					ProgStack fnCall;
					while (stackHeights.back() > 0) {
						fnCall.push_back(stackPop());
					}
					if (fnCall.empty()) {
						Logger::Error(instAddress) << "Function call empty!\n";
					}
					stackHeights.pop_back();

					std::shared_ptr<CallExpr> pCall = std::make_shared<CallExpr>(fnCall, option, args, extraList, returnType);

					if (pCall->needExtra())
						pCall->extraThing(getInt());

					std::shared_ptr<VarValueExpr> lhs(VarValueExpr::stackLoc(stackHeights));
					pExpr = new AssignExpr(lhs, pCall);
					
					if (pCall->pushRet())
						stackPush(pCall);

				} break;
				case 0x31: {
					unsigned int id = getInt();
					std::shared_ptr<ValueExpr> pText = stackPop();
					if (pText->getType() != ValueType::STR) {
						pText->print(Logger::Error(instAddress));
						Logger::Error_() << "(" << VarType(pText->getType()) << ") cannot be used to add text.\n";
					}
					pExpr = new AddTextExpr(pText, id);
				} break;
				case 0x32: {
					std::shared_ptr<ValueExpr> pName = stackPop();
					if (pName->getType() != ValueType::STR) {
						pName->print(Logger::Error(instAddress));
						Logger::Error_() << "(" << VarType(pName->getType()) << ") cannot be used to set name.\n";
					}
					pExpr = new SetNameExpr(pName);
				} break;
				default: {
					Logger::Error(instAddress) << "NOP: 0x" << toHex(opcode, 2) << std::endl;
				}
			}

			if (pExpr != nullptr)
				pBlock->expressions.push_back(pExpr);

			if (newBlock || info.checkBlock(buf->getAddress())) {
				// if i get the block here, I can dump dead code
				// it's probably mostly line numbers though

				if (opcode == 0x15)
					break;

				Block* pNextBlock = info.getBlock(buf->getAddress());

				// Add successor/predecessor no matter what
				pBlock->succ.push_back(pNextBlock);
				pNextBlock->pred.push_back(pBlock);

				// Exit this branch if target is parsed
				if (pNextBlock->parsed)
					break;

				pBlock = pNextBlock;
				newBlock = false;

				pBlock->parsed = true;
			}
		}

	}
}




BytecodeBuffer::BytecodeBuffer(std::ifstream &f, unsigned int length) {
	dataLength = length;
	bytecode = new unsigned char[dataLength];

	f.read((char*) bytecode, dataLength);
	if (f.fail()) {
		Logger::Error() << "Tried to read " << dataLength << " bytes, got" << f.gcount() << std::endl;
		throw std::exception();
	}
	Logger::Debug() << "Read " << f.gcount() << " bytes of bytecode." << std::endl;
}

BytecodeBuffer::~BytecodeBuffer(){
	delete[] bytecode;
}

unsigned int BytecodeBuffer::getInt() {
	unsigned int value = 0;
	if (currAddress + 4 <= dataLength) {
		value = readUInt32(bytecode + currAddress);
		currAddress += 4;
	} else {
		throw std::out_of_range("Buffer out of data");
	}
	return value;
}
unsigned char BytecodeBuffer::getChar() {
	unsigned char value = 0;
	if (currAddress < dataLength) {
		value = bytecode[currAddress++];
	} else {
		throw std::out_of_range("Buffer out of data");
	}
	return value;
}