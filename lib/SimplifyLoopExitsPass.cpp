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

#include "llvm/Transforms/Scalar.h"
// using char llvm::LoopInfoSimplifyID

#include "llvm/ADT/Statistic.h"
// using STATISTIC macro

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
    LoopDepthUB("sle-loop-depth-ub",
                llvm::cl::desc("loop depth upper bound (inclusive)"),
                llvm::cl::init(1u));

static llvm::cl::opt<unsigned int>
    LoopDepthLB("sle-loop-depth-lb",
                llvm::cl::desc("loop depth lower bound (inclusive)"),
                llvm::cl::init(std::numeric_limits<unsigned>::max()));

static llvm::cl::opt<unsigned int> LoopExitingDepthUB(
    "sle-loop-exiting-depth-ub",
    llvm::cl::desc("loop exiting depth upper bound (inclusive)"),
    llvm::cl::init(1u));

static llvm::cl::opt<unsigned int> LoopExitingDepthLB(
    "sle-loop-exiting-depth-lb",
    llvm::cl::desc("loop exiting depth lower bound (inclusive)"),
    llvm::cl::init(std::numeric_limits<unsigned>::max()));

static llvm::cl::opt<std::string> ReportStatsFilename(
    "sle-stats", llvm::cl::desc("simplify loop exits stats report filename"));

#if SIMPLIFYLOOPEXITS_DEBUG
bool passDebugFlag = false;
static llvm::cl::opt<bool, true>
    Debug("sle-debug", llvm::cl::desc("enable debug for simplify loop exits"),
          llvm::cl::location(passDebugFlag));
#endif // SIMPLIFYLOOPEXITS_DEBUG

namespace {

void checkCmdLineOptions(void) {
  assert(LoopDepthLB && LoopDepthUB && LoopExitingDepthLB &&
         LoopExitingDepthUB && "Loop depth bounds cannot be zero!");

  assert(LoopDepthLB > LoopDepthUB &&
         "Loop depth lower bound cannot be less that upper!");

  assert(LoopExitingDepthLB > LoopExitingDepthUB &&
         "Loop exiting block lower bound depth cannot be less that upper!");

  return;
}

} // namespace anonymous end

//

bool SimplifyLoopExitsPass::runOnModule(llvm::Module &M) {
  checkCmdLineOptions();

  bool hasModuleChanged = false;
  llvm::SmallVector<llvm::Loop *, 16> workList;
  SimplifyLoopExits sle;

  for (auto &CurFunc : M) {
    if (CurFunc.isDeclaration())
      continue;

    auto &LI = getAnalysis<llvm::LoopInfoWrapperPass>(CurFunc).getLoopInfo();
    auto &DT =
        getAnalysis<llvm::DominatorTreeWrapperPass>(CurFunc).getDomTree();

    workList.clear();
    workList.append(&*(LI.begin()), &*(LI.end()));

    for (auto i = 0; i < workList.size(); ++i)
      for (auto &e : workList[i]->getSubLoops())
        workList.push_back(e);

    workList.erase(
        std::remove_if(workList.begin(), workList.end(), [](const auto *e) {
          auto d = e->getLoopDepth();
          return d >= LoopDepthLB && d <= LoopDepthUB;
        }), workList.end());

    // remove any loops that their exiting blocks are outside of the
    // specified loop next levels
    workList.erase(
        std::remove_if(workList.begin(), workList.end(), [&LI](const auto *e) {
          llvm::SmallVector<llvm::BasicBlock *, 5> exiting;
          e->getExitingBlocks(exiting);

          return std::any_of(
              exiting.begin(), exiting.end(), [&LI](const auto *x) {
                auto d = LI[x]->getLoopDepth();
                return d < LoopExitingDepthLB || d > LoopExitingDepthUB;
              });
        }), workList.end());

    workList.erase(
        std::remove_if(workList.begin(), workList.end(), [](const auto *e) {
          return isLoopExitSimplifyForm(*e);
        }), workList.end());

    std::reverse(workList.begin(), workList.end());

    assert(std::all_of(workList.begin(), workList.end(), [](const auto &e) {
             return e->isLoopSimplifyForm();
           }) && "Loops are not in loop simplify form!");

    if (workList.empty())
      continue;

    for (auto *e : workList) {
      bool changed = sle.transform(*e, LI, &DT);

      hasModuleChanged |= changed;
      if (changed)
        NumExitsSimplifiedLoops++;
    }
  }

  return hasModuleChanged;
}

void SimplifyLoopExitsPass::getAnalysisUsage(llvm::AnalysisUsage &AU) const {
  AU.addRequiredTransitive<llvm::DominatorTreeWrapperPass>();
  AU.addPreserved<llvm::DominatorTreeWrapperPass>();
  AU.addRequiredTransitive<llvm::LoopInfoWrapperPass>();
  AU.addPreserved<llvm::LoopInfoWrapperPass>();
  AU.addRequiredTransitiveID(llvm::LoopSimplifyID);
  AU.addPreservedID(llvm::LoopSimplifyID);

  return;
}

} // namespace icsa end
