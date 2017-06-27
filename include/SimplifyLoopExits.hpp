//
//
//

#ifndef SIMPLIFYLOOPEXITS_HPP
#define SIMPLIFYLOOPEXITS_HPP

#include "llvm/ADT/SmallVector.h"
// using llvm::SmallVector

#include <cstdint>
// using std::int32_t

#include <map>
// using std::map

#include <set>
// using std::set

#include <utility>
// using std::pair

namespace llvm {
class Value;
class Loop;
class LoopInfo;
class DominatorTree;
class BasicBlock;
class Instruction;
class raw_ostream;
} // namespace llvm end

namespace icsa {

bool isLoopExitSimplifyForm(const llvm::Loop &CurLoop);

std::pair<bool, llvm::BasicBlock *>
getExitCondition(const llvm::Loop &CurLoop,
                 const llvm::BasicBlock *BB = nullptr);

class SimplifyLoopExits {
public:
  using unified_exit_case_type = std::uint32_t;
  unified_exit_case_type DefaultCase = 0;

public:
  SimplifyLoopExits();

  bool transform(llvm::Loop &CurLoop, llvm::LoopInfo &LI,
                 llvm::DominatorTree *DT = nullptr);

  llvm::Value *createExitFlag();
  llvm::Value *setExitFlag(bool On, llvm::Value *ExitFlag,
                           llvm::Instruction *InsertBefore = nullptr);
  llvm::Value *setExitFlag(llvm::Value *On, llvm::Value *ExitFlag,
                           llvm::Instruction *InsertBefore = nullptr);

  llvm::Value *createExitSwitch();
  llvm::Value *setExitSwitch(unified_exit_case_type Case,
                             llvm::Value *ExitSwitch,
                             llvm::Instruction *InsertBefore = nullptr);
  llvm::Value *setExitSwitch(llvm::Value *Case, llvm::Value *ExitSwitch,
                             llvm::Instruction *InsertBefore = nullptr);

  llvm::BasicBlock *createUnifiedExit(llvm::Value *ExitSwitch);
  llvm::BasicBlock *createHeader(llvm::Value *ExitFlag,
                                 llvm::BasicBlock *UnifiedExit = nullptr);
  llvm::BasicBlock *createLatch();

  void attachExitValues(llvm::Value *ExitFlag, llvm::Value *ExitSwitch);

private:
  llvm::LoopInfo *m_LI;
  llvm::DominatorTree *m_DT;
  llvm::Loop *m_CurLoop;
  llvm::BasicBlock *m_Preheader;
  llvm::BasicBlock *m_Header;
  llvm::BasicBlock *m_OldHeader;
  llvm::BasicBlock *m_Latch;
  llvm::BasicBlock *m_OldLatch;
  llvm::BasicBlock *m_UnifiedExit;
  // TODO use alias for vector element type
  llvm::SmallVector<
      std::pair<const llvm::BasicBlock *, const llvm::BasicBlock *>, 6> m_Edges;

  void init(llvm::Loop &CurLoop, llvm::LoopInfo &LI,
            llvm::DominatorTree *DT = nullptr);
  void updateExitEdges();
  void demoteGeneratedValues();
  void redirectExitingBlocksToLatch();
};

} // namespace icsa end

#endif // SIMPLIFYLOOPEXITS_HPP
