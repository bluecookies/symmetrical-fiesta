// Parse the bytecode
// A lot of this is stolen from tanuki
// https://github.com/bitprime/vn_translation_tools/blob/master/rewrite/
// also Inori

#include <cstdio>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <exception>
#include <algorithm>

#include "Helper.h"
#include "Structs.h"

#include "BytecodeParser.h"

//move to header
std::string toHex(unsigned int c, unsigned char width = 0) {
	std::stringstream ss;
	if (width > 0)
		ss << std::setfill('0') << std::setw(width);
	ss << std::hex << c;
	return ss.str();
}
//

Instruction::Instruction(Parser *parser, unsigned char opcode_) {
	address = parser->instAddress;
	opcode = opcode_;
}

bool Instruction::expandStrings = true;
bool Instruction::expandGlobalVars = true;
bool Instruction::expandCommandNames = true;
void Instruction::print(Parser*, std::ofstream &stream) const {
	stream << toHex(address, width) << "\t[" << toHex(opcode, 2) << "]\n";
}

unsigned char Instruction::width = 6;
void Instruction::setWidth(unsigned int address) {
	unsigned char w = 1;
	while (address > 0) {
		address /= 0x10;
		w++;
	}
	// Round down to even (up from actual)
	w = (w / 2) * 2;
}


class InstLine: public Instruction {
	unsigned int line;
	public:
		InstLine(Parser *parser, unsigned char opcode) : Instruction(parser, opcode) {
			line = parser->getInt();
		}
		void print(Parser*, std::ofstream &stream) const {
			stream << toHex(address, width) << "\tline " << std::to_string(line) << "\n";
		}
};
class InstPush: public Instruction {
	unsigned int type, value;
	public:
		InstPush(Parser *parser, unsigned char opcode) : Instruction(parser, opcode) {
			type = parser->getInt();
			value = parser->getInt();
		}
		void print(Parser* parser, std::ofstream &stream) const {
			unsigned char valType = value >> 24;
			unsigned int index = value & 0x00FFFFFF;

			if (type == 0xa && valType == 0x7e && expandCommandNames) {
				std::string cmdName;

				try {
					cmdName = parser->sceneInfo.commands.at(index).name;
				} catch (std::out_of_range &e) {
					cmdName = "command" + std::to_string(index);
				}

				stream << toHex(address, width) << "\tpush " << cmdName << ":<command>\n";
			} else if (type == 0xa && valType == 0x7f && expandGlobalVars) {
				std::string varName;
				try {
					varName = parser->sceneInfo.globalVars.at(index).name;
				} catch (std::out_of_range &e) {
					varName = "var" + std::to_string(index);
				}

				stream << toHex(address, width) << "\tpush " << varName << ":<0x" << toHex(type) << ">\n";
			} else if (type == 0x14) {
				std::string stringName;
				try {
					stringName = parser->sceneInfo.mainStrings.at(index);
				} catch (std::out_of_range &e) {
					stringName = "string" + std::to_string(index);
				}
				stream << toHex(address, width) << "\tpush \"" << stringName << "\"\n";
			} else {
				stream << toHex(address, width) << "\tpush " << std::to_string(value) << ":<0x" << toHex(type) << ">\n";
			}
		}
};
class InstPop: public Instruction {
	unsigned int type;
	public:
		InstPop(Parser *parser, unsigned char opcode) : Instruction(parser, opcode) {
			type = parser->getInt();
		}
		void print(Parser*, std::ofstream &stream) const {
			stream << toHex(address, width) << "\tpop <0x" << toHex(type) << ">\n";
		}
};
class InstSentinel: public Instruction {
	public:
		InstSentinel(Parser* parser, unsigned char opcode) : Instruction(parser, opcode) {
		}
};
class InstReturn: public Instruction {
	std::vector<unsigned int> returnTypeList;
	public:
		InstReturn(Parser *parser, unsigned char opcode) : Instruction(parser, opcode) {
			parser->readArgs(returnTypeList);
		}
		void print(Parser*, std::ofstream &stream) const {
			if (returnTypeList.empty()) {
				stream << toHex(address, width) << "\treturn\n";
			} else {
				stream << toHex(address, width) << "\treturn (";
				for (auto it = returnTypeList.rbegin(); it != returnTypeList.rend(); it++) {
					if (it != returnTypeList.rbegin())
						stream << ", ";
					stream << "0x" << toHex(*it);
				}
				stream << ")\n";
			}
		}
};
class InstEndScript: public Instruction {
	public:
		InstEndScript(Parser *parser, unsigned char opcode) : Instruction(parser, opcode) {

		}
		void print(Parser*, std::ofstream &stream) const {
			stream << toHex(address, width) << "\tendscript\n";
		}

};
class InstAssign: public Instruction {
	unsigned int LHSType, RHSType;
	unsigned int unknown;
	public:
		InstAssign(Parser *parser, unsigned char opcode) : Instruction(parser, opcode) {
			opcode = 0x20;
			LHSType = parser->getInt();
			RHSType = parser->getInt();
			unknown = parser->getInt();
		}
		void print(Parser*, std::ofstream &stream) const {
			stream << toHex(address, width) << "\tassign <0x" << toHex(LHSType) << "> <0x" << toHex(RHSType) << "> " << std::to_string(unknown) << std::endl;
		}
};
class InstCall: public Instruction {

