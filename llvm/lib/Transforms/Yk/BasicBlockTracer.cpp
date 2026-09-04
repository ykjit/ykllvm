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
#include "llvm/IR/MDBuilder.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Module.h"
#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/Yk/ControlPoint.h"
#include "llvm/YkIR/YkIRWriter.h"

#define DEBUG_TYPE "yk-basicblock-tracer-pass"

const uint8_t ThreadTracingStateNone = 0;

// Mark record branches as maximally cold to encourage the "normal" interpreter
// to be optimised.
constexpr uint32_t NotTracingBranchWeight = (1 << 20) - 1;
constexpr uint32_t TracingBranchWeight = 1;

using namespace llvm;

namespace llvm {
void initializeYkBasicBlockTracerPass(PassRegistry &);

GlobalVariable *getOrCreateThreadTracingState(Module &M) {
  LLVMContext &Context = M.getContext();
  Type *I8Ty = Type::getInt8Ty(Context);
  GlobalVariable *TL = M.getNamedGlobal(YK_THREAD_TRACING_STATE_TL);
  if (!TL) {
    TL = new GlobalVariable(M, I8Ty, false, GlobalValue::ExternalLinkage,
                            nullptr, YK_THREAD_TRACING_STATE_TL);
    TL->setAlignment(Align(1));
  }
  TL->setThreadLocalMode(GlobalValue::InitialExecTLSModel);
  return TL;
}
} // namespace llvm

namespace {
struct YkBasicBlockTracer : public ModulePass {
  static char ID;

  YkBasicBlockTracer() : ModulePass(ID) {
    initializeYkBasicBlockTracerPass(*PassRegistry::getPassRegistry());
  }

  bool runOnModule(Module &M) override {
    LLVMContext &Context = M.getContext();

    // Declare the basic block recorder function, whose sole argument is a
    // 32-bit int. This uses the `PreserveAll` API so that compiled interpreter
    // code -- at least in the sense of register allocation -- isn't perturbed
    // by the presence of these calls. In pseudocode we generate a function
    // which looks roughly as follows:
    //
    // ```
    // static preserve_all void __yk_trace_basicblock(uint32_t block_id) {
    //    uint32_t *cursor = __yk_trace_buffer.cursor;
    //    if (cursor <= __yk_trace_buffer.end) {
    //      *cursor = block_id;
    //      __yk_trace_buffer.cursor = cursor + 1;
    //    }
    //  }
    //  ```

    // Get or create the thread tracing state TLS variable.
    llvm::Type *I8Ty = llvm::Type::getInt8Ty(Context);
    GlobalVariable *ThreadTracingTL = getOrCreateThreadTracingState(M);

    Type *I32Ty = Type::getInt32Ty(Context);
    PointerType *I32PtrTy = PointerType::getUnqual(Context);
    StructType *TraceBufferTy = StructType::get(I32PtrTy, I32PtrTy);
    GlobalVariable *TraceBufferTL = M.getNamedGlobal(YK_TRACE_BUFFER_TL);
    if (!TraceBufferTL) {
      TraceBufferTL = new GlobalVariable(M, TraceBufferTy, false,
                                         GlobalValue::ExternalLinkage, nullptr,
                                         YK_TRACE_BUFFER_TL);
      TraceBufferTL->setThreadLocalMode(GlobalValue::InitialExecTLSModel);
      TraceBufferTL->setAlignment(
          M.getDataLayout().getABITypeAlign(TraceBufferTy));
    }

    IRBuilder<> Builder(Context);

    Type *ReturnType = Type::getVoidTy(Context);
    FunctionType *FType = FunctionType::get(ReturnType, {I32Ty}, false);
    Function *TraceFunc = Function::Create(
        FType, GlobalVariable::InternalLinkage, YK_TRACE_FUNCTION, M);
    TraceFunc->setCallingConv(CallingConv::PreserveAll);
    TraceFunc->addFnAttr(YK_OUTLINE_FNATTR);
    BasicBlock *CapacityBB = BasicBlock::Create(Context, "", TraceFunc);
    BasicBlock *RecordBB = BasicBlock::Create(Context, "", TraceFunc);
    BasicBlock *DoneBB = BasicBlock::Create(Context, "", TraceFunc);

    Builder.SetInsertPoint(CapacityBB);
    Value *TraceBuffer = Builder.CreateThreadLocalAddress(TraceBufferTL);
    Value *CursorPtr = Builder.CreateStructGEP(TraceBufferTy, TraceBuffer, 0);
    LoadInst *Cursor = Builder.CreateLoad(I32PtrTy, CursorPtr);
    Value *EndPtr = Builder.CreateStructGEP(TraceBufferTy, TraceBuffer, 1);
    LoadInst *End = Builder.CreateLoad(I32PtrTy, EndPtr);
    Value *HasCapacity = Builder.CreateICmpULT(Cursor, End);
    Builder.CreateCondBr(HasCapacity, RecordBB, DoneBB);

    Builder.SetInsertPoint(RecordBB);
    Builder.CreateStore(TraceFunc->getArg(0), Cursor);
    Value *NextCursor =
        Builder.CreateInBoundsGEP(I32Ty, Cursor, Builder.getInt32(1));
    Builder.CreateStore(NextCursor, CursorPtr);
    Builder.CreateBr(DoneBB);

    Builder.SetInsertPoint(DoneBB);
    Builder.CreateRetVoid();

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

    uint32_t FunctionIndex = 0;
    for (auto &F : M) {
      // If we won't ever trace this, don't insert calls to the tracer, as it
      // would only slow us down.
      if ((F.hasFnAttribute(YK_OUTLINE_FNATTR)) && (!containsControlPoint(F))) {
        FunctionIndex++;
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
            AI->moveBefore(BB->getFirstInsertionPt());
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
        LoadInst *ThreadTracingState =
            Builder.CreateLoad(I8Ty, ThreadTracingTL);
        ThreadTracingState->setAtomic(llvm::AtomicOrdering::Monotonic);
        Value *DontRec = Builder.CreateICmpEQ(
            ThreadTracingState, ConstantInt::get(I8Ty, ThreadTracingStateNone));

        // Split off the remainder of the block.
        BasicBlock *RestBB =
            BB->splitBasicBlock(cast<Instruction>(DontRec)->getNextNode());

        // Make the block that calls the recorder.
        BasicBlock *RecBB = llvm::BasicBlock::Create(Context, "", &F, RestBB);
        if (FunctionIndex > UINT16_MAX) {
          llvm::report_fatal_error("function index exceeded limit");
        }
        if (BlockIndex > UINT16_MAX) {
          llvm::report_fatal_error("block index exceeded limit");
        }
        uint64_t Arg =
            (static_cast<uint32_t>(FunctionIndex) << 16) | BlockIndex;
        Builder.SetInsertPoint(RecBB);
        CallInst *CI = Builder.CreateCall(TraceFunc, {Builder.getInt32(Arg)});
        CI->setCallingConv(llvm::CallingConv::PreserveAll);
        Builder.CreateBr(RestBB);

        // Update the terminator of the "are we tracing?" block We jump over
        // the recorder block if we are not tracing.
        Instruction *OldTerm = BB->getTerminator();
        Builder.SetInsertPoint(OldTerm);
        MDNode *TracingWeights = MDBuilder(Context).createBranchWeights(
            NotTracingBranchWeight, TracingBranchWeight);
        Builder.CreateCondBr(DontRec, RestBB, RecBB, TracingWeights);
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
