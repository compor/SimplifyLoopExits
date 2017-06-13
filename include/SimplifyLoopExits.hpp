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
  SimplifyLoopExits() = default;

  indexed_basicblock_t getHeaderExit(const llvm::Loop &CurLoop) const;
  bool getExitConditionValue(llvm::Loop &CurLoop) const;
  loop_exit_edge_t getEdges(const llvm::Loop &CurLoop);

  llvm::Value *addExitFlag(llvm::Loop &CurLoop);
  llvm::Value *setExitFlag(llvm::Value *Val, bool On,
                           llvm::BasicBlock *Insertion);
  llvm::Value *attachExitFlag(llvm::Loop &CurLoop,
                              llvm::Value *UnifiedExitFlag = nullptr);

  llvm::Value *addExitSwitchCond(llvm::Loop &CurLoop);
  llvm::Value *setExitSwitchValue(llvm::Value *Val, std::int32_t Case,
                                  llvm::BasicBlock *Insertion);

  void attachExitValues(llvm::Loop &CurLoop, llvm::Value *ExitFlag,
                        llvm::Value *ExitSwitchCond,
                        loop_exit_edge_t &LoopExitEdges);

  llvm::BasicBlock *attachExitBlock(llvm::Loop &CurLoop);
};

} // namespace icsa end

#endif // SIMPLIFYLOOPEXITS_HPP
