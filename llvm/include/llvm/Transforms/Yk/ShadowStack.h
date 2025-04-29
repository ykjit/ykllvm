#ifndef LLVM_TRANSFORMS_YK_SHADOWSTACK_H
#define LLVM_TRANSFORMS_YK_SHADOWSTACK_H

#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"

namespace llvm {

// Legacy Pass Manager wrapper
class YkShadowStack : public ModulePass {
public:
  static char ID;
  YkShadowStack(uint64_t controlPointCount);
  bool runOnModule(Module &M) override;

private:
  uint64_t controlPointCount;
};
ModulePass *createYkShadowStackPass(uint64_t controlPointCount);

// New Pass Manager wrapper
class YkShadowStackNew : public PassInfoMixin<YkShadowStackNew> {
public:
  YkShadowStackNew(uint64_t controlPointCount);
  YkShadowStackNew();
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &);
  static StringRef name() { return "YkShadowStack"; }

private:
  uint64_t controlPointCount;
};

} // namespace llvm

#endif // LLVM_TRANSFORMS_YK_SHADOWSTACK_H
