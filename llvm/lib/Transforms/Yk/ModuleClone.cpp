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

  void printModule(Module &M) {
    // M.print(llvm::outs(), nullptr);
    llvm::outs() << "\nModule:" << M.getName() << "\n";
    for (llvm::Function &F : M) {
      llvm::outs() << "(Clone) Function name: " << F.getName() << "\n";
    }
    for (llvm::GlobalVariable &GV : M.globals()) {
      llvm::outs() << "Global variable: " << GV.getName()
                   << " - Linkage: " << GV.getLinkage() << "\n";
    }
  }

  void clone(Module &M) {
    std::unique_ptr<Module> ClonedModule = CloneModule(M);
    if (!ClonedModule) {
      errs() << "Error: CloneModule returned null for module: " << M.getName() << "\n";
      return;
    }
    std::string ClonedModuleName = M.getName().str();
    ClonedModuleName.append("_ykclone");
    ClonedModule->setModuleIdentifier(ClonedModuleName);
    for (llvm::Function &F : *ClonedModule) {
      std::string FuncName = F.getName().str();
      if (F.hasExternalLinkage() && F.isDeclaration()) {
        continue;
      }
      std::string NewName = F.getName().str() + ".cloned";
      F.setName(NewName);
    }

    // Set globals to be external
    for (llvm::GlobalVariable &GV : ClonedModule->globals()) {
      std::string GlobalName = GV.getName().str();
      if (GlobalName.find('.') == 0) {
        continue; // This is likely not user-defined. Example: `.L.str`.
      }
      GV.setInitializer(nullptr); // Remove the initializer
      GV.setLinkage(llvm::GlobalValue::ExternalLinkage); // Set external linkage
    }

    // Link the modules
    llvm::Linker TheLinker(M);
    if (TheLinker.linkInModule(std::move(ClonedModule))) {
      llvm::errs() << "@@@@@@@@@@@@ Error: Linking the cloned module back into the original "
                      "module failed.\n";
    } else {
      llvm::outs() << "@@@@@@@@@@@@ Successfully linked the cloned module back into the "
                      "original module.\n";
    }
    printModule(M);
    printModule(*ClonedModule);
  }

  bool runOnModule(Module &M) override {
    // if (M.getName() == "src/perf/collect.c") {
    //   dbgs() << "@@@@ Skipping YkModuleClone on module: " << M.getName() << "\n";
    //   return false;
    // }
    // dbgs() << "@@@@ Running YkModuleClone on module: " << M.getName() << "\n";
    // clone(M);
    
    return true;
  }
};
} // namespace

char YkModuleClone::ID = 0;

INITIALIZE_PASS(YkModuleClone, DEBUG_TYPE, "yk module clone", false, false)

ModulePass *llvm::createYkModuleClone() { return new YkModuleClone(); }
