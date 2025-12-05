#pragma once
#include "base_optimization.h"
#include <cstring>

namespace umka::jit {
class ConstFolding final: public IOptimize {
  public:
    void run(
      std::vector<Command> &code,
      std::vector<Constant> &const_pool,
      FunctionTableEntry &meta
    ) override {
      std::vector<Command> out;
      out.reserve(code.size());

      std::vector<int64_t> stack;

      auto flush_stack = [&]() {
        for (int64_t v: stack) {
          Constant c;
          c.type = TYPE_INT64;
          c.data.resize(8);
          memcpy(c.data.data(), &v, 8);

          const_pool.push_back(c);
          const int64_t idx = const_pool.size() - 1;

          out.push_back(Command{
            static_cast<uint8_t>(OpCode::PUSH_CONST), idx
          });
        }
        stack.clear();
      };


      for (size_t i = 0; i < code.size(); ++i) {
        const auto op = static_cast<OpCode>(code[i].code);

        if (op == OpCode::PUSH_CONST) {
          int64_t value = load_int(const_pool[code[i].arg]);
          stack.push_back(value);
          continue;
        }

        if (is_foldable_binary(op)) {
          if (stack.size() >= 2) {
            // можем свернуть прямо сейчас
            int64_t rhs = stack.back();
            stack.pop_back();
            int64_t lhs = stack.back();
            stack.pop_back();

            int64_t res = eval(lhs, rhs, op);
            stack.push_back(res);
          } else {
            flush_stack();
            out.push_back(code[i]);
          }

          continue;
        }
        flush_stack();
        out.push_back(code[i]);
      }

      flush_stack();

      code.swap(out);
    }

  private:
    static bool is_foldable_binary(const OpCode op) {
      switch (op) {
        case OpCode::ADD:
        case OpCode::SUB:
        case OpCode::MUL:
        case OpCode::DIV:
        case OpCode::REM:
        case OpCode::LT:
        case OpCode::GT:
        case OpCode::LTE:
        case OpCode::GTE:
        case OpCode::EQ:
        case OpCode::NEQ:
        case OpCode::AND:
        case OpCode::OR:
          return true;
        default:
          return false;
      }
    }

    static int64_t load_int(const Constant &c) {
      int64_t v;
      memcpy(&v, c.data.data(), 8);
      return v;
    }

    static int64_t eval(int64_t a, int64_t b, OpCode op) {
      switch (op) {
        case OpCode::ADD: return a + b;
        case OpCode::SUB: return a - b;
        case OpCode::MUL: return a * b;
        case OpCode::DIV: return a / b;
        case OpCode::REM: return a % b;

        case OpCode::LT: return a < b;
        case OpCode::GT: return a > b;
        case OpCode::LTE: return a <= b;
        case OpCode::GTE: return a >= b;
        case OpCode::EQ: return a == b;
        case OpCode::NEQ: return a != b;
        case OpCode::AND: return (a && b);
        case OpCode::OR: return (a || b);
        default:
          return a;
      }
    }
};
} // namespace umka::jit
