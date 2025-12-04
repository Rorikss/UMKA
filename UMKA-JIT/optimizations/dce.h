#pragma once

#include "base_optimization.h"
#include <unordered_set>
#include <functional>

namespace umka::jit {

class DeadCodeElimination final: public IOptimize {
public:
    void run(
        std::vector<Command>& code,
        std::vector<Constant>& const_pool,
        FunctionTableEntry&
    ) override
    {
        if (code.empty()) {
            return;
        }

        const size_t n = code.size();

        std::vector<bool> reachable(n, false);

        std::function<void(size_t)> dfs = [&](size_t i) {
            if (i >= n || reachable[i]) return;

            reachable[i] = true;
            auto op = static_cast<OpCode>(code[i].code);

            if (op == OpCode::JMP) {
                int target = static_cast<int>(i) + static_cast<int>(code[i].arg);
                if (target >= 0 && target < static_cast<int>(n)) {
                    dfs(static_cast<size_t>(target));
                }
                return;
            }

            if (op == JMP_IF_FALSE || op == JMP_IF_TRUE) {

                bool condition_known = false;
                bool condition_value = false;

                int target = static_cast<int>(i) + static_cast<int>(code[i].arg);

                if (i > 0 && code[i-1].code == OpCode::PUSH_CONST) {
                    condition_known = true;
                    condition_value = load_int(const_pool[code[i-1].arg]);
                }

                if (!condition_known) {
                    dfs(i + 1);
                    dfs(target);
                } else {
                    if (op == JMP_IF_FALSE) {
                        if (!condition_value) dfs(target);
                        else dfs(i + 1);
                    } else { // JMP_IF_TRUE
                        if (condition_value) dfs(target);
                        else dfs(i + 1);
                    }
                }
                return;
            }

            dfs(i + 1);
        };

        dfs(0);

        auto jump_targets = compute_jump_targets(code, reachable);

        // needed[i] = надо ли оставлять эту инструкцию в итоговом коде
        std::vector<bool> needed(n, false);

        // цели прыжков всегда должны существовать
        for (size_t t : jump_targets) {
            if (t < n && reachable[t]) {
                needed[t] = true;
            }
        }

        int demand = 0;

        for (int i = static_cast<int>(n) - 1; i >= 0; --i) {
            if (!reachable[static_cast<size_t>(i)]) {
                continue;
            }

            auto op = static_cast<OpCode>(code[i].code);
            const int consumes = stack_consumed(op, code[i].arg);
            const int produces = stack_produced(op);
            const bool has_side_effects = is_side_effect(op);

            const bool is_needed =
                has_side_effects ||
                needed[static_cast<size_t>(i)] ||
                demand > 0;

            if (is_needed) {
                needed[static_cast<size_t>(i)] = true;
                demand -= produces;
                if (demand < 0) demand = 0;
                demand += consumes;
            }
        }

        std::vector<Command> new_code;
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

            auto op = static_cast<OpCode>(code[old_i].code);
            if (op == OpCode::JMP || op == OpCode::JMP_IF_FALSE || op == OpCode::JMP_IF_TRUE) {
                int old_target = static_cast<int>(old_i) + static_cast<int>(code[old_i].arg);

                if (old_target >= 0 && old_target < static_cast<int>(n) &&
                    reachable[static_cast<size_t>(old_target)] &&
                    needed[static_cast<size_t>(old_target)]) {

                    int new_target = old_to_new[static_cast<size_t>(old_target)];
                    if (new_target >= 0) {
                        new_code[static_cast<size_t>(new_i)].arg = new_target - new_i;
                    }
                }
            }
        }
        code.swap(new_code);
    }

private:

    static int stack_consumed(OpCode op, int arg) {
        switch (op) {
            case OpCode::PUSH_CONST:
                return 0;

            case OpCode::POP:
                return 1;

            case OpCode::LOAD:
                return 0;

            case OpCode::STORE:
                return 1;

            case OpCode::ADD:
            case OpCode::SUB:
            case OpCode::MUL:
            case OpCode::DIV:
            case OpCode::REM:
            case OpCode::EQ:
            case OpCode::NEQ:
            case OpCode::LT:
            case OpCode::GT:
            case OpCode::LTE:
            case OpCode::GTE:
            case OpCode::AND:
            case OpCode::OR:
                return 2;

            case OpCode::NOT:
            case OpCode::TO_STRING:
            case OpCode::TO_INT:
            case OpCode::TO_DOUBLE:
            case OpCode::OPCOT:
                return 1;

            case OpCode::CALL:
                return arg;   // arg = число аргументов функции

            case OpCode::BUILD_ARR:
                return arg;   // N элементов массива

            case OpCode::RETURN:
                return 1;

            case OpCode::JMP:
                return 0;

            case OpCode::JMP_IF_FALSE:
            case OpCode::JMP_IF_TRUE:
                return 1;

            default:
                return 0;
        }
    }

    static int stack_produced(OpCode op) {
        switch (op) {
            case OpCode::PUSH_CONST:
            case OpCode::LOAD:
                return 1;

            case OpCode::ADD:
            case OpCode::SUB:
            case OpCode::MUL:
            case OpCode::DIV:
            case OpCode::REM:
            case OpCode::EQ:
            case OpCode::NEQ:
            case OpCode::LT:
            case OpCode::GT:
            case OpCode::LTE:
            case OpCode::GTE:
            case OpCode::AND:
            case OpCode::OR:
            case OpCode::NOT:
            case OpCode::TO_STRING:
            case OpCode::TO_INT:
            case OpCode::TO_DOUBLE:
            case OpCode::OPCOT:
                return 1;

            case OpCode::CALL:
                return 1; // CALL кладёт на стек 1 возвращаемое значение

            case OpCode::BUILD_ARR:
                return 1;

            case OpCode::RETURN:
            case OpCode::STORE:
            case OpCode::POP:
                return 0;

            case OpCode::JMP:
            case OpCode::JMP_IF_FALSE:
            case OpCode::JMP_IF_TRUE:
                return 0;

            default:
                return 0;
        }
    }

    // Побочные эффекты — то, что нельзя выкинуть даже при отсутствии demand
    static bool is_side_effect(OpCode op) {
        switch (op) {
            case OpCode::STORE:
            case OpCode::RETURN:
            case OpCode::CALL:
            case OpCode::OPCOT:   // OPCOT в вашей VM — операция с эффектом
                return true;
            default:
                return false;
        }
    }

    // Собираем все jump targets среди reachable инструкций
    static std::unordered_set<size_t> compute_jump_targets(
        const std::vector<Command>& code,
        const std::vector<bool>& reachable
    ) {
        std::unordered_set<size_t> result;
        const size_t n = code.size();

        for (size_t i = 0; i < n; ++i) {
            if (!reachable[i]) continue;

            OpCode op = static_cast<OpCode>(code[i].code);
            if (op == OpCode::JMP || op == OpCode::JMP_IF_FALSE || op == OpCode::JMP_IF_TRUE) {
                int target = static_cast<int>(i) + static_cast<int>(code[i].arg);
                if (target >= 0 && target < static_cast<int>(n)) {
                    result.insert(static_cast<size_t>(target));
                }
            }
        }

        return result;
    }
        static int64_t load_int(const Constant &c) {
        int64_t v;
        memcpy(&v, c.data.data(), 8);
        return v;
    }
};

} // namespace umka::jit
