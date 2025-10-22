#ifndef LLVM_TRANSFORMS_YK_SHIM_CALLEES_H
#define LLVM_TRANSFORMS_YK_SHIM_CALLEES_H

#include "llvm/Pass.h"

// The symbol prefix given to shim functions.
#define YK_SHIM_PREFIX "__yk_shim_"

// The name of the native TLS variable that indicates if the current thread is
// tracing.
#define YK_IS_THREAD_TRACING_TLS "__yk_is_thread_tracing"

namespace llvm {

ModulePass *createYkShimCalleesPass();
} // namespace llvm

#endif
