//
//
//

#include "SimplifyLoopExits.hpp"

#include "llvm/ADT/SmallVector.h"
// using llvm::SmallVector

#include "llvm/IR/BasicBlock.h"
// using llvm::BasicBlock

#include "llvm/IR/Type.h"
// using llvm::Type

#include "llvm/IR/Constants.h"
// using llvm::ConstantInt

#include "llvm/IR/Instructions.h"
// using llvm::AllocaInst
// using llvm::StoreInst
// using llvm::LoadInst
// using llvm::BranchInst

#include "llvm/IR/Instruction.h"
// using llvm::BinaryOps

#include "llvm/Analysis/LoopInfo.h"
// using llvm::Loop

#include "llvm/Support/Casting.h"
// using llvm::dyn_cast

#include <set>
// using std::set

#include <cassert>
// using assert

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

  if(!hdrTerm->getNumSuccessors())
    return false;

  return !CurLoop.contains(hdrTerm->getSuccessor(0));
}

loop_exit_edge_t SimplifyLoopExits::getEdges(const llvm::Loop &CurLoop) {
  loop_exit_edge_t edges{};

  for (const auto *bb : CurLoop.getBlocks()) {
    if (bb == CurLoop.getHeader())
      continue;

    auto *term = bb->getTerminator();
    loop_exit_target_t externalSucc;

    auto numSucc = term->getNumSuccessors();
    for (decltype(numSucc) i = 0; i < numSucc; ++i) {
      const auto *succ = term->getSuccessor(i);
      if (!CurLoop.contains(succ))
        externalSucc.insert(succ);
    }

    if (!externalSucc.empty())
      edges.emplace(bb, externalSucc);
  }

  return edges;
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

llvm::BasicBlock *SimplifyLoopExits::attachExitBlock(llvm::Loop &CurLoop) {
  // get exiting blocks
  // llvm::SmallVector<llvm::BasicBlock *, 5> exiting;
  // CurLoop.getExitingBlocks(exiting);

  // auto NumHeaderExits = 0ul;
  // auto NumNonHeaderExits = 0ul;

  // auto *loopHdr = CurLoop.getHeader();
  // for (const auto &e : exiting)
  // if (loopHdr == e)
  // NumHeaderExits++;

  // NumNonHeaderExits = exiting.size() - NumHeaderExits;

  //// get exit landings
  // llvm::SmallVector<llvm::BasicBlock *, 5> uniqueExitLandings;
  // CurLoop.getUniqueExitBlocks(uniqueExitLandings);

  // get header exit landing
  // TODO handle headers with no exits
  auto hdrExit = getHeaderExit(CurLoop);

  auto *curFunc = hdrExit.first->getParent();
  auto &curContext = curFunc->getContext();

  // create unified exit and place as current header exit's predecessor
  auto *unifiedExit = llvm::BasicBlock::Create(curContext, "unified_loop_exit",
                                               curFunc, hdrExit.first);

  llvm::BranchInst::Create(hdrExit.first, unifiedExit);

  // set loop header exit successor to the new block
  auto hdrTerm = CurLoop.getHeader()->getTerminator();
  hdrTerm->setSuccessor(hdrExit.second, unifiedExit);

  return unifiedExit;
}

} // namespace icsa en
