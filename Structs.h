#ifndef STRUCTS_H
#define STRUCTS_H

#include <cstdint>

struct HeaderPair {
	uint32_t offset;
	uint32_t count;
};

struct FileInfo {
	uint32_t offset;
	uint32_t length;
};

struct PackHeader{
	uint32_t headerLength;
	HeaderPair VarInfo;
	HeaderPair VarNameIndex;
	HeaderPair VarName;
	HeaderPair CmdInfo;
	HeaderPair CmdNameIndex;
	HeaderPair CmdName;
	HeaderPair SceneNameIndex;
	HeaderPair SceneName;
	HeaderPair SceneInfo;
	HeaderPair SceneData;
	uint32_t ExtraKeyUse;
	uint32_t SourceHeaderLength;
};

struct ScriptHeader {
	uint32_t headerSize;
	HeaderPair bytecode;
	HeaderPair stringIndex;
	HeaderPair stringData;
	HeaderPair labels;
	HeaderPair markers;
	HeaderPair functionIndex;     // Function ID + Bytecode offset
	HeaderPair unknown1;		// could be something to do with function (address/param?)
	HeaderPair stringsIndex1;    // string index
	HeaderPair strings1;    // string data
	HeaderPair functions;
	HeaderPair functionNameIndex;
	HeaderPair functionName;
	HeaderPair stringsIndex2;   // string index
	HeaderPair strings2;   // string data
	HeaderPair unknown6;   // looks to be EOF
	HeaderPair unknown7;   // same?
};

#endif
