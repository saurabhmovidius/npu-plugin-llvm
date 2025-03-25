add_subdirectory(Utils)
add_subdirectory(Instrumentation)
add_subdirectory(AggressiveInstCombine)
add_subdirectory(InstCombine)
add_subdirectory(Scalar)
add_subdirectory(IPO)
add_subdirectory(Vectorize)
add_subdirectory(Hello)
add_subdirectory(ObjCARC)
add_subdirectory(Coroutines)
add_subdirectory(CFGuard)
add_subdirectory(HipStdPar)
#ifdef MOVIDIUS_PUSHBACK
# This pass compensates for the ISO C/C++ UACs and improves vectorisation opportunities
add_subdirectory(ExtendTruncateReduction)
#endif // MOVIDIUS_PUSHBACK
