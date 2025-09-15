//===- ShadowStack.cpp - Pass to add shadow stacks to the AOT module --===//
//
// This pass adds a shadow stack to a yk interpreter so that variables which
// may have their address taken are uniform between JITted and AOT code.
//
// It works as follows:
//
// main() is assumed to be the entry point to the interpreter. In there we
// malloc a chunk of memory for use as a shadow stack. The pointer to this
// memory is then stored into a global variable for other functions to pick up.
//
// Then for each non-main function, F, we then insert a "shadow prologue" which:
//  1. load's the shadow stack's "high water mark" from the global variable.
//  2. adds to the pointer to reserve shadow space for F.
//  3. stores the new high water mark back to the global variable.
//
// Then for each non-main function, at each return point from the function,
// we insert code to restore the shadow stack pointer back to what it was
// before we adjusted it.
//
// If a function requires no shadow space, then the above steps can be omitted.
//
// main() is assumed to not recursively call main(), as this would cause use to
// re-allocate the shadow stack from scratch, leaking the existing shadow stack
// and generally causing chaos. This is checked during this pass.
//
// Special considerations regarding setjmp/longjmp:
//
// Consider a function using setjmp like this:
//
// ```
// define f() {
//   ; allocate shadow space
//   %0 = load ptr, ptr @shadowstack_0, align 8
//   %1 = getelementptr i8, ptr %0, i32 16
//   store ptr %1, ptr @shadowstack_0, align 8
//   ...
//   call @setjmp(...)
//   ...
//   ; and suppose g() uses the shadow stack and calls longjmp() to transfer to
//   ; the above setjmp().
//   call @g(...)
//   ...
//   call @h(...) ; assume this also uses the shadow stack.
//   ...
//   return:
//    ; release shadow space
//    store ptr %0, ptr @shadowstack_0, align 8
//    ret i32 1
// }
// ```
//
// Is the system in a consistent after g() calls longjump()?
//
// This question can be split into two:
//
// 1. Will f() restore the shadow stack pointer to the right value when it
// returns after the longjmp()?
//
// 2. Will callees like h() get a useable shadow stack after the longjmp()?
//
// To answer 1: since %0 is local to f() and not changed between the calls the
// setjmp and longjmp, this value will be restored during longjmp() and thus
// the epilogue will do the right thing. See the man page for setjmp(3) for
// more on why this works.
//
// To answer 2: when g() longjumps, we will skip g()'s shadow epilogue, so the
// shadow stack pointer will not be restored. In effect, g()'s shadow frame
// leaks up until the point where f()'s shadow epilogue restores the shadow
// stack pointer.
//
// Does this matter? I believe the behaviour to be correct in the sense that
// h() will get a usable shadow frame, but just deeper in the shadow stack than
// expected. One can dream up scenarios, e.g. "h(), calls i(), calls j(), ...,
// which longjumps", where a long chain of shadow frames temporarily leak,
// potentially blowing the shadow stack. This could happen if the top-level
// interpreter loop (which is long-lived) contains a commonly jumped-to
// setjump(), so we probably do want to fix this soon.
//
// YKFIXME: If you wanted to fix this you'd have to reload the shadow stack
// pointer before calls to setjmp() and similarly to the reasoning for 1, it
// would be restored after a longjmp.

#include "llvm/Transforms/Yk/ShadowStack.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/DIBuilder.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/Verifier.h"
#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/Yk/ControlPoint.h"
#include "llvm/Transforms/Yk/ModuleClone.h"
#include "llvm/YkIR/YkIRWriter.h"

#include <map>

#define DEBUG_TYPE "yk-shadow-stack-pass"
#define YK_MT_NEW "yk_mt_new"
#define G_SHADOW_STACK "shadowstack_0"
#define G_SHADOW_HEAD "shadowstack_head"
#define G_SHADOW_SIZE "shadowstack_size"
#define MAIN "main"
// The size of the shadow stack. Defaults to 1MB.
// YKFIXME: Make this adjustable by a compiler flag.
#define SHADOW_STACK_SIZE 1000000

using namespace llvm;

namespace llvm {
void initializeYkShadowStackPass(PassRegistry &);
} // namespace llvm

