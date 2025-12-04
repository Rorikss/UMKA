#pragma once

#include <vector>
#include <memory>
#include <unordered_map>

#include <model/model.h>
#include <runtime/stack_machine.h>

#include "jitted_function.h"
#include "optimizations/base_optimization.h"

namespace umka::jit {
class JitRunner {
  public:
    JitRunner(
      std::vector<Command> &commands,
      std::vector<Constant> &const_pool,
      std::unordered_map<size_t, FunctionTableEntry> &func_table
    )
      : commands(commands)
        , const_pool(const_pool)
        , func_table(func_table) {
    }

    void add_optimization(std::unique_ptr<IOptimize> opt) {
      optimizations.push_back(std::move(opt));
    }

    std::vector<Command> optimize_range(
        const std::vector<Command>::iterator begin,
        const std::vector<Command>::iterator end,
        FunctionTableEntry& meta
    ) {
      std::vector<Command> local(begin, end);
      for (auto& p : passes)
        p->run(local, const_pool, meta);
      return local;
    }


    JittedFunction optimize_function(const size_t func_id) const {
      auto &meta = func_table.at(func_id);

      const auto begin = commands.begin() + meta.code_offset;
      const auto end = commands.begin() + meta.code_offset_end;

      std::vector<Command> local(begin, end);

      for (auto &opt: optimizations)
        opt->run(local, const_pool, meta);

      return JittedFunction{
        std::move(local),
        meta.arg_count,
        meta.local_count
      };
    }

  private:
    std::vector<Command> &commands;
    std::vector<Constant> &const_pool;
    std::unordered_map<size_t, FunctionTableEntry> &func_table;
    std::vector<std::unique_ptr<IOptimize> > optimizations;
};
} // namespace umka::jit
