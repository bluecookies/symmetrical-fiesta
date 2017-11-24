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

unsigned int readArgs(BytecodeBuffer &buf, ProgInfo &progInfo, bool pop = true) {
	unsigned int numArgs = buf.getInt();
	unsigned int arg = 0, stackCount = 0;
	StackValue var;

	for (unsigned int counter = 0; counter < numArgs; counter++) {
		arg = buf.getInt();
		progInfo.args.push_back(arg);
		if (pop) {
			if (arg == 0x0a) {
				if (!progInfo.stack.empty()) {
					var = progInfo.stack.back();
					progInfo.stack.pop_back();
					if (var.type != STACK_NUM) {
						Logger::Log(Logger::WARN, progInfo.address) << "Non-num type read.\n";
					}
					
					if (progInfo.stackPointers.back() > progInfo.stack.size()) {
						Logger::Log(Logger::WARN, progInfo.address) << " Popped past frame.\n";
					}
				}
			} else if (arg == 0x0e) {
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
			} else if (arg == 0x14) {
				if (!progInfo.stack.empty()) {
					var = progInfo.stack.back();
					progInfo.stack.pop_back();
					
					if (var.type != STACK_STR) {
						Logger::Log(Logger::WARN, progInfo.address) << "Non-string type read.\n";
					}
				}
			} else if (arg == 0x51e) {
				// TODO: HANDLE REF LATER
				unsigned int a1 = 0xdeadbeef, a2 = 0xdeadbeef, a3 = 0xdeadbeef;
				if (progInfo.stack.size() >= 4) {
					progInfo.stack.pop_back();
					a1 = progInfo.stack.back().value;
						progInfo.stack.pop_back();
					a2 = progInfo.stack.back().value;
						progInfo.stack.pop_back();
					a3 = progInfo.stack.back().value;
						progInfo.stack.pop_back();
				} else {
					
					throw std::exception();
				}
				if (a1 != 0xffffffff || a2 != 2) {
					Logger::Log(Logger::WARN, progInfo.address) << " Popping 0x51e - " << std::hex;
					Logger::Log(Logger::WARN) << a3 << " " << a2 << " " << a1 << std::endl;
				}
			} else if (arg == 0xffffffff) {
				stackCount = buf.getInt();
				for (unsigned int counter2 = 0; counter2 < stackCount; counter2++) {
					progInfo.args.push_back(buf.getInt());
				}
				progInfo.args.push_back(0xffffffff);
			} else {
				Logger::Log(Logger::DEBUG, progInfo.address) << " trying to pop stack " << arg << std::endl;
			}
		}
	}

	return numArgs;
}

// TODO: move this either into a class or another file, just have the declarations together or something
void instPush(FILE* f, BytecodeBuffer &buf, SceneInfo& sceneInfo, ProgInfo& progInfo) {
	unsigned int	arg1, arg2;
		arg1 = buf.getInt();
		arg2 = buf.getInt();

	if (arg1 == 0x0a) {
		progInfo.stack.push_back(StackValue(arg2, STACK_NUM));
		fprintf(f, "%#x, %#x", arg1, arg2);
		// TODO: take it out of here and put just when called
		// maybe. maybe not
		
		// Command ~address~ index
		if (arg2 >> 24 == 0x7e) {
			unsigned int commandIndex = arg2 & 0x00ffffff;
			if (commandIndex < sceneInfo.commands.size()) {
				ScriptCommand command = sceneInfo.commands.at(commandIndex);
				progInfo.comment = command.name;
				std::stringstream stream;
				stream << std::hex << command.offset;

				if (commandIndex < sceneInfo.numGlobalCommands) {
					progInfo.comment += " (0x" + stream.str() + " in " + sceneInfo.sceneNames.at(command.file) + ")";
				} else {
					// this is a local command
					progInfo.comment += " (0x" + stream.str() + ")";
				}
			} else {
					Logger::Log(Logger::WARN, progInfo.address) << " trying to access command index " << arg2 << std::endl;
			}
		// Var name index
		} else if (arg2 >> 24 == 0x7f) {
			unsigned int varIndex = arg2 & 0x00ffffff;
			if (varIndex < sceneInfo.globalVars.size()) {
				progInfo.comment = sceneInfo.globalVars.at(varIndex).name;
			} else if (arg2 == 0x7fffffff) {
			} else {
				Logger::Log(Logger::WARN, progInfo.address) << " trying to access var index " << arg2 << std::endl;
			}
		}
	} else if (arg1 == 0x14) {
		fprintf(f, "%#x, [%#x]", arg1, arg2);
		if (arg2 <= sceneInfo.mainStrings.size()) {
			progInfo.comment = sceneInfo.mainStrings.at(arg2);
		} else {
			Logger::Log(Logger::WARN, progInfo.address) << " trying to access string index " << arg2 << std::endl;
		}
		progInfo.stack.push_back(StackValue(arg2, STACK_STR));
	}
}


