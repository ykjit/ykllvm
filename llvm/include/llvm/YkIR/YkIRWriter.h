#ifndef __YKIRWRITER_H
#define __YKIRWRITER_H

#include "llvm/IR/Module.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCStreamer.h"

#define YK_OUTLINE_FNATTR "yk_outline"

namespace llvm {
  void embedYkIR(MCContext &Ctx, MCStreamer &OutStreamer, Module &M);
} // namespace llvm

#endif
