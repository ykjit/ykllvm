#ifndef LLVM_TRANSFORMS_YK_STACKMAPS_H
#define LLVM_TRANSFORMS_YK_STACKMAPS_H

#include "llvm/Pass.h"

namespace llvm {
ModulePass *createYkStackmapsPass(uint64_t StackmapIDStart);
} // namespace llvm

#endif
