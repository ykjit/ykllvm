//===- ControlPoint.cpp - Patch the yk control point -----------------===//
//
// This pass finds the user's call to the dummy control point and replaces it
// with a call to a new control point that implements the necessary logic to
// drive the yk JIT.
//
// The pass converts an interpreter loop that looks like this:
//
// ```
// YkMT *mt = ...;
// YkLocation loc = ...;
// pc = 0;
// while (...) {
//     yk_mt_control_point(mt, loc); // <- dummy control point
//     bc = program[pc];
//     switch (bc) {
//         // bytecode handlers here.
//     }
// }
// ```
//
// Into one that looks like this:
//
// ```
// pc = 0;
// while (...) {
//     llvm.experimental.patchpoint.void(..., __ykrt_control_point, ...)
//     bc = program[pc];
//     switch (bc) {
//         // bytecode handlers here.
//     }
// }
// ```
//
// A patchpoint is used to capture the locations of live variables immediately
// before a call to __ykrt_control_point.
//
// Note that this transformation occurs at the LLVM IR level. The above example
// is shown as C code for easy comprehension.

#include "llvm/Transforms/Yk/ControlPoint.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/DiagnosticInfo.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/Yk/LivenessAnalysis.h"

#define DEBUG_TYPE "yk-control-point"

#define YK_OLD_CONTROL_POINT_NUM_ARGS 2

#define YK_CONTROL_POINT_ARG_MT_IDX 0
#define YK_CONTROL_POINT_ARG_LOC_IDX 1
#define YK_CONTROL_POINT_ARG_VARS_IDX 2
#define YK_CONTROL_POINT_NUM_ARGS 3

// Stackmap ID zero is reserved for the control point.
//
// This will need to change when we support >1 control point.
const unsigned CPStackMapID = 0;

// The number of shadow bytes required for the control point's patchpoint.
//
// This must be large enough to accommodate the call to patchpoint target
// function and if you use a too-big value LLVM will pad the space with NOP
// bytes.
//
// This early in the pipeline we have no idea how the backend will choose the
// encode this call, so for now we use the exact size of the observed
// instruction at the time of writing, as determined by disassembling the binary
// and eyeballing it.
//
// The good news is that LLVM will assert fail if you use a too small value.
#if defined(__x86_64__) || defined(_M_X64)
const unsigned CPShadow = 13;
#else
#error "unknown control point shadow size for this arch"
#endif

using namespace llvm;

/// Find the call to the dummy control point that we want to patch.
/// Returns either a pointer the call instruction, or `nullptr` if the call
/// could not be found.
/// YKFIXME: For now assumes there's only one control point.
CallInst *findControlPointCall(Module &M) {
  // Find the declaration of `yk_mt_control_point()`.
  Function *CtrlPoint = M.getFunction(YK_DUMMY_CONTROL_POINT);
  if (CtrlPoint == nullptr)
    return nullptr;

  // Find the call site of `yk_mt_control_point()`.
  const Value::user_iterator U = CtrlPoint->user_begin();
  if (U == CtrlPoint->user_end())
    return nullptr;

  return cast<CallInst>(*U);
}

namespace llvm {
void initializeYkControlPointPass(PassRegistry &);
} // namespace llvm

namespace {
class YkControlPoint : public ModulePass {
public:
  static char ID;
  YkControlPoint() : ModulePass(ID) {
    initializeYkControlPointPass(*PassRegistry::getPassRegistry());
  }

  bool runOnModule(Module &M) override {
    LLVMContext &Context = M.getContext();
    // Locate the "dummy" control point provided by the user.
    CallInst *OldCtrlPointCall = findControlPointCall(M);
    if (OldCtrlPointCall == nullptr) {
      // This program doesn't have a control point. We can't do any
      // transformations on it, but we do still want to compile it.
      Context.diagnose(DiagnosticInfoInlineAsm(
          "ykllvm couldn't find the call to `yk_mt_control_point()`",
          DS_Warning));
      return false;
    }

    // Get function containing the control point.
    Function *Caller = OldCtrlPointCall->getFunction();

    // Check that the control point is inside a loop.
    DominatorTree DT(*Caller);
    const LoopInfo Loops(DT);
    if (!std::any_of(Loops.begin(), Loops.end(), [OldCtrlPointCall](Loop *L) {
          return L->contains(OldCtrlPointCall);
        })) {
      ;
      Context.emitError("yk_mt_control_point() must be called inside a loop.");
      return false;
    }

    // The old control point should be of the form:
    //    yk_mt_control_point(YkMT*, YkLocation*)
    assert(OldCtrlPointCall->arg_size() == YK_OLD_CONTROL_POINT_NUM_ARGS);
    Type *YkMTTy =
        OldCtrlPointCall->getArgOperand(YK_CONTROL_POINT_ARG_MT_IDX)->getType();
    Type *YkLocTy =
        OldCtrlPointCall->getArgOperand(YK_CONTROL_POINT_ARG_LOC_IDX)
            ->getType();

    // Create a call to the "new" (patched) control point, but do so via a
    // patchpoint so that we can capture the live variables at exactly the
    // moment before the call.
    Type *Int64Ty = Type::getInt64Ty(Context);
    FunctionType *FType = FunctionType::get(Type::getVoidTy(Context),
                                            {YkMTTy, YkLocTy, Int64Ty}, false);
    Function *NF = Function::Create(FType, GlobalVariable::ExternalLinkage,
                                    YK_NEW_CONTROL_POINT, M);

    IRBuilder<> Builder(OldCtrlPointCall);

    const Intrinsic::ID SMFuncID = Function::lookupIntrinsicID(CP_PPNAME);
    if (SMFuncID == Intrinsic::not_intrinsic) {
      Context.emitError("can't find stackmap()");
      return false;
    }
    Function *SMFunc = Intrinsic::getDeclaration(&M, SMFuncID);
    assert(SMFunc != nullptr);

    // Get live variables.
    LivenessAnalysis LA(Caller);
    auto Lives = LA.getLiveVarsBefore(OldCtrlPointCall);

    Value *SMID = ConstantInt::get(Type::getInt64Ty(Context), CPStackMapID);
    Value *Shadow = ConstantInt::get(Type::getInt32Ty(Context), CPShadow);
    std::vector<Value *> Args = {
        SMID,
        Shadow,
        NF,
        ConstantInt::get(Type::getInt32Ty(Context), 3),
        OldCtrlPointCall->getArgOperand(YK_CONTROL_POINT_ARG_MT_IDX),
        OldCtrlPointCall->getArgOperand(YK_CONTROL_POINT_ARG_LOC_IDX),
        SMID,
    };

    for (auto *Live : Lives) {
      Args.push_back(Live);
    }

    Builder.CreateCall(SMFunc->getFunctionType(), SMFunc,
                       ArrayRef<Value *>(Args));

    // Replace the call to the dummy control point.
    OldCtrlPointCall->eraseFromParent();

#ifndef NDEBUG
    // Our pass runs after LLVM normally does its verify pass. In debug builds
    // we run it again to check that our pass is generating valid IR.
    if (verifyModule(M, &errs())) {
      Context.emitError("Control point pass generated invalid IR!");
      return false;
    }
#endif
    return true;
  }
};
} // namespace

char YkControlPoint::ID = 0;
INITIALIZE_PASS(YkControlPoint, DEBUG_TYPE, "yk control point", false, false)

ModulePass *llvm::createYkControlPointPass() { return new YkControlPoint(); }
