#ifndef LLVM_TRANSFORMS_YK_MODULE_CLONE_H
#define LLVM_TRANSFORMS_YK_MODULE_CLONE_H

#include "llvm/Pass.h"

#define YK_CLONE_PREFIX "__yk_clone_"

namespace llvm {
ModulePass *createYkModuleClonePass();
} // namespace llvm

#endif
