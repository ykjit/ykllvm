//===- PromoteRecorder.cpp - Record value to be promtoed in a trace --===//
//
// This pass injects calls to a function which records the runtime values of
// variables to be promoted during trace compilation.
//
// It recognises call-sites of functions annotated with `yk_promote`, recording
// the values of the specified arguments prior to the call.

#include <iostream>
#include <sstream>

#include "llvm/IR/Constants.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Verifier.h"
#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/Yk/PromoteRecorder.h"

#define DEBUG_TYPE "yk-promote-recorder"

using namespace llvm;

const std::string RecordFnName = "__yk_record_promote_usize";

namespace llvm {
void initializeYkPromoteRecorderPass(PassRegistry &);
} // namespace llvm

namespace {

// Shared innards of both the old-style and new-style pass.
bool impl(Function &F) {
  LLVMContext &Context = F.getContext();

  // We do this in two stages to avoid iterator invalidation.
  //
  // First just find where we will need to inject calls to the promote
  // recorder.
  std::vector<CallInst *> CallsToPromoted;
  for (BasicBlock &BB : F) {
    for (Instruction &I : BB) {
      if (CallInst *CI = dyn_cast<CallInst>(&I)) {
        Function *CF = CI->getCalledFunction();
        if (CF && !CF->isDeclaration() && CF->hasFnAttribute("yk_promote")) {
          CallsToPromoted.push_back(CI);
        }
      }
    }
  }

  if (CallsToPromoted.empty()) {
    return false;
  }

  // Ensure there is a declaration for the recorder function.
  Module *M = F.getParent();
  Function *Recorder = M->getFunction(RecordFnName);
  DataLayout DL(M);
  if (!Recorder) {
    // args: char *funcname, size_t num_pairs, ...
    // where ... is (arg_idx, val) pairs
    FunctionType *RFT = FunctionType::get(
        Type::getVoidTy(Context),
        {PointerType::get(Context, 0), DL.getIntPtrType(Context)}, true);
    Recorder = Function::Create(RFT, GlobalValue::LinkageTypes::ExternalLinkage,
                                RecordFnName, M);
  }

  // Now inject calls to the recorder.
  for (CallInst *CI : CallsToPromoted) {
    // Get the comma separated values and parse them.
    // Do this first, so we know how many of them there are ahead of time.
    Function *CF = CI->getCalledFunction();
    std::string AttrVal =
        CF->getFnAttribute("yk_promote").getValueAsString().data();
    std::istringstream SS(AttrVal);
    std::string ArgStr;
    std::vector<size_t> PromoteIdxs;
    while (std::getline(SS, ArgStr, ',')) {
      std::stringstream CSS(ArgStr);
      size_t PromoteIdx;
      CSS >> PromoteIdx; // converts string to size_t.
      PromoteIdxs.push_back(PromoteIdx);
    }

    std::vector<Value *> RecorderArgs;

    // First arg: function name whose arg is being promoted. i.e. callee.
    //
    // YKOPT: strings get duplicated if there are many calls to the same
    // function with promoted arguments.
    llvm::Constant *InternFuncName =
        llvm::ConstantDataArray::getString(Context, CF->getName());
    llvm::GlobalVariable *InternFuncNameGV = new llvm::GlobalVariable(
        *M, InternFuncName->getType(), true,
        llvm::GlobalVariable::LinkageTypes::ExternalLinkage, InternFuncName,
        "promote_fname");
    RecorderArgs.push_back(InternFuncNameGV);

    // Second arg: number of promoted arguments.
    IRBuilder Builder(CI);
    llvm::ConstantInt *NumPromotes =
        Builder.getIntN(DL.getPointerSize() * 8, PromoteIdxs.size());
    RecorderArgs.push_back(NumPromotes);

    // Remaining varargs arguments: (arg-idx, val) pairs.
    for (size_t Idx : PromoteIdxs) {
      // Note: the language front-end is expected to have checked the promoted
      // variable indices are in-range and of the right type.
      assert(Idx <= CI->arg_size() - 1);
      Value *PVal = CI->getArgOperand(Idx);
      assert(isa<IntegerType>(PVal->getType()));
      assert(cast<IntegerType>(PVal->getType())->getBitWidth() ==
             DL.getPointerSize() * 8);
      RecorderArgs.push_back(Builder.getIntN(DL.getPointerSize() * 8, Idx));
      RecorderArgs.push_back(PVal);
    }

    // And finally, emit the call to the recorder function.
    //
    // Note that `YkStackmapsPass` will run after this pass, adding a stackmap
    // for this call, so we don't have to do it ourselves here.
    Builder.CreateCall(Recorder, RecorderArgs);
  }

  // Our pass runs after LLVM normally does its verify pass. In debug builds
  // we run it again to check that our pass is generating valid IR.
  if (verifyModule(*M, &errs())) {
    Context.emitError("Yk promote recorder pass generated invalid IR!");
    return false;
  }

  return true;
}
} // namespace

// The new-style pass.
PreservedAnalyses YkPromoteRecorderPass::run(Function &F,
                                             FunctionAnalysisManager &AM) {
  if (impl(F)) {
    return PreservedAnalyses::none();
  }
  return PreservedAnalyses::all();
}

namespace {
// The old-style pass.
class YkPromoteRecorder : public FunctionPass {
public:
  static char ID;

  YkPromoteRecorder() : FunctionPass(ID) {
    initializeYkPromoteRecorderPass(*PassRegistry::getPassRegistry());
  }

  bool runOnFunction(Function &F) override { return impl(F); }
};
} // namespace

char YkPromoteRecorder::ID = 0;
INITIALIZE_PASS(YkPromoteRecorder, DEBUG_TYPE, "Yk Promote Recorder", false,
                false)

FunctionPass *llvm::createYkPromoteRecorderPass() {
  return new YkPromoteRecorder();
}
