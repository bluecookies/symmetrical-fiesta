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
	StatementBlock statements;

	std::vector<BasicBlock*> pred, succ, calls;

	BasicBlock(unsigned int address) : index(count++), startAddress(address){}
	~BasicBlock();

	void addSuccessor(BasicBlock*);
} Block;
int Block::count;

BasicBlock::~BasicBlock() {
	for (auto s:statements)
		delete s;
}

void BasicBlock::addSuccessor(BasicBlock* pSucc) {
	succ.push_back(pSucc);
	pSucc->pred.push_back(this);
}


struct Label {
	unsigned int address;
	Block* pBlock = nullptr;
	Label(unsigned int offset) : address(offset) {}
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

struct Stack {
	std::vector<Value> values;
	std::vector<unsigned int> stackHeights;
	Stack() : stackHeights({0}) {}

	unsigned int size() { return values.size(); }
	bool empty() { return size() == 0; }

	// Caller needs to check for emptiness
	Value& back() { return values.back(); }

	unsigned int& height() { return stackHeights.back(); }
	void openFrame() { stackHeights.push_back(0); }
	void closeFrame() { 
		values.erase(values.end() - height(), values.end());
		stackHeights.pop_back();
	}


	Value pop();
	void push(ValueExpr* pValue);
};

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

// Takes ownership of raw pointer
void Stack::push(ValueExpr* pValue) {
	values.emplace_back(pValue);
	height()++;

	if (back() == nullptr) {
		Logger::Error() << "Nullptr placed onto stack.\n";
		throw std::logic_error("NULL on stack.");
	}
}

struct ProgBranch {
	Block* pBlock;
	Stack stack;
	ProgBranch(Block* pBlock) : pBlock(pBlock) {}
};

typedef class BytecodeParser Parser;
class ScriptInfo;

class BytecodeParser {
	private:
		BytecodeBuffer* buf;

	public:
		std::vector<ProgBranch> toTraverse;
		unsigned int instAddress = 0;
		// operand stack
		Stack stack;

		//std::vector<CallRet> callStack;

		unsigned int getInt() {
			return buf->getInt();
		};
		unsigned char getChar() {
			return buf->getChar();
		};
		Value getArg(unsigned int type);
		//std::shared_ptr<ValueExpr> getArray(std::shared_ptr<ValueExpr> first);
		ValueExpr* getLValue(const ScriptInfo &info);

		Function getCallFunction(const ScriptInfo& info);
	public:
		BytecodeParser(std::ifstream &f, HeaderPair index);
		~BytecodeParser();

		void addBranch(Block* pBlock, Stack* saveStack = nullptr);
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
		std::vector<Value> globalVars;
		std::vector<Function> globalCommands;

	public:
		ScriptInfo(std::ifstream &f, const ScriptHeader& header, int fileIndex);
		~ScriptInfo();

		std::string getString(unsigned int index);
		Value getGlobalVar (unsigned int index) const;
		Function getCommand (unsigned int index) const;

		Block* getBlock(unsigned int address);
		Block* getBlockAtLabel(unsigned int labelIndex);

		bool checkBlock(unsigned int address);

		void readGlobalInfo(std::string filename);

		void initEntrypoints(Parser& parser);
		void structureStatements();
		void generateStatements();

		void printBlocks(std::string filename);
		void dumpCFG(std::string filename);

		bool StructureIf(Block* pBlock);
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
	if (index >= stringData.size()) {
		Logger::Warn() << "String index " << std::to_string(index) << " out of bounds.\n";
		return "";
	}
	return stringData[index];
}

Value ScriptInfo::getGlobalVar(unsigned int index) const {
	if (index & 0xFF000000) {
		throw std::logic_error("Invalid index for global var.");
		return nullptr;
	} else if (index >= globalVars.size()) {
		throw std::out_of_range("Global var index out of range.");
		return nullptr;
	}

	return Value(globalVars[index]->clone());
}

Function ScriptInfo::getCommand(unsigned int index) const {
	if (index & 0xFF000000)
		return Function("ERROR");

	if (index >= globalCommands.size()) {
		Logger::Error() << "Command index " << std::to_string(index) << " is out of range.\n";
		return Function("ERROR");
	}

	return globalCommands[index];
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

		globalVars.push_back(make_unique<VarValueExpr>(name, type, length));
	}
	Logger::Info() << "Read " << std::to_string(count) << " global variables.\n";

