// Parse the bytecode
// A lot of this is stolen from tanuki
// https://github.com/bitprime/vn_translation_tools/blob/master/rewrite/

// TODO: dump dead/unreachable code
// TODO: ifs, switches, for loops (also one instruction jumps/dominations)

#include <cstdio>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <exception>
#include <algorithm>
#include <vector>
#include <set>

#include "Helper.h"
#include "Structs.h"
#include "Logger.h"

#include "BytecodeParser.h"

std::string VarType(unsigned int type) {
	switch (type) {
		case 0x00: return std::string("void");
		case 0x0a: return std::string("int");
		case 0x0b: return std::string("intlist");
		case 0x0d: return std::string("intref");
		case 0x0e: return std::string("intlistref");
		case 0x14: return std::string("str");
		case 0x15: return std::string("strlist");
		case 0x17: return std::string("strref");
		case 0x51e: return std::string("obj");	//unsure
		default:
			return "<0x" + toHex(type) + ">";
	}
}


StackValue StackGetArg(ProgStack &stack, const unsigned int& type, unsigned int address) {
	StackValue arg;
	if (stack.empty())
		throw std::out_of_range("Popping arguments off an empty stack.");

	if (type == 0xa) {
		if (stack.back().type != 0xa) {
			Logger::Error(address) << "Cannot get argument of type int with type " << VarType(stack.back().type) << std::endl;
			throw std::exception();
		}
		arg = stack.back();

		stack.pop_back();
	} else if (type == 0x14) {
		if (stack.back().type != 0x14) {
			Logger::Error(address) << "Cannot get argument of type str with type " << VarType(stack.back().type) << std::endl;
			throw std::exception();
		}
		arg = stack.back();

		stack.pop_back();
	}
	return arg;
}

StackValue StackGetRef(ProgStack &stack, unsigned int address, ProgStack &vars) {
	StackValue val, index;
	ProgStack::reverse_iterator curr, next;

	bool hasIndex = false;

	auto sp = stack.rbegin();
	while (stack.size() > 1 && sp != stack.rend()) {
		curr = sp;
		next = sp+1;

		if (hasIndex) {
			if (curr->value >> 24 == 0x7f) {
				unsigned int varIndex = curr->value & 0x00FFFFFF;
				try {
					val.type = vars.at(varIndex).type - 1;
					//add name here?
				} catch (std::out_of_range &e) {
					std::cerr << "Exception " << e.what() << std::endl;
					val.type = 0xa;
				}
			} else {
				val.type = 0xa;
			}
			val.name = curr->name + "[" + index.name + "]";
			hasIndex = false;
		} else if (val.type == STACK_VOID) {
			val = *curr;
		} else {
			val.name = curr->name + "_" + val.name;
		}

		/*if (curr->value >> 0x24 == 0x7f) {

		} else {

		}*/


		if (next->isIndex()) {
			if (val.type != 0xa) {
				Logger::Error(address) << "Indexing with non int: " << toHex(val.value) << " (" << VarType(val.type) << ")\n";
				throw std::domain_error("");
			}
			hasIndex = true;
			index = val;
			
			sp++;
			stack.pop_back();
			sp++;
			stack.pop_back();
			continue;
		}
		

		sp++;
		stack.pop_back();
		if (next->endFrame())
			break;
	}
	if (!stack.back().endFrame()) {
		Logger::Error(address) << "Could not evaluate.\n";
		throw std::logic_error("");
	}
	stack.pop_back();

	return val;
}

