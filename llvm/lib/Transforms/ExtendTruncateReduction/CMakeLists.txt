add_llvm_library(LLVMExtendTruncateReduction
  ExtendTruncateReduction.cpp
  )

add_dependencies(LLVMExtendTruncateReduction intrinsics_gen)
  