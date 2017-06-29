//
//
//

#include "SimplifyLoopExits.hpp"

#include "llvm/IR/Dominators.h"
// using llvm::DominatorTree

#include "llvm/IR/BasicBlock.h"
// using llvm::BasicBlock

#include "llvm/IR/DerivedTypes.h"
// using llvm::IntegerType

#include "llvm/IR/Constants.h"
// using llvm::ConstantInt

#include "llvm/IR/Value.h"
// using llvm::Value

#include "llvm/IR/Instruction.h"
// using llvm::Instruction

#include "llvm/IR/Instructions.h"
// using llvm::AllocaInst
// using llvm::StoreInst
// using llvm::LoadInst
// using llvm::BranchInst
// using llvm::SwitchInst
// using llvm::SelectInst

#include "llvm/IR/InstVisitor.h"
// using llvm::InstVisitor

#include "llvm/Analysis/LoopInfo.h"
// using llvm::Loop
// using llvm::LoopInfo

#include "llvm/Support/Casting.h"
// using llvm::dyn_cast

#include "llvm/Transforms/Utils/Local.h"
// using llvm::DemoteRegToStack

#include "llvm/Transforms/Utils/BasicBlockUtils.h"
// using llvm::SplitBlock

#include "llvm/Support/raw_ostream.h"
// using llvm::raw_ostream

#include <algorithm>
// using std::any_of

#include <limits>
// using std::numeric_limits

#include <cassert>
// using assert

// TODO remove this
#include "llvm/Support/FileSystem.h"
// using llvm::sys::fs::OpenFlags

namespace icsa {

constexpr auto unified_exit_case_type_bits =
    std::numeric_limits<SimplifyLoopExits::unified_exit_case_type>::digits;

//

bool isLoopExitSimplifyForm(const llvm::Loop &CurLoop) {
  return CurLoop.getHeader() == CurLoop.getExitingBlock();
}

std::pair<bool, llvm::BasicBlock *>
getExitCondition(const llvm::Loop &CurLoop, const llvm::BasicBlock *BB) {
  auto term = BB ? BB->getTerminator() : CurLoop.getHeader()->getTerminator();

  assert(CurLoop.contains(term->getParent()) &&
         "Basic block must belong to loop!");

  auto ec = std::make_pair(false, static_cast<llvm::BasicBlock *>(nullptr));

  assert(term->getNumSuccessors() <= 2 &&
         "Loop exiting block with more than 2 successors is not supported!");

  if (!CurLoop.contains(term->getSuccessor(0))) {
    ec.first = true;
    ec.second = term->getSuccessor(0);
  } else {
    ec.first = false;
    ec.second = term->getSuccessor(1);
  }

  return ec;
}

//

void DetectGeneratedValuesVisitor(const std::set<llvm::BasicBlock *> &Blocks,
                                  std::set<llvm::Instruction *> &Generated) {
  for (auto &bb : Blocks)
    for (auto &inst : *bb)
      if (std::any_of(inst.user_begin(), inst.user_end(), [&](const auto &u) {
            auto *inst = llvm::dyn_cast<llvm::Instruction>(u);
            return inst && Blocks.end() == Blocks.find(inst->getParent());
          }))
        Generated.insert(&inst);

  return;
}

SimplifyLoopExits::SimplifyLoopExits()
    : m_LI(nullptr), m_DT(nullptr), m_CurLoop(nullptr), m_Preheader(nullptr),
      m_Header(nullptr), m_OldHeader(nullptr), m_Latch(nullptr),
      m_OldLatch(nullptr) {}

bool SimplifyLoopExits::transform(llvm::Loop &CurLoop, llvm::LoopInfo &LI,
                                  llvm::DominatorTree *DT) {
  if (isLoopExitSimplifyForm(CurLoop))
    return false;

  init(CurLoop, LI, DT);

  auto *exitFlag = createExitFlag();
  auto *exitFlagVal = setExitFlag(!getExitCondition(*m_CurLoop).first, exitFlag,
                                  m_Preheader->getTerminator());

  createLatch();

  auto *exitSwitch = createExitSwitch();
  unified_exit_case_type initCase = 0;
  auto *exitSwitchVal =
      setExitSwitch(initCase, exitSwitch, m_Preheader->getTerminator());
  m_UnifiedExit = createUnifiedExit(exitSwitch);

  auto oldHeader = createHeader(exitFlag, m_UnifiedExit);

  demoteGeneratedValues();

  attachExitValues(exitFlag, exitSwitch);
  redirectExitingBlocksToLatch();

  if (m_DT)
    m_DT->recalculate(*(m_Header->getParent()));

  assert(m_CurLoop->isLoopSimplifyForm() &&
         "Pass did not preserve loop canonical for as expected!");

  assert(isLoopExitSimplifyForm(*m_CurLoop) &&
         "Pass did not transform loop as expected!");

  std::error_code ec;
  llvm::raw_fd_ostream dbg("dbg.ll", ec, llvm::sys::fs::F_Text);
  m_Header->getParent()->print(dbg);

  return true;
}

llvm::Value *SimplifyLoopExits::createExitFlag() {
  auto *hdrBranch = llvm::dyn_cast<llvm::BranchInst>(m_Header->getTerminator());
  assert(hdrBranch && "Loop header terminator must be a branch instruction!");

  auto *flagType = hdrBranch->isConditional()
                       ? hdrBranch->getCondition()->getType()
                       : llvm::IntegerType::get(hdrBranch->getContext(),
                                                unified_exit_case_type_bits);
  auto *flagAlloca = new llvm::AllocaInst(flagType, nullptr, "sle_flag",
                                          m_Preheader->getTerminator());

  return flagAlloca;
}

llvm::Value *SimplifyLoopExits::setExitFlag(bool On, llvm::Value *ExitFlag,
                                            llvm::Instruction *InsertBefore) {
  auto *flagType =
      llvm::dyn_cast<llvm::AllocaInst>(ExitFlag)->getAllocatedType();
  auto *onVal = llvm::ConstantInt::get(flagType, On);

  return setExitFlag(onVal, ExitFlag, InsertBefore);
}

llvm::Value *SimplifyLoopExits::setExitFlag(llvm::Value *On,
                                            llvm::Value *ExitFlag,
                                            llvm::Instruction *InsertBefore) {

  return new llvm::StoreInst(On, ExitFlag, InsertBefore);
}

llvm::BasicBlock *
SimplifyLoopExits::createUnifiedExit(llvm::Value *ExitSwitch) {
  auto &curCtx = m_Header->getContext();
  auto *hdrExit = getExitCondition(*m_CurLoop, m_Header).second;

  auto *unifiedExit =
      llvm::BasicBlock::Create(curCtx, "sle_exit", m_Preheader->getParent());

  auto *exitSwitchVal =
      new llvm::LoadInst(ExitSwitch, "sle_switch", unifiedExit);

  auto *sleBr = llvm::SwitchInst::Create(exitSwitchVal, hdrExit, m_Edges.size(),
                                         unifiedExit);

  unified_exit_case_type caseIdx = DefaultCase + 1;

  for (auto eit = m_Edges.begin(), ee = m_Edges.end(); eit != ee; ++eit) {
    if (eit->second == hdrExit)
      continue;

    auto *caseVal = llvm::ConstantInt::get(
        llvm::IntegerType::get(curCtx, unified_exit_case_type_bits), caseIdx++);
    sleBr->addCase(caseVal, const_cast<llvm::BasicBlock *>(eit->second));
  }

  auto *parentLoop = m_CurLoop->getParentLoop();
  if (parentLoop)
    parentLoop->addBasicBlockToLoop(unifiedExit, *m_LI);

  return unifiedExit;
}

llvm::BasicBlock *
SimplifyLoopExits::createHeader(llvm::Value *ExitFlag,
                                llvm::BasicBlock *UnifiedExit) {
  auto *splitPt = m_Header->getFirstNonPHI();
  m_OldHeader = llvm::SplitBlock(m_Header, splitPt, m_DT, m_LI);

  m_Header->setName("sle_header");
  m_OldHeader->setName("old_header");

  auto *exitFlagVal =
      new llvm::LoadInst(ExitFlag, "sle_flag", m_Header->getTerminator());

  if (UnifiedExit) {
    auto *hdrBr = llvm::BranchInst::Create(
        m_OldHeader, UnifiedExit, exitFlagVal, m_Header->getTerminator());
    m_Header->getTerminator()->eraseFromParent();
  }

  updateExitEdges();

  return m_Header;
}

llvm::BasicBlock *SimplifyLoopExits::createLatch() {
  auto *splitPt = m_Latch->getTerminator();

  m_OldLatch = m_Latch;
  m_Latch = llvm::SplitBlock(m_OldLatch, splitPt, m_DT, m_LI);

  m_Latch->setName("sle_latch");
  m_OldLatch->setName("old_latch");

  return m_Latch;
}

llvm::Value *SimplifyLoopExits::createExitSwitch() {
  auto *caseType = llvm::IntegerType::get(m_Preheader->getContext(),
                                          unified_exit_case_type_bits);
  auto *caseAlloca = new llvm::AllocaInst(caseType, nullptr, "sle_switch",
                                          m_Preheader->getTerminator());

  return caseAlloca;
}

llvm::Value *SimplifyLoopExits::setExitSwitch(unified_exit_case_type Case,
                                              llvm::Value *ExitSwitch,
                                              llvm::Instruction *InsertBefore) {
  auto *caseVal = llvm::ConstantInt::get(
      llvm::IntegerType::get(InsertBefore->getContext(),
                             unified_exit_case_type_bits),
      Case);

  return setExitSwitch(caseVal, ExitSwitch, InsertBefore);
}

llvm::Value *SimplifyLoopExits::setExitSwitch(llvm::Value *Case,
                                              llvm::Value *ExitSwitch,
                                              llvm::Instruction *InsertBefore) {
  return new llvm::StoreInst(Case, ExitSwitch, InsertBefore);
}

void SimplifyLoopExits::attachExitValues(llvm::Value *ExitFlag,
                                         llvm::Value *ExitSwitch) {
  unified_exit_case_type caseVal = DefaultCase;

  for (auto &e : m_Edges) {
    if (e.first == m_Header)
      continue;

    auto exitCond = getExitCondition(*m_CurLoop, e.first).first;

    auto *term = const_cast<llvm::TerminatorInst *>(e.first->getTerminator());
    assert(term->getNumSuccessors() <= 2 &&
           "Loop exiting block with more than 2 successors is not supported!");

    auto *br = llvm::dyn_cast<llvm::BranchInst>(term);
    assert(br && "Loop exiting block must be a branch instruction!");

    auto cond1FlagVal = exitCond ? false : true;
    auto cond2FlagVal = !cond1FlagVal;

    auto *flagType =
        llvm::dyn_cast<llvm::AllocaInst>(ExitFlag)->getAllocatedType();
    auto *cond1Flag = llvm::ConstantInt::get(flagType, cond1FlagVal);
    auto *cond2Flag = llvm::ConstantInt::get(flagType, cond2FlagVal);

    auto *selExitCond = llvm::SelectInst::Create(
        br->getCondition(), cond1Flag, cond2Flag, "sle_exit_cond", term);

    auto *flagStore = setExitFlag(selExitCond, ExitFlag, term);

    auto curCaseVal = e.first == m_OldHeader ? DefaultCase : ++caseVal;

    auto cond1CaseVal = exitCond ? curCaseVal : DefaultCase;
    auto cond2CaseVal = !exitCond ? curCaseVal : DefaultCase;

    auto *cond1Case = llvm::ConstantInt::get(
        llvm::IntegerType::get(term->getContext(), unified_exit_case_type_bits),
        cond1CaseVal);
    auto *cond2Case = llvm::ConstantInt::get(
        llvm::IntegerType::get(term->getContext(), unified_exit_case_type_bits),
        cond2CaseVal);

    auto *selSwitchCond = llvm::SelectInst::Create(
        br->getCondition(), cond1Case, cond2Case, "sle_switch_cond", term);

    setExitSwitch(selSwitchCond, ExitSwitch, term);
  }

  return;
}

// private methods

void SimplifyLoopExits::init(llvm::Loop &CurLoop, llvm::LoopInfo &LI,
                             llvm::DominatorTree *DT) {
  assert(CurLoop.isLoopSimplifyForm() &&
         "Loop must be in loop simplify/canonical form!");

  assert(LI[CurLoop.getHeader()] && "Loop does not belong to LoopInfo!");

  if (DT) {
    assert(DT->getNode(CurLoop.getHeader()) &&
           "Dominator tree does not contain loop header!");
  }

  m_LI = &LI;
  m_DT = DT;
  m_CurLoop = &CurLoop;
  m_Preheader = m_CurLoop->getLoopPreheader();
  m_Header = m_CurLoop->getHeader();
  m_Latch = m_CurLoop->getLoopLatch();

  m_OldHeader = nullptr;
  m_OldLatch = nullptr;

  updateExitEdges();

  return;
}

void SimplifyLoopExits::updateExitEdges() {
  m_Edges.clear();
  m_CurLoop->getExitEdges(m_Edges);

  return;
}

void SimplifyLoopExits::demoteGeneratedValues() {
  std::set<llvm::Instruction *> generated;
  std::set<llvm::BasicBlock *> blocks(m_CurLoop->getBlocks().begin(),
                                      m_CurLoop->getBlocks().end());
  DetectGeneratedValuesVisitor(blocks, generated);

  // remove header phis because they are preserved
  for (auto bi = m_Header->begin(), be = m_Header->end();
       bi != be && llvm::dyn_cast<llvm::PHINode>(&*bi); ++bi)
    generated.erase(&*bi);

  assert(m_OldLatch && "new latch node has not been created yet!");

  // old latch block has its postdominance changed due to the new latch,
  // so any generated values used elsewhere inside the loop need to be updated
  for (auto &e : *m_OldLatch)
    if (std::any_of(e.user_begin(), e.user_end(), [this](const auto &u) {
          auto *inst = llvm::dyn_cast<llvm::Instruction>(u);
          return inst && inst->getParent() != m_OldLatch &&
                 m_CurLoop->contains(inst->getParent());
        }))
      generated.insert(&e);

  for (auto &e : generated)
    llvm::DemoteRegToStack(*e, false, m_Preheader->getTerminator());

  return;
}

void SimplifyLoopExits::redirectExitingBlocksToLatch() {
  assert(m_OldLatch && m_OldLatch != m_Latch &&
         "New latch seesm to have not been attached yet!");

  for (auto e : m_Edges) {
    if (e.first == m_Header)
      continue;

    auto *exiting = const_cast<llvm::BasicBlock *>(e.first);
    auto ec = getExitCondition(*m_CurLoop, exiting);

    auto *br = llvm::dyn_cast<llvm::BranchInst>(exiting->getTerminator());
    br->setSuccessor(!ec.first, m_Latch);
  }

  updateExitEdges();

  return;
}

} // namespace icsa end
