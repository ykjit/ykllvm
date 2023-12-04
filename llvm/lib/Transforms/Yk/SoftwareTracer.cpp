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

#define DEBUG_TYPE "yk-software-tracer"

using namespace llvm;

namespace {
// void my_function() { printf("Hello from your_c_function!\n"); }

struct SoftwareTracerPass : public ModulePass {
  static char ID;
  Function *monitor;

  SoftwareTracerPass() : ModulePass(ID) {}

  bool runOnModule(Module &M) override {
    for (Module::iterator F = M.begin(), E = M.end(); F != E; ++F) {
      llvm::StringRef ref = std::string("record_trace");
      Function *calleeFunction =
          M.getFunction(ref); // Find the function by name

      for (Function::iterator BB = F->begin(), E = F->end(); BB != E; ++BB) {
        for (BasicBlock::iterator BI = BB->begin(), BE = BB->end(); BI != BE;
             ++BI) {
          if (calleeFunction) {
            Argument *TempArg = F->getArg(0); // temp argument
            Value *args[] = {TempArg};        // BI->getName()

            Instruction *newInst = CallInst::Create(calleeFunction, args);

            BasicBlock *BB = BI->getParent();
            // TODO: understand how to insert new instruction
            // Instruction::insertInto(BB, newInst); // works with iterator
            // BB->getInstList().insert(BI, newInst); // deal with getInstList
            // being private errs() << "Inserted the function!\n";
          }
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
