//
//
//

#include "Config.hpp"

#include "Utils.hpp"

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

#include "llvm/IR/Dominators.h"
// using llvm::DominatorTree
// using llvm::DominatorTreeWrapperPass

#include "llvm/IR/LegacyPassManager.h"
// using llvm::PassManagerBase

#include "llvm/Transforms/IPO/PassManagerBuilder.h"
// using llvm::PassManagerBuilder
// using llvm::RegisterStandardPasses

#include "llvm/Transforms/Utils.h"
// using char llvm::LoopSimplifyID
// using char llvm::LowerSwitchID

#include "llvm/ADT/SmallVector.h"
// using llvm::SmallVector

#include "llvm/ADT/Statistic.h"
// using STATISTIC macro

#include "llvm/Support/raw_ostream.h"
// using llvm::raw_ostream

#include "llvm/Support/CommandLine.h"
// using llvm::cl::opt
// using llvm::cl::desc
// using llvm::cl::location

#include "llvm/Support/FileSystem.h"
// using llvm::sys::fs::OpenFlags

#include "llvm/Support/Debug.h"
// using DEBUG macro
// using llvm::dbgs

#include <algorithm>
// using std::any_of
// using std::for_each

#include <limits>
// using std::numeric_limits

#include <fstream>
// using std::ifstream

#include <string>
// using std::string

#define DEBUG_TYPE "simplify_loop_exits"

#ifdef LLVM_ENABLE_STATS
STATISTIC(NumExitsSimplifiedLoops, "Number of loops that had exits simplified");
#else
static unsigned NumExitsSimplifiedLoops = 0;
#endif

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

static llvm::cl::opt<unsigned int>
    LoopDepthLB("sle-loop-depth-lb",
                llvm::cl::desc("loop depth lower bound (inclusive)"),
                llvm::cl::init(1u));

static llvm::cl::opt<unsigned int>
    LoopDepthUB("sle-loop-depth-ub",
                llvm::cl::desc("loop depth upper bound (inclusive)"),
                llvm::cl::init(std::numeric_limits<unsigned>::max()));

static llvm::cl::opt<unsigned int> LoopExitingBlockDepthLB(
    "sle-loop-exiting-block-depth-lb",
    llvm::cl::desc("loop exiting block depth lower bound (inclusive)"),
    llvm::cl::init(1u));

static llvm::cl::opt<unsigned int> LoopExitingBlockDepthUB(
    "sle-loop-exiting-block-depth-ub",
    llvm::cl::desc("loop exiting block depth upper bound (inclusive)"),
    llvm::cl::init(std::numeric_limits<unsigned>::max()));

static llvm::cl::opt<std::string> ReportStatsFilename(
    "sle-stats", llvm::cl::desc("simplify loop exits stats report filename"));

#if SIMPLIFYLOOPEXITS_DEBUG
bool passDebugFlag = false;
static llvm::cl::opt<bool, true>
    Debug("sle-debug", llvm::cl::desc("debug simplify loop exits pass"),
          llvm::cl::location(passDebugFlag));
#endif // SIMPLIFYLOOPEXITS_DEBUG

namespace {

void checkCmdLineOptions(void) {
  assert(LoopDepthLB && LoopDepthUB && LoopExitingBlockDepthLB &&
         LoopExitingBlockDepthUB && "Loop depth bounds cannot be zero!");

  assert(LoopDepthLB <= LoopDepthUB &&
         "Loop depth lower bound cannot be greater that upper!");

  assert(LoopExitingBlockDepthLB <= LoopExitingBlockDepthUB &&
         "Loop exiting block depth lower bound cannot be greater that upper!");

  return;
}

} // namespace anonymous end

//

