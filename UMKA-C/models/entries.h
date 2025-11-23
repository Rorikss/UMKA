#pragma once

#include <cstdint>
#include <string>

struct ConstEntry {
    enum Type : uint8_t { INT = 1, DOUBLE = 2, STRING = 3 } type;
    int64_t i{};
    double d{};
    std::string s;

    ConstEntry(int64_t v)  : type(INT), i(v) {}
    ConstEntry(double v)   : type(DOUBLE), d(v) {}
    ConstEntry(const std::string& ss) : type(STRING), s(ss) {}
    ConstEntry() = default;
};

struct FunctionEntry {
    int64_t codeOffset{0};
    int64_t argCount{0};
    int64_t localCount{0};
};