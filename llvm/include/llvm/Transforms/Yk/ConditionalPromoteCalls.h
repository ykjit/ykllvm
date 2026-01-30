#ifndef LLVM_TRANSFORMS_YK_CONDITIONAL_PROMOTE_CALLS_H
#define LLVM_TRANSFORMS_YK_CONDITIONAL_PROMOTE_CALLS_H

#include "llvm/Pass.h"

// Metadata key to mark the purpose of blocks created by
// ConditionalPromoteCalls. This follows the same pattern as BasicBlockTracer's
// "yk-swt-bb-purpose".
#define YK_PROMOTE_BB_PURPOSE_MD "yk-promote-bb-purpose"

// Block purposes (values for YK_PROMOTE_BB_PURPOSE_MD):
// The tracing state check block (skip contents):
#define YK_PROMOTE_CHECK_BB "promote-check-bb"
// The block containing the promote call (serialise):
#define YK_PROMOTE_BB "promote-bb"
// The continuation block with PHI (handle PHI specially):
#define YK_PROMOTE_CONTINUE_BB "promote-continue-bb"

// Block name suffixes (for debugging/readability in IR dumps):
#define YK_PROMOTE_CHECK_BB_SUFFIX ".promote.check"
#define YK_PROMOTE_BB_SUFFIX ".do_promote"
#define YK_PROMOTE_CONTINUE_BB_SUFFIX ".promote.continue"

namespace llvm {

ModulePass *createYkConditionalPromoteCallsPass();

} // namespace llvm

#endif
