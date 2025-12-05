#include <model/model.h>
#include <parser/command_parser.h>
#include "const_folding.h"
#include "dce.h"

#include "gtest/gtest.h"


static Constant make_int(int64_t v) {
  Constant c;
  c.type = TYPE_INT64;
  c.data.resize(8);
  memcpy(c.data.data(), &v, 8);
  return c;
}

static Command cmd(OpCode op, int64_t arg = 0) {
  return Command{ (uint8_t)op, arg };
}

TEST(JitConstFolding, ArithmeticNested) {
  // const pool: 2,3,4
  std::vector<Constant> pool = {
    make_int(2),
    make_int(3),
    make_int(4)
};

  std::vector<Command> code = {
    cmd(OpCode::PUSH_CONST, 0), // 2
    cmd(OpCode::PUSH_CONST, 1), // 3
    cmd(OpCode::PUSH_CONST, 2), // 4
    cmd(OpCode::MUL),           // 3*4
    cmd(OpCode::ADD),           // 2+(3*4)
    cmd(OpCode::STORE, 0)
};

  FunctionTableEntry meta{};
  meta.arg_count = 0;
  meta.local_count = 1;

  umka::jit::ConstFolding folding;
  folding.run(code, pool, meta);

  // ожидается: PUSH_CONST 14, STORE 0
  ASSERT_EQ(code.size(), 2);

  ASSERT_EQ(static_cast<OpCode>(code[0].code), OpCode::PUSH_CONST);
  int64_t result = 0;
  memcpy(&result, pool[code[0].arg].data.data(), 8);
  ASSERT_EQ(result, 14);

  ASSERT_EQ(static_cast<OpCode>(code[1].code), OpCode::STORE);
  ASSERT_EQ(code[1].arg, 0);
}

TEST(JitConstFolding, MultiplyTwoSums) {
  std::vector<Constant> pool = {
    make_int(1), // 0
    make_int(2), // 1
    make_int(3), // 2
    make_int(4)  // 3
};

  std::vector<Command> code = {
    cmd(OpCode::PUSH_CONST, 0),
    cmd(OpCode::PUSH_CONST, 1),
    cmd(OpCode::ADD),           // = 3

    cmd(OpCode::PUSH_CONST, 2),
    cmd(OpCode::PUSH_CONST, 3),
    cmd(OpCode::ADD),           // = 7

    cmd(OpCode::MUL),           // 3*7=21
    cmd(OpCode::STORE, 0)
};

  FunctionTableEntry meta{};

  umka::jit::ConstFolding folding;
  folding.run(code, pool, meta);

  ASSERT_EQ(code.size(), 2);

  ASSERT_EQ((OpCode)code[0].code, OpCode::PUSH_CONST);

  int64_t r = 0;
  memcpy(&r, pool[code[0].arg].data.data(), 8);
  ASSERT_EQ(r, 21);

  ASSERT_EQ((OpCode)code[1].code, OpCode::STORE);
}

TEST(JitConstFolding, CannotFold_LoadAndConst)
{
  std::vector<Constant> pool = { make_int(1) };

  std::vector<Command> code = {
    cmd(OpCode::PUSH_CONST, 0),
    cmd(OpCode::LOAD, 0),
    cmd(OpCode::ADD)
};

  FunctionTableEntry meta{};

  umka::jit::ConstFolding folding;
  folding.run(code, pool, meta);


  ASSERT_EQ(code.size(), 3);

  EXPECT_EQ(static_cast<OpCode>(code[0].code), OpCode::PUSH_CONST);
  EXPECT_EQ(static_cast<OpCode>(code[1].code), OpCode::LOAD);
  EXPECT_EQ(static_cast<OpCode>(code[2].code), OpCode::ADD);
}

TEST(JitConstFolding, CannotFoldAcrossStore)
{
  std::vector<Constant> pool = { make_int(2), make_int(3) };

  std::vector<Command> code = {
    cmd(OpCode::PUSH_CONST, 0),
    cmd(OpCode::PUSH_CONST, 1),
    cmd(OpCode::STORE, 0),   // ломаем стек констант!
    cmd(OpCode::ADD)
};

  FunctionTableEntry meta{};

  umka::jit::ConstFolding folding;
  folding.run(code, pool, meta);

  ASSERT_EQ(code.size(), 4);

  EXPECT_EQ(static_cast<OpCode>(code[0].code), OpCode::PUSH_CONST);
  EXPECT_EQ(static_cast<OpCode>(code[1].code), OpCode::PUSH_CONST);
  EXPECT_EQ(static_cast<OpCode>(code[2].code), OpCode::STORE);
  EXPECT_EQ(static_cast<OpCode>(code[3].code), OpCode::ADD);
}

