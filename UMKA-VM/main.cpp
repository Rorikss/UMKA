#include "model/model.h"
#include "runtime/stack_machine.h"
#include "parser/command_parser.h"
#include "runtime/profiler.h"
#include <fstream>
#include <iostream>

constexpr const char* DEFAULT_BYTECODE_PATH = "program.umka";
size_t HOT_REGIONS_COUNT = 10;

int main(int argc, char* argv[]) {
    try {
        std::string bytecode_path = (argc > 1) ? argv[1] : DEFAULT_BYTECODE_PATH;
        
        std::cout << "Loading bytecode from: " << bytecode_path << std::endl;
        std::ifstream bytecode_file(bytecode_path, std::ios::binary);
        if (!bytecode_file) {
            throw std::runtime_error("Failed to open bytecode file: " + bytecode_path);
        }
        
        CommandParser parser;
        parser.parse(bytecode_file);
        
        StackMachine vm(parser);
        vm.run();
        
        // TODO УБРАТЬ ЭТОТ ВЫСЕР ЭТО ДЕБАГ
        auto profiler = vm.get_profiler();
        auto hot_regions = profiler->get_hot_regions(HOT_REGIONS_COUNT);
        
        std::cout << "\n=== PROFILING RESULTS ===" << std::endl;
        std::cout << "Hot regions found: " << hot_regions.size() << std::endl;
        
        for (size_t i = 0; i < hot_regions.size(); ++i) {
            const auto& region = hot_regions[i];
            std::cout << "Region " << (i+1) << ": " << std::endl;
            std::cout << "  Start offset: " << region.start_offset << std::endl;
            std::cout << "  End offset: " << region.end_offset << std::endl;
            std::cout << "  Function calls: " << region.call_count << std::endl;
            std::cout << "  Backward jumps: " << region.jump_count << std::endl;
            std::cout << "  Total hotness: " << (region.call_count + region.jump_count) << std::endl;
            std::cout << std::endl;
        }
        
        // Print hottest function
        const FunctionTableEntry* hottest_func = profiler->get_hottest_function();
        if (hottest_func) {
            std::cout << "Hottest function:" << std::endl;
            std::cout << "  ID: " << hottest_func->id << std::endl;
            std::cout << "  Call count: " << profiler->get_function_call_counts().at(hottest_func->id) << std::endl;
            std::cout << "  Code offset: " << hottest_func->code_offset << " - " << hottest_func->code_offset_end << std::endl;
        }
        
        std::cout << "Execution completed successfully" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
