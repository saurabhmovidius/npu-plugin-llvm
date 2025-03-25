// ***************************************************************************
// INTEL CONFIDENTIAL
//
// Copyright 2025 Intel Corporation.
//
// This software and the related documents are Intel copyrighted materials, and
// your use of them is governed by the express license under which they were
// provided to you ("License"). Unless the License provides otherwise, you may
// not use, modify, copy, publish, distribute, disclose or transmit this software
// or the related documents without Intel's prior written permission.
//
// This software and the related documents are provided as is, with no express or
// implied warranties, other than those that are expressly stated in the License.
// ---------------------------------------------------------------------------
// File       :  SHAVENames.h
// Description:  Names for labels, prefixes, directives and sections
// ---------------------------------------------------------------------------

#ifndef LLVM_LIB_TARGET_SHAVE_SHAVENAMES_H
#define LLVM_LIB_TARGET_SHAVE_SHAVENAMES_H (1)


#define SHAVE_CODE_INDENT                    "        "
#define SHAVE_LABEL_INDENT                   "    "
#define SHAVE_ASM_INDENT                     "\t"
#define SHAVE_NO_INDENT                      ""
#define SHAVE_PARALLEL_INDENT                SHAVE_CODE_INDENT "    "

#define SHAVE_COMMENT                        "//"

#define SHAVE_GLOBAL_PREFIX                  ""
#define SHAVE_IAT_ENTRY_PREFIX               "..iat."
#define SHAVE_IAT_NAME_PREFIX                ".L.iatname_"
#define SHAVE_LINKONCE_PREFIX                ".gnu.linkonce"
#define SHAVE_LOCAL_FUNCT_PREFIX             ".I"
#define SHAVE_LOCAL_PREFIX                   ".I"
#define SHAVE_PRIVATE_FUNCT_PREFIX           ".L"
#define SHAVE_PRIVATE_PREFIX                 ".L"

#define SHAVE_DIRECTIVE_ALIGN                SHAVE_LABEL_INDENT    ".align     "
#define SHAVE_DIRECTIVE_ASCII                SHAVE_LABEL_INDENT    ".ascii     "
#define SHAVE_DIRECTIVE_ASCIIZ               SHAVE_LABEL_INDENT    ".asciiz    "
#define SHAVE_DIRECTIVE_BSS_SECTION          SHAVE_NO_INDENT       ".bss       "
#define SHAVE_DIRECTIVE_BYTE                 SHAVE_LABEL_INDENT    ".byte      "
#define SHAVE_DIRECTIVE_CODE_SECTION         SHAVE_NO_INDENT       ".code      "
#define SHAVE_DIRECTIVE_DATA_SECTION         SHAVE_NO_INDENT       ".data      "
#define SHAVE_DIRECTIVE_DEBUG_SECTION        SHAVE_NO_INDENT       ".debug     "
#define SHAVE_DIRECTIVE_END                  SHAVE_NO_INDENT       ".end"
#define SHAVE_DIRECTIVE_FILL                 SHAVE_LABEL_INDENT    ".fill      "
#define SHAVE_DIRECTIVE_GLOBAL               SHAVE_LABEL_INDENT "// .globl     "
#define SHAVE_DIRECTIVE_LALIGN               SHAVE_LABEL_INDENT    ".lalign"
#define SHAVE_DIRECTIVE_LINKONCE             SHAVE_LABEL_INDENT    ".linkonce"
#define SHAVE_DIRECTIVE_LONGLONG             SHAVE_LABEL_INDENT    ".longlong  "
#define SHAVE_DIRECTIVE_NOWARN               SHAVE_NO_INDENT       ".nowarn"
#define SHAVE_DIRECTIVE_NOWARNEND            SHAVE_NO_INDENT       ".nowarnend"
#define SHAVE_DIRECTIVE_SHORT                SHAVE_LABEL_INDENT    ".short     "
#define SHAVE_DIRECTIVE_STRING               SHAVE_LABEL_INDENT    ".string    "
#define SHAVE_DIRECTIVE_VERSION              SHAVE_NO_INDENT       ".version   "
#define SHAVE_DIRECTIVE_WEAK                 SHAVE_LABEL_INDENT    ".weak      "
#define SHAVE_DIRECTIVE_WEAK_REFERENCE       SHAVE_LABEL_INDENT    ".weak      "
#define SHAVE_DIRECTIVE_WORD                 SHAVE_LABEL_INDENT    ".word      "

#define SHAVE_SECTION_NAME_BSS               ".bss"
#define SHAVE_SECTION_NAME_CODE              ".text"
#define SHAVE_SECTION_NAME_DATA              ".data"
#define SHAVE_SECTION_NAME_DATA1             ".data1"
#define SHAVE_SECTION_NAME_DATA2             ".data2"
#define SHAVE_SECTION_NAME_DATA4             ".data4"
#define SHAVE_SECTION_NAME_DATA8             ".data8"
#define SHAVE_SECTION_NAME_DATA16            ".data16"
#define SHAVE_SECTION_NAME_IAT_NAMES         "..iatnames."
#define SHAVE_SECTION_NAME_IAT_NAMESEND      "..iatnamesend"
#define SHAVE_SECTION_NAME_IAT_STRINGS       "..str..iatstrings"
#define SHAVE_SECTION_NAME_RODATA            ".rodata"
#define SHAVE_SECTION_NAME_RODATA1           ".rodata1"
#define SHAVE_SECTION_NAME_RODATA2           ".rodata2"
#define SHAVE_SECTION_NAME_RODATA4           ".rodata4"
#define SHAVE_SECTION_NAME_RODATA8           ".rodata8"
#define SHAVE_SECTION_NAME_RODATA16          ".rodata16"

#define SHAVE_INLINE_ASM_START                                   " +------------------ Begin - User's Inline Assembly Code ----------------------+\n" \
                                             SHAVE_ASM_INDENT  "// |                                                                             |"

#define SHAVE_INLINE_ASM_END                                     " |                                                                             |\n" \
                                             SHAVE_ASM_INDENT  "// +------------------- End - User's Inline Assembly Code -----------------------+"

#define SHAVE_INLINE_ASM_END_NO_NOPSYNC                          " |                                                                             |\n" \
                                             SHAVE_ASM_INDENT  "// | !!! Have you ensured that there are enough trailing NOPs to satisfy     !!! |\n" \
                                             SHAVE_ASM_INDENT  "// | !!! all outstanding instruction latencies in your assembly code?        !!! |\n" \
                                             SHAVE_ASM_INDENT  "// | !!! See 'moviCompile.pdf' section 11.2.1 Inline Assembly and Scheduling !!! |\n" \
                                             SHAVE_ASM_INDENT  "// +-------------------- End - User's Inline Assembly Code ----------------------+"

#define SHAVE_INLINE_ASM_NOWARN              SHAVE_ASM_INDENT  ".nowarn"
#define SHAVE_INLINE_ASM_NOWARNEND           SHAVE_ASM_INDENT  ".nowarnend\n\n"

#define SHAVE_NOP_SYNC                       SHAVE_CODE_INDENT "NOP SYNC  // Wait for outstanding schedules to complete"


#endif // LLVM_LIB_TARGET_SHAVE_SHAVENAMES_H
