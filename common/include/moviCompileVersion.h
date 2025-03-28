/*  ---------------------------------------------------------------------------
 *  INTEL CONFIDENTIAL
 *
 *  Copyright 2025 Intel Corporation.
 *
 *  This software and the related documents are Intel copyrighted materials, and
 *  your use of them is governed by the express license under which they were
 *  provided to you ("License"). Unless the License provides otherwise, you may
 *  not use, modify, copy, publish, distribute, disclose or transmit this software
 *  or the related documents without Intel's prior written permission.
 *
 *  This software and the related documents are provided as is, with no express or
 *  implied warranties, other than those that are expressly stated in the License.
 *  ---------------------------------------------------------------------------
 *  File       :   moviCompileVersion.h
 *  Description:   The version number information for the 'moviCompile' compiler
 *  --------------------------------------------------------------------------- */


#ifndef __MOVICOMPILEVERSION_H__
#define __MOVICOMPILEVERSION_H__ (1)


// Define one of the following for particular release configurations
//#define MOVI_ALPHA (1)
// #define MOVI_BETA (500)
// #define MOVI_RC (500)
// #define MOVI_HOTFIX (500)


// The following three parts of the version quartet should be explicitly defined here
#define MOVI_MAJOR_REVISION (18) // Upstream LLVM major
#define MOVI_MINOR_REVISION (5)  // Functionality (features or optimizations)
#define MOVI_PATCH_REVISION (0)  // Bug fixes


#endif // __MOVICOMPILEVERSION_H__
