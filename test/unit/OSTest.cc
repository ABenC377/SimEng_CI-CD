#include "ConfigInit.hh"
#include "gtest/gtest.h"
#include "simeng/kernel/Linux.hh"
#include "simeng/kernel/LinuxProcess.hh"
#include "simeng/span.hh"

namespace simeng {

class OSTest : public testing::Test {
 public:
  OSTest()
      : os(config::SimInfo::getConfig()["CPU-Info"]["Special-File-Dir-Path"]
               .as<std::string>()),
        proc_elf(simeng::kernel::LinuxProcess(cmdLine)),
        proc_hex(simeng::span(reinterpret_cast<const uint8_t*>(demoHex),
                              sizeof(demoHex))) {}

 protected:
  ConfigInit configInit = ConfigInit(
      config::ISA::AArch64,
      R"YAML({Process-Image: {Heap-Size: 1073741824, Stack-Size: 1048576}})YAML");

  const std::vector<std::string> cmdLine = {
      SIMENG_SOURCE_DIR "/test/unit/data/stream-aarch64.elf"};

  std::vector<std::string> defaultCmdLine = {SIMENG_SOURCE_DIR
                                             "/SimEngDefaultProgram"};

  simeng::kernel::Linux os;
  simeng::kernel::LinuxProcess proc_elf;
  simeng::kernel::LinuxProcess proc_hex;

  // A simple program used to test the functionality of creating a process with
  // a stream of hex instructions.
  const uint32_t demoHex[7] = {
      0x320C03E0,  // orr w0, wzr, #1048576
      0x320003E1,  // orr w0, wzr, #1
      0x71000400,  // subs w0, w0, #1
      0x54FFFFC1,  // b.ne -8
                   // .exit:
      0xD2800000,  // mov x0, #0
      0xD2800BC8,  // mov x8, #94
      0xD4000001,  // svc #0
  };
};

// These tests verify the functionality of both the `createProcess()` and
// `getInitialStackPointer()` functions. All other functions for this class are
// syscalls and are tested in the Regression suite.
TEST_F(OSTest, processElf_stackPointer) {
  os.createProcess(proc_elf);
  // cmdLine[0] length will change depending on the host system so final stack
  // pointer needs to be calculated manually
  // cmdLineSize + 1 for null seperator
  const uint64_t cmdLineSize = cmdLine[0].size() + 1;
  // "OMP_NUM_THREADS=1" + 1 for null seperator
  const uint64_t envStringsSize = 18;
  // Size of initial stack frame as per LinuxProcess.cc:createStack()
  // - (17 push_backs) * 8
  // https://www.win.tue.nl/~aeb/linux/hh/stack-layout.html
  const uint64_t stackFrameSize = 17 * 8;
  // cmd + Env needs +1 for null seperator
  const uint64_t stackPointer =
      proc_elf.getStackStart() -
      kernel::alignToBoundary(cmdLineSize + envStringsSize + 1, 32) -
      kernel::alignToBoundary(stackFrameSize, 32);
  EXPECT_EQ(os.getInitialStackPointer(), stackPointer);
  EXPECT_EQ(os.getInitialStackPointer(), proc_elf.getInitialStackPointer());
}

TEST_F(OSTest, processHex_stackPointer) {
  os.createProcess(proc_hex);
  // cmdLine[0] length will change depending on the host system so final stack
  // pointer needs to be calculated manually
  // cmdLineSize + 1 for null seperator
  const uint64_t cmdLineSize = defaultCmdLine[0].size() + 1;
  // "OMP_NUM_THREADS=1" + 1 for null seperator
  const uint64_t envStringsSize = 18;
  // Size of initial stack frame as per LinuxProcess.cc:createStack()
  // - (17 push_backs) * 8
  // https://www.win.tue.nl/~aeb/linux/hh/stack-layout.html
  const uint64_t stackFrameSize = 17 * 8;
  // cmd + Env needs +1 for null seperator
  const uint64_t stackPointer =
      proc_hex.getStackStart() -
      kernel::alignToBoundary(cmdLineSize + envStringsSize + 1, 32) -
      kernel::alignToBoundary(stackFrameSize, 32);
  EXPECT_EQ(os.getInitialStackPointer(), stackPointer);
  EXPECT_EQ(os.getInitialStackPointer(), proc_hex.getInitialStackPointer());
}

// createProcess
// getInitialStackPointer

}  // namespace simeng
