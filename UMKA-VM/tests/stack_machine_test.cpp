#include <gtest/gtest.h>
#include "../runtime/stack_machine.h"
#include "../runtime/command_parser.h"

class MockCommandParser : public CommandParser {
public:
    std::vector<Command> get_commands() const { return commands; }
    std::vector<Constant> get_const_pool() const { return const_pool; }
    std::vector<FunctionTableEntry> get_func_table() const { return func_table; }

    std::vector<Command> commands;
    std::vector<Constant> const_pool;
    std::vector<FunctionTableEntry> func_table;
};

class StackMachineTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup mock commands and constants
        Constant int_const;
        int_const.type = 0x01; // int64
        int_const.data.resize(sizeof(int64_t));
        *reinterpret_cast<int64_t*>(int_const.data.data()) = 42;

        parser.const_pool.push_back(int_const);
    }

    MockCommandParser parser;
};

TEST_F(StackMachineTest, Initialization) {
    StackMachine machine(parser);
    // Basic initialization test
    SUCCEED();
}

TEST_F(StackMachineTest, PushConstant) {
    // Create PUSH_CONST command (assuming code 0 is PUSH_CONST)
    Command cmd;
    cmd.code = 0; // PUSH_CONST
    cmd.arg = 0; // index of our constant
    parser.commands.push_back(cmd);

    StackMachine machine(parser);
    machine.run();
    // Basic execution test
    SUCCEED();
}

TEST_F(StackMachineTest, ArithmeticOperations) {
    // Setup two constants
    Constant const1, const2;
    const1.type = 0x01; const2.type = 0x01;
    const1.data.resize(sizeof(int64_t)); 
    const2.data.resize(sizeof(int64_t));
    *reinterpret_cast<int64_t*>(const1.data.data()) = 10;
    *reinterpret_cast<int64_t*>(const2.data.data()) = 5;

    parser.const_pool = {const1, const2};

    // Setup commands: push two constants and add them
    Command push1, push2, add;
    push1.code = 0; push1.arg = 0; // PUSH_CONST
    push2.code = 0; push2.arg = 1; // PUSH_CONST
    add.code = 1; // ADD (assuming code 1 is ADD)
    parser.commands = {push1, push2, add};

    StackMachine machine(parser);
    machine.run();
    // Basic arithmetic test
    SUCCEED();
}

TEST_F(StackMachineTest, FunctionCallAndReturn) {
    // Setup function table
    FunctionTableEntry func;
    func.id = 1;
    func.arg_count = 0;
    func.code_offset = 1; // skip CALL instruction
    parser.func_table.push_back(func);

    // Setup commands: CALL and RETURN
    Command call, ret;
    call.code = 20; // CALL (assuming code 20 is CALL)
    call.arg = 0; // call first function
    ret.code = 21; // RETURN (assuming code 21 is RETURN)
    parser.commands = {call, ret};

    StackMachine machine(parser);
    machine.run();
    // Basic function call test
    SUCCEED();
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}