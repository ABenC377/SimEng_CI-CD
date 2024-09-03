#pragma once

#include <forward_list>
#include <unordered_map>

#include "simeng/OS/SyscallHandler.hh"
#include "simeng/arch/Architecture.hh"
#include "simeng/arch/riscv/Instruction.hh"

using csh = size_t;

namespace simeng {
namespace arch {
namespace riscv {

/* A basic RISC-V implementation of the `Architecture` interface. */
class Architecture : public arch::Architecture {
 public:
  Architecture();

  ~Architecture();

  /** Pre-decode instruction memory into a macro-op of `Instruction`
   * instances. Returns the number of bytes consumed to produce it (0 if
   * failure), and writes into the supplied macro-op vector. */
  uint8_t predecode(const void* ptr, uint8_t bytesAvailable,
                    uint64_t instructionAddress, MacroOp& output) override;

  /** Returns a zero-indexed register tag for a system register encoding. */
  int32_t getSystemRegisterTag(uint16_t reg) const override;

  /** Returns the number of system registers that have a mapping. */
  uint16_t getNumSystemRegisters() const override;

  /** Returns the maximum size of a valid instruction in bytes. */
  uint8_t getMaxInstructionSize() const override;

  /** Updates System registers of any system-based timers. */
  void updateSystemTimerRegisters(RegisterFileSet* regFile,
                                  const uint64_t iterations) const override;

  /** Returns the physical register structure as defined within the config file
   */
  std::vector<RegisterFileStructure> getConfigPhysicalRegisterStructure()
      const override;

  /** Returns the physical register quantities as defined within the config file
   */
  std::vector<uint16_t> getConfigPhysicalRegisterQuantities() const override;

  /** After a context switch, update any required variables. */
  void updateAfterContextSwitch(
      const simeng::OS::cpuContext& context) const override;

 private:
  /** Retrieve an ExecutionInfo object for the requested instruction. If a
   * opcode-based override has been defined for the latency and/or
   * port information, return that instead of the group-defined execution
   * information. */
  executionInfo getExecutionInfo(Instruction& insn) const;

  /** A decoding cache, mapping an instruction word to a previously decoded
   * instruction. Instructions are added to the cache as they're decoded, to
   * reduce the overhead of future decoding. */
  mutable std::unordered_map<uint32_t, Instruction> decodeCache_;

  /** A decoding metadata cache, mapping an instruction word to a previously
   * decoded instruction metadata bundle. Metadata is added to the cache as it's
   * decoded, to reduce the overhead of future decoding. */
  mutable std::forward_list<InstructionMetadata> metadataCache_;

  /** A mapping from system register encoding to a zero-indexed tag. */
  std::unordered_map<uint16_t, uint16_t> systemRegisterMap_;

  /** A map to hold the relationship between aarch64 instruction groups and
   * user-defined execution information. */
  std::unordered_map<uint16_t, executionInfo> groupExecutionInfo_;

  /** A map to hold the relationship between aarch64 instruction opcode and
   * user-defined execution information. */
  std::unordered_map<uint16_t, executionInfo> opcodeExecutionInfo_;

  /** A Capstone decoding library handle, for decoding instructions. */
  csh capstoneHandle;

  /** Counter for assigning sequence ids to all instructions. */
  mutable uint64_t instrSeqIdCtr_ = 0;

  /** The next available instruction ID. Used to identify in-order groups of
   * micro-operations. */
  mutable uint64_t insnIdCtr_ = 0;

  /** System Register of Processor Cycle Counter. */
  simeng::Register cycleSystemReg_;

  /** A mask used to determine if an address has the correct byte alignment */
  uint8_t addressAlignmentMask_;

  /** Minimum number of bytes that can represent an instruction */
  uint8_t minInsnLength_;

  mutable std::ofstream outputFile_;
};

}  // namespace riscv
}  // namespace arch
}  // namespace simeng