	unsigned int fnOption;
	std::vector<unsigned int> paramTypes, extraTypes;
	unsigned int returnType;
	public:
		InstCall(Parser *parser, unsigned char opcode) : Instruction(parser, opcode) {
			fnOption = parser->getInt();

			parser->readArgs(paramTypes);
			parser->readArgs(extraTypes);

			returnType = parser->getInt();
		}
		void print(Parser*, std::ofstream &stream) const {
			stream << toHex(address, width) << "\tcall " << std::to_string(fnOption) << " (";
			for (auto it = paramTypes.rbegin(); it != paramTypes.rend(); it++) {
				if (it != paramTypes.rbegin())
					stream << ", ";
				stream << "0x" << toHex(*it);
			}
			if (!extraTypes.empty()) {
				stream << ") ((";
				for (auto it = extraTypes.rbegin(); it != extraTypes.rend(); it++) {
					if (it != extraTypes.rbegin())
						stream << ", ";
					stream << "0x" << toHex(*it);
				}
				stream << ")) => <0x";
			} else {
				stream << ") => <0x";
			}
			stream << toHex(returnType) << ">\n";
		}
};

/*
void instDo05Thing(ProgInfo& progInfo) {
	if (progInfo.stackPointers.size() > 1) {
		while (progInfo.stackPointers.back() < progInfo.stack.size()) {
			if (!progInfo.stack.empty()) {
				progInfo.stack.pop_back();
			} else {
				Logger::Log(Logger::ERROR, progInfo.address) << " Popping empty stack.\n";
			}
		}
	
		progInfo.stackPointers.pop_back();
		progInfo.stack.push_back(StackValue(0xDEADBEEF, STACK_NUM));
	} else {
		Logger::Log(Logger::ERROR, progInfo.address) << " Tried to pop base frame.\n";
	}
}

void instDo13Thing(FILE* f, BytecodeBuffer &buf, ProgInfo& progInfo) {
	fprintf(f, "%#x ", getInt());
	unsigned int numArgs = readArgs(buf, progInfo);
	fprintf(f, "(");
	for (unsigned int i = 0; i < numArgs; i++) {
		fprintf(f, "%#x,", progInfo.args.back());
		progInfo.args.pop_back();
	}
	fprintf(f, ")");
}

void instDo15Thing(FILE* f, BytecodeBuffer &buf, ProgInfo& progInfo) {
	unsigned int numArgs = readArgs(buf, progInfo);
	fprintf(f, "(");
	for (unsigned int i = 0; i < numArgs; i++) {
		fprintf(f, "%#x,", progInfo.args.back());
		progInfo.args.pop_back();
	}
	fprintf(f, ")");
} */
/*
unsigned int BytecodeParser::parseFunction(const Label &function, ProgStack &localVars) {
	if (function.offset >= dataLength)
		throw std::out_of_range("Function offset out of range.");

	currAddress = function.offset;
	bool argsDone = false, toReturn = false;

	unsigned char opcode;

	localVars.clear();
	unsigned int numParams = 0;


	stack.clear();
	// Read instructions
	while (!toReturn) {
		opcode = getChar();

		switch (opcode) {
			case 0x03: instPop();		break;
			case 0x04: instDup();		break;
			// TODO: make safe
			case 0x05: {
				StackValue val;
				val = stack.back();
				stack.pop_back();
				// Local variable
				if (val.value >> 24 == 0x7d) {
					StackValue unknown = stack.back();
					// Throw happy
					if (unknown.value != 0x53)
						throw std::logic_error("Expected 0x53");
					stack.pop_back();
					
					popFrame();

					unsigned int varIndex = val.value & 0x00FFFFFF;
					// Real exception
					if (varIndex >= localVars.size())
						throw std::out_of_range("Local var index out of range.");
					
					stack.push_back(localVars[varIndex]);

				} else {
					throw std::logic_error("Sorry, not handled yet.");
				}
			} break;
			case 0x07: {
				unsigned int type = getInt();
				unsigned int nameIndex = getInt();
				std::string name;
				try {
					name = sceneInfo.localVarNames.at(nameIndex);	
				} catch (std::out_of_range &e) {
					Logger::Log(Logger::WARN) << "Missing string id: " << nameIndex << std::endl;
					name = "var" + std::to_string(localVars.size());
				}

				localVars.push_back(StackValue(0xDEADBEEF, type));
				localVars.back().name = name;

				if (argsDone) {
					outputString += "var " + std::to_string(type) + " " + name + ";\n";
				} else {
					numParams++;
				}

			} break;
			case 0x08: stack.push_back(StackValue());	break;
			case 0x09: argsDone = true;					break;
			case 0x10: instJump();		break;
			case 0x11: instJump(true);	break;
			case 0x12: instJump(false);	break;
			// not true, need to consider other branches
			case 0x15: {
				toReturn = true;
				outputString += "\treturn; \n";
			} break;
			case 0x20: instAssign();	break;
			case 0x21: {
				outputString += "\tInst21(" + std::to_string(getInt()) + ", " + std::to_string(getChar()) + ")\n";
			} break;
			case 0x22: instCalc();		break;
			case 0x30: instCall();		break;
			default:
				Logger::Log(Logger::WARN) << "Unhandled instruction " << std::to_string(opcode) << std::endl;
		}
	}

	return numParams;
}*/


