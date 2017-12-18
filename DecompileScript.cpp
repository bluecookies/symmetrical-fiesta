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

std::string printArgList(const std::vector<unsigned int>& argTypes);

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
		std::vector<Value> staticVars;
		std::vector<Function> globalCommands;
		std::vector<Function> localCommands;

		void readLocalCommands(std::ifstream&, const HeaderPair&);
		void readStaticVars(std::ifstream&, const HeaderPair&, const StringList&);
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
	assert(header.staticVarTypes.count == header.staticVarIndex.count);
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
	if (index & 0xFF000000)
		return make_unique<ErrValueExpr>("Invalid index for global var.");


	if (index >= globalVars.size()) {
		index -= globalVars.size();
		if (index >= staticVars.size())
			throw std::out_of_range("Error: Global var index " + std::to_string(index) + " out of range.");
		
		return Value(staticVars[index]->clone());
	}

	return Value(globalVars[index]->clone());
}

std::string ScriptInfo::getCommand(unsigned int index) const {
	if (index & 0xFF000000)
		return "ERROR_INVALIDCOMMAND";

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
	
	// Indexed the same as global ones
	readLocalCommands(stream, header.localCommandIndex);
	StringList staticVarNames;
	readStrings(stream, staticVarNames, header.staticVarIndex, header.staticVarNames);
	readStaticVars(stream, header.staticVarTypes, staticVarNames);
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

		globalVars.push_back(make_unique<VariableExpression>(name, type, length));
	}
	Logger::Info() << "Read " << std::to_string(count) << " global variables.\n";

	// Global commands
	globalInfoFile.read((char*) &count, 4);
	globalCommands.reserve(count);
	unsigned int address, functionFile;
	for (unsigned int i = 0; i < count; i++) {
		globalInfoFile.read((char*) &address, 4);
		globalInfoFile.read((char*) &functionFile, 4); // careful about signedness here - should be fine mostly
		if (functionFile >= sceneNames.size()) {
			Logger::Error() << "Command " << std::to_string(i) << " referencing non-existent file index " << std::to_string(functionFile) << std::endl;
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

void ScriptInfo::readStaticVars(std::ifstream& stream, const HeaderPair& varTypeIndex, const StringList& varNames) {
	unsigned int numVars = varTypeIndex.count;

	unsigned int type, length;
	stream.seekg(varTypeIndex.offset, std::ios::beg);
	for (unsigned int i = 0; i < numVars; i++) {
		stream.read((char*) &type, 4);
		stream.read((char*) &length, 4);

		staticVars.push_back(make_unique<VariableExpression>(varNames.at(i), type, length));
	}

	Logger::Info() << "Read " << std::to_string(numVars) << " static variables.\n";
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
	std::ofstream outStream(outFilename);

	// TODO: implement an actual way to copy assign cfg
	ControlFlowGraph mainCFG(parser, info.getEntrypoints());
	

	std::map<unsigned int, std::string> asmLines;
	std::map<unsigned int, std::string>* pAsmLines = dumpAsm ? &asmLines : nullptr;
	try {
		parser.parse(mainCFG, info, pAsmLines);

		mainCFG.structureStatements();

		mainCFG.printBlocks(outStream);

		auto fns = info.getFunctionAddresses();

		for (const auto& fn:fns) {
			ControlFlowGraph cfg(parser, {fn.address});

			Logger::Info() << "Parsing function " << fn.name << " (0x" << toHex(fn.address, parser.addressWidth) << ")\n";
			parser.parse(cfg, info, pAsmLines);

			cfg.structureStatements();

			outStream << "\nfn " << fn.name << parser.getFunctionSignature() << " {\n";
			cfg.printBlocks(outStream, 1);
			outStream << "}\n";

		}	

	} catch (std::logic_error &e) {
		std::cerr << e.what() << std::endl;
	}

	if (dumpAsm) {
		std::ofstream dumpStream(filename + ".asm");
		Logger::Info() << "Dumping assembler to " << filename << ".asm" << "\n";
		for (const auto& line:asmLines)
			dumpStream << "0x" << toHex(line.first, parser.addressWidth) << "\t" << line.second << "\n";
	}

	//cfg.dumpGraph(filename + ".gv");
		
	// TODO: handle these
	if (header.unknown6.count != 0) {
		Logger::Warn() << "Unknown6 has " << header.unknown6.count << " elements.\n";

		int u6 = 0;
		fileStream.seekg(header.unknown6.offset, std::ios::beg);
		for (uint32_t i = 0; i < header.unknown6.count; i++) {
			fileStream.read((char*) &u6, 4);
			Logger::Debug() << std::to_string(u6) << " ";
		}
		Logger::Debug() << std::endl;
	}
	if (header.unknown7.count != 0) {
		Logger::Warn() << "Unknown7 has " << header.unknown7.count << " elements.\n";
		int u7 = 0;
		fileStream.seekg(header.unknown7.offset, std::ios::beg);
		for (uint32_t i = 0; i < header.unknown7.count; i++) {
			fileStream.read((char*) &u7, 4);
			Logger::Debug() << std::to_string(u7) << " ";
		}
		Logger::Debug() << std::endl;
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
	ProgBranch(Block* pBlock_) : pBlock(pBlock_) {}
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
		branch.stack = *saveStack;
	}
}

Value BytecodeParser::getArg(unsigned int type, const ScriptInfo &info) {
	if (stack.empty()) {
		return make_unique<ErrValueExpr>("Popping arguments off an empty stack.", instAddress);
	}

	unsigned int actualType = stack.back()->getType();
	if (type == ValueType::INT || type == ValueType::STR) {
		if (actualType != type)
			Logger::Error(instAddress) << "Expected arg: " << VarType(type) << ", got " << VarType(actualType) << std::endl;
		return stack.pop();
	}
	// Yeah I'll just do this
	Expression* arg = getLValue(info);
	arg->setType(type);
	return Value(arg);
}

unsigned int BytecodeParser::getArgs(std::vector<Value> &args, std::vector<unsigned int> &argTypes, const ScriptInfo& info) {
	unsigned int numArgs = getInt();
	for (unsigned int i = 0; i < numArgs; i++) {
		unsigned int type = getInt();
		if (type == 0xFFFFFFFF) {
			std::vector<Value> list;
			argTypes.push_back(getArgs(list, argTypes, info));
			args.push_back(make_unique<ListExpression>(std::move(list)));
		} else {
			args.push_back(getArg(type, info));
		}
		argTypes.push_back(type);
	}
	return numArgs;
}


Value BytecodeParser::getLocalVar(unsigned int index) {
	if (index >= localVars.size())
		return make_unique<ErrValueExpr>("Local var index " + std::to_string(index) + " out of range.", instAddress);

	return Value(localVars[index]->clone());
}

std::string BytecodeParser::getFunctionSignature() {
	if (numParams > localVars.size())
		return "(ERROR)";
	std::string sig = "(";
	for (unsigned int i = 0; i < numParams; i++) {
		if (i != 0)
			sig += ", ";
		Value& var = localVars[i];
		sig += VarType(var->getType()) + " " + var->print();
	}
	sig += ")";
	return sig;
}

// Not too sure where this belongs
Expression* BytecodeParser::getLValue(const ScriptInfo &info) {
	auto curr = stack.getFrame();
	Value pCurr, pLast;
	bool localVar = false, indexing = false;
	std::string localString = "g_";

	if (curr == stack.end())
		return new ErrValueExpr("Cannot pop element code - stack empty!", instAddress);

	unsigned int index = (*curr)->getIndex();
	// Not right, only 0x25 and 0x26 are indices
	// 0x53 => 0x00, 0x01, 0x7d, 0x7f
	if (index == 0x53 || index == 0x25 || index == 0x26) {
		localVar = true;
		localString = "loc_" + (*curr)->print(true) + "_";
		pLast = make_unique<VariableExpression>((*curr)->print(true), ValueType::STAGE_ELEMENT);
		curr++;
	}


	while (curr != stack.end()) {
		pCurr = std::move(*curr);

		if (pCurr->getType() != ValueType::INT) {
			pLast = std::move(pCurr);
			curr++;
			continue;
		}

		switch(pCurr->getIntType()) {
			case IntegerSimple: {
				if (indexing) {
					indexing = false;
					pLast = make_unique<IndexValueExpr>(std::move(pLast), std::move(pCurr));
					Logger::VVDebug(instAddress) << "Created index reference " << pLast->print(true) << "\n";
				} else {
					if (pLast == nullptr || pLast->getType() == ValueType::STAGE_ELEMENT) {
						unsigned int type;
						if (localVar) {
							switch (pCurr->getIndex()) {
								case 0x00: type = ValueType::INT_LIST; break;
								case 0x01: type = ValueType::STR_LIST; break;
								case 0x02: type = ValueType::OBJECT_LIST; break;
								default: type = ValueType::INT_LIST; break;
							}
						} else {
							// Engine specific here
							type = ValueType::INT_LIST;
						}
						pLast = make_unique<VariableExpression>(localString + pCurr->print(true), type, 1);
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

				if (pLast != nullptr && pLast->getType() != ValueType::STAGE_ELEMENT)	Logger::Warn(instAddress) << "Overwriting variable.\n";
				unsigned int index2 = pCurr->getIndex();
				pLast = getLocalVar(index2);

				Logger::VDebug(instAddress) << pLast->print() << ": " << VarType(pLast->getType()) << std::endl;
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
				throw std::logic_error("0x" + toHex(instAddress) + ": Unexpected integer type.");
		}

		curr++;
	}

	if (pLast == nullptr) {
		throw std::logic_error("0x" + toHex(instAddress) + ": Something went horribly wrong.");
	}

	stack.closeFrame();
	return pLast->clone(); // think about whether this is needed, or maybe I could just release/pass directly back (i forget how this is used)
}

FunctionExpr* BytecodeParser::getCallFunction(const ScriptInfo& info) {
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
		//else if (index == 0x54)
		//	fn->pushRet = false;

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

void BytecodeParser::parse(ControlFlowGraph& cfg, ScriptInfo& info, std::map<unsigned int, std::string> *pAsmLines) {
	std::string asmLine;
	bool dumpAsm = pAsmLines != nullptr;

	numParams = 0;
	bool paramsDone = false;
	localVars.clear();
	while (!toTraverse.empty()) {
		ProgBranch branch = std::move(toTraverse.back());
		toTraverse.pop_back();

		Block* pBlock = branch.pBlock;
		if (pBlock->parsed)
			continue;

		buf->setAddress(pBlock->startAddress);
		stack = std::move(branch.stack);
		pBlock->parsed = true;

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
					if (type == ValueType::INT || type == ValueType::STR) {
						Value back = stack.pop();
						if (back->getType() != type) {
							Logger::Error(instAddress) << "Expected type " << VarType(type) << ", got type " << VarType(back->getType()) << std::endl;
						}
						// Turn into a statement if it has side effects
						if (back->hasSideEffect()) {
							pStatement = new ExpressionStatement(back.release());
						}
					} else if (type != ValueType::VOID) {
						Logger::Error(instAddress) << "Cannot pop " << VarType(type) << std::endl;
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
						stack.push(new ErrValueExpr(pValue->print(), instAddress));
						break;
					}

					// Move the value into a variable if it has side effects
					if (pValue->hasSideEffect()) {
						Expression* pVar = new VariableExpression(pValue->getType());
						stack.push(pVar->clone());
						pStatement = new AssignStatement(Value(pVar), stack.pop());
					} else {
						stack.push(pValue->clone());
					}

					asmLine = "dup " + VarType(type);
				} break;
				case 0x05: {
					// NOTE: maybe can change getLValue
					// make it a stack member function
					// return a value
					// change the stackheight back to the original size
					Expression* val = getLValue(info);
					unsigned int type = val->getType();
					// Element code
					if (type != ValueType::INT && type != ValueType::STR)
						stack.openFrame();

					stack.push(val);
					asmLine = "eval";
				} break;
				case 0x06: {
					stack.duplicateElement();
					asmLine = "dup element";
				} break; 
				case 0x07: {
					// TODO: support intlist and strlist
					// I saw it, so i know it exists
					// I hope it'll crash (gracefully ofc) when I get to it
					unsigned int type = getInt();
					std::string name = info.getLocalVarName(getInt());

					// objlist?
					if (type == ValueType::INT_LIST || type == ValueType::STR_LIST) {
						Value pLength = stack.pop();
						unsigned int index = pLength->getIndex();
						if (index & 0xFF000000) {
							localVars.push_back(make_unique<ErrValueExpr>("Length for " + name + " is " + pLength->print(), instAddress));
						} else {
							localVars.push_back(make_unique<VariableExpression>(name, type, index));
						}
					} else {
						localVars.push_back(make_unique<VariableExpression>(name, type, 0));
					}

					if (paramsDone)
						pStatement = new DeclareVarStatement(type, name);
					asmLine = "var " + VarType(type) + " " + name;
				} break;
				case 0x08: {
					stack.openFrame();
					asmLine = "frame";
				} break; 
				case 0x09: {
					numParams = localVars.size();
					paramsDone = true;
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
					if (opcode == 0x12) {
						negateCondition(condition);
						asmLine = "jz 0x" + toHex(pJumpBlock->startAddress, addressWidth); 
					} else {
						asmLine = "jnz 0x" + toHex(pJumpBlock->startAddress, addressWidth); 
					}
					
					pStatement = new BranchStatement(pJumpBlock->index, pNextBlock->index, std::move(condition));

					pBlock->nextAddress = buf->getAddress();
					newBlock = true;
				} break;
				case 0x13:
				case 0x14: {
					unsigned int labelIndex = getInt();
					Block* pCallBlock = cfg.getBlock(info.getLabelAddress(labelIndex));
					pCallBlock->isFunction = true;
					pBlock->calls.push_back(pCallBlock);

					std::vector<unsigned int> argTypes;
					std::vector<Value> args;
					getArgs(args, argTypes, info);

					addBranch(pCallBlock, &stack);

					stack.push(new ShortCallExpr(pCallBlock->index, std::move(args)));

					asmLine = "gosub 0x" + toHex(pCallBlock->startAddress, addressWidth) + printArgList(argTypes);
				} break;
				case 0x15: {
					std::vector<unsigned int> retTypes;
					std::vector<Value> ret;
					unsigned int numArgs = getArgs(ret, retTypes, info);

					pStatement = new ReturnStatement(std::move(ret));

					pBlock->nextAddress = buf->getAddress();
					newBlock = true;

					if (!stack.empty()) {
						Logger::Error(instAddress) << "Stack is not empty!\n" << stack.print();
					}

					asmLine = "ret" + ((numArgs > 0) ? printArgList(retTypes) : "");
				} break; 
				// TODO: handle this properly
				case 0x16:
					Logger::Info(instAddress) << "Script end reached.\n";
					pStatement = new EndScriptStatement();
					asmLine = "endscript";
				break;
				case 0x20: {
					unsigned int unknown1 = getInt();
					unsigned int type = getInt();
					unsigned int unknown2 = getInt();
					if (unknown2 != 1) {
						Logger::Warn(instAddress) << "Assigning with " << std::to_string(unknown2) << std::endl;
					}

					Value rhs = stack.pop();
					Value lhs = Value(getLValue(info));

					if (rhs->getType() != type) {
						rhs = make_unique<ErrValueExpr>("Assign - Expected type " + VarType(type) + ", got type " + VarType(rhs->getType()), instAddress);
					}

					Logger::VVDebug(instAddress) << "Assign: " << VarType(unknown1) << " <- " << VarType(type) << std::endl;

					pStatement = new AssignStatement(std::move(lhs), std::move(rhs));
					asmLine = "assign " + VarType(unknown1) + " " + VarType(type) + " 0x" + toHex(unknown2);
				} break;
				case 0x21: {
					unsigned int type = getInt();
					unsigned char op = getChar();

					Value val = stack.pop();
					if (val->getType() != type) {
						val = make_unique<ErrValueExpr>("Calc1 - Expected type " + VarType(type) + ", got type " + VarType(val->getType()), instAddress);
					}
					stack.push(new UnaryExpression(std::move(val), op));

					asmLine = "calc1 " + VarType(type) + " 0x" + toHex(op);
				} break;
				case 0x22: {
					unsigned int lhsType = getInt();
					unsigned int rhsType = getInt();
					unsigned int op = getChar();

					Value rhs = stack.pop();
					Value lhs = stack.pop();
					if (lhs->getType() != lhsType) {
						lhs = make_unique<ErrValueExpr>("Calc2 - Expected type " + VarType(lhsType) + ", got type " + VarType(lhs->getType()), instAddress);
					}
					if (rhs->getType() != rhsType) {
						rhs = make_unique<ErrValueExpr>("Calc2 - Expected type " + VarType(rhsType) + ", got type " + VarType(rhs->getType()), instAddress);
					}

					stack.push(new BinaryExpression(std::move(lhs), std::move(rhs), op));
					asmLine = "calc2 " + VarType(lhsType) + " " + VarType(rhsType) + " 0x" + toHex(op, 2);
				} break; 
				case 0x30: {
					unsigned int option = getInt();

					std::vector<unsigned int> argTypes;
					std::vector<Value> args;
					getArgs(args, argTypes, info);

					unsigned int numExtra = getInt();
					std::vector<unsigned int> extraList;
					for (unsigned int i = 0; i < numExtra; i++) {
						unsigned int u = getInt();
						extraList.push_back(u);
					}

					unsigned int returnType = getInt();

					// Reversed though
					FunctionExpr* fn = getCallFunction(info);
					unsigned int extra = 0;
					if (fn->hasExtra) {
						extra = getInt();
						fn->extraThing(extra);
					}

					CallExpr* pCall = new CallExpr(fn, option, std::move(args), extraList, returnType);
					
					if (returnType == ValueType::VOID)
						pStatement = new ExpressionStatement(pCall);
					else
						stack.push(pCall);

					asmLine = "call " + std::to_string(option) + " " + printArgList(argTypes);
					if (!extraList.empty()) {
						asmLine += "(";
						for (const auto& it:extraList)
							asmLine += "0x" + toHex(it);
						asmLine += ")";
					}
					if (returnType != ValueType::VOID)
						asmLine += " → " + VarType(returnType);
					if (fn->hasExtra)
						asmLine += " <0x" + toHex(extra) + ">";
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
				(*pAsmLines)[instAddress] = asmLine;
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
					if (!stack.empty()) {
						Logger::Warn(instAddress) << "Stack size is positive.\n" << stack.print();
					}

					if (dumpAsm) {
						if (pAsmLines->count(buf->getAddress()) == 0)
							(*pAsmLines)[buf->getAddress()] = "unmapped";
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
}

unsigned int BytecodeParser::getInt() {
	return buf->getInt();
}
unsigned char BytecodeParser::getChar() {
	return buf->getChar();
}
/*
unsigned int BytecodeParser::getArgTypes(std::vector<unsigned int> &argTypes) {
	unsigned int numArgs = getInt();
	for (unsigned int i = 0; i < numArgs; i++) {
		unsigned int type = getInt();
		if (type == 0xFFFFFFFF) {
			argTypes.push_back(getArgTypes(argTypes));
		}
		argTypes.push_back(type);
	}
	return numArgs;
} */


std::string printArgList(const std::vector<unsigned int>& argTypes) {
	std::string argList = "(";
	unsigned int type;
	for (auto it = argTypes.rbegin(); it != argTypes.rend(); it++) {
		type = *it;
		if (it != argTypes.rbegin())
			argList += ", ";
		if (type == 0xFFFFFFFF) {
			if (++it == argTypes.rend()) {
				Logger::Error() << "Out of args.\n";
				return argList;
			}
			unsigned int listSize = *it;
			if (argTypes.rend() - it > listSize) {
				argList += "{";
				for (unsigned int i = 0; i < listSize; i++) {
					it++;
					if (i != 0) argList += ", ";
					argList += VarType(*it);
				}
				argList += "}";
			} else {
				Logger::Error() << "Out of args.\n";
				return argList;
			}
		} else {
			argList += VarType(type);
		}
	}
	argList += ")";
	return argList;
}


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