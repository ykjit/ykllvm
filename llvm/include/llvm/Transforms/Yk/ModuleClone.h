#ifndef LLVM_TRANSFORMS_YK_MODULE_CLONE_H
#define LLVM_TRANSFORMS_YK_MODULE_CLONE_H

#include "llvm/Pass.h"

// The prefix for optimised functions.
#define YK_SWT_OPT_PREFIX "__yk_opt_"

// The name of the metadata indicating that a function is unoptimised.
#define YK_SWT_OPT_MD YK_SWT_OPT_PREFIX

namespace llvm {
ModulePass *createYkModuleClonePass();
} // namespace llvm

#endif
