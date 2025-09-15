#ifndef LLVM_TRANSFORMS_YK_MARKTRACEABLEOPTNONE_H
#define LLVM_TRANSFORMS_YK_MARKTRACEABLEOPTNONE_H

#include "llvm/Pass.h"

namespace llvm {
ModulePass *createYkMarkTraceableOptNonePass();
} // namespace llvm

#endif
