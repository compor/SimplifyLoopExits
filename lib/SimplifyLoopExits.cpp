//
//
//

#include "SimplifyLoopExits.hpp"

#include "Utils.hpp"

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

#include "llvm/Transforms/Utils/Local.h"
// using llvm::DemoteRegToStack

#include "llvm/Transforms/Utils/BasicBlockUtils.h"
// using llvm::SplitBlock

#include "llvm/Support/Casting.h"
// using llvm::dyn_cast

// TODO remove this include
#include "llvm/Support/raw_ostream.h"

#include <algorithm>
// using std::any_of
// using std::for_each

#include <limits>
// using std::numeric_limits

#include <cassert>
// using assert

namespace icsa {

constexpr auto unified_exit_case_type_bits =
    std::numeric_limits<SimplifyLoopExits::unified_exit_case_type>::digits;

//

bool isLoopExitSimplifyForm(const llvm::Loop &CurLoop) {
  return CurLoop.getHeader() == CurLoop.getExitingBlock();
}

std::pair<bool, llvm::BasicBlock *>
getExitCondition(const llvm::Loop &CurLoop, const llvm::BasicBlock *BB) {
  auto term = BB->getTerminator();

  assert(CurLoop.contains(term->getParent()) &&
         "Basic block must belong to loop!");

  auto ec = std::make_pair(false, static_cast<llvm::BasicBlock *>(nullptr));

  assert(term->getNumSuccessors() == 2 &&
         "Loop exiting block with != 2 successors is not supported!");

  if (!CurLoop.contains(term->getSuccessor(0))) {
    ec.first = true;
    ec.second = term->getSuccessor(0);
  } else {
    ec.first = false;
    ec.second = term->getSuccessor(1);
  }

  return ec;
}

void FindDefsInBlocksNotUsedIn(const std::set<llvm::BasicBlock *> &Blocks,
                               const std::set<llvm::BasicBlock *> &Exclude,
                               std::set<llvm::Instruction *> &Defs) {
  for (auto &bb : Blocks)
    for (auto &inst : *bb)
      if (std::any_of(inst.user_begin(), inst.user_end(), [&](const auto &u) {
            auto *inst = llvm::dyn_cast<llvm::Instruction>(u);
            return inst && Exclude.end() == Exclude.find(inst->getParent());
          }))
        Defs.insert(&inst);

  return;
}

bool UniquifyLoopExits(llvm::Loop &CurLoop) {
  bool hasChanged = false;
  llvm::SmallVector<llvm::Loop::Edge, 4> edges;
  CurLoop.getExitEdges(edges);

  using exits_t = std::set<const llvm::BasicBlock *>;
  using reverse_edge_t = std::map<const llvm::BasicBlock *, exits_t>;

  reverse_edge_t redges;

  // setup exits as keys
  for (auto &e : edges)
    redges.emplace(std::make_pair(e.second, exits_t{}));

  // associate exits to exiting blocks
  for (auto &e : edges)
    auto found = redges[e.second].insert(e.first);

  // remove exits that have only one exiting predecessor
  auto rit = redges.begin();
  while (rit != redges.end())
    if (rit->second.size() == 1)
      rit = redges.erase(rit);
    else
      ++rit;

  DEBUG_MSG("shared loop exits\n");
  DEBUG_CMD(std::for_each(redges.begin(), redges.end(), [](const auto &e) {
    llvm::errs() << e.first->getName() << " -> ";
    for (const auto &k : e.second)
      llvm::errs() << k->getName() << " ";
    llvm::errs() << "\n";
  }));

  for (auto &e : redges) {
    hasChanged = true;
    auto *exit = const_cast<llvm::BasicBlock *>(e.first);

    for (auto &k : e.second) {
      auto *exiting = const_cast<llvm::BasicBlock *>(k);

      // create a new exit block that branches unconditionally to the exit
      // this way each exiting block will get its own unique exit
      auto *uniqueExit = llvm::BasicBlock::Create(
          exit->getContext(), "sle_unique_exit", exit->getParent(), exit);
      auto *br = llvm::BranchInst::Create(exit, uniqueExit);

      // change the terminator of exiting block to use the new exit
      exiting->getTerminator()->replaceUsesOfWith(exit, uniqueExit);

      // change the phis that use old exit to new one
      for (auto bi = exit->begin(); llvm::isa<llvm::PHINode>(bi);) {
        auto *phi = llvm::cast<llvm::PHINode>(bi++);

        auto i = 0;
        while ((i = phi->getBasicBlockIndex(exiting)) >= 0)
          phi->setIncomingBlock(i, uniqueExit);
      }
    }
  }

  return hasChanged;
}

//

SimplifyLoopExits::SimplifyLoopExits()
    : m_LI(nullptr), m_DT(nullptr), m_CurLoop(nullptr), m_Preheader(nullptr),
      m_Header(nullptr), m_OldHeader(nullptr), m_Latch(nullptr),
      m_OldLatch(nullptr) {}

bool SimplifyLoopExits::transform(llvm::Loop &CurLoop, llvm::LoopInfo &LI,
                                  llvm::DominatorTree *DT) {
  if (isLoopExitSimplifyForm(CurLoop))
    return false;

  init(CurLoop, LI, DT);

  if (UniquifyLoopExits(CurLoop)) {
    // TODO update dom tree manually
    // TODO new exit blocks need to belong to the closest outer loop in the case
    // of nested loops
    if (m_DT)
      m_DT->recalculate(*(m_Header->getParent()));

    updateExitEdges();
  }

  for (auto bi = m_Header->begin(); llvm::isa<llvm::PHINode>(bi);) {
    auto *phi = llvm::cast<llvm::PHINode>(bi++);

    llvm::DemotePHIToStack(phi, m_Preheader->getTerminator());
  }

  std::set<llvm::Instruction *> toDemote;

  // find all defs defined in the loop and used outside of it
  std::set<llvm::BasicBlock *> blocks(m_CurLoop->getBlocks().begin(),
                                      m_CurLoop->getBlocks().end());
  FindDefsInBlocksNotUsedIn(blocks, blocks, toDemote);

  auto *exitFlag = createExitFlag();
  auto *exitFlagVal = setExitFlag(true, exitFlag, m_Preheader->getTerminator());

  createLatch();

  auto *exitSwitch = createExitSwitch();
  unified_exit_case_type initCase = 0;
  auto *exitSwitchVal =
      setExitSwitch(initCase, exitSwitch, m_Preheader->getTerminator());

  exit_value_map_t exitValueMap;
  m_UnifiedExit = createUnifiedExit(exitSwitch, exitValueMap);

  auto oldHeader = createHeader(exitFlag, m_UnifiedExit);

  attachExitValues(exitFlag, exitSwitch, exitValueMap);
  redirectExitingBlocksToLatch();

  for (auto &e : toDemote)
    llvm::DemoteRegToStack(*e, false, m_Preheader->getTerminator());

  if (m_DT)
    m_DT->recalculate(*(m_Header->getParent()));

  assert((m_CurLoop->isLoopSimplifyForm() ||
          dumpFunction(m_Header->getParent())) &&
         "Pass did not preserve loop canonical form as expected!");

  assert((isLoopExitSimplifyForm(*m_CurLoop) ||
          dumpFunction(m_Header->getParent())) &&
         "Pass did not transform loop as expected!");

  return true;
}

llvm::Value *SimplifyLoopExits::createExitFlag() {
  auto *flagType = llvm::IntegerType::get(m_Header->getContext(), 1);
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
SimplifyLoopExits::createUnifiedExit(llvm::Value *ExitSwitch,
                                     exit_value_map_t &exitValueMap) {
  auto &curCtx = m_Header->getContext();
  auto *defaultExit = getExitCondition(*m_CurLoop, m_Edges[0].first).second;

  exitValueMap.emplace(defaultExit, DefaultCase);

  auto *unifiedExit =
      llvm::BasicBlock::Create(curCtx, "sle_exit", m_Preheader->getParent());

  auto *exitSwitchVal =
      new llvm::LoadInst(ExitSwitch, "sle_switch", unifiedExit);

  auto *sleBr = llvm::SwitchInst::Create(exitSwitchVal, defaultExit,
                                         m_Edges.size(), unifiedExit);

  unified_exit_case_type caseIdx = DefaultCase + 1;

  for (auto eit = m_Edges.begin(), ee = m_Edges.end(); eit != ee; ++eit) {
    if (eit->second == defaultExit)
      continue;

    exitValueMap.emplace(eit->second, caseIdx);

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

  auto *term = m_Latch->getTerminator();

  if (term->getNumSuccessors() > 1) {
    auto *clonedTerm = llvm::cast<llvm::TerminatorInst>(term->clone());
    auto *succ0 = clonedTerm->getSuccessor(0);
    auto *succ1 = clonedTerm->getSuccessor(1);

    clonedTerm->setSuccessor(m_Header == succ0 ? 0 : 1, m_Latch);
    m_OldLatch->getTerminator()->eraseFromParent();
    m_OldLatch->getInstList().push_back(clonedTerm);

    m_Latch->getTerminator()->eraseFromParent();
    llvm::BranchInst::Create(m_Header, m_Latch);
  }

  updateExitEdges();

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
                                         llvm::Value *ExitSwitch,
                                         const exit_value_map_t &exitValueMap) {
  unified_exit_case_type caseVal = DefaultCase;

  for (auto &e : m_Edges) {
    if (e.first == m_Header)
      continue;

    auto exitCond = getExitCondition(*m_CurLoop, e.first).first;

    auto *term = const_cast<llvm::TerminatorInst *>(e.first->getTerminator());
    assert(term->getNumSuccessors() <= 2 &&
           "Loop exiting block with more than 2 successors is not supported!");

    auto *br = llvm::dyn_cast<llvm::BranchInst>(term);
    assert((br || dumpFunction(m_Header->getParent())) &&
           "Loop exiting block terminator must be a branch instruction!");

    auto cond1FlagVal = exitCond ? false : true;
    auto cond2FlagVal = !cond1FlagVal;

    auto *flagType =
        llvm::dyn_cast<llvm::AllocaInst>(ExitFlag)->getAllocatedType();
    auto *cond1Flag = llvm::ConstantInt::get(flagType, cond1FlagVal);
    auto *cond2Flag = llvm::ConstantInt::get(flagType, cond2FlagVal);

    auto *selExitCond = llvm::SelectInst::Create(
        br->getCondition(), cond1Flag, cond2Flag, "sle_exit_cond", term);

    auto *flagStore = setExitFlag(selExitCond, ExitFlag, term);

    auto curCaseVal = exitValueMap.at(e.second);

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

void SimplifyLoopExits::redirectExitingBlocksToLatch() {
  assert(m_OldLatch && m_OldLatch != m_Latch &&
         "New latch seems to have not been attached yet!");

  for (auto e : m_Edges) {
    if (e.first == m_Header)
      continue;

    auto *exiting = const_cast<llvm::BasicBlock *>(e.first);
    auto ec = getExitCondition(*m_CurLoop, exiting);

    if (e.first == m_Latch) {
      auto *br = llvm::dyn_cast<llvm::BranchInst>(exiting->getTerminator());
      br->setSuccessor(!ec.first, m_Header);
      br->eraseFromParent();
      llvm::BranchInst::Create(m_Header, m_Latch);
    } else {
      auto *br = llvm::dyn_cast<llvm::BranchInst>(exiting->getTerminator());
      br->setSuccessor(!ec.first, m_Latch);
    }
  }

  updateExitEdges();

  return;
}

} // namespace icsa end