namespace {

class YkShadowStackImpl {
  // Commonly used types.
  Type *Int8Ty = nullptr;
  Type *Int8PtrTy = nullptr;
  Type *Int32Ty = nullptr;
  Type *PointerSizedIntTy = nullptr;
  std::unique_ptr<DIBuilder> DIB;

  GlobalVariable *ShadowCurrent;
  GlobalVariable *ShadowHead;

  using AllocaVector = std::vector<std::pair<AllocaInst *, size_t>>;

public:
  // Checks whether the given instruction allocates space for the result of a
  // call to `yk_mt_new`.
  bool isYkMTNewAlloca(AllocaInst *I) {
    for (User *U : I->users()) {
      if (StoreInst *SI = dyn_cast<StoreInst>(U)) {
        Value *V = SI->getValueOperand();
        if (CallInst *CI = dyn_cast<CallInst>(V)) {
          if (CI->isInlineAsm())
            return false;
          if (!CI->getCalledFunction())
            return false;
          return (CI->getCalledFunction()->getName() == YK_MT_NEW);
        }
      }
    }
    return false;
  }

  // Insert a shadowstack_ptr debug variable when compiled with '-g' so that in
  // gdb we can know the offset into the shadow stack for each frame. This uses
  // llvm's debug intrinsic so does not affect optimized code.
  void insertShadowDbgInfo(Function &F, DataLayout &DL, size_t SFrameSize,
                           Value *SSPtr) {
    DISubprogram *SP = F.getSubprogram();
    if (!SP) {
      return;
    }
    DIBasicType *DISSTy = DIB->createBasicType("shadowstack", SFrameSize * 8,
                                               dwarf::DW_ATE_address);
    DIDerivedType *DISSPtrTy = DIB->createPointerType(
        DISSTy, DL.getPointerSize(), DL.getPointerABIAlignment(0).value(),
        std::nullopt, "shadowstack_ptr");

    // Create a debug local var to our opaque shadow stack block
    DILocalVariable *DebugVar = DIB->createAutoVariable(
        SP, "shadowstack", SP->getFile(), SP->getLine(), DISSPtrTy,
        false, // AlwaysPreserve
        DINode::DIFlags::FlagArtificial);

    // And insert it after the load of the shadow stack pointer.
    DILocation *DIL = DILocation::get(F.getContext(), SP->getLine(), 0, SP);
    DIB->insertDbgValueIntrinsic(SSPtr, DebugVar, DIB->createExpression(), DIL,
                                 cast<LoadInst>(SSPtr)->getNextNode());
  }

  // Insert main's prologue.
  //
  // Main is a little different, in that it actually allocates the shadow stack
  // and thus can use the allocation directly if it needs shadow space.
  //
  // Returns a pointer to the result of the call to malloc that was used to
  // heap allocate memory for a shadow stack.
  CallInst *insertMainPrologue(Function *Main, size_t SFrameSize) {
    Module *M = Main->getParent();
    Instruction *First = Main->getEntryBlock().getFirstNonPHI();
    IRBuilder<> Builder(First);

    // Create some memory on the heap for the shadow stack.
    FunctionCallee MF =
        M->getOrInsertFunction("malloc", Int8PtrTy, PointerSizedIntTy);
    CallInst *Malloc = Builder.CreateCall(
        MF, {ConstantInt::get(PointerSizedIntTy, SHADOW_STACK_SIZE)}, "");

    Builder.CreateStore(Malloc, ShadowHead);

    // If main() needs shadow space, reserve some.
    if (SFrameSize > 0) {
      GetElementPtrInst *GEP = GetElementPtrInst::Create(
          Int8Ty, Malloc, {ConstantInt::get(Int32Ty, SFrameSize)}, "",
          Malloc->getNextNode());
      // Update the global variable keeping track of the top of shadow stack.
      Builder.CreateStore(GEP, ShadowCurrent);
    } else {
      // If main doesn't require any shadow stack space then we simply
      // initialise the global with the result of the call to malloc().
      Builder.CreateStore(Malloc, ShadowCurrent);
    }

    return Malloc;
  }

