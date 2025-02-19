#include "simeng/models/emulation/Core.hh"

#include <cstring>

namespace simeng {
namespace models {
namespace emulation {

/** The number of bytes fetched each cycle. */
const uint8_t FETCH_SIZE = 4;

Core::Core(memory::MemoryInterface& instructionMemory,
           memory::MemoryInterface& dataMemory, uint64_t entryPoint,
           uint64_t programByteLength, const arch::Architecture& isa)
    : simeng::Core(dataMemory, isa, config::SimInfo::getArchRegStruct()),
      instructionMemory_(instructionMemory),
      architecturalRegisterFileSet_(registerFileSet_),
      pc_(entryPoint),
      programByteLength_(programByteLength) {
  // Ensure both interface types are flat
  assert(
      (config::SimInfo::getConfig()["L1-Data-Memory"]["Interface-Type"]
           .as<std::string>() == "Flat") &&
      "Emulation core is only compatable with a Flat Data Memory Interface.");
  assert(
      (config::SimInfo::getConfig()["L1-Instruction-Memory"]["Interface-Type"]
           .as<std::string>() == "Flat") &&
      "Emulation core is only compatable with a Flat Instruction Memory "
      "Interface.");

  // Pre-load the first instruction
  instructionMemory_.requestRead({pc_, FETCH_SIZE});

  // Query and apply initial state
  auto state = isa.getInitialState();
  applyStateChange(state);
}

void Core::tick() {
  if (hasHalted_) return;

  if (pc_ >= programByteLength_) {
    hasHalted_ = true;
    return;
  }

  ticks_++;
  isa_.updateSystemTimerRegisters(&registerFileSet_, ticks_);

  // Fetch & Decode
  assert(macroOp_.empty() &&
         "Cannot begin emulation tick with un-executed micro-ops.");
  // We only fetch one instruction at a time, so only ever one result in
  // complete reads
  const auto& instructionBytes = instructionMemory_.getCompletedReads()[0].data;
  // Predecode fetched data
  auto bytesRead = isa_.predecode(instructionBytes.getAsVector<uint8_t>(),
                                  FETCH_SIZE, pc_, macroOp_);
  // Clear the fetched data
  instructionMemory_.clearCompletedReads();

  pc_ += bytesRead;

  // Loop over all micro-ops and execute one by one
  while (!macroOp_.empty()) {
    auto& uop = macroOp_.front();

    if (uop->exceptionEncountered()) {
      handleException(uop);
      // If fatal, return
      if (hasHalted_) return;
    }

    // Issue
    auto registers = uop->getSourceRegisters();
    for (size_t i = 0; i < registers.size(); i++) {
      auto reg = registers[i];
      if (!uop->isOperandReady(i)) {
        uop->supplyOperand(i, registerFileSet_.get(reg));
      }
    }

    // Execute & Write-back
    if (uop->isLoad()) {
      auto addresses = uop->generateAddresses();
      previousAddresses_.clear();
      if (uop->exceptionEncountered()) {
        handleException(uop);
        // If fatal, return
        if (hasHalted_) return;
      }
      if (addresses.size() > 0) {
        // Memory reads required; request them
        for (auto const& target : addresses) {
          dataMemory_.requestRead(target);
          // Save addresses for use by instructions that perform a LD and STR
          // (i.e. single instruction atomics)
          previousAddresses_.push_back(target);
        }
        // Emulation core can only be used with a Flat memory interface, so data
        // is ready immediately
        const auto& completedReads = dataMemory_.getCompletedReads();
        assert(
            completedReads.size() == addresses.size() &&
            "Number of completed reads does not match the number of requested "
            "reads.");
        for (const auto& response : completedReads) {
          uop->supplyData(response.target.address, response.data);
        }
        dataMemory_.clearCompletedReads();
      }
    } else if (uop->isStoreAddress()) {
      auto addresses = uop->generateAddresses();
      previousAddresses_.clear();
      if (uop->exceptionEncountered()) {
        handleException(uop);
        // If fatal, return
        if (hasHalted_) return;
      }
      // Store addresses for use by next store data operation in `execute()`
      for (auto const& target : addresses) {
        previousAddresses_.push_back(target);
      }
      if (!uop->isStoreData()) {
        // No further action needed, move onto next micro-op
        macroOp_.erase(macroOp_.begin());
        continue;
      }
    }
    execute(uop);
    macroOp_.erase(macroOp_.begin());
  }
  instructionsExecuted_++;
  // Fetch memory for next cycle
  instructionMemory_.requestRead({pc_, FETCH_SIZE});
}

bool Core::hasHalted() const { return hasHalted_; }

const ArchitecturalRegisterFileSet& Core::getArchitecturalRegisterFileSet()
    const {
  return architecturalRegisterFileSet_;
}

uint64_t Core::getInstructionsRetiredCount() const {
  return instructionsExecuted_;
}

std::map<std::string, std::string> Core::getStats() const {
  return {{"cycles", std::to_string(ticks_)},
          {"retired", std::to_string(instructionsExecuted_)},
          {"branch.executed", std::to_string(branchesExecuted_)}};
}

void Core::execute(std::shared_ptr<Instruction>& uop) {
  uop->execute();

  if (uop->exceptionEncountered()) {
    handleException(uop);
    return;
  }

  if (uop->isStoreData()) {
    auto data = uop->getData();
    for (size_t i = 0; i < previousAddresses_.size(); i++) {
      dataMemory_.requestWrite(previousAddresses_[i], data[i]);
    }
  } else if (uop->isBranch()) {
    pc_ = uop->getBranchAddress();
    branchesExecuted_++;
  }

  // Writeback
  const auto& results = uop->getResults();
  const auto& destinations = uop->getDestinationRegisters();
  for (size_t i = 0; i < results.size(); i++) {
    auto reg = destinations[i];
    registerFileSet_.set(reg, results[i]);
  }
}

void Core::handleException(const std::shared_ptr<Instruction>& instruction) {
  exceptionHandler_ = isa_.handleException(instruction, *this, dataMemory_);
  processExceptionHandler();
}

void Core::processExceptionHandler() {
  assert(exceptionHandler_ != nullptr &&
         "Attempted to process an exception handler that wasn't present");

  // Tick until true is returned, signifying completion
  while (exceptionHandler_->tick() == false) {
  }

  const auto& result = exceptionHandler_->getResult();

  if (result.fatal) {
    pc_ = programByteLength_;
    hasHalted_ = true;
    std::cout << "[SimEng:Core] Halting due to fatal exception" << std::endl;
  } else {
    pc_ = result.instructionAddress;
    applyStateChange(result.stateChange);
  }

  // Clear the handler
  exceptionHandler_ = nullptr;
}

}  // namespace emulation
}  // namespace models
}  // namespace simeng
