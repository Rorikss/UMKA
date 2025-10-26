#include <map>
#include <memory>
#include <tuple>
#include <variant>

template<typename T>
using reference = std::weak_ptr<T>;

using unit = std::monostate;

struct Command {
    int64_t code;
    int64_t arg;
};

struct Entity {
    std::variant<int, float, bool, unit, std::string, std::map<int, reference<Entity>>> value;
};

struct StackFrame {
    uint64_t name;
    size_t instruction_index = 0;
    std::unordered_map<int64_t, reference<Entity>> name_resolver = {};
};
