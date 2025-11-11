#include <iostream>
#include <vector>
#include <cstdint>
#include "csketch/column.hpp"
#include "csketch/bitvector.hpp"


int main() {
using namespace csketch;


// Write a small column
std::vector<uint32_t> col = {1,2,3,4,5,6,7,8,9,10};
write_binary<uint32_t>("data_u32.bin", col);


// Read it back
auto col2 = read_binary<uint32_t>("data_u32.bin");
std::cout << "Read " << col2.size() << " u32 values. Last=" << col2.back() << "\n";


// Create a 17-bit bitvector and set a few bits
BitVector bv(17);
bv.set(0); bv.set(3); bv.set(16);
std::cout << "Bit 0=" << bv.get(0) << ", Bit 3=" << bv.get(3) << ", Bit 16=" << bv.get(16) << "\n";
std::cout << "Count before save: " << bv.count() << "\n";


// Save / load
bv.save("mask.bin");
auto bv2 = BitVector::load("mask.bin");
std::cout << "Loaded nbits=" << bv2.size() << ", count=" << bv2.count() << "\n";


return 0;
}