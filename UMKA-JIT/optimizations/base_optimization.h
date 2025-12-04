#pragma once
#include <model/model.h>
#include <runtime/stack_machine.h>

#include <vector>

namespace umka::jit {

struct IOptimize {
  virtual ~IOptimize() = default;

  virtual void run(
      std::vector<Command>& code,
      std::vector<Constant>& const_pool,
      FunctionTableEntry& meta
  ) = 0;
};

} // namespace umka::jit
