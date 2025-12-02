#pragma once
#include <algorithm>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "csketch/bitvector.hpp"
#include "csketch/compression_map.hpp"

namespace csketch {

struct QuerySpec {
    enum class Op { LT, EQ, BETWEEN };
    Op op;
    uint64_t v1 = 0;   // for LT/EQ: the value; for BETWEEN: low
    uint64_t v2 = 0;   // for BETWEEN: high
};

struct LoadedMap {
    MapArtifacts art;
    std::string dtype;    // "u32" or "u64"
    uint32_t code_bits;   // 8 or 16
};

// --- Minimal JSON loader for .map.json (no external deps) ---
inline LoadedMap load_map_json(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("load_map_json: cannot open file");
    std::string s((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

    auto find_str = [&](const std::string& key)->std::string {
        auto k = s.find("\""+key+"\"");
        if (k==std::string::npos) return {};
        auto c = s.find(':', k); if (c==std::string::npos) return {};
        auto a = s.find_first_not_of(" \t\r\n\"", c+1);
        auto b = s.find_first_of(",\n}", a);
        return s.substr(a, b-a);
    };
    auto find_array = [&](const std::string& key)->std::vector<uint64_t> {
        std::vector<uint64_t> out;
        auto k = s.find("\""+key+"\"");
        if (k==std::string::npos) return out;
        auto lb = s.find('[', k); auto rb = s.find(']', lb);
        if (lb==std::string::npos || rb==std::string::npos || rb<lb) return out;
        size_t i = lb+1;
        while (i < rb) {
            while (i<rb && (s[i]==' '||s[i]=='\n'||s[i]=='\r'||s[i]=='\t'||s[i]==',')) ++i;
            if (i>=rb) break;
            size_t j=i; while (j<rb && (s[j]>='0'&&s[j]<='9')) ++j;
            if (j>i) { out.push_back(std::stoull(s.substr(i,j-i))); i=j; }
            else ++i;
        }
        return out;
    };

    LoadedMap L;
    std::string dtype = find_str("dtype");
    if (dtype.find("u32")!=std::string::npos) L.dtype = "u32"; else L.dtype = "u64";
    L.code_bits = static_cast<uint32_t>(std::stoul(find_str("code_bits")));
    L.art.total_codes = static_cast<uint32_t>(std::stoul(find_str("total_codes")));
    L.art.uniques = find_array("uniques");
    L.art.endpoints = find_array("endpoints");
    return L;
}

// ---------------------------------------------------------------------
//  Fast path: specialized scan for 8-bit codes (tight loops, no lambdas)
// ---------------------------------------------------------------------

inline BitVector scan_predicate_8bit(const LoadedMap& L,
                                     const uint8_t* codes,
                                     const std::vector<uint64_t>& base,
                                     const QuerySpec& q) {
    const size_t N = base.size();
    BitVector out(N);

    auto code_of_v = [&](uint64_t v){ return NumericCompressionMap::code_of(L.art, v); };

    uint32_t c1=0, c2=0;
    if (q.op == QuerySpec::Op::LT || q.op == QuerySpec::Op::EQ) {
        c1 = code_of_v(q.v1).first;
    } else {
        c1 = code_of_v(q.v1).first;
        c2 = code_of_v(q.v2).first;
        if (c2 < c1) std::swap(c1,c2);
    }

    if (q.op == QuerySpec::Op::LT) {
        // value < v1: definite matches have code < c1; boundary bucket c1 needs base probe
        const uint32_t c1_local = c1;
        const uint64_t v1_local = q.v1;

        // Simple unrolled loop (4-at-a-time) to help auto-vectorization
        size_t i = 0;
        size_t limit = N & ~size_t(3); // round down to multiple of 4
        for (; i < limit; i += 4) {
            uint8_t c0 = codes[i];
            uint8_t c1c = codes[i+1];
            uint8_t c2c = codes[i+2];
            uint8_t c3c = codes[i+3];

            if (c0 < c1_local || (c0 == c1_local && base[i] < v1_local))     out.set(i);
            if (c1c < c1_local || (c1c == c1_local && base[i+1] < v1_local)) out.set(i+1);
            if (c2c < c1_local || (c2c == c1_local && base[i+2] < v1_local)) out.set(i+2);
            if (c3c < c1_local || (c3c == c1_local && base[i+3] < v1_local)) out.set(i+3);
        }
        for (; i < N; ++i) {
            uint8_t c = codes[i];
            if (c < c1_local) out.set(i);
            else if (c == c1_local && base[i] < v1_local) out.set(i);
        }
        return out;
    }

    if (q.op == QuerySpec::Op::EQ) {
        const uint32_t c1_local = c1;
        const uint64_t v1_local = q.v1;
        bool is_unique = std::binary_search(L.art.uniques.begin(), L.art.uniques.end(), v1_local);

        size_t i = 0;
        size_t limit = N & ~size_t(3);
        if (is_unique) {
            // All matches are exactly code == c1; no base probes needed
            for (; i < limit; i += 4) {
                uint8_t c0 = codes[i];
                uint8_t c1c = codes[i+1];
                uint8_t c2c = codes[i+2];
                uint8_t c3c = codes[i+3];
                if (c0 == c1_local) out.set(i);
                if (c1c == c1_local) out.set(i+1);
                if (c2c == c1_local) out.set(i+2);
                if (c3c == c1_local) out.set(i+3);
            }
            for (; i < N; ++i) {
                if (codes[i] == c1_local) out.set(i);
            }
        } else {
            // Only boundary bucket c1 can contain v1; need base equality check
            for (; i < limit; i += 4) {
                uint8_t c0 = codes[i];
                uint8_t c1c = codes[i+1];
                uint8_t c2c = codes[i+2];
                uint8_t c3c = codes[i+3];

                if (c0 == c1_local && base[i]   == v1_local) out.set(i);
                if (c1c == c1_local && base[i+1] == v1_local) out.set(i+1);
                if (c2c == c1_local && base[i+2] == v1_local) out.set(i+2);
                if (c3c == c1_local && base[i+3] == v1_local) out.set(i+3);
            }
            for (; i < N; ++i) {
                if (codes[i] == c1_local && base[i] == v1_local) out.set(i);
            }
        }
        return out;
    }

    // BETWEEN inclusive [v1, v2]
    {
        const uint32_t c1_local = c1;
        const uint32_t c2_local = c2;
        const uint64_t low  = q.v1;
        const uint64_t high = q.v2;

        size_t i = 0;
        size_t limit = N & ~size_t(3);

        for (; i < limit; i += 4) {
            uint8_t c0 = codes[i];
            uint8_t c1c = codes[i+1];
            uint8_t c2c = codes[i+2];
            uint8_t c3c = codes[i+3];

            // c in (c1,c2)
            if (c0 > c1_local && c0 < c2_local) out.set(i);
            else if (c0 == c1_local && base[i]   >= low && base[i]   <= high) out.set(i);
            else if (c0 == c2_local && base[i]   >= low && base[i]   <= high) out.set(i);

            if (c1c > c1_local && c1c < c2_local) out.set(i+1);
            else if (c1c == c1_local && base[i+1] >= low && base[i+1] <= high) out.set(i+1);
            else if (c1c == c2_local && base[i+1] >= low && base[i+1] <= high) out.set(i+1);

            if (c2c > c1_local && c2c < c2_local) out.set(i+2);
            else if (c2c == c1_local && base[i+2] >= low && base[i+2] <= high) out.set(i+2);
            else if (c2c == c2_local && base[i+2] >= low && base[i+2] <= high) out.set(i+2);

            if (c3c > c1_local && c3c < c2_local) out.set(i+3);
            else if (c3c == c1_local && base[i+3] >= low && base[i+3] <= high) out.set(i+3);
            else if (c3c == c2_local && base[i+3] >= low && base[i+3] <= high) out.set(i+3);
        }

        for (; i < N; ++i) {
            uint8_t c = codes[i];
            if (c > c1_local && c < c2_local) {
                out.set(i);
            } else if (c == c1_local || c == c2_local) {
                if (base[i] >= low && base[i] <= high) out.set(i);
            } else if (c1_local == c2_local && c == c1_local) {
                if (base[i] >= low && base[i] <= high) out.set(i);
            }
        }
        return out;
    }
}

// ---------------------------------------------------------------------
//  Generic scalar fallback (16-bit codes or non-optimized path)
// ---------------------------------------------------------------------

inline BitVector scan_predicate_scalar(const LoadedMap& L,
                                       const void* codes, bool codes16,
                                       const std::vector<uint64_t>& base,
                                       const QuerySpec& q) {
    const size_t N = base.size();
    BitVector out(N);

    auto code_of_v = [&](uint64_t v){ return NumericCompressionMap::code_of(L.art, v); };

    uint32_t c1=0, c2=0;
    if (q.op == QuerySpec::Op::LT || q.op == QuerySpec::Op::EQ) {
        c1 = code_of_v(q.v1).first;
    } else {
        c1 = code_of_v(q.v1).first;
        c2 = code_of_v(q.v2).first;
        if (c2 < c1) std::swap(c1,c2);
    }

    auto code_at = [&](size_t i)->uint32_t {
        if (!codes16) return static_cast<const uint8_t*>(codes)[i];
        return static_cast<const uint16_t*>(codes)[i];
    };

    if (q.op == QuerySpec::Op::LT) {
        for (size_t i=0;i<N;++i) {
            uint32_t c = code_at(i);
            if (c < c1) out.set(i);
            else if (c == c1 && base[i] < q.v1) out.set(i);
        }
        return out;
    }

    if (q.op == QuerySpec::Op::EQ) {
        bool is_unique = std::binary_search(L.art.uniques.begin(), L.art.uniques.end(), q.v1);
        if (is_unique) {
            for (size_t i=0;i<N;++i) if (code_at(i)==c1) out.set(i);
            return out;
        }
        for (size_t i=0;i<N;++i) {
            if (code_at(i)==c1 && base[i]==q.v1) out.set(i);
        }
        return out;
    }

    // BETWEEN
    for (size_t i=0;i<N;++i) {
        uint32_t c = code_at(i);
        if (c>c1 && c<c2) {
            out.set(i);
        } else if (c==c1 || c==c2) {
            if (base[i] >= q.v1 && base[i] <= q.v2) out.set(i);
        } else if (c1==c2 && c==c1) {
            if (base[i] >= q.v1 && base[i] <= q.v2) out.set(i);
        }
    }
    return out;
}

// ---------------------------------------------------------------------
//  Public entry point used by run_query / benchmark
// ---------------------------------------------------------------------

inline BitVector scan_predicate(const LoadedMap& L,
                                const void* codes, bool codes16,
                                const std::vector<uint64_t>& base,
                                const QuerySpec& q) {
    // Fast path: 8-bit codes with specialized loop
    if (!codes16 && L.code_bits == 8) {
        const uint8_t* c8 = static_cast<const uint8_t*>(codes);
        return scan_predicate_8bit(L, c8, base, q);
    }
    // Fallback: generic scalar implementation (works for 8-bit and 16-bit)
    return scan_predicate_scalar(L, codes, codes16, base, q);
}

} // namespace csketch
