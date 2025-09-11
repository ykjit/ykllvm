#ifndef LLVM_TRANSFORMS_YK_SHADOWSTACK_H
#define LLVM_TRANSFORMS_YK_SHADOWSTACK_H

#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"

namespace llvm {

// Legacy Pass Manager wrapper
class YkShadowStack : public ModulePass {
public:
  static char ID;
  YkShadowStack();
  bool runOnModule(Module &M) override;
};
ModulePass *createYkShadowStackPass();

// New Pass Manager wrapper
class YkShadowStackNew : public PassInfoMixin<YkShadowStackNew> {
public:
  YkShadowStackNew();
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &);
  static StringRef name() { return "YkShadowStack"; }
};

} // namespace llvm

#endif // LLVM_TRANSFORMS_YK_SHADOWSTACK_H
