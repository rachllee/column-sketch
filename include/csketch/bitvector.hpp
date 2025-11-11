#pragma once

#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace csketch {

class BitVector {
public:
  BitVector() = default;
  explicit BitVector(uint64_t nbits) { resize(nbits); }

  void resize(uint64_t nbits) {
    nbits_ = nbits;
    words_.assign(words_for(nbits), 0);
  }

  uint64_t size() const { return nbits_; }

  void set(uint64_t i, bool bit = true) {
    if (i >= nbits_) {
      throw std::out_of_range("BitVector::set index");
    }
    const uint64_t w = i >> 6;
    const uint64_t b = i & 63ULL;
    const uint64_t mask = 1ULL << b;
    if (bit) {
      words_[w] |= mask;
    } else {
      words_[w] &= ~mask;
    }
  }

  bool get(uint64_t i) const {
    if (i >= nbits_) {
      throw std::out_of_range("BitVector::get index");
    }
    const uint64_t w = i >> 6;
    const uint64_t b = i & 63ULL;
    return (words_[w] >> b) & 1ULL;
  }

  uint64_t count() const {
    uint64_t c = 0;
    for (uint64_t w : words_) {
      c += popcount64(w);
    }
    return c;
  }

  const std::vector<uint64_t> &words() const { return words_; }
  std::vector<uint64_t> &words() { return words_; }

  void save(const std::string &path) const {
    std::ofstream out(path, std::ios::binary);
    if (!out) {
      throw std::runtime_error("BitVector::save cannot open file");
    }
    out.write(reinterpret_cast<const char *>(&nbits_), sizeof(nbits_));
    if (!words_.empty()) {
      out.write(reinterpret_cast<const char *>(words_.data()),
                words_.size() * sizeof(uint64_t));
    }
  }

  static BitVector load(const std::string &path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
      throw std::runtime_error("BitVector::load cannot open file");
    }
    uint64_t nbits = 0;
    in.read(reinterpret_cast<char *>(&nbits), sizeof(nbits));
    if (!in) {
      throw std::runtime_error("BitVector::load truncated header");
    }
    BitVector bv(nbits);
    if (!bv.words_.empty()) {
      in.read(reinterpret_cast<char *>(bv.words_.data()),
              bv.words_.size() * sizeof(uint64_t));
      if (!in) {
        throw std::runtime_error("BitVector::load truncated payload");
      }
    }
    return bv;
  }

private:
  static inline uint64_t words_for(uint64_t nbits) {
    return (nbits + 63ULL) >> 6;
  }

  static inline uint64_t popcount64(uint64_t x) {
#if defined(__GNUG__) || defined(__clang__)
    return __builtin_popcountll(x);
#else
    x = x - ((x >> 1) & 0x5555555555555555ULL);
    x = (x & 0x3333333333333333ULL) + ((x >> 2) & 0x3333333333333333ULL);
    x = (x + (x >> 4)) & 0x0F0F0F0F0F0F0F0FULL;
    return (x * 0x0101010101010101ULL) >> 56;
#endif
  }

  uint64_t nbits_ = 0;
  std::vector<uint64_t> words_;
};

} // namespace csketch

