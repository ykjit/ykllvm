#include "llvm/Transforms/Yk/HelloWorldPass.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"

#define DEBUG_TYPE "yk-helloworld"

using namespace llvm;

namespace {
class HelloWorldPass : public ModulePass {
public:
  static char ID;
  HelloWorldPass() : ModulePass(ID) {
    
  }

  bool runOnModule(Module &M) override {
    return true;
  }
};
} // namespace

char HelloWorldPass::ID = 0;

ModulePass *llvm::createHelloWorldPass() { return new HelloWorldPass(); }
