#include "simeng/pipeline_hi/WritebackUnit.hh"

#include <iostream>

namespace simeng {
namespace pipeline_hi {

WritebackUnit::WritebackUnit(
    std::vector<PipelineBuffer<std::shared_ptr<Instruction>>>& completionSlots,
    RegisterFileSet& registerFileSet,
    std::function<void(uint64_t insnId)> flagMicroOpCommits,
    std::function<void(const std::shared_ptr<Instruction>&)> removeDep,
    std::function<bool(const std::shared_ptr<Instruction>&)> removeInstrOrderQ)
    : completionSlots_(completionSlots),
      registerFileSet_(registerFileSet),
      flagMicroOpCommits_(flagMicroOpCommits),
      removeDep_(removeDep),
      removeInstrOrderQ_(removeInstrOrderQ) {}

void WritebackUnit::tick() {
  for (size_t slot = 0; slot < completionSlots_.size(); slot++) {
    auto& uop = completionSlots_[slot].getHeadSlots()[0];

    if (uop == nullptr) {
      continue;
    }

    auto& results = uop->getResults();
    auto& destinations = uop->getDestinationRegisters();
    for (size_t i = 0; i < results.size(); i++) {
      // Write results to register file
      registerFileSet_.set(destinations[i], results[i]);
    }
    if (uop->isMicroOp()) {
      uop->setWaitingCommit();
      flagMicroOpCommits_(uop->getInstructionId());
      if (uop->isLastMicroOp()) {
        instructionsWritten_++;
        committedInstsForTrace_.push_back(uop);
      }
    } else {
      uop->setCommitReady();
      removeDep_(uop);
      instructionsWritten_++;
      committedInstsForTrace_.push_back(uop);
    }

    completionSlots_[slot].getHeadSlots()[0] = nullptr;
  }
}

uint64_t WritebackUnit::getInstructionsWrittenCount() const {
  return instructionsWritten_;
}

std::vector<std::shared_ptr<Instruction>> WritebackUnit::getInstsForTrace() {
  std::shared_ptr<Instruction> instr;
  std::deque<std::shared_ptr<Instruction>>::iterator it =  committedInstsForTrace_.begin();
  while(it != committedInstsForTrace_.end()) {
    instr = *it;
    if (removeInstrOrderQ_(instr)) {
      committedInstsForTrace_.erase(it);
      return {instr};
    }
    it++;
  }
  return {}; //committedInstsForTrace_;
}
void WritebackUnit::traceFinished() {
  //committedInstsForTrace_.clear();
}

}  // namespace pipeline_hi
}  // namespace simeng
