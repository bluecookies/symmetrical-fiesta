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

StringList populateMnemonics() {
	std::stringstream hexStream;
	hexStream << std::hex << std::setfill('0');
	
	StringList mnemonics;
	mnemonics.resize(0x100);
	// Temporary
	mnemonics[0x00] = "nop";
	for (unsigned short i = 1; i < 256; i++) {
		hexStream << std::setw(2) << +i;
		mnemonics[i] = "[" + hexStream.str() + "]";
		hexStream.str("");
	}
	mnemonics[0x02] = "push";		// value onto target stack
	mnemonics[0x03] = "pop";		// and discard
	mnemonics[0x04] = "dup";
	mnemonics[0x07] = "var";		// declare var
	mnemonics[0x08] = "[08]";		

	mnemonics[0x10] = "jmp";
	mnemonics[0x11] = "jz";
	mnemonics[0x12] = "jnz";		// no flags, just stack
	mnemonics[0x15] = "ret";
	mnemonics[0x16] = "end";
	mnemonics[0x20] = "assign";
	mnemonics[0x22] = "calc";
	mnemonics[0x30] = "call";
	//mnemonics[0x32]
	
	return mnemonics;
}

static StringList s_mnemonics = populateMnemonics();

/*
void printLabels(FILE* f, std::vector<Label>::iterator &pLabel, std::vector<Label>::iterator end, unsigned int address, const char* type) {
	Label label;
	while (pLabel != end) {
		label = *pLabel;
		if (label.offset <= address) {
			pLabel++;
			if (label.offset > 0 && label.offset == address) {
				fprintf(f, "\nSet%s %x:\t; %s\n", type, label.index, label.name.c_str());
			}
		} else {
			break;
		}
	}
}

// must guarantee stackPointers will never be empty
// TODO: handle 0x7d - not sure what they are, references?
// TODO: also handle 0x7d the same things on 0x20 calls
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
	fprintf(f, "%#x ", buf.getInt());
	unsigned int numArgs = readArgs(buf, progInfo);
	fprintf(f, "(");
	for (unsigned int i = 0; i < numArgs; i++) {
		fprintf(f, "%#x,", progInfo.args.back());
		progInfo.args.pop_back();
	}
	fprintf(f, ")");
}

void instCalc(FILE* f, BytecodeBuffer &buf, ProgInfo& progInfo) {	
	// calc. 0x10 is subtract?
	unsigned int arg1, arg2;
	StackValue var;
	
		arg1 = buf.getInt();
		arg2 = buf.getInt();
	unsigned int arg3 = buf.getChar();
	fprintf(f, "%#x, %#x, %d", arg1, arg2, arg3);
	if (arg1 == 0xa && arg2 == 0xa) {
		if (progInfo.stack.size() >= 2) {
			var = progInfo.stack.back();
			progInfo.stack.pop_back();
			if (var.type != STACK_NUM)
				Logger::Log(Logger::WARN, progInfo.address) << "Non-num type read.\n";
				
			var = progInfo.stack.back();
			progInfo.stack.pop_back();
			if (var.type != STACK_NUM)
				Logger::Log(Logger::WARN, progInfo.address) << "Non-num type read.\n";
				
			progInfo.stack.push_back(StackValue(0xdeadbeef, STACK_NUM));
		} else {
			Logger::Log(Logger::ERROR, progInfo.address) << " Not enough values for calc\n";
		}
		// TODO: figure out the calcs and do them
	} else if (arg1 == 0x14 && arg2 == 0x14) {
		if (progInfo.stack.size() >= 2) {
			var = progInfo.stack.back();
			progInfo.stack.pop_back();
			if (var.type != STACK_STR)
				Logger::Log(Logger::WARN, progInfo.address) << "Non-num type read.\n";
				
			var = progInfo.stack.back();
			progInfo.stack.pop_back();
			if (var.type != STACK_STR)
				Logger::Log(Logger::WARN, progInfo.address) << "Non-num type read.\n";
				
			progInfo.stack.push_back(StackValue(0xdeadbeef, STACK_STR));
		} else {
			Logger::Log(Logger::ERROR, progInfo.address) << " Not enough values for calc\n";
		}
		if (arg3 == 0x01)
			progInfo.comment = "string concatenation";
	} else {
		Logger::Log(Logger::DEBUG, progInfo.address) << " Comparing " << arg1 << " and " << arg2 << std::endl;
	}
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
			case 0x01: getInt();		break;
			case 0x02: instPush();		break;
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
			// not true, need to consider other branches
			case 0x15: {
				toReturn = true;
				outputString += "\treturn; \n";
			} break;
			case 0x20: instAssign();	break;
			case 0x21: {
				outputString += "\tInst21(" + std::to_string(getInt()) + ", " + std::to_string(getChar()) + ")\n";
			} break;
			case 0x30: instCall();		break;
			default:
				Logger::Log(Logger::WARN) << "Unhandled instruction " << std::to_string(opcode) << std::endl;
		}
	}

	return numParams;
}


void BytecodeParser::parseBytecode(std::string filename) {
	FILE* f = fopen(filename.c_str(), "wb");

	// Get functions defined in this file
	std::vector<Label> localCommands;
	auto predCommandFile = [this](ScriptCommand c) {
		return c.file == sceneInfo.thisFile;
	};
	std::copy_if(sceneInfo.commands.begin(), sceneInfo.commands.end(), 
		std::back_inserter(localCommands), predCommandFile);
	std::sort(sceneInfo.functions.begin(),sceneInfo.functions.end());
	std::sort(localCommands.begin(),	localCommands.end());
	
	//auto functionIt = sceneInfo.functions.begin();

	// TODO: fix naming 
	for (auto &function : localCommands) {
		if (function.name.empty())
			function.name = ("fun_" + std::to_string(function.index));
		Logger::Log(Logger::INFO) << "Parsing function: " << function.name << std::endl;

		ProgStack localVars;
		outputString.clear();
		unsigned int numParams = parseFunction(function, localVars);

		std::string paramsString;
		for (unsigned int i = 0; i < numParams; i++) {
			if (i != 0) paramsString += ", ";
			paramsString += std::to_string(localVars[i].type) + " " + localVars[i].name;
		}

		fprintf(f, "function %s(%s)@[%#x] {\n", function.name.c_str(), paramsString.c_str(), function.offset);
		fprintf(f, "%s", outputString.c_str());
		fprintf(f, "}\n");
	}


	/* std::sort(sceneInfo.labels.begin(),		sceneInfo.labels.end());
	std::sort(sceneInfo.markers.begin(),	sceneInfo.markers.end());

	auto labelIt = sceneInfo.labels.begin(), markerIt = sceneInfo.markers.begin();

	Logger::Log(Logger::DEBUG) << "Parsing " << std::to_string(buf.size()) << " bytes.\n";
	
	ProgramInfo progInfo;
	
	while (!buf.done()) {
		// Get address before incrementing
		progInfo.address = buf.getAddress();
		progInfo.opcode = buf.getChar();
		progInfo.numInsts++;

		// Print labels and commands
		printLabels(f, labelIt, sceneInfo.labels.end(), progInfo.address, "Label");
		printLabels(f, markerIt, sceneInfo.markers.end(), progInfo.address, "EntryPoint");
		printLabels(f, functionIt, sceneInfo.functions.end(), progInfo.address, "Function");
		printLabels(f, commandIt, localCommands.end(), progInfo.address, "Command");
		
		fprintf(f, "%#06x>\t%02x| %s ", progInfo.address, progInfo.opcode, s_mnemonics[progInfo.opcode].c_str());
		
		switch (progInfo.opcode) {
			case 0x02:	instPush(f, buf, sceneInfo, progInfo);	break;
			case 0x04:	instDo04Thing(f, buf, progInfo);		break;
			
			case 0x01:
			case 0x03:	
			case 0x10:
			case 0x11:
			case 0x12:
			case 0x31:	fprintf(f, "%#x", buf.getInt());		break;
			
			case 0x05:	instDo05Thing(progInfo);						break;
			
			case 0x06:
			case 0x09:
			case 0x32:
			case 0x33:
			case 0x34:																			break;
			
			case 0x16:	Logger::Log(Logger::INFO, progInfo.address) << "End of script reached.\n";
				break;
			case 0x07:	instDo07Thing(f, buf, sceneInfo, progInfo);		break;
			case 0x08:	instDo08Thing(progInfo);						break;

			case 0x13:	instDo13Thing(f, buf, progInfo);		break;
			case 0x14:
			case 0x20:	fprintf(f, "%#x, %#x, %#x", buf.getInt(), buf.getInt(), buf.getInt());
				break;
			case 0x21:	fprintf(f, "%#x, %d", buf.getInt(), buf.getChar());
				break;
			case 0x22:	instCalc(f, buf, progInfo);					break;

			case 0x15:	instDo15Thing(f, buf, progInfo);		break;

			case 0x30:	instCall(f, buf, progInfo);					break;

			case 0x00:
			default:
				progInfo.numNops++;
				Logger::Log(Logger::INFO) << "Address 0x" << std::hex << std::setw(4) << progInfo.address;
				Logger::Log(Logger::INFO) << ":  NOP encountered: 0x" << std::setw(2) << +progInfo.opcode << std::dec << std::endl;
		}
		
		if (!progInfo.comment.empty()) {
			fprintf(f, "\t; %s", progInfo.comment.c_str());
			progInfo.comment.clear();
		}
		
		fprintf(f, "\n");
	}
	
	Logger::Log(Logger::INFO) << "Parsed " << progInfo.numInsts << " instructions.\n";
	if (progInfo.numNops > 0) {
		Logger::Log(Logger::WARN) << "Warning: " << progInfo.numNops << " NOP instructions skipped.\n";
	} */

	fclose(f);
}

