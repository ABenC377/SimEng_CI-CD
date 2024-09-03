#include "simeng/pipeline/FetchUnit.hh"

namespace simeng {
namespace pipeline {

FetchUnit::FetchUnit(PipelineBuffer<MacroOp>& output,
                     std::shared_ptr<memory::MMU> mmu, uint8_t blockSize,
                     arch::Architecture& isa, BranchPredictor& branchPredictor)
    : output_(output),
      mmu_(mmu),
      isa_(isa),
      branchPredictor_(branchPredictor),
      blockSize_(blockSize),
      blockMask_(~(blockSize_ - 1)) {
  assert(blockSize_ >= isa_.getMaxInstructionSize() &&
         "fetch block size must be larger than the largest instruction");
  mopCache_ = std::vector<std::pair<RegisterValue, uint64_t>>(
      static_cast<uint64_t>(1 << mopCacheTagBits_), {{}, 0ull});
}

FetchUnit::~FetchUnit() {}

void FetchUnit::tick() {
  if (output_.isStalled() || paused_) {
    return;
  }

  if (hasHalted_) {
    return;
  }

  // Get any instruction memory reads
  const auto& fetched = mmu_->getCompletedInstrReads();

  // Check if any fetched instruction blocks have registered requests
  for (const auto& blk : fetched) {
    auto it = std::find(requestedBlocks_.begin(), requestedBlocks_.end(),
                        blk.target.vaddr);
    if (it != requestedBlocks_.end()) {
      // If the block has been requested, pre-decode all possible instructions
      // in block
      if (blk.data.size()) {
        // const uint8_t* fetchData = blk.data.getAsVector<uint8_t>();
        // uint16_t dataOffset = 0;
        uint64_t address = blk.target.vaddr;

        // while (dataOffset < blk.target.size) {
        // Get mop cache index
        uint64_t cacheIndex = address & ((1 << mopCacheTagBits_) - 1);
        //   memcpy(&mopCache_[cacheIndex].first, (fetchData + dataOffset), 4);
        mopCache_[cacheIndex].first = blk.data;
        mopCache_[cacheIndex].second = address;
        // std::cout << "Got I block " << std::hex << address << std::dec
        //           << std::endl;
        //   // Increment the offset and address
        //   dataOffset += static_cast<uint16_t>(4);
        //   address += static_cast<uint64_t>(4);
        // }
      }
      requestedBlocks_.erase(it);
    }
  }
  mmu_->clearCompletedIntrReads();

  // Determine if there's space in the mop queue
  while (mopQueue_.size() < mopQueueSize_) {
    // Determine if cached entry is correct
    uint64_t blockAddress = pc_ & blockMask_;
    uint64_t cacheIndex = blockAddress & ((1 << mopCacheTagBits_) - 1);
    std::pair<RegisterValue, uint64_t> cachedEntry = mopCache_[cacheIndex];

    if (cachedEntry.first.size() != 0 && (cachedEntry.second == blockAddress)) {
      mopQueue_.push_back({});
      auto& macroOp = mopQueue_.back();

      uint8_t bytesRead = 0;
      // std::cout << "Fetching PC " << std::hex << pc_ << std::dec <<
      // std::endl; std::cout << "\tChecking for cross on block " << std::hex <<
      // blockAddress
      //           << std::dec << ": " << (blockAddress + blockSize_) - pc_
      //           << std::endl;
      if ((blockAddress + blockSize_) - pc_ < 4) {
        uint64_t blockAddress2 = (pc_ + blockSize_) & blockMask_;
        // std::cout << "\tCrosses, trying to find block " << std::hex
        //           << blockAddress2 << std::dec << std::endl;
        uint64_t cacheIndex2 = blockAddress2 & ((1 << mopCacheTagBits_) - 1);
        std::pair<RegisterValue, uint64_t> cachedEntry2 =
            mopCache_[cacheIndex2];

        if (cachedEntry2.first.size() != 0 &&
            (cachedEntry2.second == blockAddress2)) {
          // std::cout << "\tGot second block: " << std::hex
          //           << (*cachedEntry2.first.getAsVector<uint8_t>() +
          //               ((pc_ + blockSize_) - blockAddress2))
          //           << std::dec << " -> " << std::hex
          //           << (((*cachedEntry2.first.getAsVector<uint8_t>() +
          //                 ((pc_ + blockSize_) - blockAddress2)) &
          //                0xFFFF)
          //               << 16)
          //           << std::dec << " | " << std::hex
          //           << (*cachedEntry.first.getAsVector<uint8_t>() +
          //               (pc_ - blockAddress))
          //           << std::dec << " -> " << std::hex
          //           << ((*cachedEntry.first.getAsVector<uint8_t>() +
          //                (pc_ - blockAddress)) &
          //               0xFFFF)
          //           << std::dec << std::endl;

          uint64_t boundaryCrossedBytes =
              (static_cast<uint64_t>(
                   *(cachedEntry2.first.getAsVector<uint8_t>() +
                     ((blockAddress2 + 1) - blockAddress2)))
               << 24) |
              (static_cast<uint64_t>(
                   *(cachedEntry2.first.getAsVector<uint8_t>() +
                     ((blockAddress2)-blockAddress2)))
               << 16) |
              (static_cast<uint64_t>(
                   *(cachedEntry.first.getAsVector<uint8_t>() +
                     (pc_ + 1 - blockAddress)))
               << 8) |
              (static_cast<uint64_t>(
                  *(cachedEntry.first.getAsVector<uint8_t>() +
                    (pc_ - blockAddress))));

          // std::cout << "\t" << std::hex
          //           << unsigned((static_cast<uint64_t>(*(
          //                            cachedEntry2.first.getAsVector<uint8_t>()
          //                            +
          //                            ((blockAddress2 + 1) - blockAddress2)))
          //                        << 24))
          //           << std::dec << ":" << std::hex
          //           << unsigned((static_cast<uint64_t>(*(
          //                            cachedEntry2.first.getAsVector<uint8_t>()
          //                            +
          //                            ((blockAddress2)-blockAddress2)))
          //                        << 16))
          //           << std::dec << ":" << std::hex
          //           << unsigned((static_cast<uint64_t>(*(
          //                            cachedEntry.first.getAsVector<uint8_t>()
          //                            + (pc_ + 1 - blockAddress)))
          //                        << 8))
          //           << std::dec << ":" << std::hex
          //           << unsigned((static_cast<uint64_t>(
          //                  *(cachedEntry.first.getAsVector<uint8_t>() +
          //                    (pc_ - blockAddress)))))
          //           << std::dec << " = " << std::hex << boundaryCrossedBytes
          //           << std::dec << std::endl;

          bytesRead = isa_.predecode(&boundaryCrossedBytes, 4, pc_, macroOp);
        } else {
          // Request new block from instruction memory if there isn't an
          // existing request
          mopQueue_.pop_back();
          auto it = std::find(requestedBlocks_.begin(), requestedBlocks_.end(),
                              blockAddress2);
          if (it == requestedBlocks_.end()) {
            mmu_->requestInstrRead({blockAddress2, blockSize_});
            requestedBlocks_.push_back(blockAddress2);
          }
          break;
        }
      } else {
        bytesRead = isa_.predecode(
            cachedEntry.first.getAsVector<uint8_t>() + (pc_ - blockAddress), 4,
            pc_, macroOp);
      }

      // If predecode fails, bail and wait for more data
      if (bytesRead == 0) {
        mopQueue_.pop_back();
        break;
      }

      if (bytesRead == 100) {
        mopQueue_.pop_back();
        pc_ += 4;
        continue;
      }

      // Create branch prediction after identifying instruction type
      // (e.g. RET, BL, etc).
      BranchPrediction prediction = {false,
                                     pc_ + static_cast<uint64_t>(bytesRead)};
      if (macroOp[0]->isBranch()) {
        prediction = branchPredictor_.predict(pc_, macroOp[0]->getBranchType(),
                                              macroOp[0]->getKnownOffset());
        branchesFetched_++;
      }
      macroOp[0]->setBranchPrediction(prediction);

      // Update PC based on previous branch prediction
      if (prediction.isTaken) {
        // Predicted as taken; set PC to predicted target address
        pc_ = prediction.target;
      } else {
        // Predicted as not taken; increment PC to next instruction
        pc_ += bytesRead;
      }
    } else {
      // Request new block from instruction memory if there isn't an existing
      // request
      auto it = std::find(requestedBlocks_.begin(), requestedBlocks_.end(),
                          blockAddress);
      if (it == requestedBlocks_.end()) {
        mmu_->requestInstrRead({blockAddress, blockSize_});
        requestedBlocks_.push_back(blockAddress);
      }
      break;
    }
  }

  // Send mops to decode unit up to the width of the buffer
  uint16_t idx = 0;
  while (mopQueue_.size() && idx < output_.getWidth()) {
    for (int i = 0; i < mopQueue_.front().size(); i++)
      output_.getTailSlots()[idx].push_back(std::move(mopQueue_.front()[i]));
    idx++;
    mopQueue_.pop_front();
  }
}

void FetchUnit::registerLoopBoundary(uint64_t branchAddress) {
  // Set branch which forms the loop as the loopBoundaryAddress_ and place
  // loop buffer in state to begin filling once the loopBoundaryAddress_ has
  // been fetched
  loopBufferState_ = LoopBufferState::WAITING;
  loopBoundaryAddress_ = branchAddress;
}

bool FetchUnit::hasHalted() const { return hasHalted_; }

void FetchUnit::updatePC(uint64_t address) {
  // if (mmu_->getTid() == 24)
  //   std::cerr << "Updating PC from  " << std::hex << pc_ << std::dec << " to
  //   "
  //             << std::hex << address << std::dec << std::endl;
  pc_ = address;
  requestedBlocks_.clear();

  for (const auto& insn : mopQueue_) {
    if (insn[0]->isBranch())
      branchPredictor_.flush(insn[0]->getInstructionAddress());
  }
  mopQueue_.clear();
  if (programByteLength_ == 0) {
    std::cerr
        << "[SimEng::FetchUnit] Invalid Program Byte Length of 0. Please "
           "ensure setProgramLength() is called before calling updatePC().\n";
    exit(1);
  }
  hasHalted_ = (pc_ >= programByteLength_);
}

void FetchUnit::setProgramLength(uint64_t size) { programByteLength_ = size; }

uint64_t FetchUnit::getFetchStalls() const { return fetchStalls_; }

uint64_t FetchUnit::getBranchFetchedCount() const { return branchesFetched_; }

void FetchUnit::flushLoopBuffer() {
  loopBuffer_.clear();
  loopBufferState_ = LoopBufferState::IDLE;
  loopBoundaryAddress_ = 0;
}

}  // namespace pipeline
}  // namespace simeng
