#ifndef LLVM_TRANSFORMS_YK_PROMOTE_RECORDER_H
#define LLVM_TRANSFORMS_YK_PROMOTE_RECORDER_H

#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"

namespace llvm {

// The pass as a new-style pass.
class YkPromoteRecorderPass : public PassInfoMixin<YkPromoteRecorderPass> {
public:
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM);
};

// Create the pass as an old-style pass.
FunctionPass *createYkPromoteRecorderPass();

} // namespace llvm

#endif // LLVM_TRANSFORMS_YK_PROMOTE_RECORDER_H
