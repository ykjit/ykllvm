#ifndef LLVM_TRANSFORMS_YK_MODULE_CLONE_H
#define LLVM_TRANSFORMS_YK_MODULE_CLONE_H

#include "llvm/Pass.h"

namespace llvm {
ModulePass *createYkModuleClonePass();
} // namespace llvm

#endif
