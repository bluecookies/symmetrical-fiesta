#define __USE_MINGW_ANSI_STDIO 0

#include <iomanip>
#include <fstream>
#include <sstream>
#include <vector>
#include <map>
#include <algorithm>
#include <memory>
#include <bitset>

#include <cassert>
#include <unistd.h>
#include <getopt.h>

#include "Helper.h"
#include "Logger.h"
#include "Structs.h"

#include "BytecodeParser.h"
#include "Statements.h"
#include "ControlFlow.h"

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

class ScriptInfo {
	private:
		std::vector<Label> labels, entrypoints, functions;
		std::vector<unsigned int> globalFunctionDefinitions;
	private:
		int fileIndex;
		StringList sceneNames;
		StringList functionNames;

		StringList stringData;
		StringList localVarNames;
		std::vector<Value> globalVars;
		std::vector<Function> globalCommands;
		std::vector<Function> localCommands;

		void readLocalCommands(std::ifstream&, const HeaderPair&);
	public:
		ScriptInfo(std::ifstream &f, const ScriptHeader& header, int fileIndex, std::string filename);

		std::string getString(unsigned int index) const;
		std::string getLocalVarName(unsigned int index) const;
		Value getGlobalVar(unsigned int index) const;
		std::string getCommand(unsigned int index) const;

		void readGlobalInfo(std::string filename);

		std::vector<unsigned int> getEntrypoints();
		std::vector<Function> getFunctionAddresses();
		unsigned int getLabelAddress(unsigned int labelIndex);
		bool isLabelled(unsigned int address);
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
	readHeaderPair(f, header.localCommandIndex);	// Function index

	readHeaderPair(f, header.staticVarTypes);
	readHeaderPair(f, header.staticVarIndex);    
	readHeaderPair(f, header.staticVarNames);
	
	// Static functions
	readHeaderPair(f, header.functions);
	readHeaderPair(f, header.functionNameIndex);
	readHeaderPair(f, header.functionNames);
	
	readHeaderPair(f, header.localVarIndex);
	readHeaderPair(f, header.localVarNames);
	
	readHeaderPair(f, header.unknown6);
	readHeaderPair(f, header.unknown7);

	assert(header.functions.count == header.functionNameIndex.count);
}

void readLabels(std::ifstream &stream, std::vector<Label> &labels, const HeaderPair& index) {
	labels.reserve(index.count);

	unsigned int offset;
	stream.seekg(index.offset, std::ios_base::beg);
	for (unsigned int i = 0; i < index.count; i++) {
		stream.read((char*) &offset, 4);
		labels.push_back(Label(offset));
	}
}


std::string ScriptInfo::getString(unsigned int index) const {
	if (index >= stringData.size()) {
		Logger::Warn() << "String index " << std::to_string(index) << " out of bounds.\n";
		return "";
	}
	return stringData[index];
}

std::string ScriptInfo::getLocalVarName(unsigned int index) const {
	if (index >= localVarNames.size()) {
		Logger::Warn() << "Var name index " << std::to_string(index) << " out of bounds.\n";
		return "VAR_" + std::to_string(index);
	}
	return localVarNames[index];
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

std::string ScriptInfo::getCommand(unsigned int index) const {
	if (index & 0xFF000000)
		return "ERROR";

	if (index >= globalCommands.size()) {
		//index -= globalCommands.size();
		//if (index >= localCommands.size()) {
		//	Logger::Error() << "Command index " << std::to_string(index) << " is out of range.\n";
		//	return "ERROR";
		//}

		//return localCommands[index].name;
		auto funcIt = std::find_if(localCommands.begin(), localCommands.end(), [index](Function func) {
			return (func.index == index);
		});

		if (funcIt == localCommands.end())
			return "ERROR";

		return funcIt->name;
	}

	return globalCommands[index].name;
}

ScriptInfo::ScriptInfo(std::ifstream &stream, const ScriptHeader &header, int index, std::string filename) : fileIndex(index) {

	readLabels(stream, labels, header.labels);
	readLabels(stream, entrypoints, header.entrypoints);
	readLabels(stream, functions, header.functions);
	Logger::Info() << "Found " << std::to_string(functions.size()) << " functions.\n";

	// Needs to be done before reading local commands but after reading functions
	readGlobalInfo(filename);

	readStrings(stream, stringData, header.stringIndex, header.stringData, true);
	readStrings(stream, localVarNames, header.localVarIndex, header.localVarNames);
	readStrings(stream, functionNames, header.functionNameIndex, header.functionNames);
	readLocalCommands(stream, header.localCommandIndex);
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
		
		//globalCommands.push_back(Command(name, address, fileIndex));
		globalCommands.emplace_back(name, address, globalCommands.size());
	}
	Logger::Info() << "Read " << std::to_string(count) << " global commands.\n";

	globalInfoFile.close();
}

