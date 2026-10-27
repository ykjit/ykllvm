#ifndef __LLVM_SUPPORT_YK_H
#define __LLVM_SUPPORT_YK_H

// The name of the thread tracing state thread local.
#define YK_THREAD_TRACING_STATE_TL "__yk_thread_tracing_state"

/// The name of the "are we interpreting or tracing?" function.
#define YK_IS_INTERPRETING "yk_is_interpreting"

namespace llvm {
void initYkOptions(void);
} // namespace llvm

// YKFIXME: all of our command-line arguments should be collected here instead
// of us randomly introducing `extern bool`s all over the place.
extern bool YkMarkTraceableOptNone;
extern bool YkDontOptFuncABI;
extern bool YkPatchCtrlPoint;
extern bool YkPatchIdempotent;
extern bool YkOutlineUntraceable;
extern bool YkModuleClone;
extern bool YkPatchpointDirectFunctionsCall;
extern bool YkNoVectorize;
extern bool YkNoOverflowIntrinsicFormation;

#endif
