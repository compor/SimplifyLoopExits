//
//
//

#include "Config.hpp"

#include "BWList.hpp"

#include "SimplifyLoopExits.hpp"

#include "SimplifyLoopExitsPass.hpp"

#include "llvm/Pass.h"
// using llvm::RegisterPass

#include "llvm/IR/Module.h"
// using llvm::Module

#include "llvm/IR/Function.h"
// using llvm::Function

#include "llvm/Analysis/LoopInfo.h"
// using llvm::LoopInfoWrapperPass
// using llvm::LoopInfo

#include "llvm/IR/LegacyPassManager.h"
// using llvm::PassManagerBase

#include "llvm/Transforms/IPO/PassManagerBuilder.h"
// using llvm::PassManagerBuilder
// using llvm::RegisterStandardPasses

#include "llvm/Support/raw_ostream.h"
// using llvm::raw_ostream

#include "llvm/Support/CommandLine.h"
// using llvm::cl::opt
// using llvm::cl::desc

#include "llvm/Support/FileSystem.h"
// using llvm::sys::fs::OpenFlags

#include "llvm/Support/Debug.h"
// using DEBUG macro
// using llvm::dbgs

#include <fstream>
// using std::ifstream

#include <string>
// using std::string

#define DEBUG_TYPE "simplify-loop-exits"

// plugin registration for opt

#define STRINGIFY_UTIL(x) #x
#define STRINGIFY(x) STRINGIFY_UTIL(x)

#define PRJ_CMDLINE_DESC(x)                                                    \
  x " (version: " STRINGIFY(SIMPLIFYLOOPEXITS_VERSION) ")"

namespace icsa {

char SimplifyLoopExitsPass::ID = 0;
static llvm::RegisterPass<SimplifyLoopExitsPass>
    tmp1("simplify-loop-exits", PRJ_CMDLINE_DESC("simplify loop exits"), false,
         false);

// plugin registration for clang

// the solution was at the bottom of the header file
// 'llvm/Transforms/IPO/PassManagerBuilder.h'
// create a static free-floating callback that uses the legacy pass manager to
// add an instance of this pass and a static instance of the
// RegisterStandardPasses class

static void registerSimplifyLoopExits(const llvm::PassManagerBuilder &Builder,
                                      llvm::legacy::PassManagerBase &PM) {
  PM.add(new SimplifyLoopExitsPass());

  return;
}

static llvm::RegisterStandardPasses
    RegisterSimplifyLoopExits(llvm::PassManagerBuilder::EP_EarlyAsPossible,
                              registerSimplifyLoopExits);

//

bool SimplifyLoopExitsPass::runOnModule(llvm::Module &M) {
  for (auto &CurFunc : M) {
    if (CurFunc.isDeclaration())
      continue;

    auto &LI = getAnalysis<llvm::LoopInfoWrapperPass>(CurFunc).getLoopInfo();
    auto *CurLoop = *(LI.begin());

    SimplifyLoopExits sle;
    sle.attachExitBlock(*CurLoop);
    auto *exitFlag = sle.addExitFlag(*CurLoop);
    auto *exitFlagVal =
        sle.setExitFlag(exitFlag, !sle.getExitConditionValue(*CurLoop),
                        CurLoop->getLoopPreheader());
    sle.attachExitFlag(*CurLoop, exitFlag);

    auto *exitSwitch = sle.addExitSwitchCond(*CurLoop);
    auto *exitSwitchVal =
        sle.setExitSwitchValue(exitSwitch, 0, CurLoop->getLoopPreheader());

    auto loopExitEdges = sle.getEdges(*CurLoop);
    sle.attachExitValues(*CurLoop, exitFlag, exitSwitch, loopExitEdges);
  }

  return false;
}

void SimplifyLoopExitsPass::getAnalysisUsage(llvm::AnalysisUsage &AU) const {
  AU.addRequiredTransitive<llvm::LoopInfoWrapperPass>();

  return;
}

} // namespace icsa end
