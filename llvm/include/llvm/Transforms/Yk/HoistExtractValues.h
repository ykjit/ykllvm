#ifndef LLVM_TRANSFORMS_YK_HOISTEXTRACTVALUES_H
#define LLVM_TRANSFORMS_YK_HOISTEXTRACTVALUES_H

#include "llvm/Pass.h"

namespace llvm {
ModulePass *createYkHoistExtractValuesPass();
} // namespace llvm

#endif
