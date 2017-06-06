//
//
//

#ifndef SIMPLIFYLOOPEXITS_HPP
#define SIMPLIFYLOOPEXITS_HPP

#include <utility>
// using std::pair

namespace llvm {
class Value;
class Instruction;
class Loop;
class BasicBlock;
class Instruction;
} // namespace llvm end

using indexed_basicblock_t = std::pair<llvm::BasicBlock *, unsigned>;

namespace icsa {

class SimplifyLoopExits {
public:
  SimplifyLoopExits() = default;

  indexed_basicblock_t getHeaderExit(const llvm::Loop &CurLoop) const;
  llvm::Value *addExitFlag(llvm::Loop &CurLoop);
  llvm::Value *setExitFlag(llvm::Instruction *Inst, bool Val = true,
                           llvm::BasicBlock *Entry = nullptr);
  void attachExitBlock(llvm::Loop &CurLoop);
};

} // namespace icsa end

#endif // SIMPLIFYLOOPEXITS_HPP