  // Scan the function `F` for instructions of interest and compute the layout
  // of the shadow frame.
  size_t analyseFunction(Function &F, DataLayout &DL, AllocaVector &Allocas,
                         std::vector<ReturnInst *> &Rets) {
    size_t SFrameSize = 0;
    for (BasicBlock &BB : F) {
      for (Instruction &I : BB) {
        if (AllocaInst *AI = dyn_cast<AllocaInst>(&I)) {
          if (!AI->getAllocationSize(DL).has_value()) {
            // This function contains a dynamically sized alloca. We thus can't
            // create a shadow stack for it because we can't compute the
            // stack-frame size statically. Instead, we outline this, preventing
            // it from being traced at all, which is a bit of a hack.
            //
            // This might happen with certain allocas: variable length arrays;
            // non-const array sizes; and opaque types.
            //
            // YKFIXME: This could be supported if we use a bump pointer instead
            // of calculating the high water mark in the prologue.
            F.addFnAttr(YK_OUTLINE_FNATTR);
            return 0;
          }
          // Some yk specific variables that will never be traced and thus
          // can live happily on the normal stack.
          if (StructType *ST = dyn_cast<StructType>(AI->getAllocatedType())) {
            // Don't put yk locations on the shadow stack.
            //
            // YKFIXME: This is somewhat fragile since `struct.YkLocation` is
            // a name given by LLVM which could theoretically change. Luckily,
            // this should all go away once we only move variables to the
            // shadowstack that have their reference taken.
            if (!ST->isLiteral() && ST->getName() == "struct.YkLocation") {
              continue;
            }
          }
          if (isYkMTNewAlloca(AI)) {
            // The variable created by `yk_mt_new` will never be traced, so
            // there's no need to store it on the shadow stack.
            continue;
          }
          // Record the offset at which to store the object, ensuring we obey
          // LLVM's alignment requirements.
          //
          // YKOPT: We currently allocate objects on the shadow stack in
          // whatever order we encounter them, but we may be able to waste less
          // space (to padding) by sorting them by size.
          size_t Align = AI->getAlign().value();
          SFrameSize = int((SFrameSize + (Align - 1)) / Align) * Align;
          Allocas.push_back({AI, SFrameSize});
          SFrameSize += AI->getAllocationSize(DL).value();
        } else if (ReturnInst *RI = dyn_cast<ReturnInst>(&I)) {
          Rets.push_back(RI);
        } else if (CallBase *CI = dyn_cast<CallBase>(&I)) {
          // check for recursive calls to main().
          Function *CF = CI->getCalledFunction();
          if ((CF != nullptr) && (CF->getName() == MAIN)) {
            F.getContext().emitError("detected recursive call to main!");
          }
        }
      }
    }
    // Align this shadowstack frame to the natural stack alignment.
    MaybeAlign MAlign = DL.getStackAlignment();
    if (MAlign.valueOrOne().value() > 1) {
      int Align = MAlign.valueOrOne().value();
      SFrameSize = int((SFrameSize + (Align - 1)) / Align) * Align;
    }
    return SFrameSize;
  }

  // Make space on the shadow stack for F's frame.
  //
  // Returns the shadow stack pointer before more space is allocated. Local
  // variables for the shadow frame will be pointers relative to this.
  Value *insertShadowPrologue(Function &F, size_t AllocSize) {
    Instruction *First = F.getEntryBlock().getFirstNonPHI();
    IRBuilder<> Builder(First);

    // Load the shadow stack pointer out of the global variable.
    Value *InitSSPtr = Builder.CreateLoad(Int8PtrTy, ShadowCurrent);
    // Add space for F's shadow frame.
    GetElementPtrInst *GEP = GetElementPtrInst::Create(
        Int8Ty, InitSSPtr, {ConstantInt::get(Int32Ty, AllocSize)}, "", First);
    // Update the global variable keeping track of the top of shadow stack.
    Builder.CreateStore(GEP, ShadowCurrent);

    return InitSSPtr;
  }

  // Replace alloca instructions with shadow stack accesses.
  // Returns a vector of the GEP instructions that replaced the original
  // allocas.
  std::vector<GetElementPtrInst *>
  rewriteAllocas(DataLayout &DL, AllocaVector &Allocas, Value *SSPtr) {
    std::vector<GetElementPtrInst *> Instructions;
    for (auto [AI, Off] : Allocas) {
      GetElementPtrInst *GEP = GetElementPtrInst::Create(
          Int8Ty, SSPtr, {ConstantInt::get(Int32Ty, Off)}, "", AI);
      AI->replaceAllUsesWith(GEP);
      AI->removeFromParent();
      AI->deleteValue();
      Instructions.push_back(GEP);
    }
    return Instructions;
  }

