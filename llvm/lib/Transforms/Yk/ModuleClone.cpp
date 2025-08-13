//===- ModuleClone.cpp - Yk Module Cloning Pass ---------------------------===//
//
// This pass duplicates functions within the module, producing both the
// original (optimised) and cloned (unoptimised) versions of these
// functions. The process is as follows:
//
// - **Cloning Criteria:**
//   - Functions **without** their address taken are cloned. This results in two
//     versions of such functions in the module: the original and the cloned
//     version.
//
//   - Functions **with** their address taken remain only in their original form
//     and are not cloned. These functions are not cloned because then we would
//     need to update their references in the callers to point to the newly
//     cloned function. Since function pointers can be stored in variables,
//     passed as arguments, etc. tracking and updating all these references is
//     feasible but complex.
//
// - **Cloned Function Naming:**
//   - The cloned functions are renamed by adding the prefix `__yk_unopt_` to
//     their original names. This distinguishes them from the original
//     functions.
//
// - **Optimisation Intent:**
//   - The **cloned functions** (those with the `__yk_unopt_` prefix) are
//     intended to be the **unoptimised versions** of the functions. These
//     functions will have the real __yk_trace_basicblock tracing calls.
//
//   - The **original functions** remain **optimised**. These functions are
//     used when JIT is not tracing. These functions will have the dummy
//     __yk_trace_basicblock_dummy tracing calls that do nothing.
//
//   - Since __yk_trace_basicblock_dummy calls are much cheaper than
//     real __yk_trace_basicblock calls, dynamic transition between these
//     versions reduces the overhead of the SWT tracing.
//
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

  /// Clones eligible functions within the given module.
  ///
  /// This function iterates over all functions in the provided LLVM module `M`
  /// and clones those that meet the following criteria:
  ///
  /// - The function does **not** have external linkage and is **not** a
  /// declaration.
  /// - The function's address is **not** taken.
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

      // Function with address taken are not cloned.
      // Add metadata to the function to indicate that it has address taken.
      // This metadata is used in other passes like `BasicBlockTracer`.
      if (F.hasAddressTaken()) {
        MDNode *MD = MDNode::get(Context, MDString::get(Context, "true"));
        F.setMetadata(YK_SWT_MODCLONE_FUNC_ADDR_TAKEN, MD);
        continue;
      }
      // Skip already cloned functions or functions with address taken.
      if (F.getName().startswith(YK_SWT_UNOPT_PREFIX)) {
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
      // Rename function
      auto originalName = F.getName().str();
      auto cloneName = Twine(YK_SWT_UNOPT_PREFIX) + originalName;
      ClonedFunc->setName(cloneName);
      ClonedFuncs[originalName] = ClonedFunc;
    }
    return ClonedFuncs;
  }

  /// Replace all function calls within function `F` with calls to unoptimised
  /// functions, if such a function exists.
  ///
  /// @param F The function whose calls should be updated.
  /// @param ClonedFuncs Map from original function names to their cloned
  ///        versions.
  void updateCallsInFunction(Function *F,
                             std::map<std::string, Function *> &ClonedFuncs) {
    for (BasicBlock &BB : *F) {
      for (Instruction &I : BB) {
        if (auto *Call = dyn_cast<CallInst>(&I)) {
          Function *CalledFunc = Call->getCalledFunction();
          if (CalledFunc && !CalledFunc->isIntrinsic()) {
            std::string CalledName = CalledFunc->getName().str();
            auto It = ClonedFuncs.find(CalledName);
            if (It != ClonedFuncs.end()) {
              Call->setCalledFunction(It->second);
            }
          }
        }
      }
    }
  }

  /// Updates calls in module functions.
  ///
  /// Updates calls in unoptimised functions:
  ///  1. Functions with __yk_unopt_ prefix.
  ///  2. Functions which address is taken.
  ///
  /// This ensures all unoptimised functions call unoptimised versions of other
  /// functions.
  ///
  /// @param M The LLVM module containing the functions.
  /// @param ClonedFuncs Map from original function names to their cloned
  ///        versions.
  void updateFunctionCalls(Module &M,
                           std::map<std::string, Function *> &ClonedFuncs) {
    // Update calls within cloned functions
    for (auto &Entry : ClonedFuncs) {
      Function *ClonedFunc = Entry.second;
      updateCallsInFunction(ClonedFunc, ClonedFuncs);
    }

    // Update calls within address-taken functions, since they are
    // also treated as unoptimised functions that should call unoptimised
    // versions of other functions
    for (Function &F : M) {
      if (F.getMetadata(YK_SWT_MODCLONE_FUNC_ADDR_TAKEN) != nullptr) {
        updateCallsInFunction(&F, ClonedFuncs);
      }
    }
  }

  bool runOnModule(Module &M) override {
    LLVMContext &Context = M.getContext();
    auto clonedFunctions = cloneFunctionsInModule(M);
    if (!clonedFunctions) {
      Context.emitError("Failed to clone functions in module");
      return false;
    }
    updateFunctionCalls(M, *clonedFunctions);

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