
#include <memory>
// using std::unique_ptr

#include <map>
// using std::map

#include <cassert>
// using assert

#include <cstdlib>
// using std::abort

#include "llvm/IR/LLVMContext.h"
// using llvm::LLVMContext

#include "llvm/IR/Module.h"
// using llvm::Module

#include "llvm/IR/LegacyPassManager.h"
// using llvm::legacy::PassMananger

#include "llvm/Pass.h"
// using llvm::Pass
// using llvm::PassInfo

#include "llvm/Analysis/LoopInfo.h"
// using llvm::LoopInfoWrapperPass
// using llvm::LoopInfo

#include "llvm/Support/SourceMgr.h"
// using llvm::SMDiagnostic

#include "llvm/AsmParser/Parser.h"
// using llvm::parseAssemblyString

#include "llvm/IR/Verifier.h"
// using llvm::verifyModule

#include "llvm/Support/ErrorHandling.h"
// using llvm::report_fatal_error

#include "llvm/Support/raw_ostream.h"
// using llvm::raw_string_ostream

#include "gtest/gtest.h"
// using testing::Test

#include "boost/variant.hpp"
// using boost::variant

#include "boost/variant/get.hpp"
// using boost::get

#include <string>
// using std::string

#include <exception>
// using std::exception

#include <cassert>
// using assert

#include "SimplifyLoopExits.hpp"

#ifdef BOOST_NO_EXCEPTIONS
namespace boost {
void throw_exception(std::exception const &e) { assert(true); }
}
#endif // BOOST_NO_EXCEPTIONS

namespace icsa {
namespace {

using test_result_t =
    boost::variant<bool, int, const char *, std::string, loop_exit_edge_t>;
using test_result_map = std::map<std::string, test_result_t>;

class TestSimplifyLoopExits : public testing::Test {
public:
  enum struct AssemblyHolderType : int { FILE_TYPE, STRING_TYPE };

  TestSimplifyLoopExits()
      : m_Module{nullptr}, m_TestDataDir{"./unittests/data/"} {}

  void ParseAssembly(
      const char *AssemblyHolder,
      const AssemblyHolderType asmHolder = AssemblyHolderType::FILE_TYPE) {
    llvm::SMDiagnostic err;

    if (AssemblyHolderType::FILE_TYPE == asmHolder) {
      std::string fullFilename{m_TestDataDir};
      fullFilename += AssemblyHolder;

      m_Module =
          llvm::parseAssemblyFile(fullFilename, err, llvm::getGlobalContext());
    } else {
      m_Module = llvm::parseAssemblyString(AssemblyHolder, err,
                                           llvm::getGlobalContext());
    }

    std::string errMsg;
    llvm::raw_string_ostream os(errMsg);
    err.print("", os);

    if (llvm::verifyModule(*m_Module, &(llvm::errs())))
      llvm::report_fatal_error("module verification failed\n");

    if (!m_Module)
      llvm::report_fatal_error(os.str().c_str());

    return;
  }

  void ExpectTestPass(const test_result_map &trm) {
    static char ID;

    struct UtilityPass : public llvm::FunctionPass {
      UtilityPass(const test_result_map &trm)
          : llvm::FunctionPass(ID), m_trm(trm) {}

      static int initialize() {
        auto *registry = llvm::PassRegistry::getPassRegistry();

        auto *PI = new llvm::PassInfo("Utility pass for unit tests", "", &ID,
                                      nullptr, true, true);

        registry->registerPass(*PI, false);
        llvm::initializeLoopInfoWrapperPassPass(*registry);

        return 0;
      }

      void getAnalysisUsage(llvm::AnalysisUsage &AU) const override {
        AU.addRequiredTransitive<llvm::LoopInfoWrapperPass>();

        return;
      }

      bool runOnFunction(llvm::Function &F) override {
        if (!F.hasName() || !F.getName().startswith("test"))
          return false;

        auto &LI = getAnalysis<llvm::LoopInfoWrapperPass>().getLoopInfo();

        auto *CurLoop = *LI.begin();
        assert(CurLoop && "Loop ptr is invalid");

        SimplifyLoopExits sle;
        const auto hdrExit = sle.getHeaderExit(*CurLoop);
        const bool doesHdrExitOnTrue = sle.getExitConditionValue(*CurLoop);
        const auto &loopExitEdges = sle.getEdges(*CurLoop);

        test_result_map::const_iterator found;

        // subcase
        found = lookup("header exit landing");
        if (found != std::end(m_trm)) {
          const auto &rv = hdrExit.first->getName();
          const auto &ev = boost::get<const char *>(found->second);
          EXPECT_EQ(ev, rv) << found->first;
        }

        // subcase
        found = lookup("header exit on true condition");
        if (found != std::end(m_trm)) {
          const auto &rv = doesHdrExitOnTrue;
          const auto &ev = boost::get<bool>(found->second);
          EXPECT_EQ(ev, rv) << found->first;
        }

        // subcase
        found = lookup("loop exit edge number");
        if (found != std::end(m_trm)) {
          const auto &rv = loopExitEdges.size();
          const auto &ev = boost::get<int>(found->second);
          EXPECT_EQ(ev, rv) << found->first;
        }

        return false;
      }

      test_result_map::const_iterator lookup(const std::string &subcase,
                                             bool fatalIfMissing = false) {
        auto found = m_trm.find(subcase);
        if (fatalIfMissing && m_trm.end() == found) {
          llvm::errs() << "subcase: " << subcase << " test data not found\n";
          std::abort();
        }

        return found;
      }

      const test_result_map &m_trm;
    };

    static int init = UtilityPass::initialize();
    (void)init; // do not optimize away

    auto *P = new UtilityPass(trm);
    llvm::legacy::PassManager PM;

    PM.add(P);
    PM.run(*m_Module);

    return;
  }

protected:
  std::unique_ptr<llvm::Module> m_Module;
  const char *m_TestDataDir;
};

TEST_F(TestSimplifyLoopExits, RegularLoop) {
  ParseAssembly("test101.ll");
  test_result_map trm;

  trm.insert({"header exit landing", "loop_exit_original"});
  trm.insert({"header exit on true condition", false});
  trm.insert({"loop exit edge number", 0});
  ExpectTestPass(trm);
}

TEST_F(TestSimplifyLoopExits, RegularLoopInvertedCond) {
  ParseAssembly("test101b.ll");
  test_result_map trm;

  trm.insert({"header exit landing", "loop_exit_original"});
  trm.insert({"header exit on true condition", true});
  trm.insert({"loop exit edge number", 0});
  ExpectTestPass(trm);
}

TEST_F(TestSimplifyLoopExits, RegularLoopWithPhiAtHeaderExit) {
  ParseAssembly("test102.ll");
  test_result_map trm;

  trm.insert({"header exit landing", "loop_exit_original"});
  trm.insert({"header exit on true condition", false});
  trm.insert({"loop exit edge number", 0});
  ExpectTestPass(trm);
}

TEST_F(TestSimplifyLoopExits, LoopWithBreakExit) {
  ParseAssembly("test103.ll");
  test_result_map trm;

  trm.insert({"header exit landing", "loop_exit_original"});
  trm.insert({"header exit on true condition", false});
  trm.insert({"loop exit edge number", 1});
  ExpectTestPass(trm);
}

} // namespace anonymous end
} // namespace icsa end
