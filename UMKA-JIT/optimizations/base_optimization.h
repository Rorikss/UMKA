#pragma once
#include <model/model.h>

#include <vector>

namespace umka::jit {

struct IOptimize {
  virtual ~IOptimize() = default;

  virtual void run(
      std::vector<vm::Command>& code,
      std::vector<vm::Constant>& const_pool,
      vm::FunctionTableEntry& meta
  ) = 0;
};

} // namespace umka::jit
