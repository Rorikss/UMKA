#pragma once
#include <model/model.h>

#include <vector>

namespace umka::jit {

struct IOptimize {
  virtual ~IOptimize() = default;

  virtual void run(
      std::vector<vm::Command>& code,
      std::vector<vm::Constant>& const_pool,
      std::unordered_map<size_t, vm::FunctionTableEntry>& func_table,
      vm::FunctionTableEntry& meta
  ) = 0;
};

} // namespace umka::jit
