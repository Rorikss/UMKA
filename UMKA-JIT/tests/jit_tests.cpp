#include <model/model.h>
#include <parser/command_parser.h>
#include "const_folding.h"
#include "dce.h"

#include "gtest/gtest.h"


static umka::vm::Constant make_int(int64_t v) {
  umka::vm::Constant c;
  c.type = umka::vm::TYPE_INT64;
  c.data.resize(8);
  memcpy(c.data.data(), &v, 8);
  return c;
}

static umka::vm::Command cmd(const umka::vm::OpCode op, const int64_t arg = 0) {
  return umka::vm::Command{ static_cast<uint8_t>(op), arg };
}

TEST(JitConstFolding, ArithmeticNested) {
  // const pool: 2,3,4
  std::vector<umka::vm::Constant> pool = {
    make_int(2),
    make_int(3),
    make_int(4)
};

  std::vector<umka::vm::Command> code = {
    cmd(umka::vm::OpCode::PUSH_CONST, 0), // 2
    cmd(umka::vm::OpCode::PUSH_CONST, 1), // 3
    cmd(umka::vm::OpCode::PUSH_CONST, 2), // 4
    cmd(umka::vm::OpCode::MUL),           // 3*4
    cmd(umka::vm::OpCode::ADD),           // 2+(3*4)
    cmd(umka::vm::OpCode::STORE, 0)
};

  umka::vm::FunctionTableEntry meta{};
  meta.arg_count = 0;
  meta.local_count = 1;

  umka::jit::ConstFolding folding;
  folding.run(code, pool, meta);

  // ожидается: PUSH_CONST 14, STORE 0
  ASSERT_EQ(code.size(), 2);

  ASSERT_EQ(static_cast<umka::vm::OpCode>(code[0].code), umka::vm::OpCode::PUSH_CONST);
  int64_t result = 0;
  memcpy(&result, pool[code[0].arg].data.data(), 8);
  ASSERT_EQ(result, 14);

  ASSERT_EQ(static_cast<umka::vm::OpCode>(code[1].code), umka::vm::OpCode::STORE);
  ASSERT_EQ(code[1].arg, 0);
}

TEST(JitConstFolding, MultiplyTwoSums) {
  std::vector<umka::vm::Constant> pool = {
    make_int(1), // 0
    make_int(2), // 1
    make_int(3), // 2
    make_int(4)  // 3
};

  std::vector<umka::vm::Command> code = {
    cmd(umka::vm::OpCode::PUSH_CONST, 0),
    cmd(umka::vm::OpCode::PUSH_CONST, 1),
    cmd(umka::vm::OpCode::ADD),           // = 3

    cmd(umka::vm::OpCode::PUSH_CONST, 2),
    cmd(umka::vm::OpCode::PUSH_CONST, 3),
    cmd(umka::vm::OpCode::ADD),           // = 7

    cmd(umka::vm::OpCode::MUL),           // 3*7=21
    cmd(umka::vm::OpCode::STORE, 0)
};

  umka::vm::FunctionTableEntry meta{};

  umka::jit::ConstFolding folding;
  folding.run(code, pool, meta);

  ASSERT_EQ(code.size(), 2);

  ASSERT_EQ((umka::vm::OpCode)code[0].code, umka::vm::OpCode::PUSH_CONST);

  int64_t r = 0;
  memcpy(&r, pool[code[0].arg].data.data(), 8);
  ASSERT_EQ(r, 21);

  ASSERT_EQ((umka::vm::OpCode)code[1].code, umka::vm::OpCode::STORE);
}

TEST(JitConstFolding, CannotFold_LoadAndConst)
{
  std::vector<umka::vm::Constant> pool = { make_int(1) };

  std::vector<umka::vm::Command> code = {
    cmd(umka::vm::OpCode::PUSH_CONST, 0),
    cmd(umka::vm::OpCode::LOAD, 0),
    cmd(umka::vm::OpCode::ADD)
};

  umka::vm::FunctionTableEntry meta{};

  umka::jit::ConstFolding folding;
  folding.run(code, pool, meta);


  ASSERT_EQ(code.size(), 3);

  EXPECT_EQ(static_cast<umka::vm::OpCode>(code[0].code), umka::vm::OpCode::PUSH_CONST);
  EXPECT_EQ(static_cast<umka::vm::OpCode>(code[1].code), umka::vm::OpCode::LOAD);
  EXPECT_EQ(static_cast<umka::vm::OpCode>(code[2].code), umka::vm::OpCode::ADD);
}

TEST(JitConstFolding, CannotFoldAcrossStore)
{
  std::vector<umka::vm::Constant> pool = { make_int(2), make_int(3) };

  std::vector<umka::vm::Command> code = {
    cmd(umka::vm::OpCode::PUSH_CONST, 0),
    cmd(umka::vm::OpCode::PUSH_CONST, 1),
    cmd(umka::vm::OpCode::STORE, 0),   // ломаем стек констант!
    cmd(umka::vm::OpCode::ADD)
};

  umka::vm::FunctionTableEntry meta{};

  umka::jit::ConstFolding folding;
  folding.run(code, pool, meta);

  ASSERT_EQ(code.size(), 4);

  EXPECT_EQ(static_cast<umka::vm::OpCode>(code[0].code), umka::vm::OpCode::PUSH_CONST);
  EXPECT_EQ(static_cast<umka::vm::OpCode>(code[1].code), umka::vm::OpCode::PUSH_CONST);
  EXPECT_EQ(static_cast<umka::vm::OpCode>(code[2].code), umka::vm::OpCode::STORE);
  EXPECT_EQ(static_cast<umka::vm::OpCode>(code[3].code), umka::vm::OpCode::ADD);
}

TEST(JitConstFolding, CannotFoldAcrossCall)
{
  std::vector<umka::vm::Constant> pool = { make_int(2), make_int(3) };

  std::vector<umka::vm::Command> code = {
    cmd(umka::vm::OpCode::PUSH_CONST, 0),
    cmd(umka::vm::OpCode::PUSH_CONST, 1),
    cmd(umka::vm::OpCode::CALL, 10),  // f
    cmd(umka::vm::OpCode::ADD)
};

  umka::vm::FunctionTableEntry meta{};

  umka::jit::ConstFolding folding;
  folding.run(code, pool, meta);

  ASSERT_EQ(code.size(), 4);
  EXPECT_EQ(static_cast<umka::vm::OpCode>(code[0].code), umka::vm::OpCode::PUSH_CONST);
  EXPECT_EQ(static_cast<umka::vm::OpCode>(code[1].code), umka::vm::OpCode::PUSH_CONST);
  EXPECT_EQ(static_cast<umka::vm::OpCode>(code[2].code), umka::vm::OpCode::CALL);
  EXPECT_EQ(static_cast<umka::vm::OpCode>(code[3].code), umka::vm::OpCode::ADD);
}

TEST(JitConstFolding, CannotFoldMixedStackValues)
{
  std::vector<umka::vm::Constant> pool = { make_int(1), make_int(2) };

  std::vector<umka::vm::Command> code = {
    cmd(umka::vm::OpCode::PUSH_CONST, 0),
    cmd(umka::vm::OpCode::LOAD, 0),
    cmd(umka::vm::OpCode::PUSH_CONST, 1),
    cmd(umka::vm::OpCode::ADD)
};

  umka::vm::FunctionTableEntry meta{};
  umka::jit::ConstFolding folding;

  folding.run(code, pool, meta);

  ASSERT_EQ(code.size(), 4);
  EXPECT_EQ(static_cast<umka::vm::OpCode>(code[0].code), umka::vm::OpCode::PUSH_CONST);
  EXPECT_EQ(static_cast<umka::vm::OpCode>(code[1].code), umka::vm::OpCode::LOAD);
  EXPECT_EQ(static_cast<umka::vm::OpCode>(code[2].code), umka::vm::OpCode::PUSH_CONST);
  EXPECT_EQ(static_cast<umka::vm::OpCode>(code[3].code), umka::vm::OpCode::ADD);
}

