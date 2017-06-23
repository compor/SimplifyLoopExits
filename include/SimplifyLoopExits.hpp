//
//
//

#ifndef SIMPLIFYLOOPEXITS_HPP
#define SIMPLIFYLOOPEXITS_HPP

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
class BasicBlock;
class Instruction;
class raw_ostream;
} // namespace llvm end

using indexed_basicblock_t = std::pair<llvm::BasicBlock *, unsigned>;

using loop_exit_target_t = std::set<llvm::BasicBlock *>;
using loop_exit_edge_t = std::map<llvm::BasicBlock *, loop_exit_target_t>;

llvm::raw_ostream &operator<<(llvm::raw_ostream &ros,
                              const loop_exit_edge_t &LoopExitEdges);

namespace icsa {

class SimplifyLoopExits {
public:
  using unified_exit_case_type = std::uint32_t;

public:
  SimplifyLoopExits(llvm::Loop &CurLoop);

  void transform(void);

  indexed_basicblock_t getHeaderExit(const llvm::Loop &CurLoop) const;
  bool getExitConditionValue(const llvm::Loop &CurLoop,
                             const llvm::BasicBlock *BB = nullptr) const;
  loop_exit_edge_t getEdges(const llvm::Loop &CurLoop);

  llvm::Value *createExitFlag();
  llvm::Value *setExitFlag(bool On, llvm::Value *ExitFlag,
                           llvm::Instruction *InsertBefore = nullptr);
  llvm::Value *setExitFlag(llvm::Value *On, llvm::Value *ExitFlag,
                           llvm::Instruction *InsertBefore = nullptr);
  llvm::Value *attachExitFlag(llvm::Loop &CurLoop,
                              llvm::Value *UnifiedExitFlag = nullptr);

  llvm::Value *createLoopHeader(llvm::Loop &CurLoop, llvm::Value *LoopCond,
                                llvm::BasicBlock *Latch);
  llvm::BasicBlock *createLoopLatch();

  llvm::Value *createExitSwitchCond(llvm::Loop &CurLoop);
  llvm::Value *setExitSwitchCond(unified_exit_case_type Case,
                                 llvm::Value *ExitSwitchCond,
                                 llvm::Instruction *InsertBefore = nullptr);
  llvm::Value *setExitSwitchCond(llvm::Value *Case, llvm::Value *ExitSwitchCond,
                                 llvm::Instruction *InsertBefore = nullptr);

  void attachExitValues(llvm::Loop &CurLoop, llvm::Value *ExitFlag,
                        llvm::Value *ExitSwitchCond,
                        loop_exit_edge_t &LoopExitEdges);

  llvm::BasicBlock *attachExitBlock(llvm::Loop &CurLoop,
                                    llvm::Value *ExitSwitchCond,
                                    loop_exit_edge_t &LoopExitEdges);

private:
  llvm::Loop &m_CurLoop;
  llvm::BasicBlock *m_PreHeader;
  llvm::BasicBlock *m_Header;
  llvm::BasicBlock *m_Latch;

  template <typename ForwardIter>
  void redirectLoopExitsToLatch(llvm::Loop &CurLoop,
                                ForwardIter exitTargetStart,
                                ForwardIter exitTargetEnd);
};

} // namespace icsa end

#endif // SIMPLIFYLOOPEXITS_HPP