void BytecodeParser::parseBytecode() {
	unsigned int opcode;
	while (!buf->done()) {
		instAddress = buf->getAddress();
		opcode = getChar();

		switch (opcode) {
			case 0x01: instructions.push_back(new InstLine(this, opcode));		break;
			case 0x02: instructions.push_back(new InstPush(this, opcode));		break;
			case 0x03: instructions.push_back(new InstPop(this, opcode));		break;
			case 0x08: instructions.push_back(new InstSentinel(this, opcode));	break;
			case 0x15: instructions.push_back(new InstReturn(this, opcode));	break;
			case 0x16: instructions.push_back(new InstEndScript(this, opcode));	break;
			case 0x20: instructions.push_back(new InstAssign(this, opcode));	break;
			case 0x30: instructions.push_back(new InstCall(this, opcode));		break;
			default:	
				Logger::Log(Logger::ERROR) << "Error: Unknown instruction " << toHex(opcode) << " at address 0x" << toHex(instAddress) << std::endl;
		}

	}

	Instruction::setWidth(instAddress);
	/*
	ProgramInfo progInfo;
	
	while (currAddress < dataLength) {
		progInfo.numInsts++;

		// Print labels and commands
		printLabels(labelIt, sceneInfo.labels.end(), "Label");
		printLabels(markerIt, sceneInfo.markers.end(), "EntryPoint");
		printLabels(functionIt, sceneInfo.functions.end(), "Function");
		printLabels(commandIt, localCommands.end(), "Command");
		
		fprintf(f, "%#06x>\t%02x| %s ", progInfo.address, progInfo.opcode, s_mnemonics[progInfo.opcode].c_str());
		
		switch (progInfo.opcode) {
			case 0x04: instDup();		break;

			case 0x10:
			case 0x11:
			case 0x12:
			case 0x31:	fprintf(f, "%#x", getInt());	break;
			
			case 0x06:
			case 0x09:
			case 0x32:
			case 0x33:
			case 0x34:	break;
			
			case 0x07:	instDo07Thing(f, buf, sceneInfo, progInfo);		break;

			case 0x13:	instDo13Thing(f, buf, progInfo);		break;
			
			case 0x14:
			case 0x21:	fprintf(f, "%#x, %d", getInt(), getChar());
				break;
			case 0x22:	instCalc(f, buf, progInfo);					break;
		}
		
	}
	
	Logger::Log(Logger::INFO) << "Parsed " << progInfo.numInsts << " instructions.\n";
	if (progInfo.numNops > 0) {
		Logger::Log(Logger::WARN) << "Warning: " << progInfo.numNops << " NOP instructions skipped.\n";
	} */
}

void BytecodeParser::printInstructions(std::string filename) {
	// Get functions defined in this file
	std::vector<Label> localCommands;
	auto predCommandFile = [this](ScriptCommand c) {
		return c.file == sceneInfo.thisFile;
	};

	std::vector<Label> labels = sceneInfo.labels;
	std::vector<Label> entrypoints = sceneInfo.markers;

	auto labelIt = labels.begin();

	std::ofstream out(filename);
	// this is destructive though, so make copy
	for (const auto &inst:instructions) {
		// Print labels
		labelIt = labels.begin();
		while (labelIt != labels.end()) {
			if (labelIt->offset == inst->address) {
				out << "Label " << toHex(labelIt->index) << ":\n";

				labelIt = labels.erase(labelIt);
			} else {
				labelIt++;
			}
		}
		// Print entrypoints
		labelIt = entrypoints.begin();
		while (labelIt != entrypoints.end()) {
			if (labelIt->offset == inst->address) {
				if (labelIt->offset > 0)
					out << "Entrypoint " << toHex(labelIt->index) << ":\n";

				labelIt = entrypoints.erase(labelIt);
			} else {
				labelIt++;
			}
		}

		inst->print(this, out);
	}

	out.close();
}


BytecodeParser::BytecodeParser(std::ifstream &f, HeaderPair index, SceneInfo info) {
	f.seekg(index.offset, std::ios_base::beg);
	buf = new BytecodeBuffer(f, index.count);

	sceneInfo = info;
}

BytecodeParser::~BytecodeParser() {
	delete buf;

	for (auto &inst:instructions) {
		delete inst;
	}
}

// Bytecode buffer
// Handles instruction address and getting values

