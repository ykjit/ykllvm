//===- Idempotent.cpp - Capture return values of idempotent functions --===//
//
// This pass finds call sites to functions marked with the `yk_idempotent`
// attribute and injects IR to record their return values when tracing is
// enabled. If the JIT can later prove the arguments to the idempotent function
// to be constant, then the entire call can be removed and the return value can
// be replaced with the value that was recorded at trace time.

#include "llvm/Transforms/Yk/Idempotent.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"

#define DEBUG_TYPE "yk-stackmaps"

using namespace llvm;

namespace llvm {
void initializeYkIdempotentPass(PassRegistry &);
} // namespace llvm

namespace {
class YkIdempotent : public ModulePass {
public:
  static char ID;
  YkIdempotent() : ModulePass(ID) {
    initializeYkIdempotentPass(*PassRegistry::getPassRegistry());
  }

  bool insertCalls(Module &M) {
    LLVMContext &Context = M.getContext();

    // First declare the recorder function. The JIT runtime will provide
    // the definition when the module is linked.
    //
    // FIXME: This only works for pointer-sized integers for now. Other types
    // can be added later.
    DataLayout DL(&M);
    Type *PtrSizedIntTy = DL.getIntPtrType(Context);
    FunctionType *RecFType =
        FunctionType::get(PtrSizedIntTy, {PtrSizedIntTy}, false);
    Function *RecF = Function::Create(RecFType, GlobalVariable::ExternalLinkage,
                                      YK_IDEMPOTENT_RECORDER_FUNC, M);

    // Search the module for functions marked idempotent and insert a call to
    // the recorder at all returns within the function.
    IRBuilder<> Builder(Context);
    for (Function &F : M) {
      for (BasicBlock &BB : F) {
        for (Instruction &I : BB) {
          if (CallBase *CI = dyn_cast<CallBase>(&I)) {
            // We only support direct calls for now.
            if (CI->isIndirectCall()) {
              continue;
            }
            Function *CF = CI->getCalledFunction();
            if (!CF || !CF->hasFnAttribute(YK_IDEMPOTENT_FNATTR)) {
              continue;
            }
            if (CF->getFunctionType()->getReturnType() != PtrSizedIntTy) {
              Context.emitError(
                  "unimplemented idempotent promote type. Only pointer-sized "
                  "integers are supported.");
              return false;
            }

            // Call the recorder after the call to the idempotent function.
            Builder.SetInsertPoint(CI->getNextNode());
            CallInst *RecCall = Builder.CreateCall(RecFType, RecF, {CI});
            // Rewrite all future uses of the return value of the idempotent
            // function to the return value of the call to the recorder.
            CI->replaceUsesWithIf(RecCall, [RecCall](Use &U) -> bool {
              return U.getUser() != RecCall;
            });
          }
        }
      }
    }
    return true;
  }

  bool runOnModule(Module &M) override { return insertCalls(M); }
};
} // namespace

char YkIdempotent::ID = 0;
INITIALIZE_PASS(YkIdempotent, DEBUG_TYPE, "yk idempotent", false, false)

/**
 * @brief Creates a new YkIdempotent pass.
 *
 * @return ModulePass* A pointer to the newly created YkIdempotent pass.
 */
ModulePass *llvm::createYkIdempotentPass() { return new YkIdempotent(); }