void ScriptInfo::readLocalCommands(std::ifstream& stream, const HeaderPair& pairIndex) {
	unsigned int numCommands = pairIndex.count;
	unsigned int numGlobalCommands = globalCommands.size();

	unsigned int commandIndex, commandOffset;
	stream.seekg(pairIndex.offset, std::ios::beg);
	for (unsigned int i = 0; i < numCommands; i++) {
		stream.read((char*) &commandIndex, 4);
		stream.read((char*) &commandOffset, 4);

		 if (commandIndex >= numGlobalCommands) {
			// Static function

			auto predAtOffset = [commandOffset](Label function) {
				return (function.address == commandOffset);
			};

			unsigned int fnIndex = std::find_if(functions.begin(), functions.end(), predAtOffset) - functions.begin();
			if (fnIndex != functions.size()) {
				localCommands.emplace_back(functionNames.at(fnIndex), commandOffset, commandIndex);
				Logger::Debug() << "Local command index " << std::to_string(commandIndex + numGlobalCommands) << " in range, it was " << functionNames.at(fnIndex) <<  "\n";
			} else {
				Logger::Error() << "Command " << std::to_string(commandIndex) << " at 0x" << toHex(commandOffset) << " not found.\n";
			}
		} else {
			// it is a global function
			globalFunctionDefinitions.push_back(commandIndex);
		}
	}

	Logger::Info() << "Read " << std::to_string(numCommands) << " commands.\n";
}

std::vector<unsigned int> ScriptInfo::getEntrypoints() {
	std::vector<unsigned int> addresses;
	for (const auto& ep:entrypoints)
		addresses.push_back(ep.address);

	return addresses;
}

std::vector<Function> ScriptInfo::getFunctionAddresses() {
	std::vector<Function> fns;
	for (const auto& index:globalFunctionDefinitions) {
		fns.push_back(globalCommands.at(index));
	}

	fns.insert(fns.end(), localCommands.begin(), localCommands.end());

	return fns;
}

unsigned int ScriptInfo::getLabelAddress(unsigned int labelIndex) {
	if (labelIndex >= labels.size()) {
		throw std::out_of_range("Label index out of range.");
	}
	return labels[labelIndex].address;
}

bool ScriptInfo::isLabelled(unsigned int address) {
	auto pLabel = std::find_if(labels.begin(), labels.end(), [address](Label label) {
		return label.address == address;
	});

	return (pLabel != labels.end());
}



