#include "simeng/arch/riscv/Instruction.hh"

#include <algorithm>
#include <cassert>
#include <vector>

#include "InstructionMetadata.hh"

namespace simeng {
namespace arch {
namespace riscv {

const Register Instruction::ZERO_REGISTER = {RegisterType::GENERAL, 0};

Instruction::Instruction(const Architecture& architecture,
                         const InstructionMetadata& metadata)
    : architecture_(architecture), metadata(metadata) {
  decode();
}

Instruction::Instruction(const Architecture& architecture,
                         const InstructionMetadata& metadata, uint8_t latency,
                         uint8_t stallCycles)
    : architecture_(architecture), metadata(metadata) {
  latency_ = latency;
  stallCycles_ = stallCycles;
  decode();
}

Instruction::Instruction(const Architecture& architecture,
                         const InstructionMetadata& metadata,
                         InstructionException exception)
    : architecture_(architecture), metadata(metadata) {
  exception_ = exception;
  exceptionEncountered_ = true;
}

InstructionException Instruction::getException() const { return exception_; }

const span<Register> Instruction::getOperandRegisters() const {
  return {const_cast<Register*>(sourceRegisters.data()), sourceRegisterCount};
}

const span<Register> Instruction::getDestinationRegisters() const {
  return {const_cast<Register*>(destinationRegisters.data()),
          destinationRegisterCount};
}

bool Instruction::isOperandReady(int index) const {
  return static_cast<bool>(operands[index]);
}

void Instruction::renameSource(uint8_t i, Register renamed) {
  sourceRegisters[i] = renamed;
}

void Instruction::renameDestination(uint8_t i, Register renamed) {
  destinationRegisters[i] = renamed;
}

void Instruction::supplyOperand(uint8_t i, const RegisterValue& value) {
  assert(!canExecute() &&
         "Attempted to provide an operand to a ready-to-execute instruction");
  assert(value.size() > 0 &&
         "Attempted to provide an uninitialised RegisterValue");

  operands[i] = value;
  operandsPending--;
}

bool Instruction::canExecute() const { return (operandsPending == 0); }

const span<RegisterValue> Instruction::getResults() const {
  return {const_cast<RegisterValue*>(results.data()), destinationRegisterCount};
}

void Instruction::setMemoryAddresses(
    const std::vector<memory::MemoryAccessTarget>& addresses) {
  memoryData_ = std::vector<RegisterValue>(addresses.size());
  memoryAddresses_ = addresses;
  dataPending_ = addresses.size();
}

void Instruction::supplyData(uint64_t address, const RegisterValue& data,
                             bool forwarded) {
  for (size_t i = 0; i < memoryAddresses_.size(); i++) {
    if (memoryAddresses_[i].vaddr == address && !memoryData_[i]) {
      if (!data) {
        // Raise exception for failed read
        // TODO: Move this logic to caller and distinguish between different
        // memory faults (e.g. bus error, page fault, seg fault)
        exception_ = InstructionException::DataAbort;
        exceptionEncountered_ = true;
        memoryData_[i] = RegisterValue(0, memoryAddresses_[i].size);
      } else {
        memoryData_[i] = data;
      }
      dataPending_--;
      return;
    }
  }
}

std::tuple<bool, uint64_t> Instruction::checkEarlyBranchMisprediction() const {
  assert(
      !executed_ &&
      "Early branch misprediction check shouldn't be called after execution");

  if (!isBranch()) {
    // Instruction isn't a branch; if predicted as taken, it will require a
    // flush
    return {prediction_.isTaken, instructionAddress_ + 4};
  }

  // Not enough information to determine this was a misprediction
  return {false, 0};
}

bool Instruction::isStoreAddress() const {
  return insnTypeMetadata & isStoreMask;
}

bool Instruction::isStoreData() const { return insnTypeMetadata & isStoreMask; }

bool Instruction::isLoad() const { return insnTypeMetadata & isLoadMask; }

bool Instruction::isBranch() const { return insnTypeMetadata & isBranchMask; }

bool Instruction::isAtomic() const { return insnTypeMetadata & isAtomicMask; }

bool Instruction::isAcquire() const { return insnTypeMetadata & isAcquireMask; }

bool Instruction::isRelease() const { return insnTypeMetadata & isReleaseMask; }

bool Instruction::isLoadReserved() const {
  return insnTypeMetadata & isLoadReservedMask;
}

bool Instruction::isStoreCond() const {
  return insnTypeMetadata & isStoreCondMask;
}

bool Instruction::isPrefetch() const { return 0; }

uint64_t Instruction::getOpcode() const { return metadata.opcode; }

uint16_t Instruction::getGroup() const {
  uint16_t base = InstructionGroups::INT;

  if (isBranch()) return InstructionGroups::BRANCH;
  if (isLoad()) return base + 8;
  if (isStoreAddress()) return base + 9;
  if (insnTypeMetadata & isDivideMask) return base + 7;
  if (insnTypeMetadata & isMultiplyMask) return base + 6;
  if (insnTypeMetadata & isShiftMask) return base + 5;
  if (insnTypeMetadata & isLogicalMask) return base + 4;
  if (insnTypeMetadata & isCompareMask) return base + 3;
  return base + 2;  // Default return is {Data type}_SIMPLE_ARTH
}

void Instruction::setExecutionInfo(const executionInfo& info) {
  if (isLoad() || isStoreAddress()) {
    lsqExecutionLatency_ = info.latency;
  } else {
    latency_ = info.latency;
  }
  stallCycles_ = info.stallCycles;
  supportedPorts_ = info.ports;
}

const std::vector<uint16_t>& Instruction::getSupportedPorts() {
  if (supportedPorts_.size() == 0) {
    exception_ = InstructionException::NoAvailablePort;
    exceptionEncountered_ = true;
  }
  return supportedPorts_;
}

const InstructionMetadata& Instruction::getMetadata() const { return metadata; }

void Instruction::updateCondStoreResult(const bool success) {
  assert((insnTypeMetadata & isStoreCondMask) &&
         "[SimEng:Instruction] Attempted to update the result register of a "
         "non-conditional-store instruction.");
  RegisterValue result = {(uint64_t)0 | !success, 8};
  results[0] = result;
  condResultReady_ = true;
}

}  // namespace riscv
}  // namespace arch
}  // namespace simeng
