add_llvm_component_library(LLVMYkPasses
  ControlPoint.cpp
  Idempotent.cpp
  Linkage.cpp
  LivenessAnalysis.cpp
  OutlineUntraceable.cpp
  StackMaps.cpp
  ShadowStack.cpp
  NoCallsInEntryBlocks.cpp
  SplitBlocksAfterCalls.cpp
  BasicBlockTracer.cpp
  ModuleClone.cpp
  MarkTraceableOptNone.cpp
  ConditionalPromoteCalls.cpp

  DEPENDS
  intrinsics_gen

  LINK_COMPONENTS
  Analysis
  Core
  Support
  TransformUtils # Module Clone
  Linker         # Module Linker
)