BytecodeBuffer::BytecodeBuffer(std::ifstream &f, unsigned int length) {
	dataLength = length;
	bytecode = new unsigned char[dataLength];

	f.read((char*) bytecode, dataLength);
	if (f.fail()) {
		Logger::Log(Logger::ERROR) << "Tried to read " << dataLength << " bytes, got" << f.gcount() << std::endl;
		throw std::exception();
	}
	Logger::Log(Logger::DEBUG) << "Read " << f.gcount() << " bytes of bytecode." << std::endl;
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

unsigned int BytecodeParser::readArgs(std::vector<unsigned int> &typeList) {
	unsigned int numArgs = getInt();
	unsigned int argType = 0;

	for (unsigned int counter = 0; counter < numArgs; counter++) {
		argType = getInt();
		typeList.push_back(argType);

		if (argType == 0xffffffff) {
			unsigned int arrLength = getInt();
			typeList.push_back(arrLength);

			unsigned int arrType = 0;
			for (unsigned int index = 0; index < arrLength; index++) {
				arrType = getInt();
				typeList.push_back(arrType);
			}
		}
	}

	return numArgs;
} 


/*
void BytecodeParser::instPush() {
	unsigned int type = getInt();
	unsigned int value = getInt();
	stack.push_back(StackValue(value, type));
	//TODO: also arrays
	if (type == STACK_NUM) {
		if (value >> 24 == 0x7f) {
			unsigned int varIndex = value & 0x00FFFFFF;
			if (varIndex < sceneInfo.globalVars.size())
				stack.back().name = sceneInfo.globalVars[varIndex].name;	// could just copy the var directly
			else
				stack.back().name = "g_var_" + std::to_string(varIndex);
		}
	} if (type == STACK_STR) {
		if (value >= sceneInfo.mainStrings.size())
			throw std::out_of_range("String index out of range");
		stack.back().name = '"' + sceneInfo.mainStrings[value] + '"';
	}
}

void BytecodeParser::instPop() {
	if (stack.empty())
		throw std::logic_error("Popping empty stack");
	unsigned int type = getInt();

	if (stack.back().type != type)
		throw std::logic_error("Popping unexpected type");

	if (stack.back().fnCall)
		outputString += "\t" + stack.back().name + ";\n";

	stack.pop_back();

}

void BytecodeParser::instDup() {
	if (stack.empty())
		throw std::logic_error("Duplicating empty stack");
	unsigned int type = getInt();

	if (stack.back().type != type)
		throw std::logic_error("Duplicating unexpected type");

	stack.push_back(stack.back());
}

void BytecodeParser::instCalc() {
	unsigned int type1 = getInt();
	unsigned int type2 = getInt();
	unsigned char op = getChar();

	if (type1 != type2)
		Logger::Log(Logger::ERROR) << "Comparing type " << std::to_string(type1) << " with " << std::to_string(type2) << std::endl;


	if (stack.size() < 2)
		throw std::logic_error("Not enough values on stack for comp");

	StackValue var2 = stack.back();
		stack.pop_back();
	StackValue var1 = stack.back();
		stack.pop_back();

	if (type1 == STACK_NUM) {
		StackValue result(0xDEADBEEF, STACK_NUM);
		switch (op) {
			case 1:
				result.value = var1.value + var2.value;
				result.name = "(" + var1.name + " + " + var2.name + ")";
			break;
			case 2:
				result.value = var1.value - var2.value;
				result.name = "(" + var1.name + " - " + var2.name + ")";
			break;
			case 3:
				result.value = var1.value * var2.value;
				result.name = "(" + var1.name + " * " + var2.name + ")";
			break;
			case 4:
				result.value = var1.value / var2.value;
				result.name = "(" + var1.name + " / " + var2.name + ")";
			break;
			case 16:
				result.value = var1.value != var2.value;
				result.name = "(" + var1.name + " != " + var2.name + ")";
			break;
			case 18:
				result.value = var1.value <= var2.value;
				result.name = "(" + var1.name + " <= " + var2.name + ")";
			break;
			case 19:
				result.value = var1.value > var2.value;
				result.name = "(" + var1.name + " > " + var2.name + ")";
			break;
			case 20:
				result.value = var1.value >= var2.value;
				result.name = "(" + var1.name + " >= " + var2.name + ")";
			break;
			default:
				result.name = "(" + var1.name + " <op"+std::to_string(op)+ "> " + var2.name + ")";
		}
		stack.push_back(result);
	} else if (type1 == STACK_STR) {
		StackValue result(0xDEADBEEF, STACK_STR);
		switch (op) {
			case 1:
				result.value = var1.value;
				result.name = var1.name + " + " + var2.name;
			break;
			case 16:
				result.value = var1.value != var2.value;
				result.name = var1.name + " != " + var2.name;
			break;
			default:
				result.name = "(" + var1.name + " <op"+std::to_string(op)+ "> " + var2.name + ")";
		}
		stack.push_back(result);
	} else {
		Logger::Log(Logger::INFO) << "Comparing type " << type1 << std::endl;
	}
}

// Safety depends on readArgs
void BytecodeParser::instCall() {
	StackValue arg;
	unsigned int arg1 = getInt();
	ProgStack argsA, argsB;

	std::string argString("(");

	// Arguments to pass to function to be called
	unsigned int numArgs1 = readArgs(argsA);
	for (unsigned int i = 0; i < numArgs1; i++) {
		if (i != 0)
			argString += ", ";

		arg = argsA.back();
		argsA.pop_back();
		
		if (arg.length == 0) {
			argString += arg.name;
		} else {
			argString += "<" + std::to_string(arg.type - 1) + ">[" + std::to_string(arg.length) + "]";
		}
	}
	argString += ")";

	// I don't know what these are
	unsigned int numArgs2 = readArgs(argsB);
	if (numArgs2 > 0) {
		argString += "{";
		for (unsigned int i = 0; i < numArgs2; i++) {
			if (i != 0)
				argString += ", ";

			arg = argsB.back();
			argsB.pop_back();
			
			if (arg.length == 0) {
				argString += arg.name;
			} else {
				argString += "<" + std::to_string(arg.type - 1) + ">[" + std::to_string(arg.length) + "]";
			}
		}
		argString += "}";
	}
	
	// Return type
	unsigned int retType = getInt();

	if (stack.empty())
		throw std::logic_error("Empty stack - function call");

	// Function to call
	StackValue callFn = stack.back();
	stack.pop_back();

	// Script command
	std::string name;
	if (callFn.value >> 24 == 0x7e) {
		unsigned int commandIndex = callFn.value & 0x00FFFFFF;
		popFrame();

		if (commandIndex >= sceneInfo.commands.size())
			throw std::out_of_range("Command index out of range");

		ScriptCommand command = sceneInfo.commands[commandIndex];
		name = command.name;
	} else {
		switch (callFn.value) {
			case 0x0c:
			case 0x12:
			case 0x13:
			case 0x4c:
			case 0x4d:
			case 0x5a:
			case 0x5b:
			case 0x64:
			case 0x7f:
				if (stack.back().endFrame) {
					stack.pop_back();
					getInt();	// part of command to call?
				}
			break;
			case 0xffffffff:
				Logger::Log(Logger::WARN) << " Calling function 0xffffffff.\n";
			break;
			case 0xDEADBEEF:
				Logger::Log(Logger::WARN) << " Calling deadbeef\n";
		}
		std::stringstream stream;
		stream << std::hex << callFn.value;
		name = "fun_0x" + stream.str();
		if (!stack.empty()) {
			if (!stack.back().endFrame) {
				name += "_" + stack.back().name;
			} 
			stack.pop_back();
		}

		// TODO TODO TODO TODO:
		// Dump stack here if can't handle it properly (most cases)
	}
	stack.push_back(StackValue(0xdeadbeef, retType));
	stack.back().name = name + "<" + std::to_string(arg1) + ">" + argString;
	stack.back().fnCall = true;
}


// Unsafe
void BytecodeParser::instAssign() {
	StackValue lhs, rhs;

	unsigned int LHSType = getInt();
	unsigned int RHSType = getInt();
	unsigned int unknown = getInt();
	if (unknown != 1)
		Logger::Log(Logger::INFO) << "Assigning with third param " << unknown << std::endl;

	if (LHSType != RHSType + 3)
		Logger::Log(Logger::WARN) << "Assigning type " << RHSType << " to " << LHSType << ".\n";

	if (stack.empty())
		throw std::logic_error("Empty stack - no RHS");

	rhs = stack.back();
	if (rhs.type != RHSType)
		throw std::logic_error("Incorrect type for RHS");
	stack.pop_back();


	// TODO: check these are all of type 0xa
	StackValue val;
	std::string lhs_name;


	if (stack.size() < 2)
		throw std::logic_error("Empty stack - no LHS");

	val = stack.back();
	if (val.type == STACK_NUM) {
		StackValue index = val;
		stack.pop_back();
		if (val.value >> 24 == 0x7f) {
			lhs_name = val.name;
			popFrame();
		} else {
			val = stack.back();
			if (val.type == STACK_NUM && val.value == 0xFFFFFFFF) {
				stack.pop_back();
				if (stack.empty())
					throw std::logic_error("Empty stack - get array");

				StackValue arr = stack.back();
				stack.pop_back();
				if (arr.type != RHSType + 1)
					throw std::logic_error("Incorrect LHS type - assigning from array");

				lhs_name = arr.name + "[" + index.name + "]";

				popFrame();
			} else if (val.type == STACK_OBJ) {
				stack.pop_back();
				lhs_name = val.name + "[" + index.name + "]";
			} else if (val.type == STACK_NUM) {
				// Get obj from array
				lhs_name += "DATA";
				stack.pop_back();

				if (stack.size() < 3)
					throw std::logic_error("Not enough values in stack to do index.");

				if (stack.back().value != 0xFFFFFFFF)
					throw std::logic_error(std::to_string(currAddress) + ": Expected 0xFFFFFFFF");
				stack.pop_back();
				StackValue arr = stack.back();
				stack.pop_back();
				
				lhs_name += "_"+arr.name+"["+val.name+"]";

				val = stack.back();
				stack.pop_back();

				lhs_name += "_" + val.name + "[" + index.name + "]";
				popFrame();
			}
		}
	}

	// assign RHS value to LHS
	outputString += "\t" + lhs_name + " = " + rhs.name + ";\n";	// TODO: indentation blocks
}

void BytecodeParser::instJump() {
	unsigned int labelIndex = getInt();
	auto labelIt = std::find_if(sceneInfo.labels.begin(), sceneInfo.labels.end(), [labelIndex](Label l) {
		return l.index == labelIndex;
	});

	if (labelIt == sceneInfo.labels.end()) {
		Logger::Log(Logger::ERROR) << "Could not find label " << std::hex << labelIndex << std::endl;
		throw std::logic_error("Could not find label");
	}

	currAddress = labelIt->offset;
}


void BytecodeParser::instJump(bool ifZero) {
	if (stack.empty())
		throw std::logic_error("Stack empty - conditional jump");

	StackValue condition = stack.back();
	stack.pop_back();

	unsigned int labelIndex = getInt();

	std::stringstream stream;
	stream << std::hex << labelIndex << std::endl;

	outputString += "\tif("+ std::string(ifZero ? "!" : "") + condition.name + ")\n";
	outputString += "\t\tgoto 0x" + stream.str();

	auto labelIt = std::find_if(sceneInfo.labels.begin(), sceneInfo.labels.end(), [labelIndex](Label l) {
		return l.index == labelIndex;
	});

	if (labelIt == sceneInfo.labels.end()) {
		Logger::Log(Logger::ERROR) << "Could not find label " << std::hex << labelIndex << std::endl;
		throw std::logic_error("Could not find label");
	}

	currAddress = labelIt->offset;
}
*/

/*
void BytecodeParser::popFrame() {
	if (stack.empty())
		throw std::out_of_range(std::to_string(currAddress) + ": Popping empty stack - expected [08]");

	if (!stack.back().endFrame)
		throw std::logic_error("Expected [08], got " + std::to_string(stack.back().value));

	stack.pop_back();
} */

