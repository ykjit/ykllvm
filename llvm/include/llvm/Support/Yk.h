#ifndef __LLVM_SUPPORT_YK_H
#define __LLVM_SUPPORT_YK_H

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
extern bool YkShimCallees;

#endif
