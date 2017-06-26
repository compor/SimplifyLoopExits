//
//
//

#include "SimplifyLoopExits.hpp"

#include "llvm/IR/CFG.h"
// using llvm::succ_begin
// using llvm::succ_end

#include "llvm/IR/BasicBlock.h"
// using llvm::BasicBlock

#include "llvm/IR/DerivedTypes.h"
// using llvm::IntegerType

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

#include "llvm/Transforms/Utils/Local.h"
// using llvm::DemoteRegToStack

#include "llvm/Transforms/Utils/BasicBlockUtils.h"
// using llvm::SplitBlock

#include "llvm/Support/raw_ostream.h"
// using llvm::raw_ostream

#include <iterator>
// using std::iterator_traits

#include <type_traits>
// using std::enable_if
// using std::is_same

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

struct RedirectDependentPHIsVisitor
    : public llvm::InstVisitor<RedirectDependentPHIsVisitor> {
  llvm::BasicBlock &m_Old, &m_New;

  RedirectDependentPHIsVisitor(llvm::BasicBlock &Old, llvm::BasicBlock &New)
      : m_Old(Old), m_New(New) {}

  void visitPHI(llvm::PHINode &I) {
    auto numInc = I.getNumIncomingValues();

    for (decltype(numInc) i = 0; i < numInc; ++i)
      if (I.getIncomingBlock(i) == &m_Old)
        I.setIncomingBlock(i, &m_New);

    return;
  }
};

void DetectGeneratedValuesVisitor(const std::set<llvm::BasicBlock *> &Blocks,
                                  std::set<llvm::Instruction *> &Generated) {
  for (auto &bb : Blocks)
    for (auto &inst : *bb)
      for (auto *user : inst.users()) {
        auto *uinst = llvm::dyn_cast<llvm::Instruction>(user);
        if (uinst && Blocks.end() == Blocks.find(uinst->getParent())) {
          Generated.insert(&inst);
          break;
        }
      }

  return;
}

SimplifyLoopExits::SimplifyLoopExits(llvm::Loop &CurLoop, llvm::LoopInfo &LI)
    : m_CurLoop(CurLoop), m_LI(LI), m_PreHeader(nullptr), m_Header(nullptr),
      m_OldHeader(nullptr), m_Latch(nullptr) {
  assert(m_CurLoop.isLoopSimplifyForm() &&
         "Loop must be in loop simplify/canonical form!");

  m_PreHeader = m_CurLoop.getLoopPreheader();
  m_Header = m_CurLoop.getHeader();
  m_Latch = m_CurLoop.getLoopLatch();
  m_CurLoop.getExitEdges(m_Edges);

  return;
}

void SimplifyLoopExits::transform(void) {
  auto *exitFlag = createExitFlag();
  auto *exitFlagVal = setExitFlag(!getExitCondition(m_CurLoop).first, exitFlag,
                                  m_PreHeader->getTerminator());

  createLatch();

  auto *exitSwitch = createExitSwitch();
  unified_exit_case_type initCase = 0;
  auto *exitSwitchVal =
      setExitSwitch(initCase, exitSwitch, m_PreHeader->getTerminator());
  auto *sleExit = createUnifiedExit(exitSwitch);

  auto oldHeader = createHeader(exitFlag, sleExit);
  attachExitValues(exitFlag, exitSwitch);
  // redirectExitsToLatch();

  std::error_code ec;
  llvm::raw_fd_ostream dbg("dbg.ll", ec, llvm::sys::fs::F_Text);
  m_Header->getParent()->print(dbg);

  return;
}

llvm::Value *SimplifyLoopExits::createExitFlag() {
  auto *hdrBranch = llvm::dyn_cast<llvm::BranchInst>(m_Header->getTerminator());
  assert(hdrBranch && "Loop header terminator must be a branch instruction!");

  auto *flagType = hdrBranch->isConditional()
                       ? hdrBranch->getCondition()->getType()
                       : llvm::IntegerType::get(hdrBranch->getContext(),
                                                unified_exit_case_type_bits);
  auto *flagAlloca = new llvm::AllocaInst(flagType, nullptr, "sle_flag",
                                          m_PreHeader->getTerminator());

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
  auto *hdrExit = getExitCondition(m_CurLoop, m_Header).second;

  auto *unifiedExit =
      llvm::BasicBlock::Create(curCtx, "sle_exit", m_PreHeader->getParent());

  auto *exitSwitchVal =
      new llvm::LoadInst(ExitSwitch, "sle_switch", unifiedExit);

  auto *sleBr = llvm::SwitchInst::Create(exitSwitchVal, hdrExit, m_Edges.size(),
                                         unifiedExit);

  std::set<llvm::BasicBlock *> exits;
  for (auto &e : m_Edges)
    exits.insert(const_cast<llvm::BasicBlock *>(e.second));

  std::set<llvm::Instruction *> generated;
  std::set<llvm::BasicBlock *> blocks(m_CurLoop.getBlocks().begin(),
                                      m_CurLoop.getBlocks().end());
  DetectGeneratedValuesVisitor(blocks, generated);

  // remove header phi because they are preserved
  for (auto &e : *m_Header) {
    auto *phi = llvm::dyn_cast<llvm::PHINode>(&e);
    if (!phi)
      break;
    generated.erase(phi);
  }

  for (auto &e : generated)
    llvm::DemoteRegToStack(*e, false, m_PreHeader->getTerminator());

  unified_exit_case_type caseIdx = DefaultCase + 1;

  for (auto eit = m_Edges.begin(), ee = m_Edges.end(); eit != ee; ++eit) {
    if (eit->second == hdrExit)
      continue;

    auto *caseVal = llvm::ConstantInt::get(
        llvm::IntegerType::get(curCtx, unified_exit_case_type_bits), caseIdx++);
    sleBr->addCase(caseVal, const_cast<llvm::BasicBlock *>(eit->second));
  }

  return unifiedExit;
}

llvm::BasicBlock *
SimplifyLoopExits::createHeader(llvm::Value *ExitFlag,
                                llvm::BasicBlock *UnifiedExit) {
  auto *splitPt = m_Header->getFirstNonPHI();
  m_Header->setName("sle_header");

  // TODO update dominance information
  m_OldHeader = llvm::SplitBlock(m_Header, splitPt, nullptr, &m_LI);
  m_OldHeader->setName("old_header");

  // update exit edges
  m_Edges.emplace_back(m_OldHeader,
                       getExitCondition(m_CurLoop, m_OldHeader).second);

  auto *exitFlagVal =
      new llvm::LoadInst(ExitFlag, "sle_flag", m_Header->getTerminator());

  if (UnifiedExit) {
    auto *hdrBr = llvm::BranchInst::Create(
        m_OldHeader, UnifiedExit, exitFlagVal, m_Header->getTerminator());
    m_Header->getTerminator()->eraseFromParent();

    m_CurLoop.addBasicBlockToLoop(UnifiedExit, m_LI);

    // update exit edges
    auto found =
        std::find_if(m_Edges.begin(), m_Edges.end(), [this](const auto &e) {
          return e.first == m_Header ? true : false;
        });

    if (found != m_Edges.end())
      m_Edges.erase(found);

    m_Edges.emplace_back(m_Header, UnifiedExit);
  }

  return m_Header;
}

llvm::BasicBlock *SimplifyLoopExits::createLatch() {
  auto &curCtx = m_Header->getContext();

  auto sleLatch =
      llvm::BasicBlock::Create(curCtx, "sle_latch", m_Header->getParent());

  auto *sleLatchBr = llvm::BranchInst::Create(m_Header, sleLatch);
  auto *latchBr = llvm::dyn_cast<llvm::BranchInst>(m_Latch->getTerminator());
  latchBr->setSuccessor(0, sleLatch);

  RedirectDependentPHIsVisitor rlldpVisitor{*m_Latch, *sleLatch};
  rlldpVisitor.visit(m_Header);

  m_CurLoop.addBasicBlockToLoop(sleLatch, m_LI);
  m_Latch = sleLatch;

  return sleLatch;
}

llvm::Value *SimplifyLoopExits::createExitSwitch() {
  auto *caseType = llvm::IntegerType::get(m_PreHeader->getContext(),
                                          unified_exit_case_type_bits);
  auto *caseAlloca = new llvm::AllocaInst(caseType, nullptr, "sle_switch",
                                          m_PreHeader->getTerminator());

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

    auto exitCond = getExitCondition(m_CurLoop, e.first).first;

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

void SimplifyLoopExits::redirectExitsToLatch() {
  for (auto e : m_Edges) {
    if (e.first == m_Header)
      continue;

    auto ec = getExitCondition(m_CurLoop, e.first);

    auto *br = llvm::dyn_cast<llvm::BranchInst>(
        const_cast<llvm::BasicBlock *>(e.first)->getTerminator());
    br->setSuccessor(!ec.first, m_Latch);
  }

  // update exits
  m_Edges.clear();
  m_Edges.emplace_back(m_Header, m_Latch);

  return;
}

} // namespace icsa end

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
