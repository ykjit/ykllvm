#ifndef LLVM_TRANSFORMS_YK_MODULE_CLONE_H
#define LLVM_TRANSFORMS_YK_MODULE_CLONE_H

#include "llvm/Pass.h"

// The prefix for unoptimised functions.
#define YK_SWT_UNOPT_PREFIX "__yk_unopt_"

// The name of the unoptimised main function.
#define YK_SWT_UNOPT_MAIN "__yk_unopt_main"

// The name of the metadata indicating that a function is address-taken.
#define YK_SWT_MODCLONE_FUNC_ADDR_TAKEN "__yk_func_addr_taken"

// The number of expected control points in the module.
#define YK_SWT_MODCLONE_CP_COUNT 2

namespace llvm {
ModulePass *createYkModuleClonePass();
} // namespace llvm

#endif
