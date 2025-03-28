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
#  Description:   This Makefile module is intended to be included by regular
#                 Makefiles to establish the platform specific information.
#  ---------------------------------------------------------------------------

# Determine what platform this is running on, normally this is OSTYPE and MACHTYPE, but
# environment variables are not reliable, so use 'uname' instead and normalise to lowercase
OSTYPE						:= $(shell uname -o | sed -e 's/GNU\///' | tr '[:upper:]' '[:lower:]' )
MACHTYPE					:= $(shell uname -m | tr '[:upper:]' '[:lower:]' )
ISUBUNTU					:= $(shell uname -a | tr '[:upper:]' '[:lower:]' | sed -e 's/^.*\(ubuntu\|trisquel\).*$$/ubuntu/' )
ISDEBIAN					:= $(shell uname -a | tr '[:upper:]' '[:lower:]' | sed -e 's/^.*\(debian\).*$$/debian/' )

ifeq ($(OSTYPE),cygwin)
# Setup configuration for Cygwin
MOVI_PLATFORM				= win64
else ifeq ($(OSTYPE),linux)
# Setup configuration for Linux
ifeq ($(MACHTYPE),x86_64)
MOVI_PLATFORM				?= linux64
else
$(error Unsupported Linux platform '$(MACHTYPE)')
endif
else ifeq ($(OSTYPE),msys)
# Setup configuration for MinGW
CXXFLAGS					+= -D_GLIBCXX_HAVE_FENV_H
ifeq ($(MACHTYPE),x86_64)
MOVI_PLATFORM				= mingw64
else ifeq ($(MACHTYPE),i386)
MOVI_PLATFORM				= mingw32
else ifeq ($(MACHTYPE),i686)
MOVI_PLATFORM				= mingw32
else
$(error Unsupported MinGW platform '$(MACHTYPE)')
endif
else
$(error Unrecognised OS platform '$(OSTYPE)')
endif

MOVI_TOOLS_PLATFORM			?= $(MOVI_PLATFORM)


# Default the executable suffix
ifeq ($(OSTYPE),cygwin)
DLL_SUFFIX					= .dll
EXE_SUFFIX					= .exe
else ifeq ($(OSTYPE),msys)
DLL_SUFFIX					= .dll
EXE_SUFFIX					= .exe
else
DLL_SUFFIX					= .so
EXE_SUFFIX					=
endif


# Tools used by the Makefiles
CP							?= cp
CMAKE						?= cmake
CURL						?= curl
DATE						?= date
ECHO						?= echo
EXPORT						?= export
FALSE						?= false
FGREP						?= fgrep
FIND						?= find
GREP						?= grep
MKDIR						?= mkdir
LN							?= ln
PERL						?= perl
PYTHON						?= python3
RM							?= rm
RMDIR						?= rmdir
SED							?= sed
SORT						?= sort
STRIP						?= strip
TAR							?= tar
TOUCH						?= touch
TRUE						?= true

# Special cases for Ubuntu
ifeq ($(ISUBUNTU),ubuntu)
PUSHD						?= cd
POPD						?= 
else ifeq ($(ISDEBIAN),debian)
PUSHD						?= cd
POPD						?= 
else
PUSHD						?= pushd
POPD						?= popd
endif

# Special cases for Windows and Cygwin
ifeq ($(OSTYPE),cygwin)
CMD_EXE						:= $(shell which cmd.exe)

# FIXME: Movidius - could make this a little smarter, but if we always install to the default location it should be fine
_MSBUILD_EXE_2022			:= "$(ProgramW6432)/Microsoft Visual Studio/2022/Professional/MSBuild/Current/Bin/MSBuild.exe"
_MSBUILD_EXE_2022_ALT		:= "$(PROGRAMFILES)/Microsoft Visual Studio/2022/BuildTools/MSBuild/Current/Bin/MSBuild.exe"
_MSBUILD_EXE_2019			:= "$(PROGRAMFILES)/Microsoft Visual Studio/2019/Professional/MSBuild/Current/Bin/MSBuild.exe"
_MSBUILD_EXE_2019_ALT		:= "$(PROGRAMFILES)/Microsoft Visual Studio/2019/BuildTools/MSBuild/Current/Bin/MSBuild.exe"

MSBUILD_EXE					:= $(shell \
									if [ -e $(_MSBUILD_EXE_2022) ]; then \
										cygpath -u $(_MSBUILD_EXE_2022); \
									elif [ -e $(_MSBUILD_EXE_2022_ALT) ]; then \
										cygpath -u $(_MSBUILD_EXE_2022_ALT); \
									elif [ -e $(_MSBUILD_EXE_2019) ]; then \
										cygpath -u $(_MSBUILD_EXE_2019); \
									elif [ -e $(_MSBUILD_EXE_2019_ALT) ]; then \
										cygpath -u $(_MSBUILD_EXE_2019_ALT); \
									else \
										$(ECHO) 'Unable to find MSBuild.exe for >=VS2019'; \
									fi)
else
CMD_EXE						:= 'cmd.exe is not available on $(MOVI_PLATFORM)'
MSBUILD_EXE					:= 'MSBuild.exe is not available on $(MOVI_PLATFORM)'
endif