bool SimplifyLoopExitsPass::runOnModule(llvm::Module &M) {
  checkCmdLineOptions();

  bool hasModuleChanged = false;
  llvm::SmallVector<llvm::Loop *, 32> workList;
  SimplifyLoopExits sle;

  auto loopPrintFunc = [](const auto &e) { llvm::errs() << *e << "\n"; };

  for (auto &CurFunc : M) {
    if (CurFunc.isDeclaration())
      continue;

    auto &LI = getAnalysis<llvm::LoopInfoWrapperPass>(CurFunc).getLoopInfo();
    auto &DT =
        getAnalysis<llvm::DominatorTreeWrapperPass>(CurFunc).getDomTree();

    auto loopDepthPrintFunc = [&LI](const auto &e) {
      llvm::SmallVector<llvm::BasicBlock *, 5> exiting;
      e->getExitingBlocks(exiting);

      llvm::outs() << e->getLoopDepth() << "\n";
      for (auto &x : exiting)
        llvm::errs() << x->getName() << " -> " << LI[x]->getLoopDepth() << "\n";
    };

    workList.clear();
    workList.append(&*(LI.begin()), &*(LI.end()));

    for (auto i = 0; i < workList.size(); ++i)
      for (auto &e : workList[i]->getSubLoops())
        workList.push_back(e);

    DEBUG_MSG("\nloop depths for subloops and exiting blocks\n");
    DEBUG_CMD(
        std::for_each(workList.begin(), workList.end(), loopDepthPrintFunc));

    workList.erase(
        std::remove_if(workList.begin(), workList.end(), [](const auto *e) {
          auto d = e->getLoopDepth();
          return d < LoopDepthLB || d > LoopDepthUB;
        }), workList.end());

    DEBUG_MSG("\nafter applying loop depth constraints\n");
    DEBUG_CMD(std::for_each(workList.begin(), workList.end(), loopPrintFunc));

    // remove any loops that their exiting blocks are outside of the
    // specified loop nest levels
    workList.erase(
        std::remove_if(workList.begin(), workList.end(), [&LI](const auto *e) {
          llvm::SmallVector<llvm::BasicBlock *, 5> exiting;
          e->getExitingBlocks(exiting);

          return std::any_of(exiting.begin(), exiting.end(),
                             [&LI](const auto *x) {
                               auto d = LI[x]->getLoopDepth();
                               return d < LoopExitingBlockDepthLB ||
                                      d > LoopExitingBlockDepthUB;
                             });
        }), workList.end());

    DEBUG_MSG("\nafter applying exiting block depth constraints\n");
    DEBUG_CMD(std::for_each(workList.begin(), workList.end(), loopPrintFunc));

    workList.erase(
        std::remove_if(workList.begin(), workList.end(), [](const auto *e) {
          return isLoopExitSimplifyForm(*e);
        }), workList.end());

    DEBUG_MSG("\nafter testing for loop exit simplify form\n");
    DEBUG_CMD(std::for_each(workList.begin(), workList.end(), loopPrintFunc));

    std::reverse(workList.begin(), workList.end());

    assert(std::all_of(workList.begin(), workList.end(), [](const auto &e) {
             return e->isLoopSimplifyForm();
           }) && "Loops are not in loop simplify form!");

    if (workList.empty())
      continue;

    for (auto *e : workList) {
      DEBUG_CMD(llvm::dbgs() << "processing: " << *e << "\n");

      bool changed = sle.transform(*e, LI, &DT);

      hasModuleChanged |= changed;
      if (changed)
        NumExitsSimplifiedLoops++;
    }
  }

  return hasModuleChanged;
}

void SimplifyLoopExitsPass::getAnalysisUsage(llvm::AnalysisUsage &AU) const {
  AU.addRequiredTransitiveID(llvm::LowerSwitchID);
  AU.addRequiredTransitiveID(llvm::LoopSimplifyID);
  AU.addPreservedID(llvm::LoopSimplifyID);
  AU.addRequiredTransitive<llvm::DominatorTreeWrapperPass>();
  AU.addPreserved<llvm::DominatorTreeWrapperPass>();
  AU.addRequiredTransitive<llvm::LoopInfoWrapperPass>();
  AU.addPreserved<llvm::LoopInfoWrapperPass>();

  return;
}

} // namespace icsa end
