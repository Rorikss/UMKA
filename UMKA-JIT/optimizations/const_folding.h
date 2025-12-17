#pragma once
#include "base_optimization.h"
#include <model/model.h>
#include <cstring>
#include <variant>

namespace umka::jit {
class ConstFolding final: public IOptimize {
  public:
    void run(
      std::vector<vm::Command> &code,
      std::vector<vm::Constant> &const_pool,
      std::unordered_map<size_t, vm::FunctionTableEntry> &,
      vm::FunctionTableEntry &meta
    ) override {
      std::vector<vm::Command> out;
      out.reserve(code.size());

      using ConstValue = std::variant<int64_t, double>;
      std::vector<ConstValue> stack;

      auto flush_stack = [&]() {
        for (const auto &val: stack) {
          vm::Constant c;
          if (std::holds_alternative<int64_t>(val)) {
            c.type = vm::TYPE_INT64;
            c.data.resize(8);
            int64_t v = std::get<int64_t>(val);
            memcpy(c.data.data(), &v, 8);
          } else {
            c.type = vm::TYPE_DOUBLE;
            c.data.resize(8);
            double v = std::get<double>(val);
            memcpy(c.data.data(), &v, 8);
          }

          const_pool.push_back(c);
          const int64_t idx = const_pool.size() - 1;

          out.push_back(vm::Command{
            static_cast<uint8_t>(vm::OpCode::PUSH_CONST), idx
          });
        }
        stack.clear();
      };

      auto needs_flush = [](vm::OpCode op) {
        switch (op) {
          case vm::OpCode::CALL:
          case vm::OpCode::LOAD:
          case vm::OpCode::STORE:
          case vm::OpCode::RETURN:
          case vm::OpCode::JMP:
          case vm::OpCode::JMP_IF_FALSE:
          case vm::OpCode::JMP_IF_TRUE:
          case vm::OpCode::BUILD_ARR:
          case vm::OpCode::POP:
          case vm::OpCode::CALL_METHOD:
          case vm::OpCode::GET_FIELD:
          case vm::OpCode::TO_STRING:
          case vm::OpCode::TO_INT:
          case vm::OpCode::TO_DOUBLE:
          case vm::OpCode::OPCOT:
            return true;
          default:
            return false;
        }
      };

      for (size_t i = 0; i < code.size(); ++i) {
        const auto op = static_cast<vm::OpCode>(code[i].code);

        if (needs_flush(op)) {
          flush_stack();
          out.push_back(code[i]);
          continue;
        }

        if (op == vm::OpCode::PUSH_CONST) {
          const auto &constant = const_pool[code[i].arg];
          if (constant.type == vm::TYPE_INT64) {
            int64_t value = load_int(constant);
            stack.push_back(value);
          } else if (constant.type == vm::TYPE_DOUBLE) {
            double value = load_double(constant);
            stack.push_back(value);
          } else {
            flush_stack();
            out.push_back(code[i]);
          }
          continue;
        }

        if (is_foldable_binary(op)) {
          if (stack.size() >= 2) {
            ConstValue rhs = stack.back();
            stack.pop_back();
            ConstValue lhs = stack.back();
            stack.pop_back();

            ConstValue res = eval(lhs, rhs, op);
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

    static double load_double(const vm::Constant &c) {
      double v;
      memcpy(&v, c.data.data(), 8);
      return v;
    }

    static std::variant<int64_t, double> eval(
      const std::variant<int64_t, double> &a,
      const std::variant<int64_t, double> &b,
      const vm::OpCode op) {
      if (std::holds_alternative<int64_t>(a) && std::holds_alternative<int64_t>(b)) {
        int64_t lhs = std::get<int64_t>(a);
        int64_t rhs = std::get<int64_t>(b);
        switch (op) {
          case vm::OpCode::ADD: return lhs + rhs;
          case vm::OpCode::SUB: return lhs - rhs;
          case vm::OpCode::MUL: return lhs * rhs;
          case vm::OpCode::DIV: return rhs != 0 ? lhs / rhs : 0;
          case vm::OpCode::REM: return rhs != 0 ? lhs % rhs : 0;
          case vm::OpCode::LT: return (lhs < rhs);
          case vm::OpCode::GT: return (lhs > rhs);
          case vm::OpCode::LTE: return (lhs <= rhs);
          case vm::OpCode::GTE: return (lhs >= rhs);
          case vm::OpCode::EQ: return (lhs == rhs);
          case vm::OpCode::NEQ: return (lhs != rhs);
          case vm::OpCode::AND: return (lhs && rhs);
          case vm::OpCode::OR: return (lhs || rhs);
          default: return lhs;
        }
      }

      if (std::holds_alternative<double>(a) && std::holds_alternative<double>(b)) {
        double lhs = std::get<double>(a);
        double rhs = std::get<double>(b);
        switch (op) {
          case vm::OpCode::ADD: return lhs + rhs;
          case vm::OpCode::SUB: return lhs - rhs;
          case vm::OpCode::MUL: return lhs * rhs;
          case vm::OpCode::DIV: return rhs != 0.0 ? lhs / rhs : 0.0;
          case vm::OpCode::LT: return (lhs < rhs);
          case vm::OpCode::GT: return (lhs > rhs);
          case vm::OpCode::LTE: return (lhs <= rhs);
          case vm::OpCode::GTE: return (lhs >= rhs);
          case vm::OpCode::EQ: return (lhs == rhs);
          case vm::OpCode::NEQ: return (lhs != rhs);
          case vm::OpCode::AND: return (lhs && rhs);
          case vm::OpCode::OR: return (lhs || rhs);
          default: return lhs;
        }
      }

      double lhs_d = std::holds_alternative<int64_t>(a)
                       ? static_cast<double>(std::get<int64_t>(a))
                       : std::get<double>(a);
      double rhs_d = std::holds_alternative<int64_t>(b)
                       ? static_cast<double>(std::get<int64_t>(b))
                       : std::get<double>(b);

      switch (op) {
        case vm::OpCode::ADD: return lhs_d + rhs_d;
        case vm::OpCode::SUB: return lhs_d - rhs_d;
        case vm::OpCode::MUL: return lhs_d * rhs_d;
        case vm::OpCode::DIV: return rhs_d != 0.0 ? lhs_d / rhs_d : 0.0;
        case vm::OpCode::LT: return (lhs_d < rhs_d);
        case vm::OpCode::GT: return (lhs_d > rhs_d);
        case vm::OpCode::LTE: return (lhs_d <= rhs_d);
        case vm::OpCode::GTE: return (lhs_d >= rhs_d);
        case vm::OpCode::EQ: return (lhs_d == rhs_d);
        case vm::OpCode::NEQ: return (lhs_d != rhs_d);
        case vm::OpCode::AND: return (lhs_d && rhs_d);
        case vm::OpCode::OR: return (lhs_d || rhs_d);
        default: return lhs_d;
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
