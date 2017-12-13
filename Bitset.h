#ifndef BITSET_H
#define BITSET_H

#include <vector>
#include <limits>

// Quick and dirty bitset (bit set? BitSet?)
// Could use boost (should) but 
class Bitset {
	std::vector<uint32_t> arr;
	unsigned int size;
	unsigned int arrSize;
	public:
		Bitset() : size(0), arrSize(0) {}
		Bitset(unsigned int size_, bool set=false);
		~Bitset();

		Bitset operator &(const Bitset& b2);
		Bitset& operator &=(const Bitset& other);
		bool operator ==(const Bitset& other);
		bool operator !=(const Bitset& other);

		void clear();
		void set(unsigned int pos);
		bool get(unsigned int pos);
};

#endif