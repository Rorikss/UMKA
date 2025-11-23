#pragma once

#include <cstdint>
#include <string>

struct ConstEntry {
    enum Type : uint8_t { INT = 1, DOUBLE = 2, STRING = 3 } type;
    int64_t _int{};
    double _double{};
    std::string _str;

    ConstEntry(int64_t v)  : type(INT), _int(v) {}
    ConstEntry(double v)   : type(DOUBLE), _double(v) {}
    ConstEntry(const std::string& ss) : type(STRING), _str(ss) {}
    ConstEntry() = default;
};

struct FunctionEntry {
    int64_t code_offset_beg{0};  // ← ИЗМЕНИТЬ НАЗВАНИЕ
    int64_t code_offset_end{0};  // ← ДОБАВИТЬ НОВОЕ ПОЛЕ
    int64_t arg_count{0};
    int64_t local_count{0};
};