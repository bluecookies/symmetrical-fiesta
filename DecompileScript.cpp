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

	unsigned int startAddress;
	unsigned int nextAddress;

	std::vector<Expression*> expressions;
	std::vector<Statement*> statements;

	std::vector<BasicBlock*> pred, succ;

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

class CallRet {
	public:
		unsigned int address;
		BasicBlock* pBlock;
		unsigned int stackPointer;
		CallRet(unsigned int ret, BasicBlock* retBlock, unsigned int retSP) :
			address(ret), pBlock(retBlock), stackPointer(retSP) {};
};

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

		std::vector<CallRet> callStack;

		unsigned int getInt() {
			return buf->getInt();
		};
		unsigned char getChar() {
			return buf->getChar();
		};
		std::shared_ptr<ValueExpr> getArg(unsigned int type);
	public:
		BytecodeParser(std::ifstream &f, HeaderPair index);
		~BytecodeParser();

		void addBranch(Block* pBlock);
		void addBranch(Block* pBlock, ProgStack saveStack, std::vector<unsigned int> stackHeights);
		void parse(ScriptInfo& info);
		void printInstructions(std::string filename, bool sorted = false);
		void dumpCFG(std::string filename);
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

	public:
		ScriptInfo(std::ifstream &f, const ScriptHeader& header);
		~ScriptInfo();

		Block* getBlock(unsigned int address);
		Block* getBlockAtLabel(unsigned int labelIndex);

		bool checkBlock(unsigned int address);

		void initEntrypoints(Parser& parser);
		void generateStatements();

		void printBlocks(std::string filename);
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

ScriptInfo::ScriptInfo(std::ifstream &stream, const ScriptHeader &header) {
	readLabels(stream, labels, header.labels);
	readLabels(stream, entrypoints, header.entrypoints);
	readLabels(stream, functions, header.functions);
}

ScriptInfo::~ScriptInfo() {
	for (auto &pBlock:blocks) {
		delete pBlock;
	}
}

void ScriptInfo::initEntrypoints(Parser &parser) {
	Block* pBlock;
	for (const auto& entrypoint:entrypoints) {
		if (entrypoint.address == 0x0)
			continue;
		Logger::VDebug() << "Entrypoint added at 0x" << toHex(entrypoint.address) << std::endl;
		pBlock = getBlock(entrypoint.address);
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
		out << "L" << std::to_string(pBlock->index) << "\n";
		for (auto &pStatement:pBlock->statements){
			pStatement->print(out);
		}
		out << "\n===========================" << " Next address: 0x" << toHex(pBlock->nextAddress) << "=============================\n";
	}
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

	ScriptInfo info(fileStream, header);
	info.initEntrypoints(parser);
	
	try {
		parser.parse(info);
	} catch(std::logic_error &e) {
		std::cerr << e.what() << std::endl;
	}

	info.generateStatements();

	info.printBlocks(outFilename);


	fileStream.close();
	//parser.dumpCFG(filename + ".gv");
		
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



void BytecodeParser::parse(ScriptInfo& info) {
	while (!toTraverse.empty()) {
		ProgBranch branch = toTraverse.back();
		toTraverse.pop_back();

		Block* pBlock = branch.pBlock;
		buf->setAddress(pBlock->startAddress);
		stack = branch.stack;
		stackHeights = branch.stackHeights;
		pBlock->parsed = true;

		Logger::Debug(instAddress) << "Parsing block " << std::to_string(pBlock->index) << std::endl;

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
					std::shared_ptr<ValueExpr> rhs = std::make_shared<RawValueExpr>(type, value);

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
						Logger::Error(instAddress) << "Expected type " << VarType(type) << ", got type " << VarType(rhs->getType()) << std::endl;
						break;
					}

					std::shared_ptr<VarValueExpr> lhs(VarValueExpr::stackLoc(stackHeights));
					pExpr = new AssignExpr(lhs, rhs);
					stackPush(rhs);
				} break;
				case 0x05: {
					std::shared_ptr<ValueExpr> pCurr, pLast, pIndex;
					while (stackHeights.back() > 0) {
						pCurr = stackPop();
						// handle pCurr first (eg 0x7f)

						if (pIndex != nullptr) {
							pCurr = std::make_shared<IndexValueExpr>(pCurr, pIndex);

							pIndex = nullptr;
						} else if (pCurr->isIndexer()) {
							if (pLast == nullptr)
								throw std::logic_error("Null index.");

							pIndex = pLast;
						} else if (pCurr->getType() == ValueType::INT) {

						} else {

						}

						pLast = pCurr;
					}
					stackHeights.pop_back();

					if (stackHeights.empty())
						throw std::logic_error("No frames to close.");

					std::shared_ptr<VarValueExpr> lhs(VarValueExpr::stackLoc(stackHeights));
					pExpr = new AssignExpr(lhs, pCurr);
					stackPush(pCurr);
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

					addBranch(pJumpBlock, stack, stackHeights);
					std::shared_ptr<ValueExpr> condition = stackPop();


					Block* pNextBlock = info.getBlock(buf->getAddress());
					if (opcode == 0x11)
						pExpr = new JumpExpr(pJumpBlock->index, pNextBlock->index, std::make_shared<NotExpr>(condition));
					else
						pExpr = new JumpExpr(pJumpBlock->index, pNextBlock->index, condition);

					pBlock->nextAddress = buf->getAddress();
					newBlock = true;
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
					newBlock = true;
				} break;
				case 0x20: {
					unsigned int lType = getInt();
					unsigned int rType = getInt();
					unsigned int unknown = getInt();
					if (unknown != 1) {
						Logger::Warn(instAddress) << "Assigning with " << std::to_string(unknown) << std::endl;
					}

					std::shared_ptr<ValueExpr> rhs = stackPop();
					//std::shared_ptr<LValueExpr> lhs;

					//pExpr = new 
				} break;
				case 0x22: {
					unsigned int lhsType = getInt();
					unsigned int rhsType = getInt();
					unsigned int op = getChar();

					std::shared_ptr<ValueExpr> lhs, rhs;
					rhs = stackPop();
					lhs = stackPop();
					if (lhs->getType() != lhsType) {
						Logger::Error(instAddress) << "Expected type " << VarType(lhsType) << ", got type " << VarType(lhs->getType()) << std::endl;
						break;
					}
					if (rhs->getType() != rhsType) {
						Logger::Error(instAddress) << "Expected type " << VarType(rhsType) << ", got type " << VarType(rhs->getType()) << std::endl;
						break;
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

					std::shared_ptr<ValueExpr> pCall = std::make_shared<CallExpr>(fnCall, option, args, extraList, returnType);

					std::shared_ptr<VarValueExpr> lhs(VarValueExpr::stackLoc(stackHeights));
					pExpr = new AssignExpr(lhs, pCall);
					stackPush(pCall);

				} break;
				default: {
					Logger::Error(instAddress) << "NOP: 0x" << toHex(opcode, 2) << std::endl;
				}
			}

			if (pExpr != nullptr)
				pBlock->expressions.push_back(pExpr);

			if (newBlock || info.checkBlock(buf->getAddress())) {

				pBlock = info.getBlock(buf->getAddress());
				newBlock = false;

				if (opcode == 0x15)
					break;

				if (pBlock->parsed)
					break;

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