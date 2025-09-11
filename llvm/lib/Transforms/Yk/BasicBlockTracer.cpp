#include "llvm/Transforms/Yk/BasicBlockTracer.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/Yk/ControlPoint.h"
#include "llvm/Transforms/Yk/ModuleClone.h"
#include "llvm/YkIR/YkIRWriter.h"
#define DEBUG_TYPE "yk-basicblock-tracer-pass"

using namespace llvm;

namespace llvm {
void initializeYkBasicBlockTracerPass(PassRegistry &);
} // namespace llvm

namespace {
struct YkBasicBlockTracer : public ModulePass {
  static char ID;

  YkBasicBlockTracer() : ModulePass(ID) {
    initializeYkBasicBlockTracerPass(*PassRegistry::getPassRegistry());
  }

  bool runOnModule(Module &M) override {
    LLVMContext &Context = M.getContext();
    // Create externally linked function declaration:
    //   void __yk_trace_basicblock(int functionIndex, int blockIndex)
    Type *ReturnType = Type::getVoidTy(Context);
    Type *FunctionIndexArgType = Type::getInt32Ty(Context);
    Type *BlockIndexArgType = Type::getInt32Ty(Context);

    FunctionType *FType = FunctionType::get(
        ReturnType, {FunctionIndexArgType, BlockIndexArgType}, false);

    // Trace function is used to trace the execution of the program.
    Function *TraceFunc = Function::Create(
        FType, GlobalVariable::ExternalLinkage, YK_TRACE_FUNCTION, M);

    IRBuilder<> builder(Context);
    uint32_t FunctionIndex = 0;
    for (auto &F : M) {
      // If we won't ever trace this, don't insert calls to the tracer, as it
      // would only slow us down.
      if ((F.hasFnAttribute(YK_OUTLINE_FNATTR)) && (!containsControlPoint(F))) {
        FunctionIndex++;
        continue;
      }
      // Don't insert basic block tracing calls into optimised clones.
      if (F.getMetadata(YK_SWT_OPT_MD)) {
        continue;
      }

      uint32_t BlockIndex = 0;
      for (auto &BB : F) {
        builder.SetInsertPoint(&*BB.getFirstInsertionPt());
        builder.CreateCall(TraceFunc, {builder.getInt32(FunctionIndex),
                                       builder.getInt32(BlockIndex)});

        assert(BlockIndex != UINT32_MAX &&
               "Expected BlockIndex to not overflow");
        BlockIndex++;
      }
      assert(FunctionIndex != UINT32_MAX &&
             "Expected FunctionIndex to not overflow");
      FunctionIndex++;
    }
    return true;
  }
};
} // namespace

char YkBasicBlockTracer::ID = 0;

INITIALIZE_PASS(YkBasicBlockTracer, DEBUG_TYPE, "yk basicblock tracer", false,
                false)

ModulePass *llvm::createYkBasicBlockTracerPass() {
  return new YkBasicBlockTracer();
}
