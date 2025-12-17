#pragma once

#include "base_optimization.h"
#include <parser/command_parser.h>

#include <algorithm>
#include <cstring>
#include <optional>
#include <vector>
#include <variant>

namespace umka::jit {
class ConstantPropagation final: public IOptimize {
  public:
    void run(
      std::vector<vm::Command> &code,
      std::vector<vm::Constant> &const_pool,
      std::unordered_map<size_t, vm::FunctionTableEntry> &,
      vm::FunctionTableEntry &meta
    ) override {
      using ConstValue = std::variant<int64_t, double>;

      struct Value {
        bool known = false;
        ConstValue v = static_cast<int64_t>(0);
      };

      std::vector<Value> stack;
      size_t locals_size = std::max(static_cast<size_t>(meta.local_count), static_cast<size_t>(256));
      std::vector<Value> locals(locals_size);
      std::vector used_in_jump(locals_size, false);

      for (size_t ip = 0; ip < code.size(); ++ip) {
        const auto &cmd = code[ip];
        vm::OpCode op = static_cast<vm::OpCode>(cmd.code);
        
        if (op == vm::OpCode::JMP_IF_FALSE || op == vm::OpCode::JMP_IF_TRUE) {
          for (size_t j = ip; j > 0 && (ip - j) < 20; --j) {
            vm::OpCode prev_op = static_cast<vm::OpCode>(code[j-1].code);

            if (prev_op == vm::OpCode::JMP ||
                prev_op == vm::OpCode::JMP_IF_FALSE ||
                prev_op == vm::OpCode::JMP_IF_TRUE ||
                prev_op == vm::OpCode::CALL ||
                prev_op == vm::OpCode::RETURN ||
                prev_op == vm::OpCode::CALL_METHOD ||
                prev_op == vm::OpCode::STORE) {
              break;
            }

            if (prev_op == vm::OpCode::LOAD) {
              if (code[j-1].arg >= 0 && static_cast<size_t>(code[j-1].arg) < used_in_jump.size()) {
                used_in_jump[code[j-1].arg] = true;
              }
            }
          }
        }
      }

      auto reset_state = [&]() {
        stack.clear();
        for (auto &val: locals) val.known = false;
      };

      auto pop = [&]() -> Value {
        if (stack.empty()) return {};
        auto v = stack.back();
        stack.pop_back();
        return v;
      };

      auto push = [&](Value v) { stack.push_back(v); };

      for (size_t ip = 0; ip < code.size(); ++ip) {
        auto &cmd = code[ip];
        vm::OpCode op = static_cast<vm::OpCode>(cmd.code);

        switch (op) {
          case vm::OpCode::PUSH_CONST: {
            auto c = read_const(const_pool, cmd.arg);
            if (c.has_value()) {
              Value val;
              val.known = true;
              val.v = *c;
              push(val);
            } else {
              push({});
            }
            break;
          }

          case vm::OpCode::LOAD: {
            if (cmd.arg >= 0 && static_cast<size_t>(cmd.arg) < locals.size()) {
              const auto &local = locals[cmd.arg];

              if (local.known && 
                  !used_in_jump[cmd.arg] && 
                  !is_written_later(code, cmd.arg, ip)) {
                auto idx = ensure_const(const_pool, local.v);
                if (idx.has_value()) {
                  cmd.code = vm::OpCode::PUSH_CONST;
                  cmd.arg = *idx;
                  push(local);
                } else {
                  push(local);
                }
              } else {
                push(local);
              }
            } else {
              push({});
            }
            break;
          }

          case vm::OpCode::STORE: {
            if (cmd.arg >= 0 && static_cast<size_t>(cmd.arg) < locals.size()) {
              auto v = pop();
              locals[cmd.arg] = v;
            }
            break;
          }

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
          case vm::OpCode::OR: {
            pop();
            pop();
            push({false, static_cast<int64_t>(0)});
            break;
          }

          case vm::OpCode::NOT: {
            pop();
            push({false, static_cast<int64_t>(0)});
            break;
          }

          case vm::OpCode::POP:
            pop();
            break;

          case vm::OpCode::BUILD_ARR: {
            if (cmd.arg >= 0 && cmd.arg <= 1000) {
              for (int64_t i = 0; i < cmd.arg; ++i) pop();
            }
            push({false, static_cast<int64_t>(0)});
            break;
          }

          case vm::OpCode::GET_FIELD: {
            pop();
            push({false, static_cast<int64_t>(0)});
            break;
          }

          case vm::OpCode::CALL_METHOD: {
            pop();
            reset_state();
            break;
          }

          case vm::OpCode::TO_STRING:
          case vm::OpCode::TO_INT:
          case vm::OpCode::TO_DOUBLE: {
            pop();
            push({false, static_cast<int64_t>(0)});
            break;
          }

          case vm::OpCode::OPCOT: {
            pop();
            pop();
            push({false, static_cast<int64_t>(0)});
            break;
          }

          case vm::OpCode::JMP:
            reset_state();
            break;

          case vm::OpCode::JMP_IF_FALSE:
          case vm::OpCode::JMP_IF_TRUE: {
            pop();
            reset_state();
            break;
          }

          case vm::OpCode::CALL:
          case vm::OpCode::RETURN:
            reset_state();
            break;

          default:
            reset_state();
            break;
        }
      }
    }

