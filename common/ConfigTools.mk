#  ---------------------------------------------------------------------------
#  INTEL CONFIDENTIAL
#
#  Copyright 2025 Intel Corporation.
#
#  This software and the related documents are Intel copyrighted materials, and
#  your use of them is governed by the express license under which they were
#  provided to you ("License"). Unless the License provides otherwise, you may
#  not use, modify, copy, publish, distribute, disclose or transmit this software
#  or the related documents without Intel's prior written permission.
#
#  This software and the related documents are provided as is, with no express or
#  implied warranties, other than those that are expressly stated in the License.
#  ---------------------------------------------------------------------------
#  File       :   Platform.mk
#  Description:   This Makefile module is intended to be included by the top-
#                 level Makefile for building 'moviCompile'.
#  ---------------------------------------------------------------------------

# Cannot proceed if the source root cannot be determined
ifndef MOVI_SRC_ROOT
$(error MOVI_SRC_ROOT is not defined - unable to proceed)
endif


# Get the platform specific configuration information
include $(MOVI_SRC_ROOT)/common/Platform.mk

# Artifactory locations
ARTIFACTORY_URL				?= https://af01p-ir.devtools.intel.com/artifactory/
ARTIFACTORY_TEST_HARNESS	?= mvt-releases-local/moviCompile-extras/TestHarness
ARTIFACTORY_DXIL_KERNELS	?= mvt-releases-local/moviCompile-extras/DxilTests
ARTIFACTORY_API_KEY			?=

# Identify the versions of the various projects used
VER_NEWLIB					= 2.5.0-20170922
VER_GCC						= 6.3.0
SPARC_TRIPLE				= sparc-myriad-rtems


# Which tools should be used for this build?  Allow to be overridden
MOVI_TOOLS_ROOT				?= $(MOVI_STAGING_ROOT)
_MOVI_TOOLS_BIN				= $(MOVI_TOOLS_PLATFORM)/bin
_LEON_ELF_TOOLS_BIN			= $(MOVI_TOOLS_PLATFORM)/$(SPARC_TRIPLE)-$(VER_GCC)/bin
_LEON_ELF_TOOLS_LIB			= $(MOVI_TOOLS_PLATFORM)/$(SPARC_TRIPLE)-$(VER_GCC)/lib/gcc/$(SPARC_TRIPLE)/$(VER_GCC)
_LEON_TOOLS_LIB				= $(MOVI_TOOLS_PLATFORM)/$(SPARC_TRIPLE)-$(VER_GCC)/$(SPARC_TRIPLE)/lib
MOVI_TOOLS_BIN				= $(MOVI_TOOLS_ROOT)/$(_MOVI_TOOLS_BIN)
ELF_TOOLS_BIN				= $(MOVI_TOOLS_ROOT)/$(_ELF_TOOLS_BIN)

# Where the most recent tools are located, and which files are required from them
MOVICOMPILE					?= $(MOVI_TOOLS_ROOT)/$(_MOVI_TOOLS_BIN)/moviCompile$(EXE_SUFFIX)
MOVIASM						?= $(MOVI_TOOLS_ROOT)/$(_MOVI_TOOLS_BIN)/moviAsm$(EXE_SUFFIX)
MOVISIM						?= $(MOVI_TOOLS_ROOT)/$(_MOVI_TOOLS_BIN)/moviSim$(EXE_SUFFIX)

LEON_ELF_AR					?= $(MOVI_TOOLS_ROOT)/$(_LEON_ELF_TOOLS_BIN)/$(SPARC_TRIPLE)-ar$(EXE_SUFFIX)
LEON_ELF_LD					?= $(MOVI_TOOLS_ROOT)/$(_LEON_ELF_TOOLS_BIN)/$(SPARC_TRIPLE)-ld$(EXE_SUFFIX)
LEON_ELF_OBJCOPY			?= $(MOVI_TOOLS_ROOT)/$(_LEON_ELF_TOOLS_BIN)/$(SPARC_TRIPLE)-objcopy(EXE_SUFFIX)
LEON_ELF_RANLIB				?= $(MOVI_TOOLS_ROOT)/$(_LEON_ELF_TOOLS_BIN)/$(SPARC_TRIPLE)-ranlib$(EXE_SUFFIX)
