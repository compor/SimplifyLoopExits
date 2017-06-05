//
//
//

#ifndef SIMPLIFYLOOPEXITS_HPP
#define SIMPLIFYLOOPEXITS_HPP

#include <utility>
// using std::pair

namespace llvm {
class Loop;
class BasicBlock;
} // namespace llvm end

using indexed_basicblock_t = std::pair<llvm::BasicBlock *, unsigned>;

namespace icsa {

class SimplifyLoopExits {
public:
  SimplifyLoopExits() = default;

  indexed_basicblock_t getHeaderExit(const llvm::Loop &CurLoop) const;
  void attachExitBlock(llvm::Loop &CurLoop);
};

} // namespace icsa end

#endif // SIMPLIFYLOOPEXITS_HPP