unsigned int getLabelAddress(const std::vector<Label> &labels, unsigned int index, unsigned int address) {
	auto labelIt = std::find_if(labels.begin(), labels.end(), [index](Label l) {
		return l.index == index;
	});

	if (labelIt == labels.end()) {
		Logger::Error(address) << "Could not find label 0x" << std::hex << index << std::endl;
		throw std::logic_error("Could not find label");
	}

	return labelIt->offset;
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

class InstNOP: public Instruction {
	public:
		InstNOP(Parser *parser, unsigned char opcode) : Instruction(parser, opcode) {
		}
		void print(Parser*, std::ofstream &stream) const {
			stream << toHex(address, width) << "\tnop (" << toHex(opcode, 2) << ")\n";
		}
};
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
	std::string name;
	public:
		InstPush(Parser *parser, unsigned char opcode) : Instruction(parser, opcode) {
			type = parser->getInt();
			value = parser->getInt();
			parser->stack.push_back(StackValue(value, type));
			
			if (type == 0xa) {
				unsigned char valType = value >> 24;
				unsigned int index = value & 0x00FFFFFF;

				if (valType == 0x7e && expandCommandNames) {
					try {
						name = parser->sceneInfo.commands.at(index).name;
					} catch (std::out_of_range &e) {
						name = "command" + std::to_string(index);
					}
				} else if (valType == 0x7f && expandGlobalVars) {
					try {
						name = parser->sceneInfo.globalVars.at(index).name;
					} catch (std::out_of_range &e) {
						name = "var" + std::to_string(index);
					}
				} else {
					name = "0x" + toHex(value);
				}
			} else if (type == 0x14) {
				if (expandStrings) {
					try {
						name = "\"" + parser->sceneInfo.mainStrings.at(value) + "\"";
					} catch (std::out_of_range &e) {
						Logger::Error(address) << "String index 0x" << toHex(value) << " out of range.\n";
						name = "string" + std::to_string(value);
					}
				} else {
					name = std::to_string(value);
				}
			} else {
				name = "0x" + toHex(value) + ":" + VarType(type);
			}

			parser->stack.back().name = name;
		}
		void print(Parser*, std::ofstream &stream) const {
			if (type == 0xa) {
				stream << toHex(address, width) << "\tpush " << name;

				unsigned char valType = value >> 24;

				if (valType == 0x7e) {
					stream << ":command\n";
				} else if (valType == 0x7f) {
					stream << ":variable\n";
				} else if (expandStrings) {
					stream << "\n";
				} else {
					stream << ":int\n";
				}
			} else if (type == 0x14) {
				if (expandStrings) {
					stream << toHex(address, width) << "\tpush " << name << "\n";
				} else {
					stream << toHex(address, width) << "\tpush [" << name << "]:str\n";
				}
			} else {
				stream << toHex(address, width) << "\tpush " << name << "\n";
				Logger::Info() << "Pushing 0x" << toHex(value) << " of type 0x" << toHex(type) << "\n";
			}
		}
};
class InstPop: public Instruction {
	unsigned int type;
	public:
		InstPop(Parser *parser, unsigned char opcode) : Instruction(parser, opcode) {
			type = parser->getInt();
			if (parser->stack.empty()) {
				Logger::Error(address) << "Attempting to pop empty stack.\n";
				return;
			}
			if (parser->stack.back().type != type) {
				Logger::Error(address) << "Top of stack is " << parser->stack.back().name << " (type " << VarType(parser->stack.back().type) << "); popping " << VarType(type) << std::endl;
				return;
			}

			parser->stack.pop_back();

		}
		void print(Parser*, std::ofstream &stream) const {
			stream << toHex(address, width) << "\tpop " << VarType(type) << " \n";
		}
};
class InstDup: public Instruction {
	unsigned int type;
	public:
		InstDup(Parser *parser, unsigned char opcode) : Instruction(parser, opcode) {
			type = parser->getInt();

			if (parser->stack.empty()) {
				Logger::Error(address) << "Attempting to duplicate value off empty stack.\n";
				return;
			}
			if (parser->stack.back().type != type) {
				Logger::Error(address) << "Top of stack has type " << VarType(parser->stack.back().type) << "; duplicating " << VarType(type) << std::endl;
				return;
			}

			parser->stack.push_back(parser->stack.back());
		}
		void print(Parser*, std::ofstream &stream) const {
			stream << toHex(address, width) << "\tdup " << VarType(type) << "\n";
		}
};
// Dereferences a reference
// Turns lvalue into rvalue
// Not entirely sure how to interpret
class InstEval: public Instruction {
	public:
		InstEval(Parser *parser, unsigned char opcode) : Instruction(parser, opcode) {
			StackValue val = StackGetRef(parser->stack, address, parser->sceneInfo.globalVars);
			parser->stack.push_back(val);
		}

