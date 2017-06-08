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

#include "llvm/IR/Instruction.h"
// using llvm::BinaryOps

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

bool SimplifyLoopExits::getExitConditionValue(llvm::Loop &CurLoop) const {
  auto hdrTerm = CurLoop.getHeader()->getTerminator();

  return !CurLoop.contains(hdrTerm->getSuccessor(0));
}

llvm::Value *SimplifyLoopExits::addExitFlag(llvm::Loop &CurLoop) {
  auto *loopPreHdr = CurLoop.getLoopPreheader();
  auto *hdrTerm = CurLoop.getHeader()->getTerminator();

  assert(loopPreHdr);
  assert(llvm::isa<llvm::BranchInst>(hdrTerm));

  auto *hdrBranch = llvm::dyn_cast<llvm::BranchInst>(hdrTerm);

  auto *flagType = hdrBranch->getCondition()->getType();
  auto *flagAlloca = new llvm::AllocaInst(
      flagType, nullptr, "unified_exit_flag", loopPreHdr->getTerminator());

  return flagAlloca;
}

llvm::Value *SimplifyLoopExits::setExitFlag(llvm::Value *Val, bool On,
                                            llvm::BasicBlock *Insertion) {
  auto *flagType = llvm::dyn_cast<llvm::AllocaInst>(Val)->getAllocatedType();
  auto *onVal = llvm::ConstantInt::get(flagType, On);

  auto *flagStore = new llvm::StoreInst(onVal, Val, Insertion->getTerminator());

  return flagStore;
}

llvm::Value *
SimplifyLoopExits::attachExitCondition(llvm::Loop &CurLoop,
                                       llvm::Value *UnifiedExitFlag) {
  if (!UnifiedExitFlag) {
    UnifiedExitFlag = addExitFlag(CurLoop);
    setExitFlag(UnifiedExitFlag, !getExitConditionValue(CurLoop),
                CurLoop.getLoopPreheader());
  }

  // get branch instruction to use as instruction insertion point
  auto *hdrTerm = CurLoop.getHeader()->getTerminator();
  assert(llvm::isa<llvm::BranchInst>(hdrTerm));
  auto *hdrBranch = llvm::dyn_cast<llvm::BranchInst>(hdrTerm);

  // load unified exit flag value
  auto *UnifiedExitFlagValue =
      new llvm::LoadInst(UnifiedExitFlag, "unified_exit_flag_load", hdrBranch);

  // determine operation based on what boolean value exits the loop
  llvm::Instruction::BinaryOps operation =
      getExitConditionValue(CurLoop) ? llvm::Instruction::BinaryOps::Or
                                     : llvm::Instruction::BinaryOps::And;

  // attach the new exit flag condition to the current loop header exit branch
  // condition
  auto *unifiedCond = llvm::BinaryOperator::Create(
      operation, hdrBranch->getCondition(), UnifiedExitFlagValue,
      "unified_loop_cond", hdrBranch);

  // replace loop header exit branch condition
  hdrBranch->setCondition(unifiedCond);

  return unifiedCond;
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

} // namespace icsa en
