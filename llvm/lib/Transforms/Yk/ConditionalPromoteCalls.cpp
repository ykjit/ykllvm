//===- The Conditional Promote Calls Pass --------------------------------===//
//
// For each call to __yk_promote_* or __yk_idempotent_promote_* functions,
// the IR is modified to skip the call when not tracing (state == 0).
//
// This optimisation avoids function call overhead when not tracing, while
// preserving the promote call in the IR for the YkIR writer to detect.
//
// == Transformation ==
//
// Before (pseudo-code):
// ```
// orig_block:
//   %val = ...some computation...
//   %promoted = call __yk_promote_i32(%val)
//   ...use %promoted...
// ```
//
// After:
// ```
// orig_block:
//   %val = ...some computation...
//   br check_block
//
// check_block:                       ; marked: YK_PROMOTE_CHECK_BB
//   %t = load @__yk_thread_tracing_state
//   %not_tracing = icmp eq %t, 0
//   condbr %not_tracing, continue, do_promote
//
// do_promote:                        ; marked: YK_PROMOTE_BB
//   %promoted = call __yk_promote_i32(%val)
//   br continue
//
// continue:                          ; marked: YK_PROMOTE_CONTINUE_BB
//   %result = phi [%promoted, do_promote], [%val, check_block]
//   ...use %result...
// ```
//
// The blocks are marked by setting metadata on their first instruction,
// following the same pattern as BasicBlockTracer's "yk-swt-bb-purpose".
// The serialiser uses these markers to determine how to handle each block.
//
// == Pass Ordering ==
//
// This pass must run BEFORE YkStackmapsPass and YkBasicBlockTracer so that
// stackmaps are inserted correctly and block tracing calls are added.
//
// This pass creates the `__yk_thread_tracing_state` thread-local variable
// if it doesn't exist.
//
//===---------------------------------------------------------------------===//
//
#include "llvm/Transforms/Yk/ConditionalPromoteCalls.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Module.h"
#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/Yk/BasicBlockTracer.h"
#include "llvm/Transforms/Yk/Idempotent.h"
#include "llvm/YkIR/YkIRWriter.h"

#define DEBUG_TYPE "yk-conditional-promote-calls-pass"

using namespace llvm;

namespace llvm {
void initializeYkConditionalPromoteCallsPass(PassRegistry &);
} // namespace llvm

namespace {
class YkConditionalPromoteCalls : public ModulePass {
public:
  static char ID;

  YkConditionalPromoteCalls() : ModulePass(ID) {
    initializeYkConditionalPromoteCallsPass(*PassRegistry::getPassRegistry());
  }

  bool runOnModule(Module &M) override {
    LLVMContext &Context = M.getContext();
    bool Changed = false;

    // Get or create the thread tracing state TLS variable (shared with
    // BasicBlockTracer).
    llvm::Type *I8Ty = llvm::Type::getInt8Ty(Context);
    GlobalVariable *ThreadTracingTL = getOrCreateThreadTracingState(M);

    // Collect all promote calls first to avoid iterator invalidation.
    std::vector<CallInst *> PromoteCalls;
    for (Function &F : M) {
      // Skip declarations.
      if (F.isDeclaration())
        continue;

      for (BasicBlock &BB : F) {
        for (Instruction &I : BB) {
          if (CallInst *CI = dyn_cast<CallInst>(&I)) {
            Function *Callee = CI->getCalledFunction();
            if (Callee && (Callee->getName().starts_with(YK_PROMOTE_PREFIX) ||
                           Callee->getName().starts_with(
                               YK_IDEMPOTENT_RECORDER_PREFIX))) {
              PromoteCalls.push_back(CI);
            }
          }
        }
      }
    }

    // Metadata to mark the purpose of blocks (on first instruction).
    // This follows the same pattern as BasicBlockTracer's "yk-swt-bb-purpose".
    MDNode *PromoteCheckBBMD =
        MDNode::get(Context, MDString::get(Context, YK_PROMOTE_CHECK_BB));
    MDNode *PromoteBBMD =
        MDNode::get(Context, MDString::get(Context, YK_PROMOTE_BB));
    MDNode *PromoteContinueBBMD =
        MDNode::get(Context, MDString::get(Context, YK_PROMOTE_CONTINUE_BB));

    // Transform each promote call.
    for (CallInst *CI : PromoteCalls) {
      BasicBlock *OrigBB = CI->getParent();
      Function *F = OrigBB->getParent();

      // Get the original value being promoted (first argument).
      Value *OrigVal = CI->getArgOperand(0);
      Type *ResultTy = CI->getType();

      // Split the block at the promote call.
      // ContinueBB will contain the promote call and everything after it.
      BasicBlock *ContinueBB = OrigBB->splitBasicBlock(
          CI, OrigBB->getName() + YK_PROMOTE_CONTINUE_BB_SUFFIX);

      // Remove the unconditional branch that splitBasicBlock created.
      OrigBB->getTerminator()->eraseFromParent();

      // Create the "check" block for the tracing state check.
      BasicBlock *CheckBB = BasicBlock::Create(
          Context, OrigBB->getName() + YK_PROMOTE_CHECK_BB_SUFFIX, F,
          ContinueBB);

      // Create the "do_promote" block that contains the actual call.
      BasicBlock *DoPromoteBB = BasicBlock::Create(
          Context, OrigBB->getName() + YK_PROMOTE_BB_SUFFIX, F, ContinueBB);

      // Add unconditional branch from OrigBB to CheckBB.
      IRBuilder<> Builder(OrigBB);
      Builder.CreateBr(CheckBB);

      // Build the tracing check in CheckBB.
      Builder.SetInsertPoint(CheckBB);
      LoadInst *TracingState = Builder.CreateLoad(I8Ty, ThreadTracingTL);
      Value *NotTracing =
          Builder.CreateICmpEQ(TracingState, ConstantInt::get(I8Ty, 0));
      // If not tracing, skip directly to ContinueBB; otherwise do the promote.
      Builder.CreateCondBr(NotTracing, ContinueBB, DoPromoteBB);

      // Move the promote call to DoPromoteBB.
      CI->removeFromParent();
      CI->insertInto(DoPromoteBB, DoPromoteBB->end());

      // Add branch from DoPromoteBB to ContinueBB.
      Builder.SetInsertPoint(DoPromoteBB);
      Builder.CreateBr(ContinueBB);

      // Create PHI node at the start of ContinueBB to select between promoted
      // value and original value.
      Builder.SetInsertPoint(ContinueBB, ContinueBB->begin());
      PHINode *Phi = Builder.CreatePHI(ResultTy, 2);
      Phi->addIncoming(CI, DoPromoteBB);
      Phi->addIncoming(OrigVal, CheckBB);

      // Replace all uses of the promote call result with the PHI node.
      CI->replaceAllUsesWith(Phi);
      // But the PHI itself uses CI, so fix that.
      Phi->setIncomingValue(0, CI);

      // Mark blocks by setting metadata on their first instruction.
      // This follows the same pattern as BasicBlockTracer.
      CheckBB->front().setMetadata(YK_PROMOTE_BB_PURPOSE_MD, PromoteCheckBBMD);
      DoPromoteBB->front().setMetadata(YK_PROMOTE_BB_PURPOSE_MD, PromoteBBMD);
      ContinueBB->front().setMetadata(YK_PROMOTE_BB_PURPOSE_MD,
                                      PromoteContinueBBMD);

      Changed = true;
    }

    return Changed;
  }
};
} // namespace

char YkConditionalPromoteCalls::ID = 0;

INITIALIZE_PASS(YkConditionalPromoteCalls, DEBUG_TYPE,
                "yk conditional promote calls", false, false)

ModulePass *llvm::createYkConditionalPromoteCallsPass() {
  return new YkConditionalPromoteCalls();
}
