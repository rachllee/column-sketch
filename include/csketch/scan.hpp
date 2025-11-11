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

// Minimal JSON reader for the map sidecar (no external deps)
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

// Core scan using sketch + boundary probes
inline BitVector scan_predicate(const LoadedMap& L,
                                const void* codes, bool codes16,
                                const std::vector<uint64_t>& base,
                                const QuerySpec& q) {
    const size_t N = base.size();
    BitVector out(N);

    auto code_of_v = [&](uint64_t v){ return NumericCompressionMap::code_of(L.art, v); };

    uint32_t c1=0, c2=0; 
    if (q.op == QuerySpec::Op::LT || q.op == QuerySpec::Op::EQ) {
        auto p = code_of_v(q.v1); c1 = p.first; 
    } else {
        auto p1 = code_of_v(q.v1); c1 = p1.first; 
        auto p2 = code_of_v(q.v2); c2 = p2.first; 
        if (c2 < c1) std::swap(c1,c2);
    }

    auto code_at = [&](size_t i)->uint32_t {
        if (!codes16) return static_cast<const uint8_t*>(codes)[i];
        return static_cast<const uint16_t*>(codes)[i];
    };

    if (q.op == QuerySpec::Op::LT) {
        for (size_t i=0;i<N;++i) {
            uint32_t c = code_at(i);
            if (c < c1) { out.set(i); }
            else if (c == c1) { if (base[i] < q.v1) out.set(i); }
        }
        return out;
    }

    if (q.op == QuerySpec::Op::EQ) {
        // If v1 is a unique value, no probes needed.
        bool is_unique = std::binary_search(L.art.uniques.begin(), L.art.uniques.end(), q.v1);
        if (is_unique) {
            for (size_t i=0;i<N;++i) if (code_at(i)==c1) out.set(i);
            return out;
        }
        // Otherwise probe only the boundary bucket
        for (size_t i=0;i<N;++i) {
            if (code_at(i)==c1 && base[i]==q.v1) out.set(i);
        }
        return out;
    }

    // BETWEEN inclusive [v1, v2]
    for (size_t i=0;i<N;++i) {
        uint32_t c = code_at(i);
        if (c>c1 && c<c2) { out.set(i); continue; }        // definite inside
        if (c==c1) { if (base[i] >= q.v1) out.set(i); }    // low boundary
        else if (c==c2) { if (base[i] <= q.v2) out.set(i);} // high boundary
        else if (c1==c2) { // degenerate single-bucket interval
            if (c==c1 && base[i] >= q.v1 && base[i] <= q.v2) out.set(i);
        }
    }
    return out;
}

} // namespace csketch
