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

#include "llvm/IR/InstVisitor.h"
// using llvm::InstVisitor

#include "llvm/Analysis/LoopInfo.h"
// using llvm::Loop

#include "llvm/Support/Casting.h"
// using llvm::dyn_cast

#include "llvm/Support/raw_ostream.h"
// using llvm::raw_ostream

#include <iterator>
// using std::iterator_traits

#include <type_traits>
// using std::enable_if
// using std::is_same

#include <set>
// using std::set

#include <cassert>
// using assert

namespace icsa {

struct LoopExitDependentPHIVisitor
    : public llvm::InstVisitor<LoopExitDependentPHIVisitor> {
  std::set<llvm::PHINode *> m_LoopExitPHINodes;
  llvm::Loop &m_CurLoop;
  loop_exit_target_t m_LoopExitTargets;

  LoopExitDependentPHIVisitor(llvm::Loop &CurLoop,
                              loop_exit_target_t &LoopExitTargets)
      : m_CurLoop(CurLoop), m_LoopExitTargets(LoopExitTargets) {}

  void visitPHI(llvm::PHINode &I) {
    auto numInc = I.getNumIncomingValues();

    for (decltype(numInc) i = 0; i < numInc; ++i) {
      auto *bb = I.getIncomingBlock(i);
      if (!m_CurLoop.contains(bb) &&
          m_LoopExitTargets.find(bb) != m_LoopExitTargets.end())
        m_LoopExitPHINodes.insert(&I);
    }

    return;
  }
};

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

  if (!hdrTerm->getNumSuccessors())
    return false;

  return !CurLoop.contains(hdrTerm->getSuccessor(0));
}