TEST(JitConstFolding, CannotFoldAcrossCall)
{
  std::vector<Constant> pool = { make_int(2), make_int(3) };

  std::vector<Command> code = {
    cmd(OpCode::PUSH_CONST, 0),
    cmd(OpCode::PUSH_CONST, 1),
    cmd(OpCode::CALL, 10),  // f
    cmd(OpCode::ADD)
};

  FunctionTableEntry meta{};

  umka::jit::ConstFolding folding;
  folding.run(code, pool, meta);

  ASSERT_EQ(code.size(), 4);
  EXPECT_EQ(static_cast<OpCode>(code[0].code), OpCode::PUSH_CONST);
  EXPECT_EQ(static_cast<OpCode>(code[1].code), OpCode::PUSH_CONST);
  EXPECT_EQ(static_cast<OpCode>(code[2].code), OpCode::CALL);
  EXPECT_EQ(static_cast<OpCode>(code[3].code), OpCode::ADD);
}

TEST(JitConstFolding, CannotFoldMixedStackValues)
{
  std::vector<Constant> pool = { make_int(1), make_int(2) };

  std::vector<Command> code = {
    cmd(OpCode::PUSH_CONST, 0),
    cmd(OpCode::LOAD, 0),
    cmd(OpCode::PUSH_CONST, 1),
    cmd(OpCode::ADD)
};

  FunctionTableEntry meta{};
  umka::jit::ConstFolding folding;

  folding.run(code, pool, meta);

  ASSERT_EQ(code.size(), 4);
  EXPECT_EQ(static_cast<OpCode>(code[0].code), OpCode::PUSH_CONST);
  EXPECT_EQ(static_cast<OpCode>(code[1].code), OpCode::LOAD);
  EXPECT_EQ(static_cast<OpCode>(code[2].code), OpCode::PUSH_CONST);
  EXPECT_EQ(static_cast<OpCode>(code[3].code), OpCode::ADD);
}

TEST(JitDCE, RemoveUnusedArithmetic) {
  std::vector pool = { make_int(1), make_int(2) };

  std::vector code = {
    cmd(OpCode::PUSH_CONST, 0),
    cmd(OpCode::PUSH_CONST, 1),
    cmd(OpCode::ADD),        // <- result unused
    cmd(OpCode::PUSH_CONST, 0),
    cmd(OpCode::RETURN)
};

  FunctionTableEntry meta{};
  umka::jit::DeadCodeElimination dce;
  dce.run(code, pool, meta);

  ASSERT_EQ(code.size(), 2);
  EXPECT_EQ(static_cast<OpCode>(code[0].code), OpCode::PUSH_CONST);
  EXPECT_EQ(static_cast<OpCode>(code[1].code), OpCode::RETURN);
}

TEST(JitDCE, KeepProducerBeforeStore) {
  std::vector pool = { make_int(10) };

  std::vector code = {
    cmd(OpCode::PUSH_CONST, 0),
    cmd(OpCode::STORE, 0)
};

  FunctionTableEntry meta{};
  umka::jit::DeadCodeElimination dce;
  dce.run(code, pool, meta);

  ASSERT_EQ(code.size(), 2);
  EXPECT_EQ(static_cast<OpCode>(code[0].code), OpCode::PUSH_CONST);
  EXPECT_EQ(static_cast<OpCode>(code[1].code), OpCode::STORE);
}

TEST(JitDCE, KeepCallEvenIfResultUnused) {
  std::vector<Constant> pool = {};

  std::vector<Command> code = {
    cmd(OpCode::CALL, 2),  // needs 2 args
    cmd(OpCode::POP)       // return value unused
};

  FunctionTableEntry meta{};
  umka::jit::DeadCodeElimination dce;
  dce.run(code, pool, meta);

  ASSERT_EQ(code.size(), 1);
  EXPECT_EQ((OpCode)code[0].code, OpCode::CALL);
}

TEST(JitDCE, CallArgumentsAreNeeded) {
  std::vector pool = { make_int(1), make_int(2) };

  std::vector code = {
    cmd(OpCode::PUSH_CONST, 0),
    cmd(OpCode::PUSH_CONST, 1),
    cmd(OpCode::CALL, 2),
    cmd(OpCode::POP)   // result unused
};

  FunctionTableEntry meta{};
  umka::jit::DeadCodeElimination dce;
  dce.run(code, pool, meta);

  ASSERT_EQ(code.size(), 3);
  EXPECT_EQ(static_cast<OpCode>(code[0].code), OpCode::PUSH_CONST);
  EXPECT_EQ(static_cast<OpCode>(code[1].code), OpCode::PUSH_CONST);
  EXPECT_EQ(static_cast<OpCode>(code[2].code), OpCode::CALL);
}

TEST(JitDCE, RemoveUnreachableAfterJump) {
  std::vector<Constant> pool = {};

  std::vector<Command> code = {
    cmd(OpCode::JMP, 2),      // skip 2
    cmd(OpCode::ADD),         // unreachable
    cmd(OpCode::MUL),         // live
    cmd(OpCode::RETURN)       // live
};

  FunctionTableEntry meta{};
  umka::jit::DeadCodeElimination dce;
  dce.run(code, pool, meta);

  ASSERT_EQ(code.size(), 3);
  EXPECT_EQ(static_cast<OpCode>(code[0].code), OpCode::JMP);
  EXPECT_EQ(static_cast<OpCode>(code[1].code), OpCode::MUL);
  EXPECT_EQ(static_cast<OpCode>(code[2].code), OpCode::RETURN);

  EXPECT_EQ(code[0].arg, 1); // from JMP to RETURN
}

TEST(JitDCE, DeadAfterReturn) {
  std::vector pool = { make_int(10), make_int(20) };

  std::vector code = {
    cmd(OpCode::PUSH_CONST, 0),
    cmd(OpCode::RETURN),
    cmd(OpCode::PUSH_CONST, 1), // dead
    cmd(OpCode::ADD),           // dead
};

  FunctionTableEntry meta{};
  umka::jit::DeadCodeElimination dce;
  dce.run(code, pool, meta);

  ASSERT_EQ(code.size(), 2);
  EXPECT_EQ(static_cast<OpCode>(code[0].code), OpCode::PUSH_CONST);
  EXPECT_EQ(static_cast<OpCode>(code[1].code), OpCode::RETURN);
}


TEST(JitDCE, DeadPopAndTrailingCodeAfterCall) {
  std::vector pool = { make_int(1), make_int(2), make_int(3) };

  std::vector code = {
    cmd(OpCode::PUSH_CONST, 0),
    cmd(OpCode::PUSH_CONST, 1),
    cmd(OpCode::CALL, 2),
    cmd(OpCode::POP),               // dead
    cmd(OpCode::PUSH_CONST, 2),     // dead
};

  FunctionTableEntry meta{};
  umka::jit::DeadCodeElimination dce;
  dce.run(code, pool, meta);

  ASSERT_EQ(code.size(), 3);
  EXPECT_EQ(static_cast<OpCode>(code[0].code), OpCode::PUSH_CONST);
  EXPECT_EQ(static_cast<OpCode>(code[1].code), OpCode::PUSH_CONST);
  EXPECT_EQ(static_cast<OpCode>(code[2].code), OpCode::CALL);
}

TEST(JitDCE, DeadLoopConstantFalse) {
  std::vector pool = { make_int(0) };

  std::vector code = {
    cmd(OpCode::PUSH_CONST, 0),
    cmd(OpCode::JMP_IF_FALSE, 4),
    cmd(OpCode::LOAD, 0),   // dead
    cmd(OpCode::ADD),       // dead
    cmd(OpCode::JMP, -2),   // dead
    cmd(OpCode::RETURN),
};

  FunctionTableEntry meta{};
  umka::jit::DeadCodeElimination dce;
  dce.run(code, pool, meta);

  ASSERT_EQ(code.size(), 3);
  EXPECT_EQ(static_cast<OpCode>(code[0].code), OpCode::PUSH_CONST);
  EXPECT_EQ(static_cast<OpCode>(code[1].code), OpCode::JMP_IF_FALSE);
  EXPECT_EQ(static_cast<OpCode>(code[2].code), OpCode::RETURN);
}



