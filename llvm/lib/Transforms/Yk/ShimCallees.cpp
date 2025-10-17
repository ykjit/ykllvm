//===- ShimCallees.cpp - Add call shims to functions that could be traced -===
//
// For functions that could be traced, this pass replaces calls to Yk
// instrumented functions with calls to a little "shim" functions.
//
// Each shim is basically of the form:
//
// __yk_shim_f(arg0, ...) {
//   if (__yk_is_thread_tracing) {
//     return call f(arg0, ...);
//   } else {
//     return call __yk_opt_f(arg0, ...);
//   }
// }
//
// Where `__yk_is_thread_tracing` is a native TLS variable defined by the JIT
// runtime.
//
// This means that if the system is not tracing, it can swiftly enter
// uninstrumented AOT code (optimised clone functions) at the next shimmable
// call and avoid the immense cost of calling __yk_trace_basicblock() frequently
// for no reason.
//
// Q: What happens if we enter optimised clones via a shim, then there's a call
// back to a control point, tracing is started, then the interpreter loop
// returns back throught the optimised functions?
//
// A: It works, because we cut a trace short if it returns from the function
// that turned on tracing: we emit a `ret` JIT IR opcode and we return back
// through the optimised code normally. The yk C test tracing_recursion2.c
// checks this return mechanism.

#include "llvm/Transforms/Yk/ShimCallees.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/Yk/ControlPoint.h"
#include "llvm/Transforms/Yk/ModuleClone.h"
#include "llvm/YkIR/YkIRWriter.h"

#define DEBUG_TYPE "yk-basicblock-tracer-pass"

using namespace llvm;

namespace llvm {
void initializeYkShimCalleesPass(PassRegistry &);
} // namespace llvm

namespace {

struct YkShimCallees : public ModulePass {
  static char ID;

  YkShimCallees() : ModulePass(ID) {
    initializeYkShimCalleesPass(*PassRegistry::getPassRegistry());
  }

  bool runOnModule(Module &M) override {
    std::vector<std::tuple<CallInst *, Function *, Function *>> NeedsShim;
    for (Function &F : M) {
      // Only operate on functions that could be traced.
      if ((F.hasFnAttribute(YK_OUTLINE_FNATTR) && !containsControlPoint(F))) {
        continue;
      }
      assert(!F.getName().startswith(YK_SWT_OPT_PREFIX));
      assert(!F.getName().startswith(YK_SHIM_PREFIX));

      // Search for calls that could benefit from a shim.
      for (BasicBlock &BB : F) {
        for (Instruction &I : BB) {
          if (CallInst *CI = dyn_cast<CallInst>(&I)) {
            Function *Callee = CI->getCalledFunction();
            // We can't yet shim varargs functions, because in order to forward
            // on the runtime arguments, the function would need to accept
            // `va_list`.
            if (Callee && !Callee->isVarArg() &&
                !Callee->hasFnAttribute(YK_OUTLINE_FNATTR)) {
              std::string OptCallName =
                  std::string("__yk_opt_") + Callee->getName().str();
              Function *OptF = M.getFunction(OptCallName);
              if (OptF) {
                // We will shim this call.
                NeedsShim.push_back({CI, Callee, OptF});
              }
            }
          }
        }
      }
    }

    LLVMContext &Ctx = M.getContext();
    llvm::Type *Int8Ty = llvm::Type::getInt8Ty(Ctx);

    // Declare the thread local that tells us if we are currently tracing.
    llvm::GlobalVariable *IsTracingTLS = new llvm::GlobalVariable(
        M, Int8Ty,
        /*isConstant=*/false, llvm::GlobalValue::ExternalLinkage,
        /*initializer=*/nullptr, YK_IS_THREAD_TRACING_TLS);
    IsTracingTLS->setThreadLocal(true);

    for (auto [CI, UnoptF, OptF] : NeedsShim) {
      std::string ShimName =
          std::string(YK_SHIM_PREFIX) + UnoptF->getName().str();

      Function *ShimF = M.getFunction(ShimName);

      if (!ShimF) {
        FunctionType *FTy = UnoptF->getFunctionType();
        ShimF = Function::Create(FTy, Function::ExternalLinkage, ShimName, M);

        // Create our blocks.
        LLVMContext &Ctx = M.getContext();
        BasicBlock *EntryBB = BasicBlock::Create(Ctx, "entry", ShimF);
        BasicBlock *TrueBB = BasicBlock::Create(Ctx, "true", ShimF);
        BasicBlock *FalseBB = BasicBlock::Create(Ctx, "false", ShimF);

        // Collect the arguments to forward on.
        std::vector<Value *> Args;
        for (Argument &A : ShimF->args()) {
          Args.push_back(&A);
        }

        // Populate the entry block.
        IRBuilder<> B(EntryBB);
        Value *ZeroI8 = ConstantInt::get(Int8Ty, 0);
        Value *Load = B.CreateLoad(Int8Ty, IsTracingTLS);
        Value *IsTracing = B.CreateICmp(CmpInst::ICMP_NE, Load, ZeroI8);
        B.CreateCondBr(IsTracing, TrueBB, FalseBB);

        // Populate the true block.
        B.SetInsertPoint(TrueBB);
        Value *UnoptRv = B.CreateCall(FTy, UnoptF, Args);
        if (!CI->getType()->isVoidTy()) {
          B.CreateRet(UnoptRv);
        } else {
          B.CreateRetVoid();
        }

        // Populate the false block.
        B.SetInsertPoint(FalseBB);
        Value *OptRv = B.CreateCall(FTy, OptF, Args);
        if (!CI->getType()->isVoidTy()) {
          B.CreateRet(OptRv);
        } else {
          B.CreateRetVoid();
        }

        ShimF->addFnAttr("yk_outline");
        ShimF->addFnAttr(Attribute::NoInline);
        // If the function we shimmed is idempotent, then the shim takes on
        // this responsibility.
        if (UnoptF->hasFnAttribute("yk_idempotent")) {
          ShimF->addFnAttr("yk_idempotent");
        }
      }
      CI->setCalledFunction(ShimF);
    }

    return true;
  }
};
} // namespace

char YkShimCallees::ID = 0;

INITIALIZE_PASS(YkShimCallees, DEBUG_TYPE, "yk basicblock tracer", false, false)

ModulePass *llvm::createYkShimCalleesPass() { return new YkShimCallees(); }
