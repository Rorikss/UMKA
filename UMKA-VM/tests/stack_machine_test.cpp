#include <gtest/gtest.h>
#include "../model/model.h"
#include "../runtime/stack_machine.h"
#include "../parser/command_parser.h"

using namespace umka::vm;

class MockCommandParser : public CommandParser {
public:
    const std::vector<Command>& get_commands() const { return commands; }
    const std::vector<Constant>& get_const_pool() const { return const_pool; }
    const std::unordered_map<size_t, FunctionTableEntry>& get_func_table() const { return func_table; }

    std::vector<Command> commands;
    std::vector<Constant> const_pool;
    std::unordered_map<size_t, FunctionTableEntry> func_table;
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
        
        std::cerr << "Stack top: " << stack_top << std::endl;
        std::cerr << "Instruction: " << static_cast<int>(cmd.code) << std::endl;
        (*instruction_index)++;
    } };
}

class EntityTest : public ::testing::Test {};

TEST_F(EntityTest, CmpTest) {
    Entity a = Entity{42};
    Entity b = Entity{42};
    EXPECT_TRUE(a == b);
    a = Entity{43};
    EXPECT_FALSE(a == b);
    EXPECT_TRUE(a != b);
    EXPECT_TRUE(a > b);
    EXPECT_TRUE(a >= b);
    EXPECT_FALSE(a < b);
    EXPECT_FALSE(a <= b);
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
    func.code_offset_end = 3;  // Points after the RETURN instruction
    parser.func_table[0] = func;

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
    func.code_offset = 2;  // Points to the PUSH_CONST 0 instruction
    func.code_offset_end = 6;  // Points after the RETURN instruction
    parser.func_table[0] = func;

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

TEST_F(StackMachineTest, LoopExecution) {
    // Setup constants for loop: counter initial value and loop limit
    Constant const0, const1, const3;
    const0.type = TYPE_INT64; const1.type = TYPE_INT64; const3.type = TYPE_INT64;
    const0.data.resize(sizeof(int64_t));
    const1.data.resize(sizeof(int64_t));
    const3.data.resize(sizeof(int64_t));
    *reinterpret_cast<int64_t*>(const0.data.data()) = 0;  // counter initial value
    *reinterpret_cast<int64_t*>(const1.data.data()) = 1;  // increment
    *reinterpret_cast<int64_t*>(const3.data.data()) = 3;  // loop limit
    
    parser.const_pool = {const0, const1, const3};
    
    // Setup commands for a simple loop that increments a counter from 0 to 3:
    // 1. PUSH_CONST 0 (counter initial value)
    // 2. STORE 0 (store counter in variable 0)
    // 3. LOAD 0 (load counter)
    // 4. PUSH_CONST 2 (load limit)
    // 5. LT (compare counter < limit)
    // 6. JMP_IF_FALSE 14 (jump to end if condition false)
    // 7. LOAD 0 (load counter)
    // 8. PUSH_CONST 1 (load increment)
    // 9. ADD (increment counter)
    // 10. STORE 0 (store incremented counter)
    // 11. JMP 3 (jump back to loop condition)
    // 12. RETURN (end)
    
    Command push0, store0, load0, push2, lt, jmp_if_false, push1, add, jmp, ret;
    push0.code = PUSH_CONST; push0.arg = 0;
    store0.code = STORE; store0.arg = 0;
    load0.code = LOAD; load0.arg = 0;
    push2.code = PUSH_CONST; push2.arg = 2;
    lt.code = LT;
    jmp_if_false.code = JMP_IF_FALSE; jmp_if_false.arg = 11;  // Points at RETURN instruction
    push1.code = PUSH_CONST; push1.arg = 1;
    add.code = ADD;
    jmp.code = JMP; jmp.arg = 2;  // Points to LOAD 0
    ret.code = RETURN;
    
    parser.commands = {
        push0, store0,
        load0, push2, lt, jmp_if_false, 
            load0, push1, add, store0, 
        jmp,
        ret
    };
    
    std::vector<uint8_t> flow = {
        PUSH_CONST, STORE,
        LOAD, PUSH_CONST, LT, JMP_IF_FALSE,
            LOAD, PUSH_CONST, ADD, STORE,
        JMP,
        LOAD, PUSH_CONST, LT, JMP_IF_FALSE,
            LOAD, PUSH_CONST, ADD, STORE,
        JMP, 
        LOAD, PUSH_CONST, LT, JMP_IF_FALSE,
            LOAD, PUSH_CONST, ADD, STORE,
        JMP,
        LOAD, PUSH_CONST, LT, JMP_IF_FALSE,
        RETURN
    };

    // Create validator for expected instruction sequence
    auto [calls, validator] = make_instruction_validator(flow);

    StackMachine machine(parser);
    machine.run<DebugMod>(validator);
    ASSERT_EQ(*calls, flow.size());
}

TEST_F(StackMachineTest, FunctionCallWithArguments) {
    // Setup constants for function arguments
    Constant const5, const3;
    const5.type = TYPE_INT64; const3.type = TYPE_INT64;
    const5.data.resize(sizeof(int64_t));
    const3.data.resize(sizeof(int64_t));
    *reinterpret_cast<int64_t*>(const5.data.data()) = 5;
    *reinterpret_cast<int64_t*>(const3.data.data()) = 3;
    
    parser.const_pool = {const5, const3};
    
    // Setup function table - function that takes 2 arguments and adds them
    FunctionTableEntry func;
    func.id = 1;
    func.arg_count = 2;  // Takes 2 arguments
    func.code_offset = 4;  // Points to the ADD instruction
    func.code_offset_end = 8;  // Points after the RETURN instruction
    parser.func_table[0] = func;
    
    // Setup commands:
    // 1. PUSH_CONST 0 (push first argument: 5)
    // 2. PUSH_CONST 1 (push second argument: 3)
    // 3. CALL 0 (call function with 2 arguments)
    // 4. RETURN (main return)
    // 5. ADD (function body: add the two arguments)
    // 6. RETURN (function return)
    
    std::vector<Command> cmd = {
        Command{PUSH_CONST, 0},
        Command{PUSH_CONST, 1},
        Command{CALL, 0},
        Command{RETURN},
        Command{LOAD, 0},
        Command{LOAD, 1},
        Command{ADD},
        Command{RETURN},
    };

    parser.commands = cmd;
    
    // Create validator for expected instruction sequence
    auto [calls, validator] = make_instruction_validator({
        PUSH_CONST, PUSH_CONST, CALL, LOAD, LOAD, ADD, RETURN, RETURN
    });

    StackMachine machine(parser);
    machine.run<DebugMod>(validator);
    ASSERT_EQ(*calls, 8);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}