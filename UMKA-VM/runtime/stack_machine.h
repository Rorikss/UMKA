#include <cstdint>
#include <memory>
#include <vector>
#include <unordered_map>

class StackMachine {
public:
    StackMachine() {} // пока так а так отсюда надо из парсера байткода передавать инструкции 



private:
    std::vector<command> commands;
    std::vector<std::shared_ptr<Entity>> heap = {};
    std::vector<StackFrame> stack_of_functions = {};
}
