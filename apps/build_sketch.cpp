#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "csketch/column.hpp"
#include "csketch/compression_map.hpp"

namespace {

struct Args {
  std::string in;
  std::string out;
  std::string dtype;
  uint32_t codes = 1024;
  size_t sample = 10000;
  size_t unique_cutoff = 1;
};

void usage() {
  std::cerr
      << "Usage: build_sketch --in <column.bin> --out <basename> --dtype <u32|u64>\n"
      << "       [--codes N] [--sample N] [--unique-cutoff N]\n"
      << "  --codes: target total codes (default 1024)\n"
      << "  --sample: sampled non-unique values to build ranges (default 10000)\n"
      << "  --unique-cutoff: max frequency to treat value as unique (default 1)\n";
}

template <class T>
T parse_number(const std::string &s, const char *label) {
  size_t idx = 0;
  unsigned long long val = std::stoull(s, &idx, 10);
  if (idx != s.size()) {
    throw std::runtime_error(std::string("invalid numeric value for ") + label);
  }
  if (val == 0) {
    throw std::runtime_error(std::string(label) + " must be > 0");
  }
  if (val > static_cast<unsigned long long>(std::numeric_limits<T>::max())) {
    throw std::runtime_error(std::string(label) + " is too large");
  }
  return static_cast<T>(val);
}

Args parse_args(int argc, char **argv) {
  Args args;
  for (int i = 1; i < argc; ++i) {
    std::string token = argv[i];
    if (token == "--in") {
      if (++i >= argc) throw std::runtime_error("--in requires a value");
      args.in = argv[i];
    } else if (token == "--out") {
      if (++i >= argc) throw std::runtime_error("--out requires a value");
      args.out = argv[i];
    } else if (token == "--dtype") {
      if (++i >= argc) throw std::runtime_error("--dtype requires a value");
      args.dtype = argv[i];
    } else if (token == "--codes") {
      if (++i >= argc) throw std::runtime_error("--codes requires a value");
      args.codes = parse_number<uint32_t>(argv[i], "--codes");
    } else if (token == "--sample") {
      if (++i >= argc) throw std::runtime_error("--sample requires a value");
      args.sample = parse_number<size_t>(argv[i], "--sample");
    } else if (token == "--unique-cutoff") {
      if (++i >= argc) throw std::runtime_error("--unique-cutoff requires a value");
      args.unique_cutoff = parse_number<size_t>(argv[i], "--unique-cutoff");
    } else if (token == "-h" || token == "--help") {
      usage();
      std::exit(0);
    } else {
      throw std::runtime_error("unknown argument: " + token);
    }
  }
  if (args.in.empty() || args.out.empty() || args.dtype.empty()) {
    throw std::runtime_error("--in, --out, and --dtype are required");
  }
  if (args.dtype != "u32" && args.dtype != "u64") {
    throw std::runtime_error("dtype must be u32 or u64");
  }
  return args;
}

} // namespace

int main(int argc, char **argv) {
  try {
    Args args = parse_args(argc, argv);

    std::vector<uint64_t> base64;
    if (args.dtype == "u32") {
      auto v = csketch::read_binary<uint32_t>(args.in);
      base64.assign(v.begin(), v.end());
    } else {
      base64 = csketch::read_binary<uint64_t>(args.in);
    }
    const size_t N = base64.size();
    if (N == 0) {
      throw std::runtime_error("empty input column");
    }

    auto art =
        csketch::NumericCompressionMap::build(base64, args.codes, args.sample,
                                              args.unique_cutoff);

    const uint32_t total_codes = art.total_codes;
    if (total_codes == 0) {
      throw std::runtime_error("map produced zero codes");
    }
    if (total_codes > 65536) {
      throw std::runtime_error("total codes exceed 16-bit storage limit");
    }

    uint32_t code_bits = (total_codes <= 256) ? 8u : 16u;

    std::vector<uint8_t> codes8;
    std::vector<uint16_t> codes16;
    if (code_bits == 8) {
      codes8.resize(N);
    } else {
      codes16.resize(N);
    }

    size_t boundary_hits = 0;
    for (size_t i = 0; i < N; ++i) {
      auto [code, boundary] =
          csketch::NumericCompressionMap::code_of(art, base64[i]);
      if (code_bits == 8) {
        if (code >= 256) {
          throw std::runtime_error("code does not fit in 8 bits");
        }
        codes8[i] = static_cast<uint8_t>(code);
      } else {
        codes16[i] = static_cast<uint16_t>(code);
      }
      if (boundary) {
        ++boundary_hits;
      }
    }

    std::string sketch_path = args.out;
    if (sketch_path.size() < 7 ||
        sketch_path.substr(sketch_path.size() - 7) != ".sketch") {
      sketch_path += ".sketch";
    }
    std::ofstream sk(sketch_path, std::ios::binary);
    if (!sk) {
      throw std::runtime_error("cannot open output sketch");
    }
    if (code_bits == 8) {
      sk.write(reinterpret_cast<const char *>(codes8.data()),
               static_cast<std::streamsize>(codes8.size() * sizeof(uint8_t)));
    } else {
      sk.write(reinterpret_cast<const char *>(codes16.data()),
               static_cast<std::streamsize>(codes16.size() * sizeof(uint16_t)));
    }

    std::string map_path = args.out;
    if (map_path.size() < 9 ||
        map_path.substr(map_path.size() - 9) != ".map.json") {
      map_path += ".map.json";
    }
    csketch::save_map_json(art, map_path, args.dtype, code_bits);

    std::cout << "encoded " << N << " values\n";
    std::cout << "total_codes=" << total_codes << ", code_bits=" << code_bits
              << ", uniques=" << art.uniques.size()
              << ", ranges=" << art.endpoints.size()
              << ", boundary_hits(sample-based)=" << boundary_hits << "\n";
    std::cout << "wrote:\n  " << sketch_path << "\n  " << map_path << "\n";
    return 0;

  } catch (const std::exception &e) {
    std::cerr << "error: " << e.what() << "\n";
    usage();
    return 1;
  }
}

