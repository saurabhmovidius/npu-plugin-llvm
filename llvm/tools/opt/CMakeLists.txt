set(LLVM_LINK_COMPONENTS
  AllTargetsAsmParsers
  AllTargetsCodeGens
  AllTargetsDescs
  AllTargetsInfos
  AggressiveInstCombine
  Analysis
  AsmParser
  BitWriter
  CFGuard
  CodeGen
  Core
  Coroutines
  Extensions
  IPO
  IRReader
  IRPrinter
  InstCombine
  Instrumentation
  MC
  ObjCARCOpts
  Remarks
  ScalarOpts
  Support
  Target
  TargetParser
  TransformUtils
  Vectorize
  Passes
#ifdef MOVIDIUS_PUSHBACK
# This pass compensates for ISO C's UACs for better vectorisation
  ExtendTruncateReduction
#endif // MOVIDIUS_PUSHBACK
  )

add_llvm_tool(opt
  NewPMDriver.cpp
  opt.cpp

  DEPENDS
  intrinsics_gen
  SUPPORT_PLUGINS
  )
export_executable_symbols_for_plugins(opt)
