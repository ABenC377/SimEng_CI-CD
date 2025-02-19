#include "simeng/CoreInstance.hh"

namespace simeng {

CoreInstance::CoreInstance(std::string executablePath,
                           std::vector<std::string> executableArgs,
                           ryml::ConstNodeRef config)
    : config_(config),
      kernel_(kernel::Linux(
          config_["CPU-Info"]["Special-File-Dir-Path"].as<std::string>())) {
  generateCoreModel(executablePath, executableArgs);
}

CoreInstance::CoreInstance(uint8_t* assembledSource, size_t sourceSize,
                           ryml::ConstNodeRef config)
    : config_(config),
      kernel_(kernel::Linux(
          config_["CPU-Info"]["Special-File-Dir-Path"].as<std::string>())),
      source_(assembledSource),
      sourceSize_(sourceSize),
      assembledSource_(true) {
  // Pass an empty string for executablePath and empty vector of strings for
  // executableArgs.
  generateCoreModel("", std::vector<std::string>{});
}

CoreInstance::~CoreInstance() {
  if (source_) {
    delete[] source_;
  }
}

void CoreInstance::generateCoreModel(std::string executablePath,
                                     std::vector<std::string> executableArgs) {
  createProcess(executablePath, executableArgs);
  // Check to see if either of the instruction or data memory interfaces should
  // be created. Don't create the core if either interface is marked as External
  // as they must be set manually prior to the core's creation.

  // Convert Data-Memory's Interface-Type value from a string to
  // memory::MemInterfaceType
  std::string dType_string =
      config_["L1-Data-Memory"]["Interface-Type"].as<std::string>();
  memory::MemInterfaceType dType = memory::MemInterfaceType::Flat;
  if (dType_string == "Fixed") {
    dType = memory::MemInterfaceType::Fixed;
  } else if (dType_string == "External") {
    dType = memory::MemInterfaceType::External;
  }
  // Create data memory if appropriate
  if (dType == memory::MemInterfaceType::External) {
    setDataMemory_ = true;
  } else {
    createL1DataMemory(dType);
  }

  // Convert Instruction-Memory's Interface-Type value from a string to
  // memory::MemInterfaceType
  std::string iType_string =
      config_["L1-Instruction-Memory"]["Interface-Type"].as<std::string>();
  memory::MemInterfaceType iType = memory::MemInterfaceType::Flat;
  if (iType_string == "Fixed") {
    iType = memory::MemInterfaceType::Fixed;
  } else if (iType_string == "External") {
    iType = memory::MemInterfaceType::External;
  }
  // Create instruction memory if appropriate
  if (iType == memory::MemInterfaceType::External) {
    setInstructionMemory_ = true;
  } else {
    createL1InstructionMemory(iType);
  }

  // Create the core if neither memory interfaces are externally constructed
  if (!(setDataMemory_ || setInstructionMemory_)) createCore();

  return;
}

void CoreInstance::createProcess(std::string executablePath,
                                 std::vector<std::string> executableArgs) {
  if (executablePath.length() > 0) {
    // Concatenate the command line arguments into a single vector and create
    // the process image
    std::vector<std::string> commandLine = {executablePath};
    commandLine.insert(commandLine.end(), executableArgs.begin(),
                       executableArgs.end());
    process_ = std::make_unique<kernel::LinuxProcess>(commandLine, config_);

    // Raise error if created process is not valid
    if (!process_->isValid()) {
      std::cerr << "[SimEng:CoreInstance] Could not read/parse "
                << commandLine[0] << std::endl;
      exit(1);
    }
  } else if (assembledSource_) {
    // Create a process image from the source code assembled by LLVM.
    process_ = std::make_unique<kernel::LinuxProcess>(
        span<const uint8_t>(source_, sourceSize_), config_);
    // Raise error if created process is not valid
    if (!process_->isValid()) {
      std::cerr << "[SimEng:CoreInstance] Could not create process based on "
                   "source assembled by LLVM"
                << std::endl;
      exit(1);
    }
  } else {
    // This case shouldn't be reached as the default program should always be
    // provided
    std::cerr << "[SimEng:CoreInstance] Unexpected parameters given to core "
                 "instance. No default program and no assembled source"
              << std::endl;
    exit(1);
  }

  // Create the process memory space from the generated process image
  createProcessMemory();

  // Create the OS kernel with the process
  kernel_.createProcess(*process_.get());

  return;
}

void CoreInstance::createProcessMemory() {
  // Get the process image and its size
  processMemory_ = process_->getProcessImage();
  processMemorySize_ = process_->getProcessImageSize();

  return;
}

void CoreInstance::createL1InstructionMemory(
    const memory::MemInterfaceType type) {
  // Create a L1I cache instance based on type supplied
  if (type == memory::MemInterfaceType::Flat) {
    instructionMemory_ = std::make_shared<memory::FlatMemoryInterface>(
        processMemory_.get(), processMemorySize_);
  } else if (type == memory::MemInterfaceType::Fixed) {
    uint16_t accessLat =
        config_["LSQ-L1-Interface"]["Access-Latency"].as<uint16_t>();
    instructionMemory_ = std::make_shared<memory::FixedLatencyMemoryInterface>(
        processMemory_.get(), processMemorySize_, accessLat);
  } else {
    std::cerr
        << "[SimEng:CoreInstance] Unsupported memory interface type used in "
           "createL1InstructionMemory()."
        << std::endl;
    exit(1);
  }

  return;
}

void CoreInstance::setL1InstructionMemory(
    std::shared_ptr<memory::MemoryInterface> memRef) {
  assert(setInstructionMemory_ &&
         "setL1InstructionMemory(...) called but the interface was created by "
         "the CoreInstance class.");
  // Set the L1I cache instance to use
  instructionMemory_ = memRef;
  return;
}

void CoreInstance::createL1DataMemory(const memory::MemInterfaceType type) {
  // Create a L1D cache instance based on type supplied
  if (type == memory::MemInterfaceType::Flat) {
    dataMemory_ = std::make_shared<memory::FlatMemoryInterface>(
        processMemory_.get(), processMemorySize_);
  } else if (type == memory::MemInterfaceType::Fixed) {
    uint16_t accessLat =
        config_["LSQ-L1-Interface"]["Access-Latency"].as<uint16_t>();
    dataMemory_ = std::make_shared<memory::FixedLatencyMemoryInterface>(
        processMemory_.get(), processMemorySize_, accessLat);
  } else {
    std::cerr << "[SimEng:CoreInstance] Unsupported memory interface type used "
                 "in createL1DataMemory()."
              << std::endl;
    exit(1);
  }

  return;
}

void CoreInstance::setL1DataMemory(
    std::shared_ptr<memory::MemoryInterface> memRef) {
  assert(setDataMemory_ &&
         "setL1DataMemory(...) called but the interface was created by the "
         "CoreInstance class.");
  // Set the L1D cache instance to use
  dataMemory_ = memRef;
  return;
}

void CoreInstance::createCore() {
  // If memory interfaces must be manually set, ensure they have been
  if (setDataMemory_ && (dataMemory_ == nullptr)) {
    std::cerr << "[SimEng:CoreInstance] Data memory not set. External Data "
                 "memory must be manually "
                 "set using the setL1DataMemory(...) function."
              << std::endl;
    exit(1);
  } else if (setInstructionMemory_ && (instructionMemory_ == nullptr)) {
    std::cerr << "[SimEng:CoreInstance] Instruction memory not set. External "
                 "instruction memory "
                 "interface must be manually set using the "
                 "setL1InstructionMemory(...) function."
              << std::endl;
    exit(1);
  }

  // Create the architecture, with knowledge of the OS
  if (config::SimInfo::getISA() == config::ISA::RV64) {
    arch_ = std::make_unique<arch::riscv::Architecture>(kernel_);
  } else if (config::SimInfo::getISA() == config::ISA::AArch64) {
    arch_ = std::make_unique<arch::aarch64::Architecture>(kernel_);
  }

  std::string predictorType =
      config_["Branch-Predictor"]["Type"].as<std::string>();
  if (predictorType == "Generic") {
    predictor_ = std::make_unique<GenericPredictor>();
  } else if (predictorType == "Perceptron") {
    predictor_ = std::make_unique<PerceptronPredictor>();
  }

  // Extract the port arrangement from the config file
  auto config_ports = config_["Ports"];
  std::vector<std::vector<uint16_t>> portArrangement(
      config_ports.num_children());
  for (size_t i = 0; i < config_ports.num_children(); i++) {
    auto config_groups = config_ports[i]["Instruction-Group-Support-Nums"];
    // Read groups in associated port
    for (size_t j = 0; j < config_groups.num_children(); j++) {
      uint16_t grp = config_groups[j].as<uint16_t>();
      portArrangement[i].push_back(grp);
    }
  }
  portAllocator_ =
      std::make_unique<pipeline::BalancedPortAllocator>(portArrangement);

  // Construct the core object based on the defined simulation mode
  uint64_t entryPoint = process_->getEntryPoint();
  if (config::SimInfo::getSimMode() == config::SimulationMode::Emulation) {
    core_ = std::make_shared<models::emulation::Core>(
        *instructionMemory_, *dataMemory_, entryPoint, processMemorySize_,
        *arch_);
  } else if (config::SimInfo::getSimMode() ==
             config::SimulationMode::InOrderPipelined) {
    core_ = std::make_shared<models::inorder::Core>(
        *instructionMemory_, *dataMemory_, processMemorySize_, entryPoint,
        *arch_, *predictor_);
  } else if (config::SimInfo::getSimMode() ==
             config::SimulationMode::Outoforder) {
    core_ = std::make_shared<models::outoforder::Core>(
        *instructionMemory_, *dataMemory_, processMemorySize_, entryPoint,
        *arch_, *predictor_, *portAllocator_, config_);
  }

  createSpecialFileDirectory();

  return;
}

void CoreInstance::createSpecialFileDirectory() {
  // Create the Special Files directory if indicated to do so in Config
  if (config::SimInfo::getGenSpecFiles()) {
    SpecialFileDirGen SFdir = SpecialFileDirGen();
    // Remove any current special files dir
    SFdir.RemoveExistingSFDir();
    // Create new special files dir
    SFdir.GenerateSFDir();
  }

  return;
}

std::shared_ptr<Core> CoreInstance::getCore() const {
  if (core_ == nullptr) {
    std::cerr
        << "[SimEng:CoreInstance] Core object not constructed. If either data "
           "or instruction memory "
           "interfaces are marked as an `External` type, they must be set "
           "manually and then core's creation must be called manually."
        << std::endl;
    exit(1);
  }
  return core_;
}

std::shared_ptr<memory::MemoryInterface> CoreInstance::getDataMemory() const {
  if (setDataMemory_ && (dataMemory_ == nullptr)) {
    std::cerr << "[SimEng:CoreInstance] `External` data memory object not set."
              << std::endl;
    exit(1);
  }
  return dataMemory_;
}

std::shared_ptr<memory::MemoryInterface> CoreInstance::getInstructionMemory()
    const {
  if (setInstructionMemory_ && (instructionMemory_ == nullptr)) {
    std::cerr
        << "`[SimEng:CoreInstance] External` instruction memory object not set."
        << std::endl;
    exit(1);
  }
  return instructionMemory_;
}

std::shared_ptr<char> CoreInstance::getProcessImage() const {
  return processMemory_;
}

uint64_t CoreInstance::getProcessImageSize() const {
  return processMemorySize_;
}

uint64_t CoreInstance::getHeapStart() const { return process_->getHeapStart(); }

}  // namespace simeng
