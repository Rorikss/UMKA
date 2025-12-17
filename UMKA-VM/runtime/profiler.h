#pragma once

#include <model/model.h>

#include <algorithm>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace umka::vm {
class Profiler
{
  public:
    struct HotRegion {
        size_t start_offset;
        size_t end_offset;
        int64_t call_count;
        int64_t jump_count;
    };

    Profiler(const std::unordered_map<size_t, FunctionTableEntry>& func_table, const std::vector<Command>& commands)
      : func_table(func_table)
      , commands(commands)
    {
        for (const auto& [id, func] : func_table) {
            function_call_counts[id] = 0;
        }
    }

    void increment_function_call(uint64_t function_id) { 
        function_call_counts.at(function_id)++;
    }

    void record_backward_jump(size_t jump_source_offset, size_t jump_target_offset, size_t func_id) {
        if (jump_target_offset < jump_source_offset) {
            // Backward jump
            backward_jumps[jump_source_offset] = jump_target_offset;
            function_of_jump[jump_source_offset] = func_id;
            if (backward_jump_counts.find(jump_source_offset) == backward_jump_counts.end()) {
                backward_jump_counts[jump_source_offset] = 0;
            }
            backward_jump_counts[jump_source_offset]++;
        }
    }

    bool is_function_hot(const uint64_t function_id) const {
        auto it = function_call_counts.find(function_id);
        if (it == function_call_counts.end()) {
            return false;
        }
        return it->second > threshold;
    }

    std::vector<HotRegion> get_hot_regions(size_t top_n = 10) const {
        std::vector<HotRegion> regions;

        for (const auto& [jump_offset, target_offset] : backward_jumps) {
            int64_t jump_count = backward_jump_counts.at(jump_offset);
            size_t func_id = function_of_jump.at(jump_offset);

            regions.push_back(HotRegion{ 
                static_cast<size_t>(func_table.at(func_id).code_offset),
                static_cast<size_t>(func_table.at(func_id).code_offset_end),
                function_call_counts.at(func_id),
                jump_count 
            });
        }

        for (const auto& [id, func] : func_table) {
            if (function_call_counts.at(id) > 0) {
                regions.push_back(HotRegion{ 
                    static_cast<size_t>(func.code_offset),
                    static_cast<size_t>(func.code_offset_end),
                    function_call_counts.at(id),
                    1 
                });
            }
        }

        std::sort(regions.begin(), regions.end(), [](const HotRegion& a, const HotRegion& b) {
            int64_t hotness_a = a.call_count * a.jump_count;
            int64_t hotness_b = b.call_count * b.jump_count;
            return hotness_a > hotness_b;
        });

        if (regions.size() > top_n) {
            regions.resize(top_n);
        }

        return regions;
    }

  private:
    const size_t threshold = 3;
    const std::unordered_map<size_t, FunctionTableEntry>& func_table;
    const std::vector<Command>& commands;
    std::unordered_map<uint64_t, int64_t> function_call_counts;
    std::unordered_map<size_t, size_t> backward_jumps;   // jump_offset -> target_offset
    std::unordered_map<size_t, size_t> function_of_jump; // jump_offset -> function_start_offset
    std::unordered_map<size_t, int64_t> backward_jump_counts;
};
}
