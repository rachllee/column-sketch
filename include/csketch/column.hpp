#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <fstream>
#include <stdexcept>
#include <type_traits>


namespace csketch {


enum class DType { U32, U64 };


// Generic binary reader/writer for POD integers


template <class T>
std::vector<T> read_binary(const std::string& path) {
static_assert(std::is_integral<T>::value && !std::is_same<T, bool>::value, "Integral T only");
std::ifstream in(path, std::ios::binary | std::ios::ate);
if (!in) throw std::runtime_error("read_binary: cannot open file");
const std::streamsize bytes = in.tellg();
if (bytes % static_cast<std::streamsize>(sizeof(T)) != 0) {
throw std::runtime_error("read_binary: size not multiple of T");
}
std::vector<T> out(bytes / sizeof(T));
in.seekg(0);
if (!out.empty()) in.read(reinterpret_cast<char*>(out.data()), bytes);
return out;
}


template <class T>
void write_binary(const std::string& path, const std::vector<T>& data) {
static_assert(std::is_integral<T>::value && !std::is_same<T, bool>::value, "Integral T only");
std::ofstream out(path, std::ios::binary);
if (!out) throw std::runtime_error("write_binary: cannot open file");
if (!data.empty()) out.write(reinterpret_cast<const char*>(data.data()), data.size()*sizeof(T));
}


} // namespace csketch