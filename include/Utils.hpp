//
//
//

#ifndef UTILS_HPP
#define UTILS_HPP

#include "Config.hpp"

#if SIMPLIFYLOOPEXITS_DEBUG

#include "llvm/IR/Function.h"
// using llvm::Function

#include "llvm/Support/FileSystem.h"
// using llvm::sys::fs::OpenFlags

#include "llvm/Support/raw_ostream.h"
// using llvm::errs
// using llvm::raw_fd_ostream

#include <string>
// using std::to_string

#include <random>
// std::random_device
// std::mt19937

#include <system_error>
// using std::error_code

namespace icsa {
extern bool passDebugFlag;
} // namespace icsa end

#define DEBUG_MSG(STR)                                                         \
  do {                                                                         \
    if (passDebugFlag)                                                         \
      llvm::errs() << STR;                                                     \
  } while (false)

#define DEBUG_CMD(C)                                                           \
  do {                                                                         \
    if (passDebugFlag)                                                         \
      C;                                                                       \
  } while (false)

namespace icsa {

static bool dumpFunction(const llvm::Function *CurFunc = nullptr) {
  if (!CurFunc)
    return false;

  static std::random_device rd;
  static std::mt19937 gen(rd());

  std::uniform_int_distribution<> dis(1, 1000);
  auto r = dis(gen);

  llvm::StringRef filename = "function.dump" + std::to_string(r);
  std::error_code ec;
  llvm::raw_fd_ostream dbgFile(filename, ec, llvm::sys::fs::F_Text);

  llvm::errs() << "\nfunction dumped at: " << filename << "\n";
  CurFunc->print(dbgFile);

  return false;
}

} // namespace icsa end

#else

#define DEBUG_MSG(S)                                                           \
  do {                                                                         \
  } while (false)

#define DEBUG_CMD(C)                                                           \
  do {                                                                         \
  } while (false)

namespace llvm {
class Function;
} // namespace llvm end

namespace icsa {

static constexpr bool dumpFunction(const llvm::Function *CurFunc = nullptr) {
  return true;
}

} // namespace icsa end

#endif // SIMPLIFYLOOPEXITS_DEBUG

#endif // UTILS_HPP