BytecodeParser::BytecodeParser(std::ifstream &f, HeaderPair index, SceneInfo info) {
	f.seekg(index.offset, std::ios_base::beg);
	
	dataLength = index.count;
	bytecode = new unsigned char[dataLength];
	
	f.read((char*) bytecode, dataLength);
	if (f.fail()) {
		Logger::Log(Logger::ERROR) << "Tried to read " << dataLength << " bytes, got" << f.gcount() << std::endl;
		throw std::exception();
	}
	Logger::Log(Logger::DEBUG) << "Read " << f.gcount() << " bytes of bytecode." << std::endl;

	sceneInfo = info;
}

BytecodeParser::~BytecodeParser() {
	delete[] bytecode;
}

unsigned int BytecodeParser::getInt() {
	unsigned int value = 0;
	if (currAddress + 4 <= dataLength) {
		value = readUInt32(bytecode + currAddress);
		currAddress += 4;
	} else {
		throw std::out_of_range("Buffer out of data");
	}
	return value;
}
unsigned char BytecodeParser::getChar() {
	unsigned char value = 0;
	if (currAddress < dataLength) {
		value = bytecode[currAddress++];
	} else {
		throw std::out_of_range("Buffer out of data");
	}
	return value;
}

// **************************
// Handle instructions
// **************************


