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
class BasicBlock;
class Instruction;
class raw_ostream;
} // namespace llvm end

using indexed_basicblock_t = std::pair<llvm::BasicBlock *, unsigned>;

std::pair<bool, llvm::BasicBlock *>
getExitCondition(const llvm::Loop &CurLoop,
                 const llvm::BasicBlock *BB = nullptr);

namespace icsa {

class SimplifyLoopExits {
public:
  using unified_exit_case_type = std::uint32_t;
  unified_exit_case_type DefaultCase = 0;

public:
  SimplifyLoopExits(llvm::Loop &CurLoop, llvm::LoopInfo &LI);

  void transform(void);

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
  llvm::Loop &m_CurLoop;
  llvm::LoopInfo &m_LI;
  llvm::BasicBlock *m_PreHeader;
  llvm::BasicBlock *m_Header;
  llvm::BasicBlock *m_OldHeader;
  llvm::BasicBlock *m_Latch;
  // TODO use alias for vector element type
  llvm::SmallVector<
      std::pair<const llvm::BasicBlock *, const llvm::BasicBlock *>, 16>
      m_Edges;

  void redirectExitsToLatch();
};

} // namespace icsa end

#endif // SIMPLIFYLOOPEXITS_HPP
