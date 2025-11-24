#ifndef __YKIRWRITER_H
#define __YKIRWRITER_H

#include "llvm/IR/Module.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCStreamer.h"

#define YK_OUTLINE_FNATTR "yk_outline"
#define YK_INDIRECT_INLINE_FNATTR "yk_indirect_inline"
#define YK_PROMOTE_PREFIX "__yk_promote"
#define YK_DEBUG_STR "yk_debug_str"

namespace llvm {
  void embedYkIR(MCContext &Ctx, MCStreamer &OutStreamer, Module &M);
} // namespace llvm

#endif
