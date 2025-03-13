#ifndef LLVM_TRANSFORMS_YK_IDEMPOTENT_H
#define LLVM_TRANSFORMS_YK_IDEMPOTENT_H

#include "llvm/Pass.h"

#define YK_IDEMPOTENT_RECORDER_PREFIX "__yk_idempotent_promote_"
#define YK_IDEMPOTENT_FNATTR "yk_idempotent"

namespace llvm {
ModulePass *createYkIdempotentPass();
} // namespace llvm

#endif