// Safe
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

// Safety depends on readArgs
void BytecodeParser::instCall() {
	StackValue arg;
	unsigned int arg1 = getInt();
	if (arg1 > 1)
		Logger::Log(Logger::INFO) << "Call with first value " << std::to_string(arg1) << std::endl;
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
		name = "fun_" + stream.str();
		// TODO TODO TODO TODO:
		// Dump stack here if can't handle it properly (most cases)
	}
	stack.push_back(StackValue(0xdeadbeef, retType));
	stack.back().name = name + argString;
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
	StackValue index, arr;
	std::string lhs_name;


	if (stack.size() < 2)
		throw std::logic_error("Empty stack - no LHS");

	index = stack.back();
	while(true) {
		if (stack.size() < 2)
			throw std::logic_error("Empty stack - no LHS");

		stack.pop_back();
		if (index.value >> 24 == 0x7f) {
			lhs_name += index.name;
			break;
		} else if (index.value == 0x53 || index.value == 0x26) {
			lhs_name += "_" + index.name;
			break;
		} else if (stack.back().value != 0xFFFFFFFF) {
			lhs_name += index.name + "_";
			index = stack.back();
		} else {
			stack.pop_back();

			arr = stack.back();
			stack.pop_back();
			lhs_name += arr.name+"["+index.name+"]";
			
			index = stack.back();
		}
	}
	popFrame();

	// assign RHS value to LHS
	outputString += "\t" + lhs_name + " = " + rhs.name + ";\n";	// TODO: indentation blocks
}



