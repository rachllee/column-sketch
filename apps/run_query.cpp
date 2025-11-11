#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "csketch/bitvector.hpp"
#include "csketch/column.hpp"
#include "csketch/scan.hpp"

using namespace csketch;

struct Args {
    std::string base_file;   // raw u32/u64
    std::string sketch_file; // .sketch (u8/u16)
    std::string map_json;    // .map.json
    std::string dtype;       // u32/u64
    std::string op;          // lt | eq | between
    uint64_t v1 = 0, v2 = 0; // values
    std::string out_mask;    // output bitvector (.bin)
};

static void usage() {
    std::cerr << "\nUsage: run_query --base FILE --sketch FILE --map FILE --dtype {u32,u64} --op {lt,eq,between} --v1 X [--v2 Y] --out MASK.bin\n\n";
}

static Args parse(int argc, char** argv) {
    Args a;
    for (int i=1;i<argc;++i) {
        std::string s = argv[i];
        auto need = [&](const char* f){ if (i+1>=argc) throw std::runtime_error(std::string("missing value for ")+f); return std::string(argv[++i]); };
        if (s=="--base") a.base_file = need("--base");
        else if (s=="--sketch") a.sketch_file = need("--sketch");
        else if (s=="--map") a.map_json = need("--map");
        else if (s=="--dtype") a.dtype = need("--dtype");
        else if (s=="--op") a.op = need("--op");
        else if (s=="--v1") a.v1 = std::stoull(need("--v1"));
        else if (s=="--v2") a.v2 = std::stoull(need("--v2"));
        else if (s=="--out") a.out_mask = need("--out");
        else if (s=="-h"||s=="--help") { usage(); std::exit(0);} 
        else throw std::runtime_error("unknown arg: "+s);
    }
    if (a.base_file.empty()||a.sketch_file.empty()||a.map_json.empty()||a.dtype.empty()||a.op.empty()||a.out_mask.empty())
        throw std::runtime_error("required args missing");
    if (a.op=="between" && a.v2==0 && a.v2<a.v1) std::swap(a.v1,a.v2);
    return a;
}

static std::vector<uint8_t> slurp(const std::string& path) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) throw std::runtime_error("slurp: cannot open file");
    size_t n = (size_t)in.tellg();
    std::vector<uint8_t> buf(n);
    in.seekg(0); if (n) in.read((char*)buf.data(), n);
    return buf;
}

int main(int argc, char** argv) {
    try {
        Args args = parse(argc, argv);
        LoadedMap L = load_map_json(args.map_json);
        if (args.dtype != L.dtype) std::cerr << "[warn] dtype mismatch: CLI=" << args.dtype << ", map=" << L.dtype << "\n";

        std::vector<uint64_t> base;
        if (args.dtype=="u32") {
            auto v = read_binary<uint32_t>(args.base_file); base.assign(v.begin(), v.end());
        } else {
            base = read_binary<uint64_t>(args.base_file);
        }

        auto raw = slurp(args.sketch_file);
        bool codes16 = (L.code_bits==16);
        size_t n = base.size();
        if ((!codes16 && raw.size()!=n*sizeof(uint8_t)) || (codes16 && raw.size()!=n*sizeof(uint16_t)))
            throw std::runtime_error("sketch length does not match base length");
        const void* codes = raw.data();

        QuerySpec q;
        if (args.op=="lt") q = {QuerySpec::Op::LT, args.v1, 0};
        else if (args.op=="eq") q = {QuerySpec::Op::EQ, args.v1, 0};
        else if (args.op=="between") q = {QuerySpec::Op::BETWEEN, args.v1, args.v2};
        else throw std::runtime_error("unknown --op");

        BitVector mask = scan_predicate(L, codes, codes16, base, q);
        mask.save(args.out_mask);

        std::cout << "rows=" << base.size() << ", matches=" << mask.count() << "\n";
        std::cout << "wrote mask: " << args.out_mask << "\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        usage();
        return 1;
    }
}
