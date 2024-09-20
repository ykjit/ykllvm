#include "llvm/Transforms/Yk/ModuleClone.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/InitializePasses.h"
#include "llvm/Linker/Linker.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Yk/BasicBlockTracer.h"

#define DEBUG_TYPE "yk-module-clone-pass"

using namespace llvm;

namespace llvm {
void initializeYkModuleClonePass(PassRegistry &);
} // namespace llvm

namespace {
struct YkModuleClone : public ModulePass {
  static char ID;

  YkModuleClone() : ModulePass(ID) {
    initializeYkModuleClonePass(*PassRegistry::getPassRegistry());
  }

  void updateClonedFunctions(Module &M) {
    std::vector<Function *> functionsToRemove;
    for (llvm::Function &F : M) {
      if (F.hasExternalLinkage() && F.isDeclaration()) {
        continue;
      }
      // Skip functions that are address taken
      if (!F.hasAddressTaken()) {
        // Rename the function. This will lead to having multiple
        // verisons of the same function in the linked module.
        F.setName(YK_CLONE_PREFIX + F.getName().str());
      }
    }
  }

  // This pass creates two versions for module functions.
  // original.
  // If function address is not taken, it is duplicated - resulting in
  // two versions of that function in a module. If function address is
  // taken, it remain in its original form.
  bool runOnModule(Module &M) override {
    std::unique_ptr<Module> Cloned = CloneModule(M);
    if (!Cloned) {
      errs() << "Error: CloneModule returned null for module: " << M.getName()
             << "\n";
      return false;
    }
    updateClonedFunctions(*Cloned);

    // The `OverrideFromSrc` flag tells the linker to
    // prefer definitions from the source module (the 2nd argument) when
    // there are conflicts. This means that if there are two functions
    // with the same name, the one from the Cloned module will be used.
    if (Linker::linkModules(M, std::move(Cloned),
                            Linker::Flags::OverrideFromSrc)) {
      llvm::report_fatal_error("Error linking the modules");
      return false;
    }
    return true;
  }
};
} // namespace

char YkModuleClone::ID = 0;

INITIALIZE_PASS(YkModuleClone, DEBUG_TYPE, "yk module clone", false, false)

ModulePass *llvm::createYkModuleClonePass() { return new YkModuleClone(); }
