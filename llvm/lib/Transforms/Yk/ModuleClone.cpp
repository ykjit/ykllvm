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

#define DEBUG_TYPE "yk-module-clone-pass"

using namespace llvm;

namespace llvm {
void initializeYkModuleClonePass(PassRegistry &);
} // namespace llvm

namespace {
struct YkModuleClone : public ModulePass {
  static char ID;
  const std::string ClonePrefix = "__yk_clone_";

  YkModuleClone() : ModulePass(ID) {
    initializeYkModuleClonePass(*PassRegistry::getPassRegistry());
  }

  void updateClonedFunctions(Module &M) {
    for (llvm::Function &F : M) {
      if (F.hasExternalLinkage() && F.isDeclaration()) {
        continue;
      }
      F.setName(ClonePrefix + F.getName().str());
    }
  }

  void updateClonedGlobals(Module &M) {
    for (llvm::GlobalVariable &GV : M.globals()) {
      std::string GlobalName = GV.getName().str();
      if (GlobalName.find('.') == 0) {
        continue; // This is likely not user-defined. Example: `.L.str`.
      }
      GV.setInitializer(nullptr); // Remove the initializer
      GV.setLinkage(llvm::GlobalValue::ExternalLinkage); // Set external linkage
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

    llvm::Linker TheLinker(M);
    if (TheLinker.linkInModule(std::move(Cloned))) {
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