		void print(Parser*, std::ofstream &stream) const {
			stream << toHex(address, width) << "\teval\n";
		}
};
// Duplicate everything in current frame
class InstRep: public Instruction {
	public:
		InstRep(Parser *parser, unsigned char opcode) : Instruction(parser, opcode) {
			auto stackPointer = parser->stack.rbegin();
			while (stackPointer != parser->stack.rend()) {
				if (stackPointer->endFrame())
					break;
				stackPointer++;
			}
			if (stackPointer == parser->stack.rend()) {
				Logger::Error(address) << "Beginning of frame not found!\n";
				return;
			}

			auto stackEnd = parser->stack.end();

			// stackPointer is not rend, so this is fine
			parser->stack.insert(stackEnd, (stackPointer+1).base(), stackEnd);
		}
		void print(Parser*, std::ofstream &stream) const {
			stream << toHex(address, width) << "\trep\n";
		}
};
class InstSentinel: public Instruction {
	public:
		InstSentinel(Parser* parser, unsigned char opcode) : Instruction(parser, opcode) {
			parser->stack.push_back(StackValue::Sentinel());
		}
		void print(Parser*, std::ofstream &stream) const {
			stream << toHex(address, width) << "\tframe\n";
		}
};
class InstJump: public Instruction {
	bool jumpIfZero = false;
	unsigned int labelIndex = 0;
	BasicBlock* pBlock = nullptr;
	public:
		bool conditional = false;
		unsigned int jumpAddress = 0;
		InstJump(Parser* parser, unsigned char opcode) : Instruction(parser, opcode) {
			if (opcode == 0x10) {
				conditional = false;
			} else if (opcode == 0x11) {
				conditional = true;
				jumpIfZero = true;
			} else if (opcode == 0x12) {
				conditional = true;
				jumpIfZero = false;
			} else {
				throw std::logic_error("Unknown jump");
			}

			labelIndex = parser->getInt();

			if (conditional) {
				if (parser->stack.empty()) {
					Logger::Error(address) << "No values in stack for conditional jump.\n";
					return;
				}
				if (parser->stack.back().type != 0xa) {
					Logger::Error(address) << "Conditional jump - stack top is not int.\n";
					return;
				}
				// TODO: handle this maybe
				parser->stack.pop_back();
			}

			jumpAddress = getLabelAddress(parser->sceneInfo.labels, labelIndex, address);

		}
		void setTarget(BasicBlock* pBlock_) {
			pBlock = pBlock_;
		}
		void print(Parser*, std::ofstream &stream) const {
			std::string mne("jump");
			if (opcode == 0x10)	mne = "jmp";
			else if (opcode == 0x11) mne = "jz";
			else if (opcode == 0x12) mne = "jnz";

			if (pBlock == nullptr)
				stream << toHex(address, width) << "\t" << mne << " @0x" << toHex(jumpAddress) << "\n";
			else
				stream << toHex(address, width) << "\t" << mne << " L" << std::to_string(pBlock->index) << "\n";
		}
};
class InstShortcall: public Instruction {
	unsigned int labelIndex = 0;
	std::vector<unsigned int> paramTypes;
	BasicBlock* pBlock = nullptr;
	public:
		unsigned int fnAddress = 0;

		InstShortcall(Parser *parser, unsigned char opcode) : Instruction(parser, opcode) {
			labelIndex = parser->getInt();
			parser->readArgs(paramTypes);

			fnAddress = getLabelAddress(parser->sceneInfo.labels, labelIndex, address);
		}
		void setTarget(BasicBlock* pBlock_) {
			pBlock = pBlock_;
		}
		void print(Parser*, std::ofstream &stream) const {
			if (pBlock != nullptr)
				stream << toHex(address, width) << "\tshortcall L" << std::to_string(pBlock->index);
			else
				stream << toHex(address, width) << "\tshortcall @0x" << toHex(fnAddress);

			if (paramTypes.empty()) {
				stream << "\n";
			} else {
				stream << " (";
				for (auto it = paramTypes.rbegin(); it != paramTypes.rend(); it++) {
					if (it != paramTypes.rbegin())
						stream << ", ";
					stream << "0x" << toHex(*it);
				}
				stream << ")\n";
			}
		}
};
class InstReturn: public Instruction {
	std::vector<unsigned int> returnTypeList;
	public:
		InstReturn(Parser *parser, unsigned char opcode) : Instruction(parser, opcode) {
			parser->readArgs(returnTypeList);

			parser->stack.clear();
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
// TODO TODO: Handle stack for this
class InstAssign: public Instruction {
	unsigned int LHSType, RHSType;
	unsigned int unknown;

	std::string statement;
	public:
		InstAssign(Parser *parser, unsigned char opcode) : Instruction(parser, opcode) {
			LHSType = parser->getInt();
			RHSType = parser->getInt();
			unknown = parser->getInt();

			std::string lval, rval;
			if (parser->stack.empty()) {
				Logger::Error(address) << "Assigning off empty stack.\n";
				return;
			}
			// Read rhs
			StackValue val = parser->stack.back();
			if (val.type != RHSType) {
				Logger::Error(address) << "Expected type " << VarType(RHSType) << "; got " << VarType(val.type) << std::endl;
				return;
			}
			rval = val.name;
			parser->stack.pop_back();
			// Read lhs
			val = StackGetRef(parser->stack, address, parser->sceneInfo.globalVars);
			lval = val.name;

			statement = lval + " = " + rval;
		}
		void print(Parser*, std::ofstream &stream) const {
			stream << toHex(address, width) << "\tassign " << VarType(LHSType) << " " << VarType(RHSType) << " " << std::to_string(unknown);
			stream << "\t; " << statement << std::endl;
		}
};
class Inst21: public Instruction {
	unsigned int unknown1;
	unsigned char unknown2;
	public:
		Inst21(Parser *parser, unsigned char opcode) : Instruction(parser, opcode) {
			unknown1 = parser->getInt();
			unknown2 = parser->getChar();
		}
		void print(Parser*, std::ofstream &stream) const {
			stream << toHex(address, width) << "\t[21] 0x" << toHex(unknown1) << " 0x" << toHex(unknown2) << "\n";
		}
};
class InstCalc : public Instruction {
	unsigned int LHSType, RHSType;
	unsigned char operation;
	public:
		InstCalc(Parser *parser, unsigned char opcode) : Instruction(parser, opcode) {
			LHSType = parser->getInt();
			RHSType = parser->getInt();
			operation = parser->getChar();

			if (parser->stack.size() < 2) {
				Logger::Error(address) << "Not enough values in stack to calculate.\n";
				return;
			}

			if (LHSType != RHSType) {
				Logger::Warn(address) << "Comparing type " << VarType(LHSType) << " to " << VarType(RHSType) << " with operation " << toHex(operation, 2) << std::endl;
				return;
			}
			StackValue val2 = parser->stack.back(); parser->stack.pop_back();
			StackValue val1 = parser->stack.back(); parser->stack.pop_back();
			if (val1.type != LHSType) {
				Logger::Error(address) << "Expected type " << VarType(LHSType) << " for var1; got " << VarType(val1.type) << " ("+val1.name+")" << std::endl;
			}
			if (val2.type != RHSType) {
				Logger::Error(address) << "Expected type " << VarType(RHSType) << " for var2; got " << VarType(val2.type) << " ("+val2.name+")" << std::endl;
			}

			StackValue val;
			if (LHSType == 0xa && RHSType == 0xa) {
				switch (operation) {
					case  1: val = StackValue(STACK_NUM, val1.name + " + " + val2.name); break;
					case  2: val = StackValue(STACK_NUM, val1.name + " - " + val2.name); break;
					case  3: val = StackValue(STACK_NUM, val1.name + " * " + val2.name); break;
					case  4: val = StackValue(STACK_NUM, val1.name + " / " + val2.name); break;
					case 16: val = StackValue(STACK_NUM, val1.name + " != " + val2.name); break;
					case 17: val = StackValue(STACK_NUM, val1.name + " < " + val2.name); break;
					case 18: val = StackValue(STACK_NUM, val1.name + " <= " + val2.name); break;
					case 19: val = StackValue(STACK_NUM, val1.name + " > " + val2.name); break;
					case 20: val = StackValue(STACK_NUM, val1.name + " >= " + val2.name); break;
					case 32: val = StackValue(STACK_NUM, val1.name + " && " + val2.name); break;
					default: val = StackValue(STACK_NUM, val1.name + " <op" + std::to_string(operation) + "> " + val2.name); break;
				}
			} else if (LHSType == 0x14 && RHSType == 0x14) {
				switch (operation) {
					case  1: val = StackValue(STACK_STR, val1.name + " + " + val2.name); break;
					case 16: val = StackValue(STACK_NUM, val1.name + " != " + val2.name); break;
					default: val = StackValue(STACK_STR, val1.name + " <op" + std::to_string(operation) + "> " + val2.name); break;
				}
			} else {
				val = StackValue(LHSType, val1.name + " <op" + std::to_string(operation) + "> " + val2.name);
			}

			parser->stack.push_back(val);

		}
		void print(Parser*, std::ofstream &stream) const {
			std::string op;
			if (LHSType == 0xa && RHSType == 0xa) {
				switch (operation) {
					case  1: op = "add";	break;
					case  2: op = "sub"; 	break;
					case  3: op = "mult"; 	break;
					case  4: op = "div"; 	break;
					case 16: op = "neq"; 	break;
					case 17: op = "lt"; 	break;
					case 18: op = "leq"; 	break;
					case 19: op = "gt";		break;
					case 20: op = "geq";	break;
					case 32: op = "and";	break;
				}
			} else if (LHSType == 0x14 && RHSType == 0x14) {
				switch (operation) {
					case  1: op = "concat";	break;
				}
			}
			if (op.empty())
				stream << toHex(address, width) << "\tcalc " << VarType(LHSType) << " " << VarType(RHSType) << " " << std::to_string(operation) << std::endl;
			else
				stream << toHex(address, width) << "\tcalc " << op << "\n";
		}
};
// TODO: add the popping of the return
class InstCall: public Instruction {
	unsigned int fnCall = 0;
	unsigned int fnExtra = 0;

	unsigned int fnOption;
	std::vector<unsigned int> paramTypes, extraTypes;
	ProgStack args;
	unsigned int returnType;
	public:
		InstCall(Parser *parser, unsigned char opcode) : Instruction(parser, opcode) {
			fnOption = parser->getInt();

			parser->readArgs(paramTypes);
			parser->readArgs(extraTypes);

			returnType = parser->getInt();

			// Read args off stack
			for (const auto &type:paramTypes) {
				args.push_back(StackGetArg(parser->stack, type, address));
			}

			// Read function to call (temp) TODO
			while (!parser->stack.empty()) {
				if (parser->stack.back().endFrame())
					break;

				fnCall = parser->stack.back().value;
				parser->stack.pop_back();
			}
			if (parser->stack.empty()) {
				Logger::Error(address) << "Empty stack when getting function.\n";
			} else {
				parser->stack.pop_back();
			}

			// Determine if an extra term is needed
			if (fnCall == 0x12)
				fnExtra = parser->getInt();

			// Push return value
			if (fnCall != 0x54) {
				parser->stack.push_back(StackValue(returnType, std::string("function(") + "args go here" + ")"));
			}

			Logger::VVDebug(address) << "Calling function 0x" << toHex(fnCall) << std::endl;
		}
		void print(Parser*, std::ofstream &stream) const {
			stream << toHex(address, width) << "\tcall 0x" << toHex(fnCall) << " " << std::to_string(fnOption) << " (";
			for (auto it = args.rbegin(); it != args.rend(); it++) {
				if (it != args.rbegin())
					stream << ", ";
				stream << it->name << ":" << VarType(it->type);
			}
			if (!extraTypes.empty()) {
				stream << ") ((";
				for (auto it = extraTypes.rbegin(); it != extraTypes.rend(); it++) {
					if (it != extraTypes.rbegin())
						stream << ", ";
					stream << VarType(*it);
				}
				stream << ")) ⇒ " << VarType(returnType) << std::endl;
			} else {
				stream << ") ⇒ " << VarType(returnType) << std::endl;
			}
		}
};

// TODO: add stack handle for these
// Appends line
class InstSetLine: public Instruction {
	unsigned int count = 0;
	std::string text;
	public:
		InstSetLine(Parser *parser, unsigned char opcode) : Instruction(parser, opcode) {
			count = parser->getInt();
			if (parser->stack.empty()) {
				Logger::Error(address) << "Stack empty - append line.\n";
				return;
			}
			if (parser->stack.back().type != STACK_STR) {
				Logger::Error(address) << "Appending non-string to text.\n";
				return;
			}
			text = parser->stack.back().name;
			parser->stack.pop_back();
		}
		void print(Parser*, std::ofstream &stream) const {
			stream << toHex(address, width) << "\taddtext 0x" << toHex(count);
			if (!text.empty())
				stream << "\t;" << text;
			stream << "\n";
		}
};
// Sets name
class InstSetName: public Instruction {
	std::string name;
	public:
		InstSetName(Parser *parser, unsigned char opcode) : Instruction(parser, opcode) {
			if (parser->stack.empty()) {
				Logger::Error(address) << "Stack empty - set name.\n";
				return;
			}
			if (parser->stack.back().type != STACK_STR) {
				Logger::Error(address) << "Cannot set type " << VarType(parser->stack.back().type) << " as name.\n";
				return;
			}
			name = parser->stack.back().name;
			parser->stack.pop_back();
		}
		void print(Parser*, std::ofstream &stream) const {
			stream << toHex(address, width) << "\tsetname";
			if (!name.empty())
				stream << "\t;" << name;
			stream << "\n";
		}
};


Instruction* Instruction::newInst(Parser *parser, unsigned char opcode) {
	switch (opcode) {
		case 0x01: return new InstLine(parser, opcode);
		case 0x02: return new InstPush(parser, opcode);
		case 0x03: return new InstPop(parser, opcode);
		case 0x04: return new InstDup(parser, opcode);
		case 0x05: return new InstEval(parser, opcode);
		case 0x06: return new InstRep(parser, opcode);
		case 0x08: return new InstSentinel(parser, opcode);
		case 0x10: 
		case 0x11: 
		case 0x12: return new InstJump(parser, opcode);
		case 0x13: return new InstShortcall(parser, opcode);
		case 0x15: return new InstReturn(parser, opcode);
		case 0x16: return new InstEndScript(parser, opcode);
		case 0x20: return new InstAssign(parser, opcode);
		case 0x21: return new Inst21(parser, opcode);
		case 0x22: return new InstCalc(parser, opcode);
		case 0x30: return new InstCall(parser, opcode);
		case 0x31: return new InstSetLine(parser, opcode);
		case 0x32: return new InstSetName(parser, opcode);
		default:	
			Logger::Error(parser->instAddress) << "Unknown instruction 0x" << toHex(opcode)<< std::endl;
			return new InstNOP(parser, opcode);
	}
}

void BytecodeParser::parseBytecode() {
	std::vector<ProgBranch> toTraverse;
	std::vector<Label> labels = sceneInfo.labels;

	for (auto &entrypoint:sceneInfo.markers) {
		if (entrypoint.offset > 0) {
			toTraverse.push_back(ProgBranch(entrypoint.offset));
		}

		// maybe make block here and attach pointer to label?
	}
	toTraverse.push_back(ProgBranch(0x0));

	while (!toTraverse.empty()) {
		ProgBranch branch = toTraverse.back();
		toTraverse.pop_back();
		
		// Create block if not existing
		BasicBlock* pBlock = addBlock(branch.prev, branch.address);
		if (branch.jump != nullptr)
			branch.jump->setTarget(pBlock);

		if (pBlock == nullptr) {
			Logger::VDebug() << "0x" << toHex(branch.address) << ": Block already parsed.\n";
			continue;
		}
		// New block
		Logger::VDebug() << "0x" << toHex(branch.address) << ": Parsing new branch.\n";

		// Make labels point to this block
		for (auto& label:labels) {
			if (label.offset == branch.address) {
				label.pBlock = pBlock;
				pBlock->labels.insert(label.index);
			}
		}
		// Set instruction pointer
		buf->setAddress(branch.address);

		// Restore stack
		stack = branch.stack;

		// Read instructions until a return/endofscript is reached
		// Create new blocks and push onto list as necessary
		unsigned char opcode;
		unsigned int nextAddress;
		while (true) {
			instAddress = buf->getAddress();
			opcode = getChar();

			// Handle instruction
			Instruction* pInst = Instruction::newInst(this, opcode);
			pBlock->pushInst(pInst);

			nextAddress = buf->getAddress();

			if (opcode == 0x13) {
				pBlock->blockType = Block::CALL;
				InstShortcall* pCall = static_cast<InstShortcall*>(pInst);

				callStack.push_back(CallRet(nextAddress, pBlock, stack.size()));

				BasicBlock* pCallBlock = addBlock(pBlock, pCall->fnAddress, pCall);
				pCallBlock->isFunction = true;

				pBlock = pCallBlock;
				buf->setAddress(pCall->fnAddress);

			} else if (opcode == 0x15) {
				pBlock->blockType = Block::RET;
				// add adddress to dead code
				if (callStack.empty()) {
					// We are done with this branch.
					break;
				} else {
					CallRet ret = callStack.back();
					if (stack.size() != ret.stackPointer) {
						Logger::Error(instAddress) << "Call returning to 0x" << toHex(ret.address) << " modified stack pointer from 0x" << toHex(ret.stackPointer) << " to 0x" << toHex(stack.size()) << std::endl;
					}

					stack.push_back(StackValue(0xDEADBEEF, STACK_NUM));
					buf->setAddress(ret.address);
					pBlock = addBlock(ret.pBlock, ret.address);

					callStack.pop_back();
				}
			} else if (opcode == 0x16) {
				pBlock->blockType = Block::RET;
				//if (!buf->done) {
				//	add address
				//}
				break;
			} else if (opcode >= 0x10 && opcode <= 0x12) {
				InstJump* pJump = static_cast<InstJump*>(pInst);
				if (pJump->conditional) {
					pBlock->blockType = Block::TWOWAY;

					ProgBranch branch(pJump->jumpAddress, stack);
					std::vector<ProgBranch> a;
					a.push_back(branch);
					toTraverse.push_back(branch);
					toTraverse.back().prev = pBlock;
					toTraverse.back().jump = pJump;

					pBlock = addBlock(pBlock, nextAddress);
				} else {
					pBlock->blockType = Block::ONEWAY;

					// add next address to unread list
					pBlock = addBlock(pBlock, pJump->jumpAddress, pJump);

					buf->setAddress(pJump->jumpAddress);
				}

				// If target has been parsed, this branch is done
				if (pBlock == nullptr) {
					break;
				}
			}
			// maybe do something about calls (in this file) and 0x13 instruction
			// not necessary if there is no recursion though

			// Check to see if should start a new block - at new instruction pointer
			std::vector<unsigned int> labelIndices;
			instAddress = buf->getAddress();
			for (auto& label:labels) {
				if (label.offset == instAddress) {
					labelIndices.push_back(label.index);
				}
			}
			if (!labelIndices.empty()) {
				// Only create a new block if current block not empty
				if (instAddress != pBlock->startAddress) {
					pBlock =  addBlock(pBlock, instAddress);

					if (pBlock == nullptr) {
						pBlock->blockType = Block::FALL;
						break;
					}
				}

				for (const auto &index:labelIndices) {
					labels.at(index).pBlock = pBlock;
					pBlock->labels.insert(index);
				}
			}
		}
	}

	// for each address in unread
	// if address is one of the block starts, remove it
	// otherwise read until the next exit (adding next instruction to unread)
}


void BytecodeParser::printInstructions(std::string filename, bool sorted) {
	// Get functions defined in this file
	/* std::vector<Label> localCommands;
	auto predCommandFile = [this](ScriptCommand c) {
		return c.file == sceneInfo.thisFile;
	}; */

	std::ofstream out(filename);

	if (sorted) {
		auto sorter = [](BasicBlock* pBlock1, BasicBlock* pBlock2) {
			return (pBlock1->startAddress < pBlock2->startAddress);
		};
		std::sort(blocks.begin(), blocks.end(), sorter);
	}
	for (const auto &block:blocks) {
		block->printInstructions(this, out);
	}

	out.close();
}

void BytecodeParser::dumpCFG(std::string filename) {
	std::ofstream out(filename);
	// should be same - check
	out << "strict digraph " << "CFG" << " {\n";
	out << "\tnode [shape=box]\n";
	for (const auto &block:blocks) {
		switch (block->blockType) {
			case Block::CALL: out << "\tBlock" << std::to_string(block->index) << " [color=red]\n"; break;
			case Block::RET: out << "\tBlock" << std::to_string(block->index) << " [color=blue]\n"; break;
			default: break;
		}

		out << "\tBlock" << std::to_string(block->index) << " -> {";
		for (auto p = block->succ.begin(); p != block->succ.end(); p++) {
			if (p != block->succ.begin())
				out << "; ";
			out << "Block" << std::to_string((*p)->index);
		}
		out << "}\n";
	}

	out << "}\n";

	out.close();
}



BytecodeParser::BytecodeParser(std::ifstream &f, HeaderPair index, SceneInfo info) {
	f.seekg(index.offset, std::ios_base::beg);
	buf = new BytecodeBuffer(f, index.count);

	sceneInfo = info;
	Instruction::setWidth(index.count);
}

BytecodeParser::~BytecodeParser() {
	delete buf;


	for (auto &block:blocks) {
		delete block;
	}
}

std::set<unsigned int> BasicBlock::blockAddresses;
unsigned int BasicBlock::count;
BasicBlock::~BasicBlock() {
	for (auto &inst:instructions) {
		delete inst;
	}
}

void BasicBlock::simplifyInstructions(Parser* parser) {

}


void BasicBlock::printInstructions(Parser* parser, std::ofstream &out) {
	// All entrypoints must be starts of blocks
	for (auto &entrypoint:parser->sceneInfo.markers) {
		if (entrypoint.offset > 0 && entrypoint.offset == startAddress) {
			out << "Entrypoint " << std::to_string(entrypoint.index) << ":\n";
		}
	}
	/*if (!labels.empty()) {
		out << "Label";
		for (const auto& label:labels) {
			out << " " << std::to_string(label);
		}
		out << ":\n";
	}*/
	out << "L" << std::to_string(index) << " @0x" << toHex(startAddress) << ":\n";
	for (const auto &inst:instructions) {
		if (inst->toPrint)
			inst->print(parser, out);
	}
	out << std::endl;
}

// Bytecode buffer
// Handles instruction address and getting values

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

bool BytecodeParser::isParsed(unsigned int address) {
	return std::binary_search(BasicBlock::blockAddresses.begin(), BasicBlock::blockAddresses.end(), address);
	//return (blocks.end() != std::find_if(blocks.begin(), blocks.end(), [address](BasicBlock* pBlock) {
	//	return (pBlock->startAddress == address);
	//}));
}

// Returns nullptr if block already exists
// maybe undo the set target thing, find a way to handle that later
BasicBlock* BytecodeParser::addBlock(BasicBlock* pBlock, unsigned int address, Instruction* inst) {
	BasicBlock* pNextBlock;
	auto blockIt = std::find_if(blocks.begin(), blocks.end(), [address](BasicBlock* pb) {
		return (pb->startAddress == address);
	});
	if (blockIt != blocks.end()) {
		pNextBlock = *blockIt;
		if (pBlock != nullptr) {
			pNextBlock->prec.insert(pBlock);
			pBlock->succ.insert(pNextBlock);
		}
		if (inst)
			inst->setTarget(pNextBlock);

		return nullptr;
	}
	pNextBlock = new BasicBlock(address);
	blocks.push_back(pNextBlock);
	if (pBlock != nullptr) {
		pNextBlock->prec.insert(pBlock);
		pBlock->succ.insert(pNextBlock);
	}
	if (inst)
		inst->setTarget(pNextBlock);
	return pNextBlock;
}