	// Global commands
	globalInfoFile.read((char*) &count, 4);
	globalCommands.reserve(count);
	unsigned int address, fileIndex;
	for (unsigned int i = 0; i < count; i++) {
		globalInfoFile.read((char*) &address, 4);
		globalInfoFile.read((char*) &fileIndex, 4); // careful about signedness here - should be fine mostly
		if (fileIndex >= sceneNames.size()) {
			Logger::Error() << "Command " << std::to_string(i) << " referencing non-existent file index " << std::to_string(fileIndex) << std::endl;
			throw std::exception();
		}
		std::getline(globalInfoFile, name, '\0');
		
		globalCommands.push_back(Function(name, address, fileIndex));
	}
	Logger::Info() << "Read " << std::to_string(count) << " global commands.\n";

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

bool ScriptInfo::StructureIf(Block* pBlock) {
	if (pBlock->blockType != Block::TWOWAY) {
		return false;
	}
	Logger::VVDebug() << std::to_string(pBlock->index) << " has succ size " << std::to_string(pBlock->succ.size()) << std::endl;
	if (pBlock->succ.size() != 2)
		throw std::logic_error("Fake TWOWAY");

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
	Block* final = trueBlock->succ.at(0);

	// watch out for this, things might change
	Value* ppCond = pBlock->statements.back()->getChild();
	if (ppCond == nullptr) {
		Logger::Error() << "If-else condition is null.\n";
		return false;
	}

	// Remove the last (jump) statements
	delete trueBlock->statements.back();
	trueBlock->statements.pop_back();
	// Move one to the block to save
	Statement* jumpPointer = falseBlock->statements.back();
	falseBlock->statements.pop_back();

	IfStatement* pIf;
	// If false block only has one statement (not including the jump)
	if (falseBlock->statements.size() == 1) {
		pIf = falseBlock->statements.back()->makeIf(std::move(*ppCond), trueBlock->statements);
		trueBlock->statements.clear();
		falseBlock->statements.pop_back();
	} else {
		pIf = new IfStatement(std::move(*ppCond), trueBlock->statements, falseBlock->statements);
		trueBlock->statements.clear();
		falseBlock->statements.clear();
	}
	/* Ahem
	 The last statement (which should contain the last expression which is a conditional jump) is destroyed,
	 destroying the jump expression, which by now has its condition moved out into the if statement.
	 This condition will be destroyed by the if statement which is destroyed when the vector is cleared. */

	// Remove the if-goto
	delete pBlock->statements.back();	// careful careful
	pBlock->statements.pop_back();
	// Push the if statement
	pBlock->statements.push_back(pIf);
	/* Every statement in both blocks have been moved out, and they are not referenced anywhere else 
	   except maybe as a subroutine, so should probably watch out for that. */
	
	// Replace successor with single target
	pBlock->succ.pop_back();
	pBlock->succ.pop_back();
	pBlock->addSuccessor(final);

	// Remove true/false blocks from control flow
	trueBlock->succ.pop_back();
	falseBlock->succ.pop_back();
	final->pred.erase(std::remove(final->pred.begin(), final->pred.end(), trueBlock), final->pred.end()); 
	final->pred.erase(std::remove(final->pred.begin(), final->pred.end(), falseBlock), final->pred.end());

	// Transfer calls
	pBlock->calls.insert(pBlock->calls.end(), trueBlock->calls.begin(), trueBlock->calls.end());
	pBlock->calls.insert(pBlock->calls.end(), falseBlock->calls.begin(), falseBlock->calls.end());
	trueBlock->calls.clear();
	falseBlock->calls.clear();

	// Add jump instruction to target
	pBlock->statements.push_back(jumpPointer);

	// Change the type of block (not a two way anymore)
	pBlock->blockType = Block::ONEWAY;

	// delete true and false blocks
	// but not yet
	trueBlock->pred.pop_back();
	falseBlock->pred.pop_back();
	return true;
}

void ScriptInfo::structureStatements() {
	Logger::Info() << "Structuring if else statements.\n";
	// Structure ifs
	bool changed = true;
	while (changed) {
		changed = false;
		for (auto &pBlock:blocks) {
			if (StructureIf(pBlock)) {
				changed = true;
				break;
			}
		}
	}

	// Turn ifs into switches
	//for (auto &pBlock:blocks) {
	//	if (!pBlock->statements.empty()) {
	//		pBlock->statements.back()->foldSwitch();
	//	}
	//}

	// Simplify blocks
	for (auto &pBlock:blocks) {
		if (pBlock->succ.size() == 1) {
			Block* pSucc = pBlock->succ[0];
			if (pSucc->pred.size() == 1) {
				// Delete jump statement
				delete pBlock->statements.back();
				pBlock->statements.pop_back();
				// Update successors
				pBlock->blockType = pSucc->blockType;
				pBlock->succ.pop_back();
				pSucc->pred.pop_back();

				pBlock->succ.insert(pBlock->succ.end(), pSucc->succ.begin(), pSucc->succ.end());
				pSucc->succ.clear();
				// Move statements
				pBlock->statements.reserve(pBlock->statements.size() + pSucc->statements.size());
				pBlock->statements.insert(pBlock->statements.end(), pSucc->statements.begin(), pSucc->statements.end());
				pSucc->statements.clear();
			}
		}
	}
}

void ScriptInfo::generateStatements() {
	for (auto &pBlock:blocks) {
		for (auto &pExpr:pBlock->expressions){
			pBlock->statements.push_back(new Statement(pExpr));
		}
	}

	structureStatements();
}

void ScriptInfo::printBlocks(std::string filename) {
	std::ofstream out(filename);
	for (auto &pBlock:blocks) {
		if (pBlock->statements.size() == 0)
			continue;

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
	out << "\tnode [shape=box, fixedsize=true, fontsize=8, labelloc=\"t\"]\n";
	for (const auto &block:blocks) {
		if (block->statements.size() == 0)
			continue;

		std::string props;
		if (block->isEntrypoint)
			props += "penwidth=2,";
		if (block->isFunction)
			props += "color=red,";
		props += "height=" + std::to_string(block->statements.size()*0.05);

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

	out << "\tExit [penwidth=2]\n";

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
		std::cerr << "Error: 0x" << toHex(parser.instAddress) << " " << e.what() << std::endl;
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
}

BytecodeParser::~BytecodeParser() {
	delete buf;
}


void BytecodeParser::addBranch(Block* pBlock, Stack* saveStack) {
	if (pBlock == nullptr)
		throw std::logic_error("Null block");
	if (pBlock->parsed)
		return;
	for (const auto &branch:toTraverse) {
		// Operate under assumption block pointers will be unique
		if (branch.pBlock == pBlock)
			return;
	}

	toTraverse.push_back(ProgBranch(pBlock));

	if (saveStack != nullptr) {
		ProgBranch& branch = toTraverse.back();
		branch.stack.stackHeights = saveStack->stackHeights;
		for (const auto& value:saveStack->values) {
			branch.stack.values.emplace_back(value->clone());
		}
	}
}

Value BytecodeParser::getArg(unsigned int type) {
	if (stack.empty())
		throw std::out_of_range("Popping arguments off an empty stack.");

	unsigned int actualType = stack.back()->getType();
	if (type == actualType) {
		return stack.pop();
	}

	if (type == ValueType::INT) {
		Logger::Error(instAddress) << "Cannot get argument of type int with type " << VarType(actualType) << std::endl;
		return stack.pop();
	} else if (type == ValueType::STR) {
		Logger::Error(instAddress) << "Cannot get argument of type str with type " << VarType(actualType) << std::endl;
		return stack.pop();
	} else if (type == ValueType::OBJ_STR) {
		//Value pIndex = stack.pop();
		//if (!stack.pop()->isIndexer()) {
		//	Logger::Error(instAddress) << "Unexpected item.\n";
		//	return std::make_shared<ErrValueExpr>();
		//}
		//std::shared_ptr<ValueExpr> pArray = getArray(stack.pop());
		//stackHeights.pop_back();
		//return std::make_shared<IndexValueExpr>(pArray, pIndex);
	}

	return make_unique<ErrValueExpr>();
}

ValueExpr* BytecodeParser::getLValue(const ScriptInfo &info) {
	if (stack.stackHeights.size() <= 1) {
		Logger::Error(instAddress) << "No frames to close!\n";
		return nullptr;
	}
	auto frame = stack.values.end() - stack.height();
	auto curr = frame;

	Value pCurr, pLast;
	bool localVar = false, indexing = false;
	std::string localString = "g_";
	while (curr != stack.values.end()) {
		pCurr = std::move(*curr);
		if (pCurr->getType() != ValueType::INT)
			throw std::logic_error("Non int encountered when getting LValue.");

		switch(pCurr->getIntType()) {
			case IntegerLocalRef:
				localVar = true;
				localString = "loc_" + pCurr->print(true) + "_";
			break;
			case IntegerSimple: {
				if (indexing) {
					indexing = false;
					pLast = make_unique<IndexValueExpr>(std::move(pLast), std::move(pCurr));
					Logger::VVDebug(instAddress) << "Created index reference " << pLast->print(true) << "\n";
				} else {
					pLast = make_unique<VarValueExpr>(localString + pCurr->print(true), ValueType::INTLIST, 1);
					Logger::VVDebug(instAddress) << "Created array " << pLast->print(true) << "\n";
				}

			} break;
			case IntegerLocalVar: {
				if (!localVar)
					Logger::Warn(instAddress) << "Getting local var without local reference.\n";

				if (pLast != nullptr)	Logger::Warn(instAddress) << "Overwriting variable.\n";
				pLast = make_unique<VarValueExpr>("A local var goes here", ValueType::INT, 0);
			} break;
			case IntegerGlobalVar: {
				if (localVar)
					Logger::Warn(instAddress) << "Getting global var with local reference.\n";

				if (pLast != nullptr)	Logger::Warn(instAddress) << "Overwriting variable.\n";
				pLast = info.getGlobalVar(pCurr->getIndex());
			} break;
			case IntegerIndexer:
				indexing = true;
			break;
			default:
				throw std::logic_error("Unexpected integer type.");
		}

		curr++;
	}

	if (pLast == nullptr) {
		throw std::logic_error("Something went horribly wrong.");
	}


	if (!pLast->isLValue())
		Logger::Error(instAddress) << "Could not get lvalue!\n";

	stack.closeFrame();
	return pLast->clone();
}

Function BytecodeParser::getCallFunction(const ScriptInfo& info) {
	if (stack.stackHeights.size() <= 1) {
		Logger::Error(instAddress) << "No frames to close!\n";
	}
	if (stack.height() == 0) {
		Logger::Error(instAddress) << "Empty function!\n";
		return Function("err");
	} else if (stack.height() == 1) {
		Value pCall = stack.pop();
		stack.closeFrame();

		if (pCall->getIntType() == IntegerFunction) {
			return info.getCommand(pCall->getIndex());
		}
		// else
		Function fn("FN_" + pCall->print(true));

		// Play voice
		if (pCall->getIndex() == 0x12)
			fn.hasExtra = true;
		else if (pCall->getIndex() == 0x54)
			fn.pushRet = false;

		return fn;
	}
	auto frame = stack.values.end() - stack.height();
	auto curr = frame;
	Value pCurr;
	std::string name = "FN";
	while (curr != stack.values.end()) {
		pCurr = std::move(*curr);
		name += "_{" + pCurr->print(true) + "}";
		curr++;
	}
	return Function(name);
}

void BytecodeParser::parse(ScriptInfo& info) {
	while (!toTraverse.empty()) {
		ProgBranch branch = std::move(toTraverse.back());
		toTraverse.pop_back();

		Block* pBlock = branch.pBlock;
		buf->setAddress(pBlock->startAddress);
		stack = std::move(branch.stack);
		pBlock->parsed = true;

		Logger::Debug(instAddress) << "Parsing new branch - starting at block " << std::to_string(pBlock->index) << std::endl;

		unsigned char opcode;
		Expression* pExpr;
		bool newBlock = false;

		int lineNum = -1;
		bool attachLineNum = false;
		while (!buf->done()) {
			instAddress = buf->getAddress();
			opcode = getChar();

			pExpr = nullptr;
			switch (opcode) {
				case 0x01: {
					lineNum = getInt();
					attachLineNum = true;
				} break;
				case 0x02: {
					unsigned int type = getInt();
					unsigned int value = getInt();
					if (type == ValueType::STR) {
						std::string str = info.getString(value);
						stack.push(new RawValueExpr(str, value));
					} else {
						stack.push(new RawValueExpr(type, value));
					}
				} break; 
				case 0x03: {
					unsigned int type = getInt();
					Value back = stack.pop();
					if (back->getType() != type) {
						Logger::Error(instAddress) << "Expected type " << VarType(type) << ", got type " << VarType(back->getType()) << std::endl;
					}
				} break;
				case 0x04: {
					unsigned int type = getInt();
					if (stack.empty()) {
						Logger::Error(instAddress) << "Duplicating empty stack.\n";
						break;
					}
					Value& pValue = stack.back();
					if (pValue->getType() != type) {
						Logger::Error(instAddress) << "Dup - Expected type " << VarType(type) << ", got type " << VarType(pValue->getType()) << std::endl;
						break;
					}

					// Move the value into a variable if it has side effects
					if (pValue->hasSideEffect()) {
						ValueExpr* pVar = new VarValueExpr(pValue->getType());
						pExpr = new AssignExpr(Value(pVar), stack.pop());
						stack.push(pVar->clone());
					} else {
						stack.push(pValue->clone());
					}
				} break;
				case 0x05: {
					stack.push(getLValue(info)->toRValue());
				} break;
				case 0x06: {
					unsigned int count = stack.height();
					stack.openFrame();
					for (unsigned int i = 0; i < count; i++) {
						stack.push(stack.values.at(stack.size() - count)->clone());
					}
				} break; 
				case 0x08: {
					stack.openFrame();
				} break; 
				case 0x10: {
					unsigned int labelIndex = getInt();
					Block* pJumpBlock = info.getBlockAtLabel(labelIndex);

					pExpr = new JumpExpr(pJumpBlock->index);

					pBlock->nextAddress = buf->getAddress();
					buf->setAddress(pJumpBlock->startAddress);
					pBlock->blockType = Block::ONEWAY;
					newBlock = true;
				} break; 
				case 0x11: 
				case 0x12: {
					unsigned int labelIndex = getInt();
					Block* pJumpBlock = info.getBlockAtLabel(labelIndex);

					// Pop the condition
					Value condition = stack.pop();

					// Store stack after condition has been popped
					addBranch(pJumpBlock, &stack);

					pBlock->addSuccessor(pJumpBlock);


					Block* pNextBlock = info.getBlock(buf->getAddress());
					if (opcode == 0x11) {
						condition->negateBool();
					}
					
					pExpr = new JumpExpr(pJumpBlock->index, pNextBlock->index, std::move(condition));

					pBlock->nextAddress = buf->getAddress();
					pBlock->blockType = Block::TWOWAY;
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

					std::vector<Value> args;
					for (auto &type:argTypes) {
						args.push_back(getArg(type));
					}

					addBranch(pCallBlock, &stack);


					ShortCallExpr* pCall = new ShortCallExpr(pCallBlock->index, std::move(args));
					stack.push(pCall->clone());
					pExpr = pCall;
				} break;
				case 0x15: {
					unsigned int numArgs = getInt();
					std::vector<unsigned int> retTypes;
					for (unsigned int i = 0; i < numArgs; i++) {
						retTypes.push_back(getInt());
					}

					std::vector<Value> ret;
					for (auto &type:retTypes) {
						ret.push_back(getArg(type));
					}

					pExpr = new RetExpr(std::move(ret));

					pBlock->nextAddress = buf->getAddress();
					pBlock->blockType = Block::RET;
					newBlock = true;

					if (!stack.empty()) {
						Logger::Error(instAddress) << "Stack is not empty! (" << std::to_string(stack.size()) << ")\n";
						for (auto &value:stack.values) {
							Logger::Debug() << value->print() << "\n";
						}
					}

				} break; 
				// case 0x16:
				case 0x20: {
					unsigned int lType = getInt();
					unsigned int rType = getInt();
					unsigned int unknown = getInt();
					if (unknown != 1) {
						Logger::Warn(instAddress) << "Assigning with " << std::to_string(unknown) << std::endl;
					}

					Value rhs = stack.pop();
					Value lhs = Value(getLValue(info));

					if (lhs->getType() != lType) {
						Logger::Error(instAddress) << "Assign - Expected type " << VarType(lType) << ", got type " << VarType(lhs->getType()) << std::endl;
						lhs = make_unique<ErrValueExpr>();
					}
					if (rhs->getType() != rType) {
						Logger::Error(instAddress) << "Assign - Expected type " << VarType(rType) << ", got type " << VarType(rhs->getType()) << std::endl;
						rhs = make_unique<ErrValueExpr>();
					}

					Logger::VVDebug(instAddress) << "Assign: " << VarType(lType) << " <- " << VarType(rType) << std::endl;

					pExpr = new AssignExpr(std::move(lhs), std::move(rhs));
				} break;
				case 0x22: {
					unsigned int lhsType = getInt();
					unsigned int rhsType = getInt();
					unsigned int op = getChar();

					Value rhs = stack.pop();
					Value lhs = stack.pop();
					if (lhs->getType() != lhsType) {
						Logger::Error(instAddress) << "Calc - Expected type " << VarType(lhsType) << ", got type " << VarType(lhs->getType()) << std::endl;
						lhs = make_unique<ErrValueExpr>();
					}
					if (rhs->getType() != rhsType) {
						Logger::Error(instAddress) << "Calc - Expected type " << VarType(rhsType) << ", got type " << VarType(rhs->getType()) << std::endl;
						rhs = make_unique<ErrValueExpr>();
					}

					stack.push(new BinaryValueExpr(std::move(lhs), std::move(rhs), op));
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

					std::vector<Value> args;
					for (auto &type:argTypes) {
						args.push_back(getArg(type));
					}

					// Reversed though
					Function fn = getCallFunction(info);

					CallExpr* pCall = new CallExpr(fn, option, std::move(args), extraList, returnType);

					if (fn.hasExtra)
						fn.extraThing(getInt());

					pExpr = pCall;
					
					if (fn.pushRet)
						stack.push(pCall->clone());

				} break;
				case 0x31: {
					unsigned int id = getInt();
					Value pText = stack.pop();
					if (pText->getType() != ValueType::STR) {
						Logger::Error(instAddress) << pText->print() << "(" << VarType(pText->getType()) << ") cannot be used to add text.\n";
					}
					pExpr = new AddTextExpr(std::move(pText), id);
				} break;
				case 0x32: {
					Value pName = stack.pop();
					if (pName->getType() != ValueType::STR) {
						Logger::Error(instAddress) << pName->print() << "(" << VarType(pName->getType()) << ") cannot be used to set name.\n";
					}
					pExpr = new SetNameExpr(std::move(pName));
				} break;
				default: {
					Logger::Error(instAddress) << "NOP: 0x" << toHex(opcode, 2) << std::endl;
				}
			}

			if (pExpr != nullptr) {
				pBlock->expressions.push_back(pExpr);
				if (attachLineNum) {
					pBlock->expressions.back()->setLineNum(lineNum);
					attachLineNum = false;
				}
			}


			if (newBlock || info.checkBlock(buf->getAddress())) {
				// if i get the block here, I can dump dead code
				// it's probably mostly line numbers though

				if (opcode == 0x15) {
					if (stack.values.size() > 0)
						Logger::Warn(instAddress) << "Stack size is positive.\n";
					break;
				}

				Block* pNextBlock = info.getBlock(buf->getAddress());

				// Add successor/predecessor no matter what
				pBlock->addSuccessor(pNextBlock);

				if (!newBlock) {
					pBlock->blockType = Block::FALL;
					pBlock->expressions.push_back(new JumpExpr(pNextBlock->index));
				}

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