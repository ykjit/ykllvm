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
#include "llvm/Transforms/Yk/ControlPoint.h"
#include "llvm/YkIR/YkIRWriter.h"

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

  // Return the name of the idempotent recorder function for an integer type.
  std::optional<std::string> intRecorderFuncName(IntegerType *ITy) {
    switch (ITy->getBitWidth()) {
    case 32:
      return std::string(YK_IDEMPOTENT_RECORDER_PREFIX "i32");
    case 64:
      return std::string(YK_IDEMPOTENT_RECORDER_PREFIX "i64");
    default:
      return std::nullopt;
    }
  }

  // If necessary, create a declaration to the idempotent recorder function for
  // the type `Ty`.
  Function *getRecorderFunc(Module &M, Type *Ty) {
    DataLayout DL(&M);

    if (IntegerType *ITy = dyn_cast<IntegerType>(Ty)) {
      std::optional<std::string> OptRecFName = intRecorderFuncName(ITy);
      if (!OptRecFName.has_value()) {
        return nullptr;
      }
      std::string RecFName = OptRecFName.value();
      if (Function *RecF = M.getFunction(RecFName)) {
        return RecF; // already declared.
      } else {
        FunctionType *RecFType = FunctionType::get(ITy, {ITy}, false);
        return Function::Create(RecFType, GlobalVariable::ExternalLinkage,
                                RecFName, M);
      }
    } else {
      return nullptr;
    }
  }

  bool insertCalls(Module &M) {
    LLVMContext &Context = M.getContext();
    // Search the module for functions marked idempotent and insert a call to
    // the recorder at all returns within the function.
    IRBuilder<> Builder(Context);
    for (Function &F : M) {
      if ((F.hasFnAttribute(YK_OUTLINE_FNATTR)) && (!containsControlPoint(F))) {
        continue;
      }
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
            Function *RecF =
                getRecorderFunc(M, CF->getFunctionType()->getReturnType());
            if (RecF == nullptr) {
              Context.emitError("unimplemented idempotent promote type");
              return false;
            }
            // Call the recorder after the call to the idempotent function.
            Builder.SetInsertPoint(CI->getNextNode());
            CallInst *RecCall =
                Builder.CreateCall(RecF->getFunctionType(), RecF, {CI});
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
