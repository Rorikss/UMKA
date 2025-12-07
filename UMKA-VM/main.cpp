#include "model/model.h"
#include "runtime/stack_machine.h"
#include "parser/command_parser.h"
#include "runtime/profiler.h"
#include <fstream>
#include <iostream>

using namespace umka::vm;

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
        
        StackMachine<ReleaseMod> vm(parser);
        vm.run([init = false](Command cmd, std::string stack_top) mutable {
            if (!init) {
                init = true;
                std::cout << "Executing command" << std::endl;
            }
            std::cout 
                << std::hex << "0x"
                << (int)cmd.code 
                << std::dec
                << ' ' << cmd.arg 
                << ' ' << stack_top 
                << std::endl;
        });
        
        auto profiler = vm.get_profiler();
        auto hot_regions = profiler->get_hot_regions(HOT_REGIONS_COUNT);
        
        std::cout << "Execution completed successfully" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
