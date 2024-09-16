#ifndef LLVM_TRANSFORMS_YK_MODULECLONE_H
#define LLVM_TRANSFORMS_YK_MODULECLONE_H

#include "llvm/Pass.h"

namespace llvm {
ModulePass *createYkModuleClonePass();
} // namespace llvm

#endif
