//===- The Basic Block Tracer Pass -------------------------------------===//
//
// For each basic block, the IR is modified such that it has the following
// control flow (pseudo-code):
//
// ```
// tracing_check:
//   %t <- load the "is this thread tracing?" thread local
//   %dont_record <- t == 0
//   condbr %dont_record, done, record
//
// record:
//   call __yk_trace_basicblock(...)
//   br done
//
// done:
//   ...original contents of block...
// ```
//
//===-------------------------------------------------------------------===//
//
#include "llvm/Transforms/Yk/BasicBlockTracer.h"
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
#include "llvm/Transforms/Yk/ControlPoint.h"
#include "llvm/Transforms/Yk/ModuleClone.h"
#include "llvm/YkIR/YkIRWriter.h"
#define DEBUG_TYPE "yk-basicblock-tracer-pass"

const uint8_t ThreadTracingStateNone = 0;

using namespace llvm;

namespace llvm {
void initializeYkBasicBlockTracerPass(PassRegistry &);
} // namespace llvm

namespace {
struct YkBasicBlockTracer : public ModulePass {
  static char ID;

  YkBasicBlockTracer() : ModulePass(ID) {
    initializeYkBasicBlockTracerPass(*PassRegistry::getPassRegistry());
  }

  bool runOnModule(Module &M) override {
    LLVMContext &Context = M.getContext();

    // Declare the thread tracing state thread local (if not already present --
    // the input program could have already defined it extern).
    llvm::Type *I8Ty = llvm::Type::getInt8Ty(Context);
    GlobalVariable *ThreadTracingTL =
        M.getNamedGlobal(YK_THREAD_TRACING_STATE_TL);
    if (!ThreadTracingTL) {
      ThreadTracingTL = new llvm::GlobalVariable(
          M, I8Ty, false, llvm::GlobalValue::ExternalLinkage, nullptr,
          YK_THREAD_TRACING_STATE_TL);
      ThreadTracingTL->setThreadLocalMode(
          llvm::GlobalValue::GeneralDynamicTLSModel);
      ThreadTracingTL->setAlignment(Align(1));
    }

    // Trace function is used to trace the execution of the program.
    Type *ReturnType = Type::getVoidTy(Context);
    Type *FunctionIndexArgType = Type::getInt32Ty(Context);
    Type *BlockIndexArgType = Type::getInt32Ty(Context);

    FunctionType *FType = FunctionType::get(
        ReturnType, {FunctionIndexArgType, BlockIndexArgType}, false);
    Function *TraceFunc = Function::Create(
        FType, GlobalVariable::ExternalLinkage, YK_TRACE_FUNCTION, M);

    // Metadata used to help the serialiser identify the purpose of a block.
    //
    // This block is a "are we tracing" check:
    MDNode *TracingCheckBBMD =
        MDNode::get(Context, MDString::get(Context, "swt-tracing-check-bb"));
    // This block records the block:
    MDNode *RecordBBMD =
        MDNode::get(Context, MDString::get(Context, "swt-record-bb"));
    // This is a block we will serialise (the above two we don't):
    MDNode *SerialiseBBMD =
        MDNode::get(Context, MDString::get(Context, "swt-serialise-bb"));

    IRBuilder<> Builder(Context);
    uint32_t FunctionIndex = 0;
    for (auto &F : M) {
      // If we won't ever trace this, don't insert calls to the tracer, as it
      // would only slow us down.
      if ((F.hasFnAttribute(YK_OUTLINE_FNATTR)) && (!containsControlPoint(F))) {
        FunctionIndex++;
        continue;
      }
      // Don't insert basic block tracing calls into optimised clones.
      if (F.getMetadata(YK_SWT_OPT_MD)) {
        continue;
      }

      // Collect *original* blocks that require instrumentation.
      std::vector<BasicBlock *> BBs;
      for (auto &BB : F) {
        BBs.push_back(&BB);
      }

      uint32_t BlockIndex = 0;
      for (BasicBlock *BB : BBs) {
        // If there are allocas in an entry block, then they have to stay
        // there, otherwise stackmaps will consider the frame to have dynamic
        // size (and we won't know how big the frame is at runtime).
        std::vector<AllocaInst *> EntryAllocas;
        if (BlockIndex == 0) {
          for (Instruction &I : *BB) {
            if (AllocaInst *AI = dyn_cast<AllocaInst>(&I)) {
              EntryAllocas.push_back(AI);
            }
          }
          // We also move the allocas to be first in the block to simplify
          // serialisation. `llvm_reverse` ensures they appear in the same
          // order.
          //
          // Note: There can be no PHI nodes in an entry block, so we don't
          // need to check they appear first.
          Builder.SetInsertPoint(&*BB->getFirstInsertionPt());
          for (AllocaInst *AI : llvm::reverse(EntryAllocas)) {
            AI->moveBefore(&*BB->getFirstInsertionPt());
          }
        }

        // Insert a "are we tracing?" check.
        //
        // It's actually a "are we NOT tracing?" check so that the branch
        // predictor has an easier time for the common case (that we are not
        // tracing).
        //
        // If this is an entry block with allocas, the check comes after the
        // allocas, otherwise the check comes first in the block.
        if (EntryAllocas.size() > 0) {
          Builder.SetInsertPoint(EntryAllocas.back()->getNextNode());
        } else {
          Builder.SetInsertPoint(&*BB->getFirstInsertionPt());
        }
        Instruction *ThreadTracingState =
            Builder.CreateLoad(I8Ty, ThreadTracingTL);
        Value *DontRec = Builder.CreateICmpEQ(
            ThreadTracingState, ConstantInt::get(I8Ty, ThreadTracingStateNone));

        // Split off the remainder of the block.
        BasicBlock *RestBB =
            BB->splitBasicBlock(cast<Instruction>(DontRec)->getNextNode());

        // Make the block that calls the recorder.
        BasicBlock *RecBB = llvm::BasicBlock::Create(Context, "", &F, RestBB);
        Builder.SetInsertPoint(RecBB);
        Builder.CreateCall(TraceFunc, {Builder.getInt32(FunctionIndex),
                                       Builder.getInt32(BlockIndex)});
        Builder.CreateBr(RestBB);

        // Update the terminator of the "are we tracing?" block We jump over
        // the recorder block if we are not tracing.
        Instruction *OldTerm = BB->getTerminator();
        Builder.SetInsertPoint(OldTerm);
        Builder.CreateCondBr(DontRec, RestBB, RecBB);
        OldTerm->eraseFromParent();

        // Attach metadata to the first instruction of each of the blocks so
        // that we can more easily identify their purpose in the serialiser.
        BB->front().setMetadata("yk-swt-bb-purpose", TracingCheckBBMD);
        RecBB->front().setMetadata("yk-swt-bb-purpose", RecordBBMD);
        RestBB->front().setMetadata("yk-swt-bb-purpose", SerialiseBBMD);

        assert(BlockIndex != UINT32_MAX &&
               "Expected BlockIndex to not overflow");
        BlockIndex++;
      }
      assert(FunctionIndex != UINT32_MAX &&
             "Expected FunctionIndex to not overflow");
      FunctionIndex++;
    }
    return true;
  }
};
} // namespace

char YkBasicBlockTracer::ID = 0;

INITIALIZE_PASS(YkBasicBlockTracer, DEBUG_TYPE, "yk basicblock tracer", false,
                false)

ModulePass *llvm::createYkBasicBlockTracerPass() {
  return new YkBasicBlockTracer();
}
