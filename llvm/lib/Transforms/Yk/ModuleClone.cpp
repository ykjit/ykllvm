//===- ModuleClone.cpp - Yk Module Cloning Pass ---------------------------===//
//
// This pass duplicates functions within the module, so that we can have
// optimised and unoptimised versions of functions.
//
// The cloned function (named with a `__yk_opt` prefix) is the optimised
// version. The original version is the unoptimised version.
//
// Once an optimised clone has been created, we then rewrite all calls inside
// to call optimised versions of callees (where possible).
//
// All optimised clones are marked `yk_outline`.
//
// For functions whose address is taken we still clone them, but we can't
// simply remove the original (that would break function pointer identity).
// Instead we prepend a check to the original function: if we're not currently
// tracing, tail-call the optimised clone and return; otherwise fall through to
// the original (unoptimised) body so the tracer can observe it.
//
// Functions that contain the control point are never cloned.
//===----------------------------------------------------------------------===//

#include "llvm/Transforms/Yk/ModuleClone.h"
#include "llvm/ADT/Twine.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Linker/Linker.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Yk/BasicBlockTracer.h"
#include "llvm/Transforms/Yk/ControlPoint.h"
#include "llvm/YkIR/YkIRWriter.h"

#define DEBUG_TYPE "yk-module-clone-pass"

using namespace llvm;