TEST(JitDCE, RemoveUnusedArithmetic) {
  std::vector pool = { make_int(1), make_int(2) };

  std::vector code = {
    cmd(umka::vm::OpCode::PUSH_CONST, 0),
    cmd(umka::vm::OpCode::PUSH_CONST, 1),
    cmd(umka::vm::OpCode::ADD),        // <- result unused
    cmd(umka::vm::OpCode::PUSH_CONST, 0),
    cmd(umka::vm::OpCode::RETURN)
};

  umka::vm::FunctionTableEntry meta{};
  umka::jit::DeadCodeElimination dce;
  dce.run(code, pool, meta);

  ASSERT_EQ(code.size(), 2);
  EXPECT_EQ(static_cast<umka::vm::OpCode>(code[0].code), umka::vm::OpCode::PUSH_CONST);
  EXPECT_EQ(static_cast<umka::vm::OpCode>(code[1].code), umka::vm::OpCode::RETURN);
}

TEST(JitDCE, KeepProducerBeforeStore) {
  std::vector pool = { make_int(10) };

  std::vector code = {
    cmd(umka::vm::OpCode::PUSH_CONST, 0),
    cmd(umka::vm::OpCode::STORE, 0)
};

  umka::vm::FunctionTableEntry meta{};
  umka::jit::DeadCodeElimination dce;
  dce.run(code, pool, meta);

  ASSERT_EQ(code.size(), 2);
  EXPECT_EQ(static_cast<umka::vm::OpCode>(code[0].code), umka::vm::OpCode::PUSH_CONST);
  EXPECT_EQ(static_cast<umka::vm::OpCode>(code[1].code), umka::vm::OpCode::STORE);
}

TEST(JitDCE, KeepCallEvenIfResultUnused) {
  std::vector<umka::vm::Constant> pool = {};

  std::vector<umka::vm::Command> code = {
    cmd(umka::vm::OpCode::CALL, 2),  // needs 2 args
    cmd(umka::vm::OpCode::POP)       // return value unused
};

  umka::vm::FunctionTableEntry meta{};
  umka::jit::DeadCodeElimination dce;
  dce.run(code, pool, meta);

  ASSERT_EQ(code.size(), 1);
  EXPECT_EQ((umka::vm::OpCode)code[0].code, umka::vm::OpCode::CALL);
}

TEST(JitDCE, CallArgumentsAreNeeded) {
  std::vector pool = { make_int(1), make_int(2) };

  std::vector code = {
    cmd(umka::vm::OpCode::PUSH_CONST, 0),
    cmd(umka::vm::OpCode::PUSH_CONST, 1),
    cmd(umka::vm::OpCode::CALL, 2),
    cmd(umka::vm::OpCode::POP)   // result unused
};

  umka::vm::FunctionTableEntry meta{};
  umka::jit::DeadCodeElimination dce;
  dce.run(code, pool, meta);

  ASSERT_EQ(code.size(), 3);
  EXPECT_EQ(static_cast<umka::vm::OpCode>(code[0].code), umka::vm::OpCode::PUSH_CONST);
  EXPECT_EQ(static_cast<umka::vm::OpCode>(code[1].code), umka::vm::OpCode::PUSH_CONST);
  EXPECT_EQ(static_cast<umka::vm::OpCode>(code[2].code), umka::vm::OpCode::CALL);
}

