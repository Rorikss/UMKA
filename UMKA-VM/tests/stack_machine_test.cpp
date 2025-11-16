#include <gtest/gtest.h>
#include "../model/model.h"
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

// Helper function to create a debugger that validates instruction execution
auto make_instruction_validator(const std::vector<uint8_t>& expected_instructions)
    -> std::tuple<std::shared_ptr<size_t>, StackMachine::debugger_t>
    {
    auto instruction_index = std::make_shared<size_t>(0);
    auto expected = std::make_shared<std::vector<uint8_t>>(expected_instructions);
    
    return { instruction_index, [instruction_index, expected](Command cmd, std::string stack_top) {
        ASSERT_LT(*instruction_index, expected->size())
            << "Executed more instructions than expected. Current instruction: "
            << static_cast<int>(cmd.code) << ", stack top: " << stack_top;
        
        EXPECT_EQ(cmd.code, (*expected)[*instruction_index])
            << "Instruction mismatch at position " << *instruction_index
            << ". Expected: " << static_cast<int>((*expected)[*instruction_index])
            << ", Got: " << static_cast<int>(cmd.code)
            << ", Stack top: " << stack_top;
        
        (*instruction_index)++;
    } };
}

class StackMachineTest : public ::testing::Test {
protected:
    Command return_cmd = Command{RETURN, 0};
    void SetUp() override {
        // Setup mock commands and constants
        Constant int_const;
        int_const.type = TYPE_INT64;
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
    cmd.code = PUSH_CONST;
    cmd.arg = 0; // index of our constant
    parser.commands.push_back(cmd);
    parser.commands.push_back(return_cmd);

    auto [calls, validator] = make_instruction_validator({PUSH_CONST, RETURN});

    StackMachine machine(parser);
    machine.run<DebugMod>(validator);
    ASSERT_EQ(*calls, 2);
}

TEST_F(StackMachineTest, ArithmeticOperations) {
    // Setup two constants
    Constant const1, const2;
    const1.type = TYPE_INT64; const2.type = TYPE_INT64;
    const1.data.resize(sizeof(int64_t));
    const2.data.resize(sizeof(int64_t));
    *reinterpret_cast<int64_t*>(const1.data.data()) = 10;
    *reinterpret_cast<int64_t*>(const2.data.data()) = 5;

    parser.const_pool = {const1, const2};

    // Setup commands: push two constants and add them
    Command push1, push2, add;
    push1.code = PUSH_CONST; push1.arg = 0;
    push2.code = PUSH_CONST; push2.arg = 1;
    add.code = ADD;
    parser.commands = {push1, push2, add, return_cmd};

    auto [calls, validator] = make_instruction_validator(
        {PUSH_CONST, PUSH_CONST, ADD, RETURN}
    );

    StackMachine machine(parser);
    machine.run<DebugMod>(validator);
    ASSERT_EQ(*calls, 4);
}

TEST_F(StackMachineTest, FunctionCallAndReturn) {
    // Setup function table
    FunctionTableEntry func;
    func.id = 1;
    func.arg_count = 0;
    func.code_offset = 2;  // Points to the RETURN instruction
    parser.func_table.push_back(func);

    // Setup commands: CALL and RETURN
    Command call, ret;
    call.code = CALL;
    call.arg = 0;
    ret.code = RETURN;
    parser.commands = {call, return_cmd, ret};

    // Create validator for expected instruction sequence
    auto [calls, validator] = make_instruction_validator({CALL, RETURN, RETURN});

    StackMachine machine(parser);
    machine.run<DebugMod>(validator);
    ASSERT_EQ(*calls, 3);
}

TEST_F(StackMachineTest, FunctionCallSum) {
    // Setup function table
    FunctionTableEntry func;
    func.id = 1;
    func.arg_count = 0;
    func.code_offset = 2;  // Points to the RETURN instruction
    parser.func_table.push_back(func);

    // Setup constants
    Constant const1, const2;
    const1.type = TYPE_INT64; const2.type = TYPE_INT64;
    const1.data.resize(sizeof(int64_t));
    const2.data.resize(sizeof(int64_t));
    *reinterpret_cast<int64_t*>(const1.data.data()) = 10;
    *reinterpret_cast<int64_t*>(const2.data.data()) = 5;

    parser.const_pool = {const1, const2};

    // Setup commands: CALL and RETURN
    Command call, ret;
    call.code = CALL;
    call.arg = 0;
    ret.code = RETURN;
    parser.commands = {
        call,
        return_cmd,
        Command{PUSH_CONST, 0},
        Command{PUSH_CONST, 1},
        Command{ADD},
        ret,
    };

    // Create validator for expected instruction sequence
    auto [calls, validator] = make_instruction_validator({
        CALL, PUSH_CONST, PUSH_CONST, ADD, RETURN, RETURN
    });

    StackMachine machine(parser);
    machine.run<DebugMod>(validator);
    ASSERT_EQ(*calls, 6);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}