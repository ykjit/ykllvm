//===- ModuleClone.cpp - Yk Module Cloning Pass ---------------------------===//
//
// This pass duplicates functions within the module, producing both optimised
// and unoptimised versions of these functions. The process creates two separate
// call paths that can be dynamically switched over based on the jit tracing
// mode.
//
// - **Cloning Strategy:**
//   All module functions are cloned into two versions. The handling differs
//   based on whether the function's address is taken:
//
//   - **Functions WITHOUT address taken:**
//     - Original function → marked as **optimised** (YK_SWT_OPT_MD metadata)
//     - Clone function → prefixed with `__yk_unopt_` and marked as
//     **unoptimised** (YK_SWT_UNOPT_MD metadata)
//
//   - **Functions WITH address taken:**
//     - Original function → marked as **unoptimised** (YK_SWT_UNOPT_MD
//     metadata)
//     - Clone function → prefixed with `__yk_opt_` and marked as **optimised**
//     (YK_SWT_OPT_MD metadata)
//
// - **Optimisation Intent:**
//   - **Unoptimised functions** are intended for tracing mode and will have
//     real `__yk_trace_basicblock` tracing calls that collect execution data.
//
//   - **Optimised functions** are intended for non-tracing mode and will have
//     dummy `__yk_trace_basicblock_dummy` calls that do nothing.
//
//   - Since dummy tracing calls are much cheaper than real tracing calls,
//     dynamic switching between these versions reduces the overhead when
//     tracing is not active.
//
// - **Call Graph Consistency:**
//   Optimised functions only call other optimised functions, unless the call is
//   indirect in which case it calls the unoptimised version.
//   Unoptimised functions only call other unoptimised functions.
//
//  Transformation examples can be seen in
//  llvm/test/Transforms/Yk/ModuleClone.ll
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

  /// Clones all eligible functions within the given module.
  ///
  /// For each eligible function, both address-taken and non-address-taken
  /// functions are cloned, but they are handled differently:
  /// - Non-address-taken: original stays optimised, clone becomes unoptimised
  /// - Address-taken: original becomes unoptimised, clone becomes optimised
  ///
  /// @param M The LLVM module containing the functions to be cloned.
  /// @return A map where the keys are the original function names and the
  ///         values are pointers to the cloned `Function` objects. Returns
  ///         a map if cloning succeeds, or `nullopt` if cloning fails.
  std::optional<std::map<std::string, Function *>>
  cloneFunctionsInModule(Module &M) {
    LLVMContext &Context = M.getContext();
    std::map<std::string, Function *> ClonedFuncs;
    for (llvm::Function &F : M) {
      // Skip external declarations.
      if (F.hasExternalLinkage() && F.isDeclaration()) {
        continue;
      }
      MDNode *MD = MDNode::get(Context, MDString::get(Context, "true"));
      // Skip already cloned functions.
      if (F.getMetadata(YK_SWT_OPT_MD) != nullptr ||
          F.getMetadata(YK_SWT_UNOPT_MD) != nullptr) {
        continue;
      }
      ValueToValueMapTy VMap;
      Function *ClonedFunc = CloneFunction(&F, VMap);
      if (ClonedFunc == nullptr) {
        Context.emitError("Failed to clone function: " + F.getName());
        return std::nullopt;
      }
      // Copy arguments
      auto DestArgIt = ClonedFunc->arg_begin();
      for (const Argument &OrigArg : F.args()) {
        DestArgIt->setName(OrigArg.getName());
        VMap[&OrigArg] = &*DestArgIt++;
      }
      // Handle cloning based on whether the function's address is taken.
      auto originalName = F.getName().str();
      if (F.hasAddressTaken()) {
        F.setMetadata(YK_SWT_UNOPT_MD, MD); // Mark original as unoptimised
        auto cloneName = Twine(YK_SWT_OPT_PREFIX) + originalName;
        ClonedFunc->setName(cloneName);
        ClonedFuncs[originalName] = ClonedFunc;
        ClonedFunc->setMetadata(YK_SWT_OPT_MD, MD); // Mark clone as optimised
      } else {
        F.setMetadata(YK_SWT_OPT_MD, MD); // Mark original as optimised
        auto cloneName = Twine(YK_SWT_UNOPT_PREFIX) + originalName;
        ClonedFunc->setName(cloneName);
        ClonedFuncs[originalName] = ClonedFunc;
        ClonedFunc->setMetadata(YK_SWT_UNOPT_MD,
                                MD); // Mark clone as unoptimised
      }
    }
    return ClonedFuncs;
  }

  /// Updates direct function calls within function `F` to maintain call graph
  /// consistency between optimised and unoptimised versions.
  /// This ensures that optimised functions only call other optimised functions,
  /// and unoptimised functions only call other unoptimised functions.
  ///
  /// @param F The function whose calls should be updated.
  /// @param OriginalFuncNameToClonedFuncMap Map from original function names to
  /// their cloned
  ///        versions.
  void updateCallsInFunction(
      Function *F,
      std::map<std::string, Function *> &OriginalFuncNameToClonedFuncMap) {
    for (BasicBlock &BB : *F) {
      for (Instruction &I : BB) {
        if (auto *Call = dyn_cast<CallInst>(&I)) {
          Function *CalledFunc = Call->getCalledFunction();
          // Update only direct calls.
          if (CalledFunc && !CalledFunc->isIntrinsic()) {
            std::string CalledName = CalledFunc->getName().str();
            auto Clone = OriginalFuncNameToClonedFuncMap.find(CalledName);
            if (Clone != OriginalFuncNameToClonedFuncMap.end()) {
              auto clonedFunc = Clone->second;
              // If this function is unoptimised and there's an unoptimised
              // version of the callee, use it
              if (F->getMetadata(YK_SWT_UNOPT_MD) != nullptr &&
                  clonedFunc->getMetadata(YK_SWT_UNOPT_MD) != nullptr) {
                Call->setCalledFunction(clonedFunc);
              }
              // If this function is optimised and there's an optimised version
              // of the callee, use it
              if (F->getMetadata(YK_SWT_OPT_MD) != nullptr &&
                  clonedFunc->getMetadata(YK_SWT_OPT_MD) != nullptr) {
                Call->setCalledFunction(clonedFunc);
              }
            }
          }
        }
      }
    }
  }

  bool runOnModule(Module &M) override {
    LLVMContext &Context = M.getContext();
    auto OriginalFuncNameToClonedFuncMap = cloneFunctionsInModule(M);
    if (!OriginalFuncNameToClonedFuncMap) {
      Context.emitError("Failed to clone functions in module");
      return false;
    }
    // Update function calls in all module functions.
    for (Function &F : M) {
      updateCallsInFunction(&F, *OriginalFuncNameToClonedFuncMap);
    }

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
