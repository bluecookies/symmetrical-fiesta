#ifndef STRUCTS_H
#define STRUCTS_H

#include <cstdint>

// Structure of pack files
// most of it figured out by Kelebek

struct HeaderPair {
	uint32_t offset = 0;
	uint32_t count = 0;
};

// Pack file index
struct FileInfo {
	uint64_t offset;
	uint64_t length;
};

struct ScenePackHeader{
	uint32_t headerSize = 0;
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
	uint32_t extraKeyUse = 0;
	uint32_t sourceHeaderLength = 0;
};

struct ScriptHeader {
	uint32_t headerSize;
	HeaderPair bytecode;
	HeaderPair stringIndex;
	HeaderPair stringData;
	HeaderPair labels;
	HeaderPair markers;
	HeaderPair localCommandIndex;     // Function ID + Bytecode offset
	HeaderPair unknown1;		// could be something to do with function (address/param/type?)
	HeaderPair staticVarIndex;
	HeaderPair staticVars;
	HeaderPair functions;
	HeaderPair functionNameIndex;
	HeaderPair functionName;
	HeaderPair varStringIndex;
	HeaderPair varStringData;
	HeaderPair unknown6;   // 
	HeaderPair unknown7;   // 
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