int Logger::LogLevel = Logger::LEVEL_INFO;
int main(int argc, char* argv[]) {
	extern char *optarg;
	extern int optind;
	
	std::string outFilename;
	static char usageString[] = "Usage: decompiless [-o outfile] [-v] [-i file index] [-d] <input.ss>";

	int fileIndex = -1;
	bool dumpAsm = false;
	// Handle options
	int option = 0;
	while ((option = getopt(argc, argv, "o:vi:d")) != -1) {
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
		case 'd':
			dumpAsm = true;
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
		outFilename = filename + ".src";
	
	// Start reading stuff

	// Read script header
	ScriptHeader header;
	readScriptHeader(fileStream, header);

	BytecodeParser parser(fileStream, header.bytecode);

	ScriptInfo info(fileStream, header, fileIndex, filename);
	fileStream.close();

	ControlFlowGraph cfg(parser, info.getEntrypoints(), info.getFunctionAddresses());
	
	try {
		if (dumpAsm)
			parser.parse(info, cfg, filename + ".asm");
		else
			parser.parse(info, cfg);
	} catch(std::logic_error &e) {
		std::cerr << "Error: 0x" << toHex(parser.instAddress) << " " << e.what() << std::endl;
	}

	cfg.structureStatements();

	cfg.printBlocks(outFilename);

	cfg.dumpGraph(filename + ".gv");
		
	// TODO: handle these
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

	unsigned int ll = index.count;
	unsigned char w = 1;
	while (ll > 0) {
		ll /= 0x10;
		w++;
	}
	addressWidth = (w / 2) * 2;
}

BytecodeParser::~BytecodeParser() {
	delete buf;
}

struct ProgBranch {
	Block* pBlock;
	Stack stack;
	ProgBranch(Block* pBlock) : pBlock(pBlock) {}
};


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

Value BytecodeParser::getArg(unsigned int type, const ScriptInfo &info) {
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
		return Value(getLValue(info));
	}

	return make_unique<ErrValueExpr>(VarType(type));
}