// Find all functions that have IR.
std::vector<Function *> ModuleClonePass::getFunctionsWithIR(Module &M) {
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

// Returns true if the function should be cloned and have a tracing-state
// dispatch block prepended to its entry.
// Example:
//   f(args):
//     if __yk_thread_tracing_state == 0:
//       return __yk_opt_f(args)
//     <original body>
bool ModuleClonePass::isCloneableWithTracingCheckOptCall(Function &F) {
  assert(!F.isDeclaration());
  return !F.hasAvailableExternallyLinkage() && F.hasAddressTaken() &&
         !containsControlPoint(F) && !F.isVarArg() &&
         !F.hasFnAttribute(YK_OUTLINE_FNATTR);
}

// Returns true if the function should be cloned. Calls from within other opt
// clones are redirected to the clone.
bool ModuleClonePass::isCloneableOpt(Function &F) {
  assert(!F.isDeclaration());
  return !F.hasAvailableExternallyLinkage() && !F.hasAddressTaken() &&
         !containsControlPoint(F);
}

/// Clones all eligible functions within the given module.
///
/// @param M The LLVM module containing the functions to be cloned.
///
/// @return A map from the original function to the cloned function.
/// Functions which were not cloned will have a nullptr value in the map.
std::optional<std::map<Function *, Function *>>
ModuleClonePass::cloneFunctionsInModule(Module &M) {
  LLVMContext &Context = M.getContext();
  std::vector Funcs = getFunctionsWithIR(M);
  MDNode *OptMD = MDNode::get(Context, MDString::get(Context, ""));

  std::map<Function *, Function *> CloneMap;
  for (Function *F : Funcs) {
    Function *ClonedOptFunc = nullptr;
    if (isCloneableOpt(*F) || isCloneableWithTracingCheckOptCall(*F)) {
      auto OrigName = F->getName().str();
      ValueToValueMapTy VMap;
      ClonedOptFunc = CloneFunction(F, VMap);
      if (ClonedOptFunc == nullptr) {
        Context.emitError("Failed to clone function: " + F->getName());
        return std::nullopt;
      }
      auto DestArgIt = ClonedOptFunc->arg_begin();
      for (const Argument &OrigArg : F->args())
        (DestArgIt++)->setName(OrigArg.getName());
      removePromotionsAndDebugStrings(ClonedOptFunc);
      ClonedOptFunc->setName(Twine(YK_SWT_OPT_PREFIX) + OrigName);
      ClonedOptFunc->setMetadata(YK_SWT_OPT_MD, OptMD);
      if (!ClonedOptFunc->hasFnAttribute(Attribute::OptimizeNone))
        ClonedOptFunc->removeFnAttr(Attribute::NoInline);
      ClonedOptFunc->addFnAttr(YK_OUTLINE_FNATTR);
    }
    if (isCloneableWithTracingCheckOptCall(*F)) {
      prependTracingCheck(F, ClonedOptFunc, M);
    }
    // Update the map. If we didn't clone, then we insert a nullptr value.
    CloneMap[F] = ClonedOptFunc;
  }
  return CloneMap;
}

void ModuleClonePass::removePromotion(CallBase *CB) {
  CallInst *CI = cast<CallInst>(CB);
  assert(CI->arg_size() == 1);
  Value *Arg = CI->getArgOperand(0);
  assert(CI->getType() == Arg->getType());
  CI->replaceAllUsesWith(Arg);
  CI->eraseFromParent();
}

void ModuleClonePass::removePromotionsAndDebugStrings(Function *F) {
  // We have to do this in two phases to avoid iterator invalidation.
  //
  // 1. Find instructions to remove.
  std::vector<CallInst *> Promotions;
  std::vector<CallInst *> DebugStrs;
  for (BasicBlock &BB : *F) {
    for (Instruction &I : BB) {
      if (CallBase *CB = dyn_cast<CallBase>(&I)) {
        if (Function *CF = CB->getCalledFunction()) {
          if (CF->getName().starts_with(YK_PROMOTE_PREFIX)) {
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

/// Prepends a tracing check to an address-taken function.
///
/// When called via a function pointer the function itself must remain
/// in place (to preserve pointer identity). We handle this by inserting a
/// new entry block that loads __yk_thread_tracing_state: if not tracing,
/// tail-call the optimised clone and return immediately; otherwise fall
/// through to the original body so the tracer can observe it.
///
/// Before:
/// ```
/// int foo(int x) {
///   <original body>
/// }
/// ```
///
/// After:
/// ```
/// int foo(int x) {
///   if (__yk_thread_tracing_state == 0)
///     return __yk_opt_foo(x);   // not tracing: take the opt path
///   <original body>
/// }
///
/// int __yk_opt_foo(int x) { ... }          ; yk_outline, optimised clone
/// ```
void ModuleClonePass::prependTracingCheck(Function *F, Function *OptClone,
                                          Module &M) {
  LLVMContext &Context = M.getContext();
  Type *I8Ty = Type::getInt8Ty(Context);

  // Split at the very first instruction. OrigEntry becomes an empty stub;
  // OrigBody gets all the original instructions.
  BasicBlock &OrigEntry = F->getEntryBlock();
  BasicBlock *OrigBody =
      OrigEntry.splitBasicBlock(&OrigEntry.front(), "orig_body");

  // Remove the unconditional branch that splitBasicBlock inserted.
  OrigEntry.getTerminator()->eraseFromParent();

  // Build the dispatch logic in the (now empty) original entry block.
  IRBuilder<> Builder(&OrigEntry);
  GlobalVariable *TracingStateTL = getOrCreateThreadTracingState(M);
  LoadInst *State = Builder.CreateLoad(I8Ty, TracingStateTL, "tracing_state");
  Value *NotTracing =
      Builder.CreateICmpEQ(State, ConstantInt::get(I8Ty, 0), "not_tracing");

  // Not-tracing block: tail-call the opt clone and return.
  BasicBlock *NotTracingBB =
      BasicBlock::Create(Context, "not_tracing", F, OrigBody);
  Builder.CreateCondBr(NotTracing, NotTracingBB, OrigBody);

  IRBuilder<> NTBuilder(NotTracingBB);
  // All calls in a function with debug info must carry a !dbg location.
  // Prefer the first instruction's location; fall back to a synthetic location
  // anchored at the function's subprogram (line 0 = compiler-generated).
  if (DebugLoc DL = OrigBody->front().getDebugLoc()) {
    NTBuilder.SetCurrentDebugLocation(DL);
  } else if (DISubprogram *SP = F->getSubprogram()) {
    NTBuilder.SetCurrentDebugLocation(DILocation::get(Context, 0, 0, SP));
  }
  std::vector<Value *> Args;
  for (Argument &Arg : F->args())
    Args.push_back(&Arg);
  CallInst *OptCall =
      NTBuilder.CreateCall(OptClone->getFunctionType(), OptClone, Args);
  OptCall->setTailCall(true);
  if (F->getReturnType()->isVoidTy())
    NTBuilder.CreateRetVoid();
  else
    NTBuilder.CreateRet(OptCall);
}

/// Updates direct calls in optimised clones so that they call optimised
/// callees (if possible).
///
/// @param CloneMap Maps unoptimised function to their optimised counterparts
/// (if they were cloned).
void ModuleClonePass::updateCalls(std::map<Function *, Function *> &CloneMap,
                                  Module &M) {
  // Replace calls to unoptimised versions to their optimised versions (if
  // existent) in optimised clones and functions marked yk_outline. Since the
  // caller can't be traced, neither can the callee.
  for (Function &F : M) {
    // If it's not a yk_outline function, then it could be traced, and we
    // shouldn't update the callees.
    if (!F.hasFnAttribute(YK_OUTLINE_FNATTR)) {
      continue;
    }
    // If it contains a control point, replacing the callees would be
    // detrimental to traces, since we wouldn't be able to inline callees into
    // the trace.
    if (containsControlPoint(F)) {
      continue;
    }
    for (BasicBlock &BB : F) {
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

PreservedAnalyses ModuleClonePass::run(Module &M, ModuleAnalysisManager &MAM) {
  LLVMContext &Context = M.getContext();
  auto CloneMap = cloneFunctionsInModule(M);
  if (!CloneMap) {
    Context.emitError("Failed to clone functions in module");
    return PreservedAnalyses::none();
  }
  updateCalls(*CloneMap, M);

  if (verifyModule(M, &errs())) {
    Context.emitError("Module verification failed!");
  }

  // Be absolutely sure that all cloned functions are yk_outline.
  for (Function &F : M) {
    if (F.getMetadata(YK_SWT_OPT_MD)) {
      assert(F.hasFnAttribute(YK_OUTLINE_FNATTR));
    }
  }

  return PreservedAnalyses::none();
}
