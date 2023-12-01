#ifndef LLVM_TRANSFORMS_YK_HELLOWORLD_H
#define LLVM_TRANSFORMS_YK_HELLOWORLD_H

#include "llvm/Pass.h"

namespace llvm {
ModulePass *createHelloWorldPass();
} // namespace llvm

#endif
