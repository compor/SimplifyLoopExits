//
//
//

#include "SimplifyLoopExits.hpp"

#include "llvm/ADT/SmallVector.h"
// using llvm::SmallVector

#include "llvm/IR/BasicBlock.h"
// using llvm::BasicBlock

#include "llvm/IR/IRBuilder.h"
// using llvm::IRBuilder

#include "llvm/IR/Type.h"
// using llvm::Type

#include "llvm/IR/Constants.h"
// using llvm::ConstantInt

#include "llvm/IR/Instructions.h"
// using llvm::StoreInst

#include "llvm/Analysis/LoopInfo.h"
// using llvm::Loop

#include <set>
// using std::set

namespace icsa {

indexed_basicblock_t
SimplifyLoopExits::getHeaderExit(const llvm::Loop &CurLoop) const {
  llvm::BasicBlock *hdrSuccessor = nullptr;
  indexed_basicblock_t::second_type hdrSuccessorIdx = 0;

  auto hdrTerm = CurLoop.getHeader()->getTerminator();
  auto numHdrSucc = hdrTerm->getNumSuccessors();

  for (auto i = 0u; i < numHdrSucc; ++i) {
    auto *succ = hdrTerm->getSuccessor(i);
    if (!CurLoop.contains(succ)) {
      hdrSuccessor = succ;
      hdrSuccessorIdx = i;
      break;
    }
  }

  return std::make_pair(hdrSuccessor, hdrSuccessorIdx);
}

llvm::Value *SimplifyLoopExits::addExitFlag(llvm::Loop &CurLoop) {
  auto *loopPreHdr = CurLoop.getLoopPreheader();
  // TODO a case for caching these as we process same loop?
  auto *curFunc = loopPreHdr->getParent();
  auto *curModule = curFunc->getParent();
  auto &curContext = curFunc->getContext();

  auto *flagType = llvm::Type::getInt8Ty(curContext);
  auto *flagAlloca = new llvm::AllocaInst(flagType, nullptr, "unifiedExitFlag",
                                          loopPreHdr->getTerminator());

  return flagAlloca;
}

llvm::Value *SimplifyLoopExits::setExitFlag(llvm::Instruction *Inst, bool Val,
                                            llvm::BasicBlock *Entry) {
  if (!Entry)
    Entry = Inst->getParent();

  auto &curContext = Inst->getContext();

  auto *flagType = llvm::Type::getInt8Ty(curContext);
  auto *constantVal = llvm::ConstantInt::get(flagType, Val);

  auto *flagStore =
      new llvm::StoreInst(constantVal, Inst, Entry->getTerminator());

  return flagStore;
}

void SimplifyLoopExits::attachExitBlock(llvm::Loop &CurLoop) {
  // get exiting blocks
  llvm::SmallVector<llvm::BasicBlock *, 5> exiting;
  CurLoop.getExitingBlocks(exiting);

  auto NumHeaderExits = 0ul;
  auto NumNonHeaderExits = 0ul;

  auto *loopHdr = CurLoop.getHeader();
  for (const auto &e : exiting)
    if (loopHdr == e)
      NumHeaderExits++;

  NumNonHeaderExits = exiting.size() - NumHeaderExits;

  // get exit landings
  llvm::SmallVector<llvm::BasicBlock *, 5> uniqueExitLandings;
  CurLoop.getUniqueExitBlocks(uniqueExitLandings);

  // get header exit landing
  // TODO handle headers with no exits
  auto hdrExit = getHeaderExit(CurLoop);

  auto *curFunc = loopHdr->getParent();
  auto *curModule = curFunc->getParent();
  auto &curContext = curFunc->getContext();

  // create unified exit and place as current header exit's predecessor
  auto *unifiedExit = llvm::BasicBlock::Create(curContext, "loop_unified_exit",
                                               curFunc, hdrExit.first);

  llvm::IRBuilder<> builder(unifiedExit);
  builder.CreateBr(hdrExit.first);

  auto hdrTerm = CurLoop.getHeader()->getTerminator();
  hdrTerm->setSuccessor(hdrExit.second, unifiedExit);

  return;
}

} // namespace icsa end
