#include "llvm/Transforms/Yk/ModuleClone.h"
#include "llvm/ADT/Twine.h"
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
#include "llvm/IR/Verifier.h"

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
    for (llvm::Function &F : M) {
      if (F.hasExternalLinkage() && F.isDeclaration()) {
        continue;
      }
      // Skip functions that are address taken
      if (!F.hasAddressTaken()) {
        F.setName(Twine(YK_CLONE_PREFIX) + F.getName());
      }
    }
  }

  // This pass duplicates functions within the module:
  // the original and the cloned version.
  // - If a function's address is not taken, it is cloned, resulting in 
  //   two versions of that function in the module.
  // - If a function's address is taken, the function remains in its 
  //   original form.
  bool runOnModule(Module &M) override {
    std::unique_ptr<Module> Cloned = CloneModule(M);
    if (!Cloned) {
      errs() << "Error: CloneModule returned null for module: " << M.getName()
             << "\n";
      return false;
    }
    updateClonedFunctions(*Cloned);

    // The `OverrideFromSrc` flag instructs the linker to prioritize
    // definitions from the source module (the second argument) when
    // conflicts arise. This means that if two functions have the same
    // name in both the original and cloned modules, the version from
    // the cloned module will overwrite the original.
    if (Linker::linkModules(M, std::move(Cloned),
                            Linker::Flags::OverrideFromSrc)) {
      llvm::report_fatal_error("Error linking the modules");
      return false;
    }

    if (verifyModule(M, &errs())) {
      errs() << "Module verification failed!";
      return false;
    }

    return true;
  }
};
} // namespace

char YkModuleClone::ID = 0;

INITIALIZE_PASS(YkModuleClone, DEBUG_TYPE, "yk module clone", false, false)

ModulePass *llvm::createYkModuleClonePass() { return new YkModuleClone(); }
