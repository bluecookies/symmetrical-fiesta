#include "Bitset.h"

Bitset::Bitset(unsigned int size_, bool set) : size(size_) {
	arrSize = (size + 31)/32;	// ceil(size/32)
	arr.resize(arrSize, - (unsigned int) set);
}

Bitset::~Bitset() {
	// delete array
}

void Bitset::clear() {
	std::fill(arr.begin(), arr.end(), 0);
}

void Bitset::set(unsigned int pos) {
	if (pos < size)
		arr[pos/32] |= 1 << (pos % 32);
}

bool Bitset::get(unsigned int pos) {
	if (pos < size)
		return (arr[pos/32] & 1 << (pos % 32));

	return false;
}

Bitset Bitset::operator &(const Bitset& b2) {
	Bitset b = Bitset(size);
	for (unsigned int i = 0; i < arrSize; i++)
		b.arr[i] = (arr[i] & b2.arr[i % b2.arrSize]);
	return b;
}


// will give funky results not aligned to 32 bits
// only intended for same size anyway, so I don't really care
Bitset& Bitset::operator &=(const Bitset& other) {
	for (unsigned int i = 0; i < arrSize; i++)
		arr[i] &= other.arr[i % other.arrSize];

	return *this;
}

bool Bitset::operator ==(const Bitset& other) {
	if (size != other.size)
		return false;

	for (unsigned int i = 0; i < arrSize - 1; i++) {
		if (arr[i] != other.arr[i])
			return false;
	}
	if (arrSize == 0)
		return true;

	unsigned char numBits = 32 - size % 32;
	return (arr[arrSize - 1] << numBits) == (other.arr[arrSize - 1] << numBits);

}

bool Bitset::operator !=(const Bitset& other) {
	return !(*this == other);
}

