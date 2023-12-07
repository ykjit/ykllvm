#include "llvm/Transforms/Yk/SoftwareTracer.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"

#define DEBUG_TYPE "yk-software-tracer-pass"

using namespace llvm;

namespace {

struct SoftwareTracerPass : public ModulePass {
  static char ID;
  Function *externalFunc = NULL;

  SoftwareTracerPass() : ModulePass(ID) {}

  bool runOnModule(Module &M) override {
    for (Module::iterator F = M.begin(), E = M.end(); F != E; ++F) {
      LLVMContext &Context = M.getContext();
      if (externalFunc == NULL) {
        FunctionType *FType =
            FunctionType::get(Type::getVoidTy(Context), {}, false);
        externalFunc = Function::Create(FType, GlobalVariable::ExternalLinkage,
                                        YK_TRACE_FUNCTION, M);
      }
      for (Function::iterator BB = F->begin(), E = F->end(); BB != E; ++BB) {
        for (BasicBlock::iterator BI = BB->begin(), BE = BB->end(); BI != BE;
             ++BI) {
          // Insert function call instruction
          IRBuilder<> builder(Context);
          builder.SetInsertPoint(&(*BI));
          builder.CreateCall(externalFunc);
        }
      }
    }
    return true;
  }
};
} // namespace
char SoftwareTracerPass::ID = 0;

ModulePass *llvm::createSoftwareTracerPass() {
  return new SoftwareTracerPass();
}
