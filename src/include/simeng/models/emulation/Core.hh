#pragma once

#include <map>
#include <queue>
#include <string>

#include "simeng/ArchitecturalRegisterFileSet.hh"
#include "simeng/Core.hh"
#include "simeng/arch/Architecture.hh"
#include "simeng/span.hh"

namespace simeng {
namespace models {
namespace emulation {

/** An emulation-style core model. Executes each instruction in turn. */
class Core : public simeng::Core {
 public:
  /** Construct an emulation-style core, providing memory interfaces for
   * instructions and data, along with the instruction entry point and an ISA to
   * use. */
  Core(memory::MemoryInterface& instructionMemory,
       memory::MemoryInterface& dataMemory, uint64_t entryPoint,
       uint64_t programByteLength, const arch::Architecture& isa);

  /** Tick the core. */
  void tick() override;

  /** Check whether the program has halted. */
  bool hasHalted() const override;

  /** Retrieve the architectural register file set. */
  const ArchitecturalRegisterFileSet& getArchitecturalRegisterFileSet()
      const override;

  /** Retrieve the number of instructions retired. */
  uint64_t getInstructionsRetiredCount() const override;

  /** Retrieve a map of statistics to report. */
  std::map<std::string, std::string> getStats() const override;

 private:
  /** Execute an instruction. */
  void execute(std::shared_ptr<Instruction>& uop);

  /** Handle an encountered exception. */
  void handleException(const std::shared_ptr<Instruction>& instruction);

  /** Process an active exception handler. */
  void processExceptionHandler();

  /** A memory interface to access instructions. */
  memory::MemoryInterface& instructionMemory_;

  /** An architectural register file set, serving as a simple wrapper around the
   * register file set. */
  ArchitecturalRegisterFileSet architecturalRegisterFileSet_;

  /** A reusable macro-op vector to fill with uops. */
  MacroOp macroOp_;

  /** The previously generated addresses. */
  std::vector<simeng::memory::MemoryAccessTarget> previousAddresses_;

  /** The current program counter. */
  uint64_t pc_ = 0;

  /** The length of the available instruction memory. */
  uint64_t programByteLength_ = 0;

  /** The number of instructions executed. */
  uint64_t instructionsExecuted_ = 0;

  /** The number of branches executed. */
  uint64_t branchesExecuted_ = 0;
};

}  // namespace emulation
}  // namespace models
}  // namespace simeng
