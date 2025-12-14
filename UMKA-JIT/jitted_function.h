#pragma once

#include <vector>
#include <model/model.h>   // for Command
#include <parser/command_parser.h>

namespace umka::jit {
struct JittedFunction {
  std::vector<vm::Command> code;
  int64_t arg_count{};
  int64_t local_count{};
};

}