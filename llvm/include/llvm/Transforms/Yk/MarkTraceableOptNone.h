#ifndef LLVM_TRANSFORMS_YK_MARKTRACEABLEOPTNONE_H
#define LLVM_TRANSFORMS_YK_MARKTRACEABLEOPTNONE_H

#include "llvm/Pass.h"

#define YK_AUTO_OPTNONE_FNATTR "yk_autooptnone"

namespace llvm {
ModulePass *createYkMarkTraceableOptNonePass();
} // namespace llvm

#endif
