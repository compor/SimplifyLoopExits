//
//
//

#ifndef SIMPLIFYLOOPEXITS_HPP
#define SIMPLIFYLOOPEXITS_HPP

#include <vector>
// using std::vector

#include <utility>
// using std::pair

namespace llvm {
class Value;
class Loop;
class BasicBlock;
} // namespace llvm end

using indexed_basicblock_t = std::pair<llvm::BasicBlock *, unsigned>;

using loop_exit_target_t = std::vector<const llvm::BasicBlock *>;
using loop_exit_edge_t =
    std::pair<const llvm::BasicBlock *, loop_exit_target_t>;
using loop_exit_edge_vector = std::vector<loop_exit_edge_t>;

namespace icsa {

class SimplifyLoopExits {
public:
  SimplifyLoopExits() = default;

  indexed_basicblock_t getHeaderExit(const llvm::Loop &CurLoop) const;
  bool getExitConditionValue(llvm::Loop &CurLoop) const;
  loop_exit_edge_vector getEdges(const llvm::Loop &CurLoop);

  llvm::Value *addExitFlag(llvm::Loop &CurLoop);
  llvm::Value *setExitFlag(llvm::Value *Val, bool On,
                           llvm::BasicBlock *Insertion);
  llvm::Value *attachExitCondition(llvm::Loop &CurLoop,
                                   llvm::Value *UnifiedExitFlag = nullptr);
  llvm::BasicBlock *attachExitBlock(llvm::Loop &CurLoop);
};

} // namespace icsa end

#endif // SIMPLIFYLOOPEXITS_HPP
