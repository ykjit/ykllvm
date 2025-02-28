#ifndef LLVM_TRANSFORMS_YK_IDEMPOTENT_H
#define LLVM_TRANSFORMS_YK_IDEMPOTENT_H

#include "llvm/Pass.h"

#define YK_IDEMPOTENT_RECORDER_FUNC "__yk_idempotent_promote_usize"
#define YK_IDEMPOTENT_FNATTR "yk_idempotent"

namespace llvm {
ModulePass *createYkIdempotentPass();
} // namespace llvm

#endif