TEST(JitDCE, RemoveUnreachableAfterJump) {
  std::vector<umka::vm::Constant> pool = {};

  std::vector<umka::vm::Command> code = {
    cmd(umka::vm::OpCode::JMP, 2),      // skip 2
    cmd(umka::vm::OpCode::ADD),         // unreachable
    cmd(umka::vm::OpCode::MUL),         // live
    cmd(umka::vm::OpCode::RETURN)       // live
};

  umka::vm::FunctionTableEntry meta{};
  umka::jit::DeadCodeElimination dce;
  dce.run(code, pool, meta);

  ASSERT_EQ(code.size(), 3);
  EXPECT_EQ(static_cast<umka::vm::OpCode>(code[0].code), umka::vm::OpCode::JMP);
  EXPECT_EQ(static_cast<umka::vm::OpCode>(code[1].code), umka::vm::OpCode::MUL);
  EXPECT_EQ(static_cast<umka::vm::OpCode>(code[2].code), umka::vm::OpCode::RETURN);

  EXPECT_EQ(code[0].arg, 1); // from JMP to RETURN
}

TEST(JitDCE, DeadAfterReturn) {
  std::vector pool = { make_int(10), make_int(20) };

  std::vector code = {
    cmd(umka::vm::OpCode::PUSH_CONST, 0),
    cmd(umka::vm::OpCode::RETURN),
    cmd(umka::vm::OpCode::PUSH_CONST, 1), // dead
    cmd(umka::vm::OpCode::ADD),           // dead
};

  umka::vm::FunctionTableEntry meta{};
  umka::jit::DeadCodeElimination dce;
  dce.run(code, pool, meta);

  ASSERT_EQ(code.size(), 2);
  EXPECT_EQ(static_cast<umka::vm::OpCode>(code[0].code), umka::vm::OpCode::PUSH_CONST);
  EXPECT_EQ(static_cast<umka::vm::OpCode>(code[1].code), umka::vm::OpCode::RETURN);
}


TEST(JitDCE, DeadPopAndTrailingCodeAfterCall) {
  std::vector pool = { make_int(1), make_int(2), make_int(3) };

  std::vector code = {
    cmd(umka::vm::OpCode::PUSH_CONST, 0),
    cmd(umka::vm::OpCode::PUSH_CONST, 1),
    cmd(umka::vm::OpCode::CALL, 2),
    cmd(umka::vm::OpCode::POP),               // dead
    cmd(umka::vm::OpCode::PUSH_CONST, 2),     // dead
};

  umka::vm::FunctionTableEntry meta{};
  umka::jit::DeadCodeElimination dce;
  dce.run(code, pool, meta);

  ASSERT_EQ(code.size(), 3);
  EXPECT_EQ(static_cast<umka::vm::OpCode>(code[0].code), umka::vm::OpCode::PUSH_CONST);
  EXPECT_EQ(static_cast<umka::vm::OpCode>(code[1].code), umka::vm::OpCode::PUSH_CONST);
  EXPECT_EQ(static_cast<umka::vm::OpCode>(code[2].code), umka::vm::OpCode::CALL);
}

TEST(JitDCE, DeadLoopConstantFalse) {
  std::vector pool = { make_int(0) };

  std::vector code = {
    cmd(umka::vm::OpCode::PUSH_CONST, 0),
    cmd(umka::vm::OpCode::JMP_IF_FALSE, 4),
    cmd(umka::vm::OpCode::LOAD, 0),   // dead
    cmd(umka::vm::OpCode::ADD),       // dead
    cmd(umka::vm::OpCode::JMP, -2),   // dead
    cmd(umka::vm::OpCode::RETURN),
};

  umka::vm::FunctionTableEntry meta{};
  umka::jit::DeadCodeElimination dce;
  dce.run(code, pool, meta);

  ASSERT_EQ(code.size(), 3);
  EXPECT_EQ(static_cast<umka::vm::OpCode>(code[0].code), umka::vm::OpCode::PUSH_CONST);
  EXPECT_EQ(static_cast<umka::vm::OpCode>(code[1].code), umka::vm::OpCode::JMP_IF_FALSE);
  EXPECT_EQ(static_cast<umka::vm::OpCode>(code[2].code), umka::vm::OpCode::RETURN);
}



