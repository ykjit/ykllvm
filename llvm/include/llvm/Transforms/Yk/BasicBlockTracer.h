#ifndef LLVM_TRANSFORMS_YK_BASIC_BLOCK_TRACER_H
#define LLVM_TRANSFORMS_YK_BASIC_BLOCK_TRACER_H

#include "llvm/Pass.h"

// The name of the trace function - used in swt tracing.
#define YK_TRACE_FUNCTION "__yk_trace_basicblock"

// The name of the dummy (noop) trace function - used in multi-module swt
// tracing.
#define YK_TRACE_FUNCTION_DUMMY "__yk_trace_basicblock_dummy"

namespace llvm {

ModulePass *createYkBasicBlockTracerPass();
} // namespace llvm

#endif