  /// At each place the function can return, insert IR to restore the shadow
  /// stack pointer to it's initial value (as it was before the prologue
  /// allocate shadow sack space).
  void insertShadowEpilogues(std::vector<ReturnInst *> &Rets,
                             Value *InitSSPtr) {
    for (ReturnInst *RI : Rets) {
      IRBuilder<> Builder(RI);
      Builder.CreateStore(InitSSPtr, ShadowCurrent);
    }
  }

  // Handle `main` separatley, since it works slightly differently to other
  // functions: it allocates the shadow stack
  //
  // For `__yk_unopt_main` we update the allocas while reusing the same shadow
  // stack instance allocated by `main`.
  //
  // Note that since we assuming main() doesn't call main(), we can consider
  // the shadow stack disused at the point main() returns. For this reason,
  // there's no need to emit a shadow epilogue for main().
  //
  // YKFIXME: Investigate languages that don't have/use main as the first
  // entry point.
  void updateMainFunctions(DataLayout &DL, Module &M, LLVMContext &Context) {
    Function *Main = M.getFunction(MAIN);
    if (Main == nullptr || Main->isDeclaration()) {
      Context.emitError("Unable to add shadow stack: could not find definition "
                        "of \"main\" function!");
      return;
    }

    // Update the original main function.
    AllocaVector MainAllocas;
    std::vector<ReturnInst *> MainRets;
    size_t SFrameSize = analyseFunction(*Main, DL, MainAllocas, MainRets);
    CallInst *Malloc = insertMainPrologue(Main, SFrameSize);
    auto MainGEPs = rewriteAllocas(DL, MainAllocas, Malloc);
  }

