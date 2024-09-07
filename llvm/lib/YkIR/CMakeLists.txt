# This is a temporary hack
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fpermissive")

add_llvm_component_library(LLVMYkIR
  YkIRWriter.cpp

  LINK_COMPONENTS
  Core
  MC
  Support
  TransformUtils # Module Clone
  Linker         # Module Linker
)
