#ifndef LLVM_TRANSFORMS_YK_OUTLINE_UNTRACEABLE_H
#define LLVM_TRANSFORMS_YK_OUTLINE_UNTRACEABLE_H

#include "llvm/Pass.h"

namespace llvm {
ModulePass *createOutlineUntraceablePass();
} // namespace llvm

#endif
