#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <chrono>

#include "csketch/bitvector.hpp"
#include "csketch/column.hpp"
#include "csketch/scan.hpp"

using namespace csketch;

struct Args {
    std::string base_file;
    std::string sketch_file;
    std::string map_json;
    std::string dtype;    // u32/u64
    std::string op;       // lt | eq | between
    uint64_t v1 = 0, v2 = 0;
    std::string csv;      // output CSV path
};

static void usage() {
    std::cerr <<
      "\nUsage: benchmark --base FILE --sketch FILE --map FILE --dtype {u32,u64}\n"
      "                 --op {lt,eq,between} --v1 X [--v2 Y] --csv results/bench.csv\n\n";
}

static Args parse(int argc, char** argv) {
    Args a;
    for (int i=1;i<argc;++i) {
        std::string s = argv[i];
        auto need = [&](const char* f){
            if (i+1>=argc) throw std::runtime_error(std::string("missing value for ")+f);
            return std::string(argv[++i]);
        };
        if (s=="--base") a.base_file = need("--base");
        else if (s=="--sketch") a.sketch_file = need("--sketch");
        else if (s=="--map") a.map_json = need("--map");
        else if (s=="--dtype") a.dtype = need("--dtype");
        else if (s=="--op") a.op = need("--op");
        else if (s=="--v1") a.v1 = std::stoull(need("--v1"));
        else if (s=="--v2") a.v2 = std::stoull(need("--v2"));
        else if (s=="--csv") a.csv = need("--csv");
        else if (s=="-h"||s=="--help") { usage(); std::exit(0); }
        else throw std::runtime_error("unknown arg: "+s);
    }
    if (a.base_file.empty()||a.sketch_file.empty()||a.map_json.empty()||
        a.dtype.empty()||a.op.empty()||a.csv.empty()) {
        throw std::runtime_error("required args missing");
    }
    if (a.op=="between" && a.v2<a.v1) std::swap(a.v1,a.v2);
    return a;
}

static std::vector<uint8_t> slurp(const std::string& path) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) throw std::runtime_error("slurp: cannot open file");
    size_t n = (size_t)in.tellg();
    std::vector<uint8_t> buf(n);
    in.seekg(0);
    if (n) in.read((char*)buf.data(), n);
    return buf;
}

// Baseline full scan — no sketch
static BitVector full_scan(const std::vector<uint64_t>& base, const QuerySpec& q) {
    const size_t N = base.size();
    BitVector out(N);
    if (q.op == QuerySpec::Op::LT) {
        for (size_t i=0;i<N;++i)
            if (base[i] < q.v1) out.set(i);
    } else if (q.op == QuerySpec::Op::EQ) {
        for (size_t i=0;i<N;++i)
            if (base[i] == q.v1) out.set(i);
    } else { // BETWEEN inclusive
        for (size_t i=0;i<N;++i)
            if (base[i] >= q.v1 && base[i] <= q.v2) out.set(i);
    }
    return out;
}

int main(int argc, char** argv) {
    try {
        Args args = parse(argc, argv);

        // Load map + base + sketch
        LoadedMap L = load_map_json(args.map_json);
        if (args.dtype != L.dtype)
            std::cerr << "[warn] dtype mismatch: CLI=" << args.dtype
                      << ", map=" << L.dtype << "\n";

        std::vector<uint64_t> base;
        if (args.dtype=="u32") {
            auto v = read_binary<uint32_t>(args.base_file);
            base.assign(v.begin(), v.end());
        } else {
            base = read_binary<uint64_t>(args.base_file);
        }
        const size_t N = base.size();

        auto raw = slurp(args.sketch_file);
        bool codes16 = (L.code_bits==16);
        if ((!codes16 && raw.size()!=N*sizeof(uint8_t)) ||
            (codes16 && raw.size()!=N*sizeof(uint16_t))) {
            throw std::runtime_error("sketch length does not match base length");
        }
        const void* codes = raw.data();

        // Build query spec
        QuerySpec q;
        if (args.op=="lt") q = {QuerySpec::Op::LT, args.v1, 0};
        else if (args.op=="eq") q = {QuerySpec::Op::EQ, args.v1, 0};
        else if (args.op=="between") q = {QuerySpec::Op::BETWEEN, args.v1, args.v2};
        else throw std::runtime_error("unknown --op");

        // Warm-up (optional) — run once without timing
        (void) scan_predicate(L, codes, codes16, base, q);

        // Time full scan
        auto t0 = std::chrono::steady_clock::now();
        BitVector full = full_scan(base, q);
        auto t1 = std::chrono::steady_clock::now();

        // Time sketch scan
        auto t2 = std::chrono::steady_clock::now();
        BitVector sketch = scan_predicate(L, codes, codes16, base, q);
        auto t3 = std::chrono::steady_clock::now();

        using ms = std::chrono::duration<double, std::milli>;
        double full_ms = std::chrono::duration_cast<ms>(t1-t0).count();
        double sketch_ms = std::chrono::duration_cast<ms>(t3-t2).count();

        uint64_t matches_full = full.count();
        uint64_t matches_sketch = sketch.count();
        if (matches_full != matches_sketch) {
            std::cerr << "[warn] count mismatch full=" << matches_full
                      << " sketch=" << matches_sketch << "\n";
        }

        double speedup = full_ms / sketch_ms;

        // CSV append (create header if new)
        bool exists = false;
        {
            std::ifstream check(args.csv);
            exists = check.good();
        }
        std::ofstream out(args.csv, std::ios::app);
        if (!out) throw std::runtime_error("cannot open csv for append");
        if (!exists) {
            out << "op,dtype,rows,matches,v1,v2,code_bits,time_full_ms,time_sketch_ms,speedup\n";
        }
        out << args.op << "," << args.dtype << ","
            << N << "," << matches_full << ","
            << args.v1 << "," << args.v2 << ","
            << L.code_bits << ","
            << full_ms << "," << sketch_ms << ","
            << speedup << "\n";

        std::cout << "rows=" << N
                  << " matches=" << matches_full
                  << " full_ms=" << full_ms
                  << " sketch_ms=" << sketch_ms
                  << " speedup=" << speedup << "x\n";
        std::cout << "appended to " << args.csv << "\n";

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        usage();
        return 1;
    }
}