ValueExpr* BytecodeParser::getLValue(const ScriptInfo &info) {
	if (stack.stackHeights.size() <= 1) {
		return new ErrValueExpr("No frames to close!", instAddress);
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
					if (pLast == nullptr) {
						unsigned int type;
						if (localVar) {
							switch (pCurr->getIndex()) {
								case 0x00: type = ValueType::INTLIST; break;
								case 0x01: type = ValueType::STRLIST; break;
								case 0x02: type = ValueType::OBJLIST; break;
								default: type = ValueType::INTLIST; break;
							}
						} else {
							// Engine specific here
							type = ValueType::INTLIST;
						}
						pLast = make_unique<VarValueExpr>(localString + pCurr->print(true), type, 1);
						Logger::VVDebug(instAddress) << "Created array " << pLast->print(true) << "\n";
					} else {
						pLast = make_unique<MemberExpr>(std::move(pLast), std::move(pCurr));
						Logger::VVDebug(instAddress) << "Created member access " << pLast->print(true) << "\n";
					}
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

	ValueExpr* ret = pLast->clone(); // think about whether this is needed, or maybe I could just release/pass directly back (i forget how this is used)
	stack.closeFrame();
	return ret;
}

FunctionExpr* BytecodeParser::getCallFunction(const ScriptInfo& info) {
	if (stack.stackHeights.size() <= 1) {
		Logger::Error(instAddress) << "No frames to close!\n";
	}
	if (stack.height() == 0) {
		Logger::Error(instAddress) << "Empty function!\n";
		return new FunctionExpr("FN_ERROR");
	} else if (stack.height() == 1) {
		Value pCall = stack.pop();
		stack.closeFrame();

		if (pCall->getIntType() == IntegerFunction) {
			return new FunctionExpr(info.getCommand(pCall->getIndex()));
		}
		// else
		FunctionExpr* fn = new FunctionExpr("FN_" + pCall->print(true));

		// Unknown/Play voice/Select choice
		unsigned int index = pCall->getIndex();
		if (index == 0xc || index == 0x12 || index == 0x4c)
			fn->hasExtra = true;
		else if (index == 0x54)
			fn->pushRet = false;

		return fn;
	}

	/* auto frame = stack.values.end() - stack.height();
	auto curr = frame;
	Value pCurr;
	std::string name = "FN";
	while (curr != stack.values.end()) {
		pCurr = std::move(*curr);
		name += "_{" + pCurr->print(true) + "}";
		curr++;
	}
	stack.closeFrame(); */
	return new FunctionExpr(Value(getLValue(info)));
}

void BytecodeParser::parse(ScriptInfo& info, ControlFlowGraph& cfg, std::string asmFile) {
	std::string asmLine;
	bool dumpAsm = !asmFile.empty();
	std::map<unsigned int, std::string> asmLines;

	unsigned int numParams = 0;
	std::vector<Value> localVars;
	while (!toTraverse.empty()) {
		ProgBranch branch = std::move(toTraverse.back());
		toTraverse.pop_back();

		Block* pBlock = branch.pBlock;
		if (pBlock->parsed)
			continue;

		buf->setAddress(pBlock->startAddress);
		stack = std::move(branch.stack);
		pBlock->parsed = true;

		if (pBlock->isFunction) {
			numParams = 0;
			localVars.clear();
		}

		Logger::VVDebug(instAddress) << "Parsing new branch - starting at block " << std::to_string(pBlock->index) << std::endl;

		unsigned char opcode;
		Statement* pStatement;
		bool newBlock = false;

		while (!buf->done()) {
			instAddress = buf->getAddress();
			opcode = getChar();

			pStatement = nullptr;
			asmLine = "";
			switch (opcode) {
				case 0x01: {
					unsigned int lineNum = getInt();
					pStatement = new LineNumStatement(lineNum);
					asmLine = "line " + std::to_string(lineNum);
				} break;
				case 0x02: {
					unsigned int type = getInt();
					unsigned int value = getInt();

					asmLine = "push " + VarType(type);
					if (type == ValueType::STR) {
						std::string str = info.getString(value);
						stack.push(new RawValueExpr(str, value));
						asmLine += " \"" + str + "\"";
					} else {
						stack.push(new RawValueExpr(type, value));
						asmLine += " 0x" + toHex(value);
					}
				} break; 
				case 0x03: {
					unsigned int type = getInt();
					Value back = stack.pop();
					if (back->getType() != type) {
						Logger::Error(instAddress) << "Expected type " << VarType(type) << ", got type " << VarType(back->getType()) << std::endl;
					}
					// Turn into a statement if it has side effects
					if (back->hasSideEffect()) {
						pStatement = new ExpressionStatement(back.release());
					}

					asmLine = "pop " + VarType(type);
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
						//break;	// TODO: this crashes if i remove with a bug - examine the bug
					}

					// Move the value into a variable if it has side effects
					if (pValue->hasSideEffect()) {
						ValueExpr* pVar = new VarValueExpr(pValue->getType());
						stack.push(pVar->clone());
						pStatement = new AssignStatement(Value(pVar), stack.pop());
					} else {
						stack.push(pValue->clone());
					}

					asmLine = "dup " + VarType(type);
				} break;
				case 0x05: {
					stack.push(getLValue(info)->toRValue());
					asmLine = "eval";
				} break;
				case 0x06: {
					unsigned int count = stack.height();
					stack.openFrame();
					for (unsigned int i = 0; i < count; i++) {
						stack.push(stack.values.at(stack.size() - count)->clone());
					}
					asmLine = "rep";
				} break; 
				case 0x07: {
					unsigned int type = getInt();
					std::string name = info.getLocalVarName(getInt());

					localVars.push_back(make_unique<VarValueExpr>(name, type, 0));

					asmLine = "param " + VarType(type) + " " + name;
				} break;
				case 0x08: {
					stack.openFrame();
					asmLine = "frame";
				} break; 
				case 0x09: {
					numParams = localVars.size();
					asmLine = "endparams";
				} break;
				case 0x10: {
					unsigned int labelIndex = getInt();
					Block* pJumpBlock = cfg.getBlock(info.getLabelAddress(labelIndex));

					pStatement = new GotoStatement(pJumpBlock->index);

					pBlock->nextAddress = buf->getAddress();
					buf->setAddress(pJumpBlock->startAddress);
					newBlock = true;

					asmLine = "jmp 0x" + toHex(buf->getAddress(), addressWidth); 
				} break; 
				case 0x11: 
				case 0x12: {
					unsigned int labelIndex = getInt();
					Block* pJumpBlock = cfg.getBlock(info.getLabelAddress(labelIndex));

					// Pop the condition
					Value condition = stack.pop();

					// Store stack after condition has been popped
					addBranch(pJumpBlock, &stack);

					pBlock->addSuccessor(pJumpBlock);


					Block* pNextBlock = cfg.getBlock(buf->getAddress());
					if (opcode == 0x11) {
						negateCondition(condition);
						asmLine = "jz 0x" + toHex(pJumpBlock->startAddress, addressWidth); 
					} else {
						asmLine = "jnz 0x" + toHex(pJumpBlock->startAddress, addressWidth); 
					}
					
					pStatement = new BranchStatement(pJumpBlock->index, pNextBlock->index, std::move(condition));

					pBlock->nextAddress = buf->getAddress();
					newBlock = true;
				} break;
				case 0x13: {
					unsigned int labelIndex = getInt();
					Block* pCallBlock = cfg.getBlock(info.getLabelAddress(labelIndex));
					pCallBlock->isFunction = true;
					pBlock->calls.push_back(pCallBlock);

					unsigned int numArgs = getInt();
					std::vector<unsigned int> argTypes;
					for (unsigned int i = 0; i < numArgs; i++) {
						argTypes.push_back(getInt());
					}

					std::vector<Value> args;
					for (auto &type:argTypes) {
						args.push_back(getArg(type, info));
					}

					addBranch(pCallBlock, &stack);

					stack.push(new ShortCallExpr(pCallBlock->index, std::move(args)));

					asmLine = "shortcall 0x" + toHex(pCallBlock->startAddress, addressWidth);
				} break;
				case 0x15: {
					unsigned int numArgs = getInt();
					std::vector<unsigned int> retTypes;
					for (unsigned int i = 0; i < numArgs; i++) {
						retTypes.push_back(getInt());
					}

					std::vector<Value> ret;
					asmLine = "ret (";
					for (auto &type:retTypes) {
						ret.push_back(getArg(type, info));
						asmLine += VarType(type) + ", ";
					}
					asmLine += ")";

					pStatement = new ReturnStatement(std::move(ret));

					pBlock->nextAddress = buf->getAddress();
					newBlock = true;

					if (!stack.empty()) {
						Logger::Error(instAddress) << "Stack is not empty! (" << std::to_string(stack.size()) << ")\n";
						for (auto &value:stack.values) {
							Logger::Debug() << value->print() << "\n";
						}
					}


				} break; 
				// TODO: handle this properly
				case 0x16:
					Logger::Info(instAddress) << "Script end reached.\n";
					pStatement = new EndScriptStatement();
					asmLine = "endscript";
				break;
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

					pStatement = new AssignStatement(std::move(lhs), std::move(rhs));
					asmLine = "assign " + VarType(lType) + " " + VarType(rType) + " 0x" + toHex(unknown);
				} break;
				case 0x21: {
					unsigned int u1 = getInt();
					unsigned char u2 = getChar();
					pStatement = new Op21Statement(u1, u2);
					asmLine = "[21] 0x" + toHex(u1) + " " + std::to_string(u2);
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
					asmLine = "calc " + VarType(lhsType) + " " + VarType(rhsType) + " 0x" + toHex(op, 2);
				} break; 
				case 0x30: {
					unsigned int option = getInt();

					unsigned int numArgs = getInt();
					asmLine = "call " + std::to_string(option) + " (";
					std::vector<unsigned int> argTypes;
					for (unsigned int i = 0; i < numArgs; i++) {
						unsigned int type = getInt();
						argTypes.push_back(type);

						asmLine += VarType(type) + ", ";
					}
					asmLine += ") ";

					unsigned int numExtra = getInt();
					std::vector<unsigned int> extraList;
					if (numExtra > 0) {
						asmLine += "(";
						for (unsigned int i = 0; i < numExtra; i++) {
							unsigned int u = getInt();
							extraList.push_back(u);
							asmLine += "0x" + toHex(u) + ", ";
						}
						asmLine += ") ";
					}

					unsigned int returnType = getInt();
					asmLine += VarType(returnType);

					std::vector<Value> args;
					
					for (auto &type:argTypes) {
						args.push_back(getArg(type, info));

					}

					// Reversed though
					FunctionExpr* fn = getCallFunction(info);
					if (fn->hasExtra) {
						unsigned int extra = getInt();
						fn->extraThing(extra);

						asmLine += " <0x" + toHex(extra) + ">";
					}

					CallExpr* pCall = new CallExpr(fn, option, std::move(args), extraList, returnType);
					
					// Use it just one more time
					if (fn->pushRet) {
						stack.push(pCall);
					} else {
						pStatement = new ClearBufferStatement();
					}


				} break;
				case 0x31: {
					unsigned int id = getInt();
					Value pText = stack.pop();
					if (pText->getType() != ValueType::STR) {
						Logger::Error(instAddress) << pText->print() << "(" << VarType(pText->getType()) << ") cannot be used to add text.\n";
					}

					pStatement = new AddTextStatement(std::move(pText), id);
					Statement* pLast = pBlock->statements.back();
					if (pLast->type == Statement::CLEAR_BUFFER) {
						if (pLast->getLineNum() >= 0)
							pStatement->setLineNum(pLast->getLineNum());
						delete pBlock->statements.back();
						pBlock->statements.pop_back();
					} else {
						Logger::Warn(instAddress) << "No preceding 0x54 call. (is this bad?)\n";
					}

					asmLine = "addtext " + std::to_string(id);
				} break;
				case 0x32: {
					Value pName = stack.pop();
					if (pName->getType() != ValueType::STR) {
						Logger::Error(instAddress) << pName->print() << "(" << VarType(pName->getType()) << ") cannot be used to set name.\n";
					}

					pStatement = new SetNameStatement(std::move(pName));
					Statement* pLast = pBlock->statements.back();
					if (pLast->type == Statement::CLEAR_BUFFER) {
						if (pLast->getLineNum() >= 0)
							pStatement->setLineNum(pLast->getLineNum());
						delete pBlock->statements.back();
						pBlock->statements.pop_back();
					} else {
						Logger::Warn(instAddress) << "No preceding 0x54 call. (is this bad?)\n";
					}

					asmLine = "setname";
				} break;
				default: {
					Logger::Error(instAddress) << "NOP: 0x" << toHex(opcode, 2) << std::endl;
					asmLine = "[" + toHex(opcode, 2) + "]";
				}
			}
			if (dumpAsm) {
				asmLines[instAddress] = asmLine;
			}

			if (pStatement != nullptr) {
				if (pStatement->type != Statement::LINE_NUM && !pBlock->statements.empty()) {
					Statement* pLast = pBlock->statements.back();
					if (pLast->type == Statement::LINE_NUM) {
						pStatement->setLineNum(pLast->getLineNum());
						pBlock->pop();
					}
				}
				pBlock->statements.push_back(pStatement);
			}

			// Create block if label exists, otherwise just check
			Block* pNextBlock = cfg.getBlock(buf->getAddress(), info.isLabelled(buf->getAddress()));
			if (newBlock || pNextBlock) {
				// if i get the block here, I can dump dead code
				// it's probably mostly line numbers though

				if (opcode == 0x15 || opcode == 0x16) {
					if (stack.values.size() > 0)
						Logger::Warn(instAddress) << "Stack size is positive.\n";

					if (dumpAsm) {
						if (asmLines.count(buf->getAddress()) == 0)
							asmLines[buf->getAddress()] = "unmapped";
					}
					break;
				}

				if (pNextBlock == nullptr)
					pNextBlock = cfg.getBlock(buf->getAddress(), true);

				// Add successor/predecessor no matter what
				pBlock->addSuccessor(pNextBlock);

				if (!newBlock) {
					pBlock->statements.push_back(new GotoStatement(pNextBlock->index));
				}

				// Exit this branch if target is parsed
				if (pNextBlock->parsed)
					break;

				// Next block
				pBlock = pNextBlock;
				newBlock = false;

				pBlock->parsed = true;
			}
		}
	}

	if (dumpAsm) {
		std::ofstream dumpStream(asmFile);
		Logger::Info() << "Dumping assembler to " << asmFile << "\n";
		for (const auto& line:asmLines)
			dumpStream << "0x" << toHex(line.first, addressWidth) << "\t" << line.second << "\n";
	}
}

unsigned int BytecodeParser::getInt() {
	return buf->getInt();
};
unsigned char BytecodeParser::getChar() {
	return buf->getChar();
};


//
// Buffer of bytecode
//

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