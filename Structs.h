#ifndef STRUCTS_H
#define STRUCTS_H

#include <cstdint>

struct HeaderPair {
	uint32_t offset;
	uint32_t count;
};

// Pack file index
struct FileInfo {
	uint64_t offset;
	uint64_t length;
};

struct ScenePackHeader{
	uint32_t headerSize;
	HeaderPair varInfo;
	HeaderPair varNameIndex;
	HeaderPair varName;
	HeaderPair cmdInfo;
	HeaderPair cmdNameIndex;
	HeaderPair cmdName;
	HeaderPair sceneNameIndex;
	HeaderPair sceneName;
	HeaderPair sceneInfo;
	HeaderPair sceneData;
	uint32_t extraKeyUse;
	uint32_t sourceHeaderLength;
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

struct PackHeader {
	uint32_t v1;
	uint32_t numFiles;
	uint32_t fileInfoOffset1;
	uint32_t fileInfoOffset2;
	uint32_t a1;
	uint32_t a2;
	uint32_t a3;
	uint32_t a4;
};

#endif
