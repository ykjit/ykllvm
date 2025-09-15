//===- MarkTraceableOptNone.cpp - Add `optnone` for the Yk JIT ---------===//
//
// This pass marks functions which could be traced with the `optnone` function
// attribute just before instruction selection begins. This is because yk can't
// get deal with functions that are isel/backend optimised.

#include "llvm/Transforms/Yk/MarkTraceableOptNone.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/Yk/ControlPoint.h"
#include "llvm/Transforms/Yk/ModuleClone.h"
#include "llvm/YkIR/YkIRWriter.h"

#define DEBUG_TYPE "yk-mark-traceable-optnone"

using namespace llvm;

namespace llvm {
void initializeYkMarkTraceableOptNonePass(PassRegistry &);
} // namespace llvm

namespace {
class YkMarkTraceableOptNone : public ModulePass {
public:
  static char ID;
  YkMarkTraceableOptNone() : ModulePass(ID) {
    initializeYkMarkTraceableOptNonePass(*PassRegistry::getPassRegistry());
  }

  bool runOnModule(Module &M) override {
    bool Changed = false;
    for (Function &F : M) {
      if (F.getMetadata(YK_SWT_OPT_MD)) {
        continue;
      }
      if ((F.hasFnAttribute(YK_OUTLINE_FNATTR)) && (!containsControlPoint(F))) {
        continue;
      }
      if (F.isDeclaration()) {
        continue;
      }

      F.addFnAttr(Attribute::OptimizeNone);
      F.addFnAttr(Attribute::NoInline);
      Changed = true;
    }
    return Changed;
  }
};
} // namespace

char YkMarkTraceableOptNone::ID = 0;
INITIALIZE_PASS(YkMarkTraceableOptNone, DEBUG_TYPE, "yk-markoptnone", false,
                false)

ModulePass *llvm::createYkMarkTraceableOptNonePass() {
  return new YkMarkTraceableOptNone();
}
