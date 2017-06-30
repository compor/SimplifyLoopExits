//
//
//

#ifndef UTILS_HPP
#define UTILS_HPP

#if SIMPLIFYLOOPEXITS_DEBUG

#include "llvm/Support/raw_ostream.h"
// using llvm::errs

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

#else

#define DEBUG_MSG(S)                                                           \
  do {                                                                         \
  } while (false)

#define DEBUG_CMD(C)                                                           \
  do {                                                                         \
  } while (false)

#endif // SIMPLIFYLOOPEXITS_DEBUG

#endif // UTILS_HPP