unsigned int BytecodeParser::readArgs(ProgStack &args) {
	unsigned int numArgs = getInt();
	unsigned int argType = 0, stackCount = 0;
	StackValue var;

	for (unsigned int counter = 0; counter < numArgs; counter++) {
		if (stack.empty())
			throw std::logic_error("Reading args off empty stack.");

		argType = getInt();

		if (argType == STACK_NUM || argType == STACK_STR) {
			var = stack.back();
			stack.pop_back();
			if (var.type != argType) {
				Logger::Log(Logger::ERROR) << "Wrong type.\n";
			}
			args.push_back(var);
		}
		/*} else if (arg == 0x0e) {
			if (!progInfo.stack.empty()) {
				var = progInfo.stack.back();
				progInfo.stack.pop_back();
				// TODO: handle this type
				
				if (var.value >> 24 != 0x7f) {
					Logger::Log(Logger::DEBUG, progInfo.address) << " Type 0xe: " << std::hex << var.value << std::endl;
				}	
				if (progInfo.stackPointers.back() > progInfo.stack.size()) {
					Logger::Log(Logger::WARN, progInfo.address) << " Popped past frame.\n";
				}
			}
		} */
		// Unsafe
		else if (argType == STACK_OBJ) {
			std::string name = "DATA";
			var = stack.back();
			if (var.type == argType) {
				stack.pop_back();
				args.push_back(var);
			} else if (var.type == STACK_NUM) {
				// index
				StackValue index, arr;
				do {
					if (stack.size() < 3)
						throw std::logic_error("Not enough values in stack to do index.");

					index = stack.back();
					stack.pop_back();
					if (stack.back().value != 0xFFFFFFFF)
						throw std::logic_error(std::to_string(currAddress) + ": Expected 0xFFFFFFFF");
					stack.pop_back();
					arr = stack.back();
					stack.pop_back();
					
					name += "_"+std::to_string(arr.value)+"["+index.name+"]";
				} while (arr.value != 0x02);

				if (stack.size() < 2)
					throw std::logic_error("Reading args off empty stack.");


				var = stack.back();
				stack.pop_back();
				popFrame();

				name += "_" + std::to_string(var.value);

				args.push_back(StackValue(0xDEADFACE, STACK_OBJ));
				args.back().name = name;
			} else {
				throw std::logic_error("Expected either numerical or object.");
			}
		} else if (argType == 0xffffffff) {
			stackCount = getInt();
			if (stack.size() < stackCount)
				throw std::logic_error("Not enough arguments to pop");
			unsigned int arrType = 0;
			for (unsigned int counter2 = 0; counter2 < stackCount; counter2++) {
				arrType = getInt(); // and check its the same
				stack.pop_back();
			}

			args.push_back(StackValue(0xDEADBEEF, arrType+1));
			args.back().length = stackCount;
		} else {
			Logger::Log(Logger::INFO) << "Popping type " << argType << std::endl;
			if (!stack.empty())
				stack.pop_back();
			args.push_back(StackValue(0xDEADBEEF, STACK_VOID));
		}
	}

	return numArgs;
}

void BytecodeParser::popFrame() {
	if (stack.empty())
		throw std::out_of_range("Popping empty stack - expected [08]");

	if (!stack.back().endFrame)
		throw std::logic_error("Expected [08], got " + std::to_string(stack.back().value));

	stack.pop_back();
}