  private:
    using ConstValue = std::variant<int64_t, double>;

    static bool is_barrier(vm::OpCode op) {
      return op == vm::OpCode::JMP ||
             op == vm::OpCode::JMP_IF_FALSE ||
             op == vm::OpCode::JMP_IF_TRUE ||
             op == vm::OpCode::CALL ||
             op == vm::OpCode::CALL_METHOD ||
             op == vm::OpCode::RETURN;
    }

    static bool is_written_later(const std::vector<vm::Command>& code, size_t var, size_t from_ip) {
      for (size_t i = from_ip + 1; i < code.size(); ++i) {
        vm::OpCode op = static_cast<vm::OpCode>(code[i].code);

        if (op == vm::OpCode::STORE &&
            code[i].arg >= 0 &&
            static_cast<size_t>(code[i].arg) == var) {
          return true;
        }

        if (is_barrier(op)) {
          break;
        }
      }
      return false;
    }

    static std::optional<ConstValue> read_const(
      const std::vector<vm::Constant> &pool,
      int64_t idx
    ) {
      if (idx < 0 || static_cast<size_t>(idx) >= pool.size()) return std::nullopt;
      const auto &c = pool[idx];

      if (c.type == vm::TYPE_INT64 && c.data.size() == sizeof(int64_t)) {
        int64_t v;
        std::memcpy(&v, c.data.data(), sizeof(int64_t));
        return v;
      }

      if (c.type == vm::TYPE_DOUBLE && c.data.size() == sizeof(double)) {
        double v;
        std::memcpy(&v, c.data.data(), sizeof(double));
        return v;
      }

      return std::nullopt;
    }

    static std::optional<int64_t> ensure_const(
      const std::vector<vm::Constant> &pool,
      const ConstValue &value
    ) {
      for (size_t i = 0; i < pool.size(); ++i) {
        const auto &c = pool[i];

        if (std::holds_alternative<int64_t>(value)) {
          if (c.type == vm::TYPE_INT64 && c.data.size() == sizeof(int64_t)) {
            int64_t v;
            std::memcpy(&v, c.data.data(), sizeof(int64_t));
            if (v == std::get<int64_t>(value)) return static_cast<int64_t>(i);
          }
        } else {
          if (c.type == vm::TYPE_DOUBLE && c.data.size() == sizeof(double)) {
            double v;
            std::memcpy(&v, c.data.data(), sizeof(double));
            if (v == std::get<double>(value)) return static_cast<int64_t>(i);
          }
        }
      }
      return std::nullopt;
    }
};
} // namespace umka::jit
