#ifndef LLVM_TRANSFORMS_YK_MODULE_CLONE_H
#define LLVM_TRANSFORMS_YK_MODULE_CLONE_H

#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/PassManager.h"
#include <map>

// The prefix for optimised functions.
#define YK_SWT_OPT_PREFIX "__yk_opt_"

// The name of the metadata indicating that a function is unoptimised.
#define YK_SWT_OPT_MD YK_SWT_OPT_PREFIX

namespace llvm {

class ModuleClonePass : public PassInfoMixin<ModuleClonePass> {
public:
  ModuleClonePass() {}
  ModuleClonePass(ModuleClonePass &&Arg) = default;
  PreservedAnalyses run(Module &, ModuleAnalysisManager &);

  std::vector<Function *> getFunctionsWithIR(Module &);
  bool shouldClone(Function &);
  std::optional<std::map<Function *, Function *>>
  cloneFunctionsInModule(Module &);
  void removePromotion(CallBase *);
  void removePromotionsAndDebugStrings(Function *);
  void updateCalls(std::map<Function *, Function *> &);
};

} // namespace llvm

#endif
