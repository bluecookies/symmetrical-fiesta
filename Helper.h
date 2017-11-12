#include <fstream>

#include "Structs.h"

unsigned int readUInt32(unsigned char* buf);
unsigned int readUInt32(char* buf);
void readHeaderPair(std::ifstream &stream, HeaderPair &pair);
void readHeaderPair(unsigned char* buf, HeaderPair &pair);

void decrypt2(unsigned char* debuf, unsigned int desize);
void decompressData(unsigned char* compData, unsigned char* decompBegin, unsigned int decompSize);