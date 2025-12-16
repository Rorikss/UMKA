#pragma once

#include "base_optimization.h"
#include <unordered_set>
#include <functional>
#include <limits>


namespace umka::jit {
class DeadCodeElimination final: public IOptimize {
  public:
    void run(
      std::vector<vm::Command> &code,
      std::vector<vm::Constant> &const_pool,
      std::unordered_map<size_t, vm::FunctionTableEntry> &func_table,
      vm::FunctionTableEntry &
    ) override {
      if (code.empty()) {
        return;
      }

      const size_t n = code.size();
      std::vector reachable(n, false);

      std::function<void(size_t)> dfs = [&](size_t i) {
        if (i >= n || reachable[i]) return;

        reachable[i] = true;
        const auto op = static_cast<vm::OpCode>(code[i].code);

        if (op == vm::OpCode::JMP) {
          int64_t target = static_cast<int64_t>(i) + (code[i].arg) + 1;
          if (target >= 0 && target < static_cast<int>(n)) {
            dfs(target);
          }
          return;
        }

        if (op == vm::OpCode::JMP_IF_FALSE || op == vm::OpCode::JMP_IF_TRUE) {
          const int64_t target = static_cast<int64_t>(i) + (code[i].arg) + 1;
          dfs(i + 1);
          if (target >= 0 && target < static_cast<int>(n)) {
            dfs(target);
          }
          return;
        }

        dfs(i + 1);
      };

      dfs(0);

      auto jump_targets = compute_jump_targets(code, reachable);

      std::vector needed(n, false);
      for (size_t t: jump_targets) {
        if (t < n && reachable[t]) {
          needed[t] = true;
        }
      }

      int demand = 0;

      for (int i = static_cast<int>(n) - 1; i >= 0; --i) {
        if (!reachable[static_cast<size_t>(i)]) {
          continue;
        }

        auto op = static_cast<vm::OpCode>(code[i].code);

        if (op == vm::OpCode::JMP ||
          op == vm::OpCode::JMP_IF_FALSE ||
          op == vm::OpCode::JMP_IF_TRUE) {
          int64_t target = static_cast<int64_t>(i) + code[i].arg + 1;
          if (target >= 0 && target < static_cast<int>(n) && reachable[static_cast<size_t>(
            target)]) {
            needed[i] = true;
          }
        }

        const int64_t consumes = stack_consumed(op, code[i].arg, func_table);
        const int64_t produces = stack_produced(op);
        const bool has_side_effects = is_side_effect(op);

        const bool is_needed =
            has_side_effects ||
            needed[static_cast<size_t>(i)] ||
            demand > 0;

        if (is_needed) {
          needed[static_cast<size_t>(i)] = true;

          demand -= static_cast<int>(produces);
          if (demand < 0) demand = 0;

          demand += static_cast<int>(consumes);
        }
      }

      std::vector<vm::Command> new_code;
      new_code.reserve(n);

      std::vector old_to_new(n, -1);

      for (size_t i = 0; i < n; ++i) {
        if (reachable[i] && needed[i]) {
          old_to_new[i] = static_cast<int>(new_code.size());
          new_code.push_back(code[i]);
        }
      }

      for (size_t old_i = 0; old_i < n; ++old_i) {
        if (!reachable[old_i] || !needed[old_i]) continue;

        const int new_i = old_to_new[old_i];
        if (new_i < 0) continue;

        auto op = static_cast<vm::OpCode>(code[old_i].code);
        if (op == vm::OpCode::JMP || op == vm::OpCode::JMP_IF_FALSE || op ==
          vm::OpCode::JMP_IF_TRUE) {
          const int64_t old_target = static_cast<int64_t>(old_i) + code[old_i].arg +
              1;

          if (old_target >= 0 && old_target < static_cast<int>(n) &&
            reachable[static_cast<size_t>(old_target)] &&
            needed[static_cast<size_t>(old_target)]) {
            int new_target = old_to_new[static_cast<size_t>(old_target)];
            if (new_target >= 0) {
              new_code[static_cast<size_t>(new_i)].arg = new_target - new_i - 1;
            }
          }
        }
      }

      code.swap(new_code);
    }

  private:
    static int stack_consumed(
      const vm::OpCode op,
      const int64_t arg,
      const std::unordered_map<size_t, vm::FunctionTableEntry> &func_table
    ) {
      switch (op) {
        case vm::OpCode::PUSH_CONST: return 0;
        case vm::OpCode::POP: return 1;
        case vm::OpCode::LOAD: return 0;
        case vm::OpCode::STORE: return 1;

        case vm::OpCode::ADD:
        case vm::OpCode::SUB:
        case vm::OpCode::MUL:
        case vm::OpCode::DIV:
        case vm::OpCode::REM:
        case vm::OpCode::EQ:
        case vm::OpCode::NEQ:
        case vm::OpCode::LT:
        case vm::OpCode::GT:
        case vm::OpCode::LTE:
        case vm::OpCode::GTE:
        case vm::OpCode::AND:
        case vm::OpCode::OR:
          return 2;

        case vm::OpCode::NOT:
        case vm::OpCode::TO_STRING:
        case vm::OpCode::TO_INT:
        case vm::OpCode::TO_DOUBLE:
        case vm::OpCode::OPCOT:
          return 1;

        case vm::OpCode::CALL:
          return call_arity(arg, func_table);

        case vm::OpCode::BUILD_ARR:
          return static_cast<int>(arg);

        case vm::OpCode::RETURN:
          return 1;

        case vm::OpCode::JMP:
          return 0;

        case vm::OpCode::JMP_IF_FALSE:
        case vm::OpCode::JMP_IF_TRUE:
          return 1;

        default:
          return 0;
      }
    }

    static int stack_produced(const vm::OpCode op) {
      switch (op) {
        case vm::OpCode::PUSH_CONST:
        case vm::OpCode::LOAD:
          return 1;

        case vm::OpCode::ADD:
        case vm::OpCode::SUB:
        case vm::OpCode::MUL:
        case vm::OpCode::DIV:
        case vm::OpCode::REM:
        case vm::OpCode::EQ:
        case vm::OpCode::NEQ:
        case vm::OpCode::LT:
        case vm::OpCode::GT:
        case vm::OpCode::LTE:
        case vm::OpCode::GTE:
        case vm::OpCode::AND:
        case vm::OpCode::OR:
        case vm::OpCode::NOT:
        case vm::OpCode::TO_STRING:
        case vm::OpCode::TO_INT:
        case vm::OpCode::TO_DOUBLE:
        case vm::OpCode::OPCOT:
          return 1;

        case vm::OpCode::CALL:
          return 1;

        case vm::OpCode::BUILD_ARR:
          return 1;

        case vm::OpCode::RETURN:
        case vm::OpCode::STORE:
        case vm::OpCode::POP:
          return 0;

        case vm::OpCode::JMP:
        case vm::OpCode::JMP_IF_FALSE:
        case vm::OpCode::JMP_IF_TRUE:
          return 0;

        default:
          return 0;
      }
    }

    static bool is_side_effect(const vm::OpCode op) {
      switch (op) {
        case vm::OpCode::STORE:
        case vm::OpCode::RETURN:
        case vm::OpCode::CALL:
        case vm::OpCode::POP:
          return true;
        default:
          return false;
      }
    }

    static std::unordered_set<size_t> compute_jump_targets(
      const std::vector<vm::Command> &code,
      const std::vector<bool> &reachable
    ) {
      std::unordered_set<size_t> result;
      const size_t n = code.size();

      for (size_t i = 0; i < n; ++i) {
        if (!reachable[i]) continue;

        auto op = static_cast<vm::OpCode>(code[i].code);
        if (op == vm::OpCode::JMP || op == vm::OpCode::JMP_IF_FALSE || op ==
          vm::OpCode::JMP_IF_TRUE) {
          int64_t target = static_cast<int64_t>(i) + static_cast<int64_t>(code[i].arg) + 1;
          if (target >= 0 && target < static_cast<int64_t>(n)) {
            result.insert(target);
          }
        }
      }
      return result;
    }

    static int call_arity(
      const int64_t id,
      const std::unordered_map<size_t, vm::FunctionTableEntry> &func_table
    ) {
      switch (id) {
        case vm::PRINT_FUN: return 1;
        case vm::LEN_FUN: return 1;
        case vm::GET_FUN: return 2;
        case vm::SET_FUN: return 3;
        case vm::ADD_FUN: return 2;
        case vm::REMOVE_FUN: return 2;
        case vm::WRITE_FUN: return 2;
        case vm::READ_FUN: return 1;
        case vm::ASSERT_FUN: return 1;
        case vm::INPUT_FUN: return 0;
        case vm::RANDOM_FUN: return 0;
        default:
          break;
      }

      if (const auto it = func_table.find(id); it != func_table.end()) {
        if (it->second.arg_count >= 0 &&
          it->second.arg_count <= std::numeric_limits<int64_t>::max()) {
          return static_cast<int>(it->second.arg_count);
        }
      }
      return 0;
    }
};
} // namespace umka::jit
