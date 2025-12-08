#pragma once
#include "base_optimization.h"
#include <cstring>

namespace umka::jit {
class ConstFolding final: public IOptimize {
  public:
    void run(
      std::vector<vm::Command> &code,
      std::vector<vm::Constant> &const_pool,
      vm::FunctionTableEntry &meta
    ) override {
      std::vector<vm::Command> out;
      out.reserve(code.size());

      std::vector<int64_t> stack;

      auto flush_stack = [&]() {
        for (int64_t v: stack) {
          vm::Constant c;
          c.type = vm::TYPE_INT64;
          c.data.resize(8);
          memcpy(c.data.data(), &v, 8);

          const_pool.push_back(c);
          const int64_t idx = const_pool.size() - 1;

          out.push_back(vm::Command{
            static_cast<uint8_t>(vm::OpCode::PUSH_CONST), idx
          });
        }
        stack.clear();
      };


      for (size_t i = 0; i < code.size(); ++i) {
        const auto op = static_cast<vm::OpCode>(code[i].code);

        if (op == vm::OpCode::PUSH_CONST) {
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
    static bool is_foldable_binary(const vm::OpCode op);

    static int64_t load_int(const vm::Constant &c) {
      int64_t v;
      memcpy(&v, c.data.data(), 8);
      return v;
    }

    static int64_t eval(int64_t a, int64_t b, vm::OpCode op) {
      switch (op) {
        case vm::OpCode::ADD: return a + b;
        case vm::OpCode::SUB: return a - b;
        case vm::OpCode::MUL: return a * b;
        case vm::OpCode::DIV: return a / b;
        case vm::OpCode::REM: return a % b;

        case vm::OpCode::LT: return a < b;
        case vm::OpCode::GT: return a > b;
        case vm::OpCode::LTE: return a <= b;
        case vm::OpCode::GTE: return a >= b;
        case vm::OpCode::EQ: return a == b;
        case vm::OpCode::NEQ: return a != b;
        case vm::OpCode::AND: return (a && b);
        case vm::OpCode::OR: return (a || b);
        default:
          return a;
      }
    }
};
inline bool ConstFolding::is_foldable_binary(const vm::OpCode op) {
  switch (op) {
    case vm::OpCode::ADD:
    case vm::OpCode::SUB:
    case vm::OpCode::MUL:
    case vm::OpCode::DIV:
    case vm::OpCode::REM:
    case vm::OpCode::LT:
    case vm::OpCode::GT:
    case vm::OpCode::LTE:
    case vm::OpCode::GTE:
    case vm::OpCode::EQ:
    case vm::OpCode::NEQ:
    case vm::OpCode::AND:
    case vm::OpCode::OR:
      return true;
    default:
      return false;
  }
}
} // namespace umka::jit