  bool run(Module &M) {
    LLVMContext &Context = M.getContext();
    llvm::NamedMDNode *CU_Nodes = M.getNamedMetadata("llvm.dbg.cu");
    if (CU_Nodes && CU_Nodes->getNumOperands() > 0) {
      llvm::DICompileUnit *CU =
          llvm::cast<llvm::DICompileUnit>(CU_Nodes->getOperand(0));

      DIB = std::make_unique<DIBuilder>(M, false, CU);
    }

    // Cache commonly used types.
    Int8Ty = Type::getInt8Ty(Context);
    Int8PtrTy = Type::getInt8PtrTy(Context);
    Int32Ty = Type::getInt32Ty(Context);
    DataLayout DL(&M);
    PointerSizedIntTy = DL.getIntPtrType(Context);

    GlobalVariable *ShadowSize =
        cast<GlobalVariable>(M.getOrInsertGlobal(G_SHADOW_SIZE, Int32Ty));
    ShadowSize->setInitializer(
        ConstantInt::get(Int32Ty, SHADOW_STACK_SIZE, false));
    ShadowSize->setSection(".rodata");
    ShadowSize->setLinkage(llvm::GlobalValue::ExternalLinkage);

    ShadowCurrent =
        cast<GlobalVariable>(M.getOrInsertGlobal(G_SHADOW_STACK, Int8PtrTy));
    ShadowCurrent->setInitializer(
        ConstantPointerNull::get(cast<PointerType>(Int8PtrTy)));
    ShadowHead =
        cast<GlobalVariable>(M.getOrInsertGlobal(G_SHADOW_HEAD, Int8PtrTy));
    ShadowHead->setInitializer(
        ConstantPointerNull::get(cast<PointerType>(Int8PtrTy)));

    ShadowCurrent->setThreadLocal(true);
    ShadowHead->setThreadLocal(true);

    updateMainFunctions(DL, M, Context);

    // Instrument each remaining function with shadow stack code.
    for (Function &F : M) {
      if (F.empty()) {
        // skip declarations.
        continue;
      }
      if (F.getMetadata(YK_SWT_OPT_MD)) {
        continue;
      }
      // skip already handled main and unopt functions
      if (F.getName() == MAIN) {
        continue;
      }
      // skip functions that will never be traced.
      if ((F.hasFnAttribute(YK_OUTLINE_FNATTR)) && (!containsControlPoint(F))) {
        continue;
      }
      // skip optimised clones.
      if (F.getMetadata(YK_SWT_OPT_MD)) {
        continue;
      }
      assert(F.getMetadata(YK_SWT_OPT_MD) == nullptr);

      AllocaVector Allocas;
      std::vector<ReturnInst *> Rets;
      size_t SFrameSize = analyseFunction(F, DL, Allocas, Rets);
      if (SFrameSize > 0) {
        Value *InitSSPtr = insertShadowPrologue(F, SFrameSize);
        insertShadowDbgInfo(F, DL, SFrameSize, InitSSPtr);
        rewriteAllocas(DL, Allocas, InitSSPtr);
        insertShadowEpilogues(Rets, InitSSPtr);
      }
    }

    // Now scan the module for things like setjmp() that require us to restore
    // the shadow stack pointer in the event of a longjmp(). We do this even for
    // functions that themselves don't have shadow stack instrumentation,
    // because their callees could still mutate the shadow stack pointer.
    for (Function &F : M) {
      for (BasicBlock &BB : F) {
        for (Instruction &I : BB) {
          if (CallBase *CB = dyn_cast<CallBase>(&I)) {
            Function *CF = CB->getCalledFunction();
            if (CF && CF->hasFnAttribute(Attribute::ReturnsTwice)) {
              IRBuilder<> Builder(&I);
              // Found one.
              //
              // Before the call, load the current shadow stack pointer. We do
              // a volatile load to ensure this value is restored in the event
              // of a longjmp.
              Value *OrigPtr =
                  Builder.CreateLoad(PointerType::get(M.getContext(), 0),
                                     ShadowCurrent, /*isVolatile=*/true);
              // Assuming this is setjmp() or similar, after the call, restore
              // the shadow stack pointer if a longjmp() happened.
              if ((CF->getName() == "setjmp") || (CF->getName() == "_setjmp") ||
                  (CF->getName() == "sigsetjmp")) {

                BasicBlock *SetJmpBB = I.getParent();
                BasicBlock *DoneBB = SetJmpBB->splitBasicBlock(I.getNextNode());

                // Remove unconditional branch inserted by the split.
                Instruction *BrInst = I.getNextNode();
                assert(SetJmpBB->getTerminator() == BrInst);
                BrInst->eraseFromParent();

                // Make a new block to contain the store.
                BasicBlock *StoreBB = BasicBlock::Create(
                    M.getContext(), "", SetJmpBB->getParent(), DoneBB);

                // Test if the store is needed.
                Builder.SetInsertPoint(SetJmpBB);
                Value *StoreNotNeeded =
                    Builder.CreateICmp(CmpInst::Predicate::ICMP_EQ, &I,
                                       ConstantInt::get(I.getType(), 0));
                Builder.CreateCondBr(StoreNotNeeded, DoneBB, StoreBB);

                // Emit the store.
                Builder.SetInsertPoint(StoreBB);
                Builder.CreateStore(OrigPtr, ShadowCurrent);
                Builder.CreateBr(DoneBB);
              } else {
                Context.emitError("Unknown returns_twice function encountered "
                                  "in shadow stack pass: " +
                                  CF->getName());
                return true;
              }
            }
          }
        }
      }
    }

    // Finalize the DIBuilder after all debug info is created
    if (DIB)
      DIB->finalize();

#ifndef NDEBUG
    // Our pass runs after LLVM normally does its verify pass. In debug builds
    // we run it again to check that our pass is generating valid IR.
    if (verifyModule(M, &errs())) {
      Context.emitError("ShadowStack insertion pass generated invalid IR!");
      return false;
    }
#endif
    return true;
  }
};
} // namespace

char YkShadowStack::ID = 0;
INITIALIZE_PASS(YkShadowStack, DEBUG_TYPE, "yk shadowstack", false, false)

YkShadowStack::YkShadowStack() : ModulePass(ID) {
  initializeYkShadowStackPass(*PassRegistry::getPassRegistry());
}

bool YkShadowStack::runOnModule(Module &M) {
  return YkShadowStackImpl().run(M);
}

ModulePass *llvm::createYkShadowStackPass() { return new YkShadowStack(); }

YkShadowStackNew::YkShadowStackNew() {}

PreservedAnalyses YkShadowStackNew::run(Module &M, ModuleAnalysisManager &) {
  bool Changed = YkShadowStackImpl().run(M);
  return Changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
}
