#include "llvm/Transforms/Yk/ModuleClone.h"
#include "llvm/Transforms/Yk/BasicBlockTracer.h"
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
      // Rename the function to avoid name conflicts
      F.setName(YK_CLONE_PREFIX + F.getName().str());
    }
  }

  void updateClonedGlobals(Module &M) {
    for (llvm::GlobalVariable &GV : M.globals()) {
      if (GV.getName().str().find('.') == 0) {
        continue; // This is likely not user-defined. Example: `.L.str`.
      }
      // Remove the initializer
      GV.setInitializer(nullptr);
      // Set external linkage
      GV.setLinkage(llvm::GlobalValue::ExternalLinkage);
    }
  }

  bool runOnModule(Module &M) override {
    std::unique_ptr<Module> Cloned = CloneModule(M);
    if (!Cloned) {
      errs() << "Error: CloneModule returned null for module: " << M.getName()
             << "\n";
      return false;
    }
    updateClonedFunctions(*Cloned);
    updateClonedGlobals(*Cloned);

    if (Linker::linkModules(M, std::move(Cloned))) {
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
