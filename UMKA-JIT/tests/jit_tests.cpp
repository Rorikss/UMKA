#include <model/model.h>
#include <parser/command_parser.h>

#include "constant_propagation.h"
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
  return umka::vm::Command{static_cast<uint8_t>(op), arg};
}


TEST(JitConstantPropagation, PropagatesKnownLocalsThroughLoad) {
  using namespace umka;
  using vm::OpCode;


  std::vector pool = {
    make_int(2), // const 0
    make_int(3) // const 1
  };

  std::vector code = {
    cmd(OpCode::PUSH_CONST, 0),
    cmd(OpCode::STORE, 0),

    cmd(OpCode::PUSH_CONST, 1),
    cmd(OpCode::STORE, 1),

    cmd(OpCode::LOAD, 0),
    cmd(OpCode::LOAD, 1),
    cmd(OpCode::ADD),
    cmd(OpCode::STORE, 2),

    cmd(OpCode::LOAD, 2),
    cmd(OpCode::CALL, std::numeric_limits<int64_t>::max()),

    cmd(OpCode::RETURN)
  };

  vm::FunctionTableEntry meta{};
  std::unordered_map<size_t, vm::FunctionTableEntry> funcs;

  jit::ConstantPropagation cp;
  jit::ConstFolding cf;
  cp.run(code, pool, funcs, meta);
  cf.run(code, pool, funcs, meta);
  cp.run(code, pool, funcs, meta);

  auto find_first = [&](vm::OpCode op) -> int {
    for (size_t i = 0; i < code.size(); ++i) {
      if (static_cast<vm::OpCode>(code[i].code) == op)
        return static_cast<int>(i);
    }
    return -1;
  };

  EXPECT_EQ(find_first(OpCode::ADD), -1);

  EXPECT_NE(find_first(OpCode::STORE), -1);

  int load2 = find_first(OpCode::PUSH_CONST);
  EXPECT_NE(load2, -1);

  EXPECT_EQ(static_cast<OpCode>(code.back().code), OpCode::RETURN);
  EXPECT_EQ(static_cast<OpCode>(code[code.size() - 2].code), OpCode::CALL);
}

TEST(JitConstantPropagation, ReplacesLoadWithKnownConstant) {
  using umka::vm::OpCode;

  std::vector pool = {
    make_int(42)
  };

  std::vector code = {
    cmd(OpCode::PUSH_CONST, 0),
    cmd(OpCode::STORE, 0),
    cmd(OpCode::LOAD, 0),
    cmd(OpCode::RETURN)
  };

  umka::jit::ConstantPropagation cp;
  umka::jit::ConstFolding cf;
  umka::vm::FunctionTableEntry meta{};
  std::unordered_map<size_t, umka::vm::FunctionTableEntry> funcs;

  cp.run(code, pool, funcs, meta);

  EXPECT_EQ(static_cast<OpCode>(code[2].code), OpCode::PUSH_CONST);
  EXPECT_EQ(code[2].arg, 0);
}

TEST(JitConstantPropagation, StoreUnknownInvalidatesConstant) {
  using umka::vm::OpCode;

  std::vector pool = {
    make_int(1)
  };

  std::vector code = {
    cmd(OpCode::PUSH_CONST, 0),
    cmd(OpCode::STORE, 0),

    cmd(OpCode::LOAD, 1), // unknown
    cmd(OpCode::STORE, 0),

    cmd(OpCode::LOAD, 0), // must NOT be replaced
    cmd(OpCode::RETURN)
  };

  umka::jit::ConstantPropagation cp;
  umka::vm::FunctionTableEntry meta{};
  std::unordered_map<size_t, umka::vm::FunctionTableEntry> funcs;

  cp.run(code, pool, funcs, meta);

  EXPECT_EQ(static_cast<OpCode>(code[4].code), OpCode::LOAD);
}


TEST(JitConstFolding, ArithmeticNested) {
  // const pool: 2,3,4
  std::vector pool = {
    make_int(2),
    make_int(3),
    make_int(4)
  };

  std::vector code = {
    cmd(umka::vm::OpCode::PUSH_CONST, 0), // 2
    cmd(umka::vm::OpCode::PUSH_CONST, 1), // 3
    cmd(umka::vm::OpCode::PUSH_CONST, 2), // 4
    cmd(umka::vm::OpCode::MUL), // 3*4
    cmd(umka::vm::OpCode::ADD), // 2+(3*4)
    cmd(umka::vm::OpCode::STORE, 0)
  };

  umka::vm::FunctionTableEntry meta{};
  meta.arg_count = 0;
  meta.local_count = 1;

  umka::jit::ConstFolding folding;
  std::unordered_map<size_t, umka::vm::FunctionTableEntry> funcs;
  folding.run(code, pool, funcs, meta);

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
  std::vector pool = {
    make_int(1), // 0
    make_int(2), // 1
    make_int(3), // 2
    make_int(4) // 3
  };

  std::vector code = {
    cmd(umka::vm::OpCode::PUSH_CONST, 0),
    cmd(umka::vm::OpCode::PUSH_CONST, 1),
    cmd(umka::vm::OpCode::ADD), // = 3

    cmd(umka::vm::OpCode::PUSH_CONST, 2),
    cmd(umka::vm::OpCode::PUSH_CONST, 3),
    cmd(umka::vm::OpCode::ADD), // = 7

    cmd(umka::vm::OpCode::MUL), // 3*7=21
    cmd(umka::vm::OpCode::STORE, 0)
  };

  umka::vm::FunctionTableEntry meta{};

  umka::jit::ConstFolding folding;
  std::unordered_map<size_t, umka::vm::FunctionTableEntry> funcs;
  folding.run(code, pool, funcs, meta);

  ASSERT_EQ(code.size(), 2);

  ASSERT_EQ(static_cast<umka::vm::OpCode>(code[0].code), umka::vm::OpCode::PUSH_CONST);

  int64_t r = 0;
  memcpy(&r, pool[code[0].arg].data.data(), 8);
  ASSERT_EQ(r, 21);

  ASSERT_EQ(static_cast<umka::vm::OpCode>(code[1].code), umka::vm::OpCode::STORE);
}

TEST(JitConstFolding, CannotFold_LoadAndConst) {
  std::vector pool = {make_int(1)};

  std::vector code = {
    cmd(umka::vm::OpCode::PUSH_CONST, 0),
    cmd(umka::vm::OpCode::LOAD, 0),
    cmd(umka::vm::OpCode::ADD)
  };

  umka::vm::FunctionTableEntry meta{};

  umka::jit::ConstFolding folding;
  std::unordered_map<size_t, umka::vm::FunctionTableEntry> funcs;
  folding.run(code, pool, funcs, meta);


  ASSERT_EQ(code.size(), 3);

  EXPECT_EQ(static_cast<umka::vm::OpCode>(code[0].code), umka::vm::OpCode::PUSH_CONST);
  EXPECT_EQ(static_cast<umka::vm::OpCode>(code[1].code), umka::vm::OpCode::LOAD);
  EXPECT_EQ(static_cast<umka::vm::OpCode>(code[2].code), umka::vm::OpCode::ADD);
}

TEST(JitConstFolding, CannotFoldAcrossStore) {
  std::vector pool = {make_int(2), make_int(3)};

  std::vector code = {
    cmd(umka::vm::OpCode::PUSH_CONST, 0),
    cmd(umka::vm::OpCode::PUSH_CONST, 1),
    cmd(umka::vm::OpCode::STORE, 0),
    cmd(umka::vm::OpCode::ADD)
  };

  umka::vm::FunctionTableEntry meta{};

  umka::jit::ConstFolding folding;
  std::unordered_map<size_t, umka::vm::FunctionTableEntry> funcs;
  folding.run(code, pool, funcs, meta);

  ASSERT_EQ(code.size(), 4);

  EXPECT_EQ(static_cast<umka::vm::OpCode>(code[0].code), umka::vm::OpCode::PUSH_CONST);
  EXPECT_EQ(static_cast<umka::vm::OpCode>(code[1].code), umka::vm::OpCode::PUSH_CONST);
  EXPECT_EQ(static_cast<umka::vm::OpCode>(code[2].code), umka::vm::OpCode::STORE);
  EXPECT_EQ(static_cast<umka::vm::OpCode>(code[3].code), umka::vm::OpCode::ADD);
}

TEST(JitConstFolding, CannotFoldAcrossCall) {
  std::vector pool = {make_int(2), make_int(3)};

  std::vector code = {
    cmd(umka::vm::OpCode::PUSH_CONST, 0),
    cmd(umka::vm::OpCode::PUSH_CONST, 1),
    cmd(umka::vm::OpCode::CALL, 10), // f
    cmd(umka::vm::OpCode::ADD)
  };

  umka::vm::FunctionTableEntry meta{};

  umka::jit::ConstFolding folding;
  std::unordered_map<size_t, umka::vm::FunctionTableEntry> funcs;
  folding.run(code, pool, funcs, meta);

  ASSERT_EQ(code.size(), 4);
  EXPECT_EQ(static_cast<umka::vm::OpCode>(code[0].code), umka::vm::OpCode::PUSH_CONST);
  EXPECT_EQ(static_cast<umka::vm::OpCode>(code[1].code), umka::vm::OpCode::PUSH_CONST);
  EXPECT_EQ(static_cast<umka::vm::OpCode>(code[2].code), umka::vm::OpCode::CALL);
  EXPECT_EQ(static_cast<umka::vm::OpCode>(code[3].code), umka::vm::OpCode::ADD);
}

TEST(JitConstFolding, CannotFoldMixedStackValues) {
  std::vector pool = {make_int(1), make_int(2)};

  std::vector code = {
    cmd(umka::vm::OpCode::PUSH_CONST, 0),
    cmd(umka::vm::OpCode::LOAD, 0),
    cmd(umka::vm::OpCode::PUSH_CONST, 1),
    cmd(umka::vm::OpCode::ADD)
  };

  umka::vm::FunctionTableEntry meta{};
  umka::jit::ConstFolding folding;

  std::unordered_map<size_t, umka::vm::FunctionTableEntry> funcs;
  folding.run(code, pool, funcs, meta);

  ASSERT_EQ(code.size(), 4);
  EXPECT_EQ(static_cast<umka::vm::OpCode>(code[0].code), umka::vm::OpCode::PUSH_CONST);
  EXPECT_EQ(static_cast<umka::vm::OpCode>(code[1].code), umka::vm::OpCode::LOAD);
  EXPECT_EQ(static_cast<umka::vm::OpCode>(code[2].code), umka::vm::OpCode::PUSH_CONST);
  EXPECT_EQ(static_cast<umka::vm::OpCode>(code[3].code), umka::vm::OpCode::ADD);
}

TEST(JitDCE, RemoveUnusedArithmetic) {
  std::vector pool = {make_int(1), make_int(2)};

  std::vector code = {
    cmd(umka::vm::OpCode::PUSH_CONST, 0),
    cmd(umka::vm::OpCode::PUSH_CONST, 1),
    cmd(umka::vm::OpCode::ADD),
    cmd(umka::vm::OpCode::PUSH_CONST, 0),
    cmd(umka::vm::OpCode::RETURN)
  };

  umka::vm::FunctionTableEntry meta{};
  umka::jit::DeadCodeElimination dce;
  std::unordered_map<size_t, umka::vm::FunctionTableEntry> funcs;
  dce.run(code, pool, funcs, meta);

  ASSERT_EQ(code.size(), 2);
  EXPECT_EQ(static_cast<umka::vm::OpCode>(code[0].code), umka::vm::OpCode::PUSH_CONST);
  EXPECT_EQ(static_cast<umka::vm::OpCode>(code[1].code), umka::vm::OpCode::RETURN);
}

TEST(JitDCE, KeepProducerBeforeStore) {
  std::vector pool = {make_int(10)};

  std::vector code = {
    cmd(umka::vm::OpCode::PUSH_CONST, 0),
    cmd(umka::vm::OpCode::STORE, 0)
  };

  umka::vm::FunctionTableEntry meta{};
  umka::jit::DeadCodeElimination dce;
  std::unordered_map<size_t, umka::vm::FunctionTableEntry> funcs;
  dce.run(code, pool, funcs, meta);

  ASSERT_EQ(code.size(), 2);
  EXPECT_EQ(static_cast<umka::vm::OpCode>(code[0].code), umka::vm::OpCode::PUSH_CONST);
  EXPECT_EQ(static_cast<umka::vm::OpCode>(code[1].code), umka::vm::OpCode::STORE);
}

TEST(JitDCE, KeepCallEvenIfResultUnused) {
  std::vector pool = {make_int(3)};

  std::vector code = {
    cmd(umka::vm::OpCode::PUSH_CONST, 0), // arg 1
    cmd(umka::vm::OpCode::PUSH_CONST, 0), // arg 2
    cmd(umka::vm::OpCode::CALL, 2),
    cmd(umka::vm::OpCode::POP) // return value unused
  };

  umka::vm::FunctionTableEntry meta{};
  umka::jit::DeadCodeElimination dce;
  std::unordered_map<size_t, umka::vm::FunctionTableEntry> funcs;

  umka::vm::FunctionTableEntry func2;
  func2.id = 2;
  func2.arg_count = 2;
  funcs[2] = func2;

  dce.run(code, pool, funcs, meta);

  ASSERT_EQ(code.size(), 4); // PUSH_CONST, PUSH_CONST, CALL (POP removed)
  EXPECT_EQ(static_cast<umka::vm::OpCode>(code[0].code), umka::vm::OpCode::PUSH_CONST);
  EXPECT_EQ(static_cast<umka::vm::OpCode>(code[1].code), umka::vm::OpCode::PUSH_CONST);
  EXPECT_EQ(static_cast<umka::vm::OpCode>(code[2].code), umka::vm::OpCode::CALL);
  EXPECT_EQ(static_cast<umka::vm::OpCode>(code[3].code), umka::vm::OpCode::POP);
}

TEST(JitDCE, RemoveUnreachableAfterJump) {
  std::vector<umka::vm::Constant> pool = {};

  std::vector code = {
    cmd(umka::vm::OpCode::JMP, 2), // skip 2
    cmd(umka::vm::OpCode::ADD), // unreachable
    cmd(umka::vm::OpCode::MUL), // unreachable
    cmd(umka::vm::OpCode::RETURN) // live
  };

  umka::vm::FunctionTableEntry meta{};
  umka::jit::DeadCodeElimination dce;
  std::unordered_map<size_t, umka::vm::FunctionTableEntry> funcs;
  dce.run(code, pool, funcs, meta);

  ASSERT_EQ(code.size(), 2);
  EXPECT_EQ(static_cast<umka::vm::OpCode>(code[0].code), umka::vm::OpCode::JMP);
  EXPECT_EQ(static_cast<umka::vm::OpCode>(code[1].code), umka::vm::OpCode::RETURN);

  EXPECT_EQ(code[0].arg, 0); // from JMP to RETURN
}

TEST(JitDCE, DeadAfterReturn) {
  std::vector pool = {make_int(10), make_int(20)};

  std::vector code = {
    cmd(umka::vm::OpCode::PUSH_CONST, 0),
    cmd(umka::vm::OpCode::RETURN),
    cmd(umka::vm::OpCode::PUSH_CONST, 1), // dead
    cmd(umka::vm::OpCode::ADD), // dead
  };

  umka::vm::FunctionTableEntry meta{};
  umka::jit::DeadCodeElimination dce;
  std::unordered_map<size_t, umka::vm::FunctionTableEntry> funcs;
  dce.run(code, pool, funcs, meta);

  ASSERT_EQ(code.size(), 2);
  EXPECT_EQ(static_cast<umka::vm::OpCode>(code[0].code), umka::vm::OpCode::PUSH_CONST);
  EXPECT_EQ(static_cast<umka::vm::OpCode>(code[1].code), umka::vm::OpCode::RETURN);
}
