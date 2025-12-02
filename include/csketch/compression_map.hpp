#pragma once

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace csketch {

struct MapArtifacts {
  std::vector<uint64_t> uniques;   // explicit singleton codes (sorted)
  std::vector<uint64_t> endpoints; // inclusive range boundaries (sorted, deduped)
  uint32_t total_codes = 0;
};

class NumericCompressionMap {
public:
  static MapArtifacts build(const std::vector<uint64_t> &values, uint32_t max_codes,
                            size_t sample_size, size_t unique_cutoff) {
    if (values.empty()) {
      throw std::invalid_argument("NumericCompressionMap::build requires non-empty input");
    }
    if (max_codes < 1) {
      throw std::invalid_argument("NumericCompressionMap::build max_codes must be >= 1");
    }
    if (sample_size == 0) {
      sample_size = 1;
    }
    if (unique_cutoff == 0) {
      unique_cutoff = 1;
    }

    std::vector<uint64_t> sorted(values);
    std::sort(sorted.begin(), sorted.end());

    struct Run {
      uint64_t value;
      size_t freq;
    };

    std::vector<uint64_t> uniques;
    std::vector<Run> nonuniq_runs;
    uniques.reserve(sorted.size());

    for (size_t i = 0; i < sorted.size();) {
      size_t j = i + 1;
      while (j < sorted.size() && sorted[j] == sorted[i]) {
        ++j;
      }
      size_t freq = j - i;
      if (freq >= unique_cutoff) {
        uniques.push_back(sorted[i]);
      } else {
        nonuniq_runs.push_back({sorted[i], freq});
      }
      i = j;
    }

    uniques.shrink_to_fit();

    MapArtifacts art;
    art.uniques = std::move(uniques);

    if (art.uniques.size() >= max_codes) {
      // Demote uniques to ranges to stay within code budget.
      art.uniques.clear();
      nonuniq_runs.clear();
      for (size_t i = 0; i < sorted.size();) {
        size_t j = i + 1;
        while (j < sorted.size() && sorted[j] == sorted[i]) {
          ++j;
        }
        nonuniq_runs.push_back({sorted[i], j - i});
        i = j;
      }
    } else if (nonuniq_runs.empty()) {
      art.total_codes = static_cast<uint32_t>(art.uniques.size());
      return art;
    }

    if (max_codes <= art.uniques.size()) {
      throw std::runtime_error("max_codes must exceed number of unique codes");
    }

    size_t total_nonuniq = 0;
    for (const auto &run : nonuniq_runs) {
      total_nonuniq += run.freq;
    }

    const size_t sample_points = std::min(sample_size, total_nonuniq);

    auto value_at_rank = [&](size_t rank) {
      size_t acc = 0;
      for (const auto &run : nonuniq_runs) {
        if (rank < acc + run.freq) {
          return run.value;
        }
        acc += run.freq;
      }
      return nonuniq_runs.back().value;
    };

    std::vector<uint64_t> sample_nonuniq;
    sample_nonuniq.reserve(sample_points ? sample_points : 1);

    if (sample_points == 0) {
      sample_nonuniq.push_back(nonuniq_runs.back().value);
    } else {
      for (size_t i = 1; i <= sample_points; ++i) {
        size_t target = static_cast<size_t>(
            (static_cast<long double>(i) * total_nonuniq) / (sample_points + 1));
        if (target >= total_nonuniq) {
          target = total_nonuniq - 1;
        }
        sample_nonuniq.push_back(value_at_rank(target));
      }
    }

    const uint32_t range_codes =
        static_cast<uint32_t>(max_codes - static_cast<uint32_t>(art.uniques.size()));

    if (range_codes == 0) {
      throw std::runtime_error("range_codes resolved to zero");
    }

    std::vector<uint64_t> endpoints;
    endpoints.reserve(range_codes);

    const size_t M = sample_nonuniq.size();
    for (uint32_t i = 1; i <= range_codes; ++i) {
      size_t idx =
          static_cast<size_t>((static_cast<long double>(i) * M) / range_codes);
      if (idx == 0) {
        idx = 1;
      }
      if (idx > M) {
        idx = M;
      }
      uint64_t bound = sample_nonuniq[idx - 1];
      endpoints.push_back(bound);
    }

    std::sort(endpoints.begin(), endpoints.end());
    endpoints.erase(std::unique(endpoints.begin(), endpoints.end()), endpoints.end());
    if (endpoints.empty() || endpoints.back() < sample_nonuniq.back()) {
      endpoints.push_back(sample_nonuniq.back());
    }

    art.endpoints = std::move(endpoints);
    art.total_codes = static_cast<uint32_t>(art.uniques.size() + art.endpoints.size());
    return art;
  }

  static std::pair<uint32_t, bool> code_of(const MapArtifacts &art, uint64_t v) {
    const auto &U = art.uniques;
    const auto &E = art.endpoints;
    auto uit = std::lower_bound(U.begin(), U.end(), v);
    if (uit != U.end() && *uit == v) {
      uint32_t idx = static_cast<uint32_t>(uit - U.begin());
      return {idx, false};
    }

    if (E.empty()) {
      throw std::runtime_error("value not encodable (no range endpoints)");
    }

    auto eit = std::lower_bound(E.begin(), E.end(), v);
    if (eit == E.end()) {
      uint32_t code = static_cast<uint32_t>(U.size() + (E.size() - 1));
      return {code, false};
    }
    bool is_boundary = (*eit == v);
    uint32_t e_index = static_cast<uint32_t>(eit - E.begin());
    uint32_t code =
        static_cast<uint32_t>((uit - U.begin()) + e_index);
    return {code, is_boundary};
  }
};

inline void save_map_json(const MapArtifacts &art, const std::string &path,
                          const std::string &dtype, uint32_t code_bits) {
  std::ofstream out(path);
  if (!out) {
    throw std::runtime_error("save_map_json: cannot open file");
  }
  out << "{\n";
  out << " \"dtype\": \"" << dtype << "\",\n";
  out << " \"code_bits\": " << code_bits << ",\n";
  out << " \"total_codes\": " << art.total_codes << ",\n";
  out << " \"uniques\": [";
  for (size_t i = 0; i < art.uniques.size(); ++i) {
    if (i) {
      out << ",";
    }
    out << art.uniques[i];
  }
  out << "],\n \"endpoints\": [";
  for (size_t i = 0; i < art.endpoints.size(); ++i) {
    if (i) {
      out << ",";
    }
    out << art.endpoints[i];
  }
  out << "]\n}";
}

} // namespace csketch
