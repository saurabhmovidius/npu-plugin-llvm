//===-- InitLLVMDLL.cpp ---------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

//
// FIXME - Movidius: This file is a duplicate of InitLLVM.cpp for the DLL build
//   of moviCompile. It doesn't include the Windows function calls for fetching
//   the command-line arguments.
//   The function windows::GetCommandLineArguments uses the function 
//   CommandLineToArgvW to fetch the values of argc and argv, rather than the
//   values passed to main. It uses the values passed to the executable by the
//   Operating System. When using clang as a DLL, this means that the values of 
//   argc and argv that are used are the values for the executable calling out 
//   to the DLL and not the values that executable has passed to the DLL.
//

#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/Signals.h"

#ifdef _WIN32
// FIXME-LLVM11 #include "Windows/WindowsSupport.h"
#endif
#if 0
using namespace llvm;
using namespace llvm::sys;

InitLLVMDLL::InitLLVMDLL(int &Argc, const char **&Argv) : StackPrinter(Argc, Argv) {
#ifndef NDEBUG
  static std::atomic<bool> Initialized{false};
  assert(!Initialized && "InitLLVMDLL was already initialized!");
  Initialized = true;
#endif
  sys::PrintStackTraceOnErrorSignal(Argv[0]);
}

InitLLVMDLL::~InitLLVMDLL() { 
  llvm_shutdown();
#ifdef _WIN32
  TeardownDLL();
#endif // _WIN32
}
#endif
