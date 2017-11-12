#include <iostream>
#include <fstream>
#include <vector>

#include "Structs.h"

typedef std::vector<std::string> StringList;

unsigned int readUInt32(unsigned char* buf);
unsigned int readUInt32(char* buf);
void readHeaderPair(std::ifstream &stream, HeaderPair &pair);
void readHeaderPair(unsigned char* buf, HeaderPair &pair);

StringList readStrings(std::ifstream &f, HeaderPair index, HeaderPair data, bool decode = false);
void printStrings(StringList strings, std::ostream &f = std::cout);

void decodeData(unsigned char* debuf, unsigned int desize);
void decompressData(unsigned char* compData, unsigned char* decompBegin, unsigned int decompSize);