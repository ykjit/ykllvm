#include "llvm/Transforms/Yk/SoftwareTracer.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"

#define DEBUG_TYPE "yk-software-tracer"

using namespace llvm;

namespace {

struct SoftwareTracerPass : public ModulePass {
  static char ID;
  Function *monitor;

  SoftwareTracerPass() : ModulePass(ID) {}

  bool runOnModule(Module &M) override {
    // TODO: locate tracing function
    // llvm::StringRef ref = std::string("tracing_function");
    // Function *calleeFunction = M.getFunction(ref);

    for (Module::iterator F = M.begin(), E = M.end(); F != E; ++F) {
      for (Function::iterator BB = F->begin(), E = F->end(); BB != E; ++BB) {
        for (BasicBlock::iterator BI = BB->begin(), BE = BB->end(); BI != BE;
             ++BI) {
              // TODO: Inject tracing call
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
