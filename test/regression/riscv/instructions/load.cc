#include "RISCVRegressionTest.hh"

namespace {

using InstLoad = RISCVRegressionTest;
using namespace simeng::arch::riscv::InstructionGroups;

TEST_P(InstLoad, lb) {
  initialHeapData_.resize(16);
  uint32_t* heap = reinterpret_cast<uint32_t*>(initialHeapData_.data());
  heap[0] = 0xDEADBEEF;
  heap[1] = 0x12345678;
  heap[2] = 0xFEEBDAED;
  heap[3] = 0x87654321;

  RUN_RISCV(R"(
      li a7, 214
      ecall

      add t5, t5, a0
      lb t6, 0(t5)
      lb t4, 4(t5)
      lb t3, 7(t5)
      addi t5, t5, 4
      lb t2, -2(t5)
  )");
  EXPECT_EQ(getGeneralRegister<uint64_t>(31), 0xFFFFFFFFFFFFFFEF);
  EXPECT_EQ(getGeneralRegister<uint64_t>(29), 0x0000000000000078);
  EXPECT_EQ(getGeneralRegister<uint64_t>(28), 0x0000000000000012);
  EXPECT_EQ(getGeneralRegister<uint64_t>(7), 0xFFFFFFFFFFFFFFAD);

  EXPECT_GROUP(R"(lb t2, -2(t5))", LOAD_INT);

  // Load byte unsigned
  RUN_RISCV(R"(
      li a7, 214
      ecall

      add t5, t5, a0
      lbu t6, 0(t5)
      lbu t4, 4(t5)
      lbu t3, 7(t5)
      addi t5, t5, 4
      lbu t2, -2(t5)
  )");
  EXPECT_EQ(getGeneralRegister<uint64_t>(31), 0x00000000000000EF);
  EXPECT_EQ(getGeneralRegister<uint64_t>(29), 0x0000000000000078);
  EXPECT_EQ(getGeneralRegister<uint64_t>(28), 0x0000000000000012);
  EXPECT_EQ(getGeneralRegister<uint64_t>(7), 0x00000000000000AD);

  EXPECT_GROUP(R"(lbu t2, -2(t5))", LOAD_INT);
}

TEST_P(InstLoad, lh) {
  initialHeapData_.resize(16);
  uint32_t* heap = reinterpret_cast<uint32_t*>(initialHeapData_.data());
  heap[0] = 0xDEADBEEF;
  heap[1] = 0x12345678;
  heap[2] = 0xFEEBDAED;
  heap[3] = 0x87654321;

  RUN_RISCV(R"(
      li a7, 214
      ecall

      add t5, t5, a0
      lh t6, 0(t5)
      lh t4, 4(t5)
      lh t3, 7(t5)
      addi t5, t5, 4
      lh t2, -2(t5)
  )");
  EXPECT_EQ(getGeneralRegister<uint64_t>(31), 0xFFFFFFFFFFFFBEEF);
  EXPECT_EQ(getGeneralRegister<uint64_t>(29), 0x0000000000005678);
  EXPECT_EQ(getGeneralRegister<uint64_t>(28), 0xFFFFFFFFFFFFED12);
  EXPECT_EQ(getGeneralRegister<uint64_t>(7), 0xFFFFFFFFFFFFDEAD);

  EXPECT_GROUP(R"(lh t2, -2(t5))", LOAD_INT);

  // Load half word unsigned
  RUN_RISCV(R"(
      li a7, 214
      ecall

      add t5, t5, a0
      lhu t6, 0(t5)
      lhu t4, 4(t5)
      lhu t3, 7(t5)
      addi t5, t5, 4
      lhu t2, -2(t5)
  )");
  EXPECT_EQ(getGeneralRegister<uint64_t>(31), 0x000000000000BEEF);
  EXPECT_EQ(getGeneralRegister<uint64_t>(29), 0x0000000000005678);
  EXPECT_EQ(getGeneralRegister<uint64_t>(28), 0x000000000000ED12);
  EXPECT_EQ(getGeneralRegister<uint64_t>(7), 0x000000000000DEAD);

  EXPECT_GROUP(R"(lhu t2, -2(t5))", LOAD_INT);
}

TEST_P(InstLoad, lw) {
  initialHeapData_.resize(16);
  uint32_t* heap = reinterpret_cast<uint32_t*>(initialHeapData_.data());
  heap[0] = 0xDEADBEEF;
  heap[1] = 0x12345678;
  heap[2] = 0xFEEBDAED;
  heap[3] = 0x87654321;

  RUN_RISCV(R"(
      li a7, 214
      ecall

      add t5, t5, a0
      lw t6, 0(t5)
      lw t4, 4(t5)
      lw t3, 7(t5)
      addi t5, t5, 4
      lw t2, -2(t5)
  )");
  EXPECT_EQ(getGeneralRegister<uint64_t>(31), 0xFFFFFFFFDEADBEEF);
  EXPECT_EQ(getGeneralRegister<uint64_t>(29), 0x0000000012345678);
  EXPECT_EQ(getGeneralRegister<uint64_t>(28), 0xFFFFFFFFEBDAED12);
  EXPECT_EQ(getGeneralRegister<uint64_t>(7), 0x000000005678DEAD);

  EXPECT_GROUP(R"(lw t2, -2(t5))", LOAD_INT);

  RUN_RISCV(R"(
      li a7, 214
      ecall

      add t5, t5, a0
      lwu t6, 0(t5)
      lwu t4, 4(t5)
      lwu t3, 7(t5)
      addi t5, t5, 4
      lwu t2, -2(t5)
  )");
  EXPECT_EQ(getGeneralRegister<uint64_t>(31), 0x00000000DEADBEEF);
  EXPECT_EQ(getGeneralRegister<uint64_t>(29), 0x0000000012345678);
  EXPECT_EQ(getGeneralRegister<uint64_t>(28), 0x00000000EBDAED12);
  EXPECT_EQ(getGeneralRegister<uint64_t>(7), 0x000000005678DEAD);

  EXPECT_GROUP(R"(lwu t2, -2(t5))", LOAD_INT);
}

TEST_P(InstLoad, ld) {
  initialHeapData_.resize(16);
  uint32_t* heap = reinterpret_cast<uint32_t*>(initialHeapData_.data());
  heap[0] = 0xDEADBEEF;
  heap[1] = 0x12345678;
  heap[2] = 0xFEEBDAED;
  heap[3] = 0x87654321;

  RUN_RISCV(R"(
      li a7, 214
      ecall

      add t5, t5, a0
      ld t6, 0(t5)
      ld t4, 4(t5)
      ld t3, 7(t5)
      addi t5, t5, 4
      ld t2, -2(t5)
  )");
  EXPECT_EQ(getGeneralRegister<uint64_t>(31), 0x12345678DEADBEEF);
  EXPECT_EQ(getGeneralRegister<uint64_t>(29), 0xFEEBDAED12345678);
  EXPECT_EQ(getGeneralRegister<uint64_t>(28), 0x654321FEEBDAED12);
  EXPECT_EQ(getGeneralRegister<uint64_t>(7), 0xDAED12345678DEAD);

  EXPECT_GROUP(R"(ld t2, -2(t5))", LOAD_INT);
}

INSTANTIATE_TEST_SUITE_P(RISCV, InstLoad,
                         ::testing::Values(std::make_tuple(EMULATION, "{}")),
                         paramToString);

}  // namespace
