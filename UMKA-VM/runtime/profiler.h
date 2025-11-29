#pragma once

#include "../model/model.h"
#include <vector>
#include <unordered_map>
#include <cstdint>

class Profiler {
public:
    struct HotRegion {
        size_t start_offset;
        size_t end_offset;
        int64_t call_count;
        int64_t jump_count;
    };

    Profiler(const std::vector<FunctionTableEntry>& func_table, const std::vector<Command>& commands)
        : func_table(func_table), commands(commands) {
        for (const auto& func : func_table) {
            function_call_counts[func.id] = 0;
        }
    }

    void increment_function_call(uint64_t function_id) {
        if (function_call_counts.find(function_id) != function_call_counts.end()) {
            function_call_counts[function_id]++;
            func_table[function_id].call_count++;
        }
    }

    void record_backward_jump(size_t jump_source_offset, size_t jump_target_offset, size_t function_start_offset) {
        if (jump_target_offset < jump_source_offset) {  // Backward jump
            backward_jumps[jump_source_offset] = jump_target_offset;
            function_of_jump[jump_source_offset] = function_start_offset;
            if (backward_jump_counts.find(jump_source_offset) == backward_jump_counts.end()) {
                backward_jump_counts[jump_source_offset] = 0;
            }
            backward_jump_counts[jump_source_offset]++;
        }
    }

    std::vector<HotRegion> get_hot_regions(size_t top_n = 10) const {
        std::vector<HotRegion> regions;
        
        std::unordered_map<size_t, const FunctionTableEntry*> function_by_start;
        for (const auto& func : func_table) {
            function_by_start[static_cast<size_t>(func.code_offset)] = &func;
        }
        
        for (const auto& [jump_offset, target_offset] : backward_jumps) {
            int64_t jump_count = backward_jump_counts.at(jump_offset);
            size_t func_start_offset = function_of_jump.at(jump_offset);
            
            auto func_it = function_by_start.find(func_start_offset);
            if (func_it != function_by_start.end()) {
                const FunctionTableEntry* func = func_it->second;
                
                regions.push_back(HotRegion{
                    target_offset,
                    jump_offset + 1,
                    0,
                    jump_count
                });
            }
        }
        
        for (const auto& func : func_table) {
            if (func.call_count > 0) {
                regions.push_back(HotRegion{
                    static_cast<size_t>(func.code_offset),
                    static_cast<size_t>(func.code_offset_end),
                    func.call_count,
                    0
                });
            }
        }
        
        std::sort(regions.begin(), regions.end(), [](const HotRegion& a, const HotRegion& b) {
            int64_t hotness_a = a.call_count + a.jump_count;
            int64_t hotness_b = b.call_count + b.jump_count;
            return hotness_a > hotness_b;
        });
        
        if (regions.size() > top_n) {
            regions.resize(top_n);
        }
        
        return regions;
    }

    const FunctionTableEntry* get_hottest_function() const {
        const FunctionTableEntry* hottest = nullptr;
        int64_t max_calls = -1;
        
        for (const auto& func : func_table) {
            if (func.call_count > max_calls) {
                max_calls = func.call_count;
                hottest = &func;
            }
        }
        
        return hottest;
    }

    const std::unordered_map<size_t, int64_t>& get_jump_counts() const {
        return backward_jump_counts;
    }

    const std::unordered_map<uint64_t, int64_t>& get_function_call_counts() const {
        return function_call_counts;
    }

private:
    const std::vector<FunctionTableEntry>& func_table;
    const std::vector<Command>& commands;
    std::unordered_map<uint64_t, int64_t> function_call_counts;
    std::unordered_map<size_t, size_t> backward_jumps;  // jump_offset -> target_offset
    std::unordered_map<size_t, size_t> function_of_jump; // jump_offset -> function_start_offset
    std::unordered_map<size_t, int64_t> backward_jump_counts;
};