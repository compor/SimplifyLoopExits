//
//
//

#include "SimplifyLoopExits.hpp"

#include "llvm/ADT/SmallVector.h"
// using llvm::SmallVector

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

#include "llvm/Support/raw_ostream.h"
// using llvm::raw_ostream

#include <iterator>
// using std::iterator_traits

#include <type_traits>
// using std::enable_if
// using std::is_same

#include <set>
// using std::set

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

struct RedirectLoopLatchDependentPHIsVisitor
    : public llvm::InstVisitor<RedirectLoopLatchDependentPHIsVisitor> {
  llvm::BasicBlock &m_OldLatch, &m_NewLatch;

  RedirectLoopLatchDependentPHIsVisitor(llvm::BasicBlock &OldLatch,
                                        llvm::BasicBlock &NewLatch)
      : m_OldLatch(OldLatch), m_NewLatch(NewLatch) {}

  void visitPHI(llvm::PHINode &I) {
    auto numInc = I.getNumIncomingValues();

    for (decltype(numInc) i = 0; i < numInc; ++i)
      if (I.getIncomingBlock(i) == &m_OldLatch)
        I.setIncomingBlock(i, &m_NewLatch);

    return;
  }
};

struct LoopExitDependentPHIVisitor
    : public llvm::InstVisitor<LoopExitDependentPHIVisitor> {
  std::set<llvm::PHINode *> m_LoopExitPHINodes;
  const llvm::Loop &m_CurLoop;
  loop_exit_target_t m_LoopExitTargets;

  LoopExitDependentPHIVisitor(const llvm::Loop &CurLoop,
                              const loop_exit_target_t &LoopExitTargets)
      : m_CurLoop(CurLoop), m_LoopExitTargets(LoopExitTargets) {
    for (auto bi = llvm::succ_begin(CurLoop.getHeader()),
              be = llvm::succ_end(CurLoop.getHeader());
         bi != be; ++bi)
      if (!m_CurLoop.contains(*bi))
        m_LoopExitTargets.insert(*bi);

    return;
  }

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

SimplifyLoopExits::SimplifyLoopExits(llvm::Loop &CurLoop)
    : m_CurLoop(CurLoop), m_PreHeader(nullptr), m_Header(nullptr),
      m_Latch(nullptr) {
  assert(m_CurLoop.isLoopSimplifyForm() &&
         "Loop must be in loop simplify/canonical form!");

  m_PreHeader = m_CurLoop.getLoopPreheader();
  m_Header = m_CurLoop.getHeader();
  m_Latch = m_CurLoop.getLoopLatch();

  return;
}

void SimplifyLoopExits::transform(void) {
  auto *exitFlag = createExitFlag();
  auto *exitFlagVal = setExitFlag(!getExitConditionValue(m_CurLoop), exitFlag,
                                  m_PreHeader->getTerminator());

  createLoopLatch();

  return;
}

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

bool SimplifyLoopExits::getExitConditionValue(
    const llvm::Loop &CurLoop, const llvm::BasicBlock *BB) const {
  auto term = BB ? BB->getTerminator() : CurLoop.getHeader()->getTerminator();

  assert(CurLoop.contains(term->getParent()) &&
         "Basic block must belong to loop!");

  if (!term->getNumSuccessors())
    return false;

  return !CurLoop.contains(term->getSuccessor(0));
}

loop_exit_edge_t SimplifyLoopExits::getEdges(const llvm::Loop &CurLoop) {
  assert(CurLoop.hasDedicatedExits() &&
         "Loop exit predecessors must belong only to the loop!");

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

llvm::Value *SimplifyLoopExits::createExitFlag() {
  auto *hdrBranch =
      llvm::dyn_cast<llvm::BranchInst>(m_Header->getTerminator());
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

llvm::Value *SimplifyLoopExits::createLoopHeader(llvm::Loop &CurLoop,
                                                 llvm::Value *LoopCond,
                                                 llvm::BasicBlock *Latch) {
  return nullptr;
}

llvm::BasicBlock *SimplifyLoopExits::createLoopLatch() {
  auto &curCtx = m_Header->getContext();

  auto sleLoopLatch =
      llvm::BasicBlock::Create(curCtx, "sle_latch", m_Header->getParent());

  auto *sleLatchBr = llvm::BranchInst::Create(m_Header, sleLoopLatch);
  auto *latchBr = llvm::dyn_cast<llvm::BranchInst>(m_Latch->getTerminator());
  latchBr->setSuccessor(0, sleLoopLatch);

  RedirectLoopLatchDependentPHIsVisitor rlldpVisitor{*m_Latch, *sleLoopLatch};
  rlldpVisitor.visit(m_Header);

  return sleLoopLatch;
}

llvm::Value *SimplifyLoopExits::attachExitFlag(llvm::Loop &CurLoop,
                                               llvm::Value *UnifiedExitFlag) {
  auto *loopPreHdr = CurLoop.getLoopPreheader();
  assert(loopPreHdr && "Loop is required to have a preheader!");

  if (!UnifiedExitFlag) {
    UnifiedExitFlag = createExitFlag();
    setExitFlag(!getExitConditionValue(CurLoop), UnifiedExitFlag,
                loopPreHdr->getTerminator());
  }

  // get branch instruction to use as instruction insertion point
  auto *hdrBranch =
      llvm::dyn_cast<llvm::BranchInst>(CurLoop.getHeader()->getTerminator());
  assert(hdrBranch && "Loop header terminator must be a branch instruction!");

  // load unified exit flag value
  auto *UnifiedExitFlagValue =
      new llvm::LoadInst(UnifiedExitFlag, "sle_flag_load", hdrBranch);

  if (!hdrBranch->isConditional())
    hdrBranch->setCondition(UnifiedExitFlagValue);
  else {
    // determine operation based on what boolean value exits the loop
    auto operationType = getExitConditionValue(CurLoop)
                             ? llvm::Instruction::BinaryOps::Or
                             : llvm::Instruction::BinaryOps::And;

    // attach the new exit flag condition to
    // the current loop header exit branch condition
    auto *unifiedCond = llvm::BinaryOperator::Create(
        operationType, hdrBranch->getCondition(), UnifiedExitFlagValue,
        "sle_cond", hdrBranch);

    hdrBranch->setCondition(unifiedCond);
  }

  assert(hdrBranch->isConditional() &&
         "Loop header branch must have a condition at this point!");
  return hdrBranch->getCondition();
}

llvm::Value *SimplifyLoopExits::createExitSwitchCond(llvm::Loop &CurLoop) {
  auto *loopPreHdr = CurLoop.getLoopPreheader();
  assert(loopPreHdr && "Loop is required to have a preheader!");

  auto *caseType = llvm::IntegerType::get(loopPreHdr->getContext(),
                                          unified_exit_case_type_bits);
  auto *caseAlloca = new llvm::AllocaInst(caseType, nullptr, "sle_switch",
                                          loopPreHdr->getTerminator());

  return caseAlloca;
}

llvm::Value *
SimplifyLoopExits::setExitSwitchCond(unified_exit_case_type Case,
                                     llvm::Value *ExitSwitchCond,
                                     llvm::Instruction *InsertBefore) {
  auto *caseVal = llvm::ConstantInt::get(
      llvm::IntegerType::get(InsertBefore->getContext(),
                             unified_exit_case_type_bits),
      Case);

  return setExitSwitchCond(caseVal, ExitSwitchCond, InsertBefore);
}

llvm::Value *
SimplifyLoopExits::setExitSwitchCond(llvm::Value *Case,
                                     llvm::Value *ExitSwitchCond,
                                     llvm::Instruction *InsertBefore) {
  return new llvm::StoreInst(Case, ExitSwitchCond, InsertBefore);
}

void SimplifyLoopExits::attachExitValues(llvm::Loop &CurLoop,
                                         llvm::Value *ExitFlag,
                                         llvm::Value *ExitSwitchCond,
                                         loop_exit_edge_t &LoopExitEdges) {
  // TODO denote default case value in a better way
  unified_exit_case_type caseVal = 0;

  for (auto &e : LoopExitEdges) {
    auto exitCond = getExitConditionValue(CurLoop, e.first);

    auto *term = e.first->getTerminator();
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

    ++caseVal;

    auto cond1CaseVal = exitCond ? caseVal : 0;
    auto cond2CaseVal = !exitCond ? caseVal : 0;

    auto *cond1Case = llvm::ConstantInt::get(
        llvm::IntegerType::get(term->getContext(), unified_exit_case_type_bits),
        cond1CaseVal);
    auto *cond2Case = llvm::ConstantInt::get(
        llvm::IntegerType::get(term->getContext(), unified_exit_case_type_bits),
        cond2CaseVal);

    auto *selSwitchCond = llvm::SelectInst::Create(
        br->getCondition(), cond1Case, cond2Case, "sle_switch_cond", term);

    setExitSwitchCond(selSwitchCond, ExitSwitchCond, term);
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

  std::set<llvm::Instruction *> ledRegisters;

  for (auto &phi : ledPHIVisitor.m_LoopExitPHINodes) {
    for (auto &e : phi->incoming_values()) {
      auto *reg = llvm::dyn_cast<llvm::Instruction>(e);
      if (reg && CurLoop.contains(reg->getParent()))
        ledRegisters.insert(reg);
    }
  }

  auto *loopPreHdr = CurLoop.getLoopPreheader();
  assert(loopPreHdr && "Loop is required to have a preheader!");

  for (auto &reg : ledRegisters)
    llvm::DemoteRegToStack(*reg, false, loopPreHdr->getTerminator());

  redirectLoopExitsToLatch(CurLoop, exitTargets.begin(), exitTargets.end());

  unified_exit_case_type caseIdx = 1;
  auto et = exitTargets.begin();

  for (; caseIdx <= exitTargets.size(); ++caseIdx, ++et) {
    auto *caseVal = llvm::ConstantInt::get(
        llvm::IntegerType::get(CurLoop.getHeader()->getContext(),
                               unified_exit_case_type_bits),
        caseIdx);
    sleSwitch->addCase(caseVal, *et);
  }

  // std::error_code ec;
  // llvm::raw_fd_ostream dbg("dbg.ll", ec, llvm::sys::fs::F_Text);
  // CurLoop.getHeader()->getParent()->print(dbg);

  return unifiedExit;
}

// private methods

template <typename ForwardIter>
void SimplifyLoopExits::redirectLoopExitsToLatch(llvm::Loop &CurLoop,
                                                 ForwardIter exitTargetStart,
                                                 ForwardIter exitTargetEnd) {
  auto *loopLatch = CurLoop.getLoopLatch();
  assert(loopLatch && "Loop is required to have a single loop latch!");

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
          brInst->setSuccessor(i, loopLatch);
      }
    }

  return;
}

} // namespace icsa end

// external functions

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
