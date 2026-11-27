// HoistExtractValues.cpp
//
// yk deopt expects each live variable to be a single value, it cannot
// deoptimise an aggregate (e.g. a struct returned in two registers).
// This pass moves each `extractvalue` to just after the aggregate, so that
// stackmaps track the extracted fields (each a single value) instead of the
// aggregate itself.
//
// Before:
//
//   %r = call {i64, i32} @find(...)
//   %p = extractvalue {i64, i32} %r, 0
//   br i1 %cond, ...                        ; stackmap: %r
//   ...
//   %flags = extractvalue {i64, i32} %r, 1
//
// After:
//
//   %r = call {i64, i32} @find(...)
//   %p = extractvalue {i64, i32} %r, 0
//   %flags = extractvalue {i64, i32} %r, 1
//   br i1 %cond, ...                        ; stackmap: %p, %flags

#include "llvm/Transforms/Yk/HoistExtractValues.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"

#include <vector>

#define DEBUG_TYPE "yk-hoist-extractvalues"

using namespace llvm;

namespace llvm {
void initializeYkHoistExtractValuesPass(PassRegistry &);
} // namespace llvm

namespace {

class YkHoistExtractValues : public ModulePass {
public:
  static char ID;
  YkHoistExtractValues() : ModulePass(ID) {
    initializeYkHoistExtractValuesPass(*PassRegistry::getPassRegistry());
  }

  bool runOnModule(Module &M) override {
    bool Changed = false;
    for (Function &F : M) {
      std::vector<ExtractValueInst *> EVs;
      for (Instruction &I : instructions(F))
        if (ExtractValueInst *EV = dyn_cast<ExtractValueInst>(&I))
          EVs.push_back(EV);

      for (ExtractValueInst *EV : reverse(EVs)) {
        Instruction *Agg = dyn_cast<Instruction>(EV->getAggregateOperand());
        if (!Agg || Agg->isTerminator())
          continue;
        // LLVM requires a block's phis to be grouped at its start, so we move
        // the `extractvalue` to after the last phi.
        if (isa<PHINode>(Agg))
          EV->moveBefore(Agg->getParent()->getFirstInsertionPt());
        else
          EV->moveAfter(Agg);
        Changed = true;
      }
    }
    return Changed;
  }
};
} // namespace

char YkHoistExtractValues::ID = 0;
INITIALIZE_PASS(YkHoistExtractValues, DEBUG_TYPE, "yk hoist extractvalues",
                false, false)

ModulePass *llvm::createYkHoistExtractValuesPass() {
  return new YkHoistExtractValues();
}