void instDo04Thing(FILE* f, BytecodeBuffer &buf, ProgInfo& progInfo) {
	unsigned int arg1 = buf.getInt();
	fprintf(f, "%#x", arg1);
	if (arg1 == 0x0a) {
		progInfo.stack.push_back(StackValue(0xdeadbeef, STACK_NUM));
	} else {
		Logger::Log(Logger::INFO, progInfo.address) << "Non-num with 0x4\n";
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
}

// TODO: is 20 a store?


// TODO: pop stack pointer if reached
void instCall(FILE* f, BytecodeBuffer &buf, ProgInfo& progInfo) {
	unsigned int arg;
	unsigned int arg1 = buf.getInt();
	fprintf(f, "%#x, (", arg1);
	
	// Arguments to pass to function to be called
	unsigned int numArgs1 = readArgs(buf, progInfo);
	for (unsigned int i = 0; i < numArgs1; i++) {
		arg = progInfo.args.back();
		progInfo.args.pop_back();
		
		if (arg != 0xffffffff) {
			fprintf(f, "%#x,", arg);
		} else {
			fprintf(f, "{");
			while ((arg = progInfo.args.back()) != 0xffffffff) {
				fprintf(f, "%#x,", arg);
				progInfo.args.pop_back();
			}
			progInfo.args.pop_back();	// pop off the first 0xffffffff
			fprintf(f, "}, ");
		}
	}
	fprintf(f, "), (");
	// I don't know what these are
	unsigned int numArgs2 = readArgs(buf, progInfo, false);
	for (unsigned int i = 0; i < numArgs2; i++) {
		fprintf(f, "%#x,", progInfo.args.back());
		progInfo.args.pop_back();
	}
	
	// Return type
	unsigned int arg2 = buf.getInt();
	fprintf(f, "), %#x", arg2);

	// Function to call
	StackValue stackTop = progInfo.stack.back();
	progInfo.stack.pop_back();
	fprintf(f, " [%#x] ", stackTop.value);
	
	switch (stackTop.value) {
		// TODO: "understand" what these mean
		case 0x0c:
		case 0x12:
		case 0x13:
		case 0x4c:
		case 0x4d:
		case 0x5a:
		case 0x5b:
		case 0x64:
		case 0x7f:
			if (progInfo.stackPointers.back() == progInfo.stack.size()) {
				fprintf(f, ", %#x", buf.getInt());
				progInfo.stackPointers.pop_back();
			}
		break;
		case 0xffffffff:
			Logger::Log(Logger::WARN, progInfo.address) << " Calling function 0xffffffff.\n";
		break;
		case 0xDEADBEEF:
			Logger::Log(Logger::WARN, progInfo.address) << " Calling deadbeef\n";
	}
	
	// TEMP?
	while (progInfo.stackPointers.back() < progInfo.stack.size()) {
		progInfo.stack.pop_back();
	}
	if (progInfo.stackPointers.size() > 1) {
		progInfo.stackPointers.pop_back();
	} else {
		Logger::Log(Logger::WARN, progInfo.address) << " Popped past.\n";
	}
	
	if (arg2 == 0x0a) {
		progInfo.stack.push_back(StackValue(0xdeadbeef, STACK_NUM));
	} else if (arg2 == 0x14) {
		progInfo.stack.push_back(StackValue(0xdeadbeef, STACK_STR));
	}
}
*/

unsigned int BytecodeParser::parseFunction(const Label &function, ProgStack &localVars, std::string &outputString) {
	if (function.offset >= dataLength)
		throw std::out_of_range("Function offset out of range.");

	currAddress = function.offset;
	bool argsDone = false, toReturn = false;

	unsigned char opcode;

	localVars.clear();
	unsigned int numParams = 0;

	// Read instructions
	while (!toReturn) {
		opcode = getChar();

		switch (opcode) {
			case 0x01: getInt();						break;
			case 0x02: {
				unsigned int type = getInt();
				unsigned int value = getInt();
				stack.push_back(StackValue(value, type));
			} break;
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
					if (!stack.back().endFrame)
						throw std::logic_error("Expected [0x08]");

					stack.pop_back();
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
			case 0x20: {
				StackValue lhs, rhs;
				StackValue val;

				unsigned int LHSType = getInt();
				unsigned int RHSType = getInt();
				unsigned int unknown = getInt();
				if (LHSType != RHSType + 3)
					Logger::Log(Logger::WARN) << "Assigning type " << RHSType << " to " << LHSType << ".\n";

				rhs = stack.back();
				if (rhs.type != RHSType)
					throw std::logic_error("Incorrect type for RHS");
				stack.pop_back();

				val = stack.back();
				stack.pop_back();
				// TODO: check these are all of type 0xa
				if (val.value >> 24 == 0x00) {
					unsigned int index = val.value & 0x00FFFFFF;
					// TODO: implement equality and inequality check
					// and make static values to test against
					if (stack.back().value != 0xFFFFFFFF)
						throw std::logic_error("Expected 0xFFFFFFFF");
					stack.pop_back();

					val = stack.back();
					// Global var
					if (val.value >> 24 == 0x7f) {
						unsigned int varIndex = val.value & 0x00FFFFFF;
						if (varIndex >= sceneInfo.globalVars.size())
							throw std::out_of_range("Variable index out of range");
						StackValue var = sceneInfo.globalVars[varIndex];

						stack.pop_back();
						// this is more important
						if (!stack.back().endFrame)
							throw std::logic_error("Expected [0x08]");

						stack.pop_back();
						lhs = StackValue(var.value, var.type - 1);
						lhs.name = var.name + "[" + std::to_string(index) + "]";
					}
					

				}
				// assign RHS value to LHS

				outputString += "\t" + lhs.name + " = " + rhs.name + ";\n";	// TODO: indentation blocks
			} break;
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
	
	auto functionIt = sceneInfo.functions.begin();

	// TODO: fix naming 
	for (auto &function : localCommands) {
		if (function.name.empty())
			function.name = ("fun_" + std::to_string(function.index));

		std::string parsed;
		ProgStack localVars;
		unsigned int numParams = parseFunction(function, localVars, parsed);

		std::string paramsString;
		for (unsigned int i = 0; i < numParams; i++) {
			if (i != 0) paramsString += ", ";
			paramsString += std::to_string(localVars[i].type) + " " + localVars[i].name;
		}

		fprintf(f, "function %s(%s)@[%#x] {\n", function.name.c_str(), paramsString.c_str(), function.offset);
		fprintf(f, "%s", parsed.c_str());
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