loop_exit_edge_t SimplifyLoopExits::getEdges(const llvm::Loop &CurLoop) {
  loop_exit_edge_t edges{};

  for (auto *bb : CurLoop.getBlocks()) {
    if (bb == CurLoop.getHeader())
      continue;

    auto *term = bb->getTerminator();
    loop_exit_target_t externalSucc;

    auto numSucc = term->getNumSuccessors();
    for (decltype(numSucc) i = 0; i < numSucc; ++i) {
      auto *succ = term->getSuccessor(i);
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
  auto *flagAlloca = new llvm::AllocaInst(flagType, nullptr, "sle_flag",
                                          loopPreHdr->getTerminator());

  return flagAlloca;
}

llvm::Value *SimplifyLoopExits::setExitFlag(llvm::Value *Val, bool On,
                                            llvm::BasicBlock *Insertion) {
  auto *flagType = llvm::dyn_cast<llvm::AllocaInst>(Val)->getAllocatedType();
  auto *onVal = llvm::ConstantInt::get(flagType, On);

  auto *flagStore = new llvm::StoreInst(onVal, Val, Insertion->getTerminator());

  return flagStore;
}

llvm::Value *SimplifyLoopExits::attachExitFlag(llvm::Loop &CurLoop,
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
      new llvm::LoadInst(UnifiedExitFlag, "sle_flag_load", hdrBranch);

  // determine operation based on what boolean value exits the loop
  llvm::Instruction::BinaryOps operation =
      getExitConditionValue(CurLoop) ? llvm::Instruction::BinaryOps::Or
                                     : llvm::Instruction::BinaryOps::And;

  // attach the new exit flag condition to the current loop header exit branch
  // condition
  auto *unifiedCond =
      llvm::BinaryOperator::Create(operation, hdrBranch->getCondition(),
                                   UnifiedExitFlagValue, "sle_cond", hdrBranch);

  // replace loop header exit branch condition
  hdrBranch->setCondition(unifiedCond);

  return unifiedCond;
}

llvm::Value *SimplifyLoopExits::addExitSwitchCond(llvm::Loop &CurLoop) {
  auto *loopPreHdr = CurLoop.getLoopPreheader();
  assert(loopPreHdr);

  auto *term = loopPreHdr->getTerminator();
  assert(llvm::isa<llvm::BranchInst>(term));

  auto *caseType = llvm::Type::getInt32Ty(loopPreHdr->getContext());
  auto *caseAlloca =
      new llvm::AllocaInst(caseType, nullptr, "sle_switch", term);

  return caseAlloca;
}

llvm::Value *
SimplifyLoopExits::setExitSwitchValue(llvm::Value *Val, std::int32_t Case,
                                      llvm::BasicBlock *Insertion) {
  auto *caseVal = llvm::ConstantInt::get(
      llvm::Type::getInt32Ty(Insertion->getContext()), Case);
  auto *caseStore =
      new llvm::StoreInst(caseVal, Val, Insertion->getTerminator());

  return caseStore;
}

void SimplifyLoopExits::attachExitValues(llvm::Loop &CurLoop,
                                         llvm::Value *ExitFlag,
                                         llvm::Value *ExitSwitchCond,
                                         loop_exit_edge_t &LoopExitEdges) {
  std::int32_t caseVal = 0;
  auto exitCond = getExitConditionValue(CurLoop);

  for (auto &e : LoopExitEdges) {
    auto *exitVal = setExitFlag(ExitFlag, exitCond, e.first);

    ++caseVal;
    auto *exitSwitchVal = setExitSwitchValue(ExitSwitchCond, caseVal, e.first);
  }

  return;
}

llvm::BasicBlock *
SimplifyLoopExits::attachExitBlock(llvm::Loop &CurLoop,
                                   llvm::Value *ExitSwitchCond,
                                   loop_exit_edge_t &LoopExitEdges) {
  // get header exit landing
  // TODO handle headers with no exits
  auto hdrExit = getHeaderExit(CurLoop);

  auto *curFunc = hdrExit.first->getParent();
  auto &curContext = curFunc->getContext();

  // create unified exit and place as current header exit's predecessor
  auto *unifiedExit =
      llvm::BasicBlock::Create(curContext, "sle_exit", curFunc, hdrExit.first);

  // set loop header exit successor to the new block
  auto hdrTerm = CurLoop.getHeader()->getTerminator();
  hdrTerm->setSuccessor(hdrExit.second, unifiedExit);

  auto *exitSwitchCondVal =
      new llvm::LoadInst(ExitSwitchCond, "sle_switch_cond_load", unifiedExit);
  auto *sleSwitch = llvm::SwitchInst::Create(exitSwitchCondVal, hdrExit.first,
                                             LoopExitEdges.size(), unifiedExit);

  // this loop might be unnecessary if each loop exiting block is always
  // matched to a single exit block
  // furthermore, this might also mean that the loop exit target can be a simple
  // basic block pointer instead of a collection them, which translates to some
  // sort of a data structure
  loop_exit_target_t exitTargets;
  for (auto &e : LoopExitEdges)
    for (auto &t : e.second)
      exitTargets.insert(t);

  LoopExitDependentPHIVisitor ledPHIVisitor{CurLoop, exitTargets};
  ledPHIVisitor.visit(CurLoop.getHeader()->getParent());

  assert(CurLoop.getLoopLatch());
  redirectLoopExitsToLatch(CurLoop, exitTargets.begin(), exitTargets.end());

  auto et = exitTargets.begin();
  for (std::int32_t caseIdx = 1; caseIdx <= exitTargets.size(); ++caseIdx) {
    auto *caseVal = llvm::ConstantInt::get(
        llvm::Type::getInt32Ty(CurLoop.getHeader()->getContext()), caseIdx);
    sleSwitch->addCase(caseVal, *et);
    ++et;
  }

  return unifiedExit;
}

template <typename ForwardIter>
void SimplifyLoopExits::redirectLoopExitsToLatch(llvm::Loop &CurLoop,
                                                 ForwardIter exitTargetStart,
                                                 ForwardIter exitTargetEnd) {
  typename std::enable_if<
      std::is_same<typename std::iterator_traits<ForwardIter>::value_type,
                   llvm::BasicBlock *>::value,
      ForwardIter>::type etIt = exitTargetStart;

  for (; etIt != exitTargetEnd; ++etIt)
    for (auto *u : (*etIt)->users()) {
      auto *brInst = llvm::dyn_cast<llvm::BranchInst>(u);
      if (!brInst)
        continue;

      if (!CurLoop.contains(brInst))
        continue;

      auto numSucc = brInst->getNumSuccessors();
      for (decltype(numSucc) i = 0; i < numSucc; ++i) {
        auto *succ = brInst->getSuccessor(i);
        if (*etIt == succ)
          brInst->setSuccessor(i, CurLoop.getLoopLatch());
      }
    }

  return;
}

} // namespace icsa end

llvm::raw_ostream &operator<<(llvm::raw_ostream &ros,
                              const loop_exit_edge_t &LoopExitEdges) {
  const llvm::StringRef title1{"Edge Head    "};
  const llvm::StringRef title2{"Edge Target  "};
  const llvm::StringRef anon{"unnamed"};
  const llvm::StringRef delimiter{"|"};
  const auto titleLength = title1.size();
  const std::string separator(2 * titleLength + delimiter.size(), '-');
  const std::string entryTemplate(titleLength, ' ');

  ros << title1 << "|" << title2;
  ros << "\n" << separator << "\n";

  for (const auto &e : LoopExitEdges) {
    std::string entry1{anon.str()};
    std::string fill1{""};
    auto fillLength1 = 0;

    if (e.first->hasName())
      entry1 = e.first->getName().substr(0, titleLength).str();

    auto entryLength1 = entry1.size();
    if (entryLength1 < titleLength)
      fillLength1 = titleLength - entryLength1;

    fill1.assign(fillLength1, ' ');

    for (const auto &t : e.second) {
      ros << entry1 << fill1 << "|";

      if (t->hasName())
        ros << t->getName().substr(0, titleLength);
      else
        ros << anon;

      ros << "\n";
    }
  }

  return ros;
}
