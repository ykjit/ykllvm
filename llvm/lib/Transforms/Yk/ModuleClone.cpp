//===- ModuleClone.cpp - Yk Module Cloning Pass ---------------------------===//
//
// This pass duplicates functions within the module, so that we can have
// optimised and unoptimised versions of functions.
//
// The cloned function (named with a `__yk_opt` prefix) is the optimised
// version. The original version is the unoptimised version.
//
// Once an optimisd clone has been created, we then rewrite all calls inside to
// call optimised versions of callees (where possible).
//
// Sometimes we are unable to clone a function. For example when a function has
// its address taken. Cloning such a function would break "function pointer
// identity", which the program may be relying on.
//===----------------------------------------------------------------------===//

#include "llvm/Transforms/Yk/ModuleClone.h"
#include "llvm/ADT/Twine.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/InitializePasses.h"
#include "llvm/Linker/Linker.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Yk/ControlPoint.h"
#include "llvm/YkIR/YkIRWriter.h"

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

  // Find all functions that have IR.
  std::vector<Function *> getFunctionsWithIR(Module &M) {
    std::vector<Function *> Funcs;
    for (llvm::Function &F : M) {
      // Skip declarations. They have no IR, so there's nothing to clone.
      if (F.isDeclaration()) {
        continue;
      }
      Funcs.push_back(&F);
    }
    return Funcs;
  }

  // Decide if we should clone a function.
  bool shouldClone(Function &F) {
    assert(!F.isDeclaration());

    if (F.isDeclaration()) {
      return false;
    }
    // If the address of a function is taken, then cloning it would break
    // function pointer identity, which the program may rely on.
    if (F.hasAddressTaken()) {
      return false;
    }
    // For now we don't clone functions that contain a call to the control
    // point.
    if (containsControlPoint(F)) {
      return false;
    }
    return true;
  }

  /// Clones all eligible functions within the given module.
  ///
  /// @param M The LLVM module containing the functions to be cloned.
  ///
  /// @return A map from the original function to the cloned function.
  /// Functions which were no cloned will have a nullptr value in the map.
  std::optional<std::map<Function *, Function *>>
  cloneFunctionsInModule(Module &M) {
    LLVMContext &Context = M.getContext();
    std::vector Funcs = getFunctionsWithIR(M);
    MDNode *OptMD = MDNode::get(Context, MDString::get(Context, ""));

    std::map<Function *, Function *> CloneMap;
    for (Function *F : Funcs) {
      auto OrigName = F->getName().str();

      // Maybe clone the function.
      Function *ClonedOptFunc = nullptr;
      if (shouldClone(*F)) {
        ValueToValueMapTy VMap;
        ClonedOptFunc = CloneFunction(F, VMap);
        if (ClonedOptFunc == nullptr) {
          Context.emitError("Failed to clone function: " + F->getName());
          return std::nullopt;
        }
        // Copy arguments
        auto DestArgIt = ClonedOptFunc->arg_begin();
        for (const Argument &OrigArg : F->args()) {
          DestArgIt->setName(OrigArg.getName());
          VMap[&OrigArg] = &*DestArgIt++;
        }
        // Promotions and calls to yk_debug_str are not required for optimised
        // clones and would only slow us down.
        removePromotionsAndDebugStrings(ClonedOptFunc);
        // The cloned function will be the optimised one.
        Twine OptName = Twine(YK_SWT_OPT_PREFIX) + OrigName;
        ClonedOptFunc->setName(OptName);
        ClonedOptFunc->setMetadata(YK_SWT_OPT_MD, OptMD);
      }
      // Update the map. If we didn't clone, then we insert a nullptr value.
      CloneMap[F] = ClonedOptFunc;
    }
    return CloneMap;
  }

  void removePromotion(CallBase *CB) {
    CallInst *CI = cast<CallInst>(CB);
    assert(CI->arg_size() == 1);
    Value *Arg = CI->getArgOperand(0);
    assert(CI->getType() == Arg->getType());
    CI->replaceAllUsesWith(Arg);
    CI->eraseFromParent();
  }

  void removePromotionsAndDebugStrings(Function *F) {
    // We have to do this in two phases to avoid iterator invalidation.
    //
    // 1. Find instructions to remove.
    std::vector<CallInst *> Promotions;
    std::vector<CallInst *> DebugStrs;
    for (BasicBlock &BB : *F) {
      for (Instruction &I : BB) {
        if (CallBase *CB = dyn_cast<CallBase>(&I)) {
          if (Function *CF = CB->getCalledFunction()) {
            if (CF->getName().startswith(YK_PROMOTE_PREFIX)) {
              Promotions.push_back(cast<CallInst>(CB));
            } else if (CF->getName() == YK_DEBUG_STR) {
              DebugStrs.push_back(cast<CallInst>(CB));
            }
          }
        }
      }
    }
    // 2. Remove instructions.
    for (CallInst *P : Promotions) {
      removePromotion(P);
    }
    for (CallInst *D : DebugStrs) {
      D->eraseFromParent();
    }
  }

  /// Updates direct calls in optimised clones so that they call optimised
  /// callees (if possible).
  ///
  /// @param CloneMap Maps unoptimised function to their optimised counterparts
  /// (if they were cloned).
  void updateCalls(std::map<Function *, Function *> &CloneMap) {

    // Update calls in the optimised function.
    for (auto [UnoptF, OptF] : CloneMap) {
      if (OptF == nullptr) {
        // We didn't clone that function. Skip.
        continue;
      }
      // Loop over the optimised function, replacing callees with optimised
      // versions where possible.
      for (BasicBlock &BB : *OptF) {
        for (Instruction &I : BB) {
          if (auto *Call = dyn_cast<CallInst>(&I)) {
            Function *CalledFunc = Call->getCalledFunction();
            // We can only update only direct calls.
            if (CalledFunc && !CalledFunc->isDeclaration() &&
                !CalledFunc->isIntrinsic()) {
              // Can we replace this call with a call to an optimised version?
              // If OptCallee is nullptr then we didn't cloned the callee.
              Function *OptCallee = CloneMap.at(CalledFunc);
              if (OptCallee != nullptr) {
                Call->setCalledFunction(OptCallee);
              }
            }
          }
        }
      }
    }
  }

  bool runOnModule(Module &M) override {
    LLVMContext &Context = M.getContext();
    auto CloneMap = cloneFunctionsInModule(M);
    if (!CloneMap) {
      Context.emitError("Failed to clone functions in module");
      return false;
    }
    updateCalls(*CloneMap);

    if (verifyModule(M, &errs())) {
      Context.emitError("Module verification failed!");
      return false;
    }

    return true;
  }
};
} // namespace

char YkModuleClone::ID = 0;

INITIALIZE_PASS(YkModuleClone, DEBUG_TYPE, "yk module clone", false, false)

ModulePass *llvm::createYkModuleClonePass() { return new YkModuleClone(); }
