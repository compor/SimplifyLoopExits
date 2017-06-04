//
//
//

#ifndef SIMPLIFYLOOPEXITSPASS_HPP
#define SIMPLIFYLOOPEXITSPASS_HPP

#include "llvm/Pass.h"
// using llvm::ModulePass

namespace llvm {
class Module;
} // namespace llvm end

namespace icsa {

class SimplifyLoopExitsPass : public llvm::ModulePass {
public:
  static char ID;

  SimplifyLoopExitsPass() : llvm::ModulePass(ID) {}

  void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;
  bool runOnModule(llvm::Module &M) override;
};

} // namespace icsa end

#endif // CLASSIFYLOOPSEXITPASS_HPP
