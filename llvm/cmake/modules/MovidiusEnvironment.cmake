# FIXME: Movidius - this needs a lot of work to ensure that it properly mirrors the 'Makefile' equivalent

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
#  File       :   MovidiusEnvironment.cmake
#  Description:   This CMake modules extends the main LLVM rules with those
#                 needed to build 'moviCompile'
#  ---------------------------------------------------------------------------

# The root to where the libraries should be installed to and where the other tools reside
set(DEFAULT_RELEASE_DIR "${LLVM_MAIN_SRC_DIR}/../StagingArea")
set(MCC_RELEASE_DIR "${DEFAULT_RELEASE_DIR}" CACHE PATH "Path to the 'moviCompile' staging area directory")
message(STATUS "Septi: LLVM_MAIN_SRC_DIR = ${LLVM_MAIN_SRC_DIR} DEFAULT_RELEASE_DIR = ${DEFAULT_RELEASE_DIR} MCC_RELEASE_DIR = ${MCC_RELEASE_DIR}")

# Verify that the directory exists - it must pre-exist
if(NOT EXISTS ${MCC_RELEASE_DIR})
    message(FATAL_ERROR "MCC_RELEASE_DIR must be set to an existing directory like 'StagingArea'. MCC_RELEASE_DIR = ${MCC_RELEASE_DIR}")
endif(NOT EXISTS ${MCC_RELEASE_DIR})

set(NEWLIB_ROOT_DIR "${LLVM_MAIN_SRC_DIR}/../../LeonTools/src/newlib" CACHE PATH "Path to the Newlib sources")


# Setup the OS specific settings
if(WIN32)
    set(MCC_TARGET_PLATFORM "win32")
else(WIN32)
    # FIXME: Movidius - need to distinguish between Cygwin and Linux
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(MCC_TARGET_PLATFORM "linux64")
    else()
        set(MCC_TARGET_PLATFORM "linux32")
    endif()
endif(WIN32)

# Locate the 'moviTools' needed to build the libraries
set(MCC_TARGET_DIR "${MCC_RELEASE_DIR}/${MCC_TARGET_PLATFORM}/bin")
set(MCC_SPARC_TARGET_DIR "${MCC_RELEASE_DIR}/${MCC_TARGET_PLATFORM}/sparc-myriad-elf-4.8.2/bin")
set(MCC_CC_BIN "${MCC_TARGET_DIR}/moviCompile${CMAKE_EXECUTABLE_SUFFIX}")
set(MCC_ASM_BIN "${MCC_TARGET_DIR}/moviAsm${CMAKE_EXECUTABLE_SUFFIX}")
set(MCC_AR_BIN "${MCC_SPARC_TARGET_DIR}/sparc-myriad-elf-ar${CMAKE_EXECUTABLE_SUFFIX}")
set(MCC_RANLIB_BIN "${MCC_SPARC_TARGET_DIR}/sparc-myriad-elf-ranlib${CMAKE_EXECUTABLE_SUFFIX}")

function(add_shave_library_for_target LIBNAME ARCH)
    set(SRCLIST "")
    set(ASMLIST "")
    set(OBJLIST "")
    set(OBJLIST_REL "")
    set(INCLUDES "")

    set(MCC_CFLAGS -O3 -Wall -g -gdwarf-2 -gstrict-dwarf ${MCC_CFLAGS} -mcpu=${ARCH})
    set(AFLAGS -a --noSPrefixing ${MCC_AFLAGS} --cv ${ARCH})
    
    set(MCC_CXXFLAGS ${MCC_CFLAGS})
    set(MCC_CLFLAGS ${MCC_CFLAGS} -x cl)
    
    # Generate the list of include directories.
    get_directory_property(INCLUDE_DIRS INCLUDE_DIRECTORIES)
    foreach(INCLUDE_DIR ${INCLUDE_DIRS})
        set(INCLUDES ${INCLUDES} -I${INCLUDE_DIR})
    endforeach(INCLUDE_DIR ${INCLUDE_DIRS})
    
    set(INTER_DIR "${CMAKE_CURRENT_BINARY_DIR}/${ARCH}")
    set(LIB_DIR "${MCC_LIB_INSTALL_DIR}/${ARCH}")
    file(MAKE_DIRECTORY ${INTER_DIR})
    file(MAKE_DIRECTORY ${LIB_DIR})
    
    # Commands to compile source files to asm
    foreach(SRC ${ARGN})
        # Locate the source file
        if(IS_ABSOLUTE ${SRC})
            set(SRC_PATH "${SRC}")
        else(IS_ABSOLUTE ${SRC})
            set(SRC_PATH "${CMAKE_CURRENT_SOURCE_DIR}/${SRC}")
        endif(IS_ABSOLUTE ${SRC})
        

        # Add this source file to the set of sources for this library
        set(SRCLIST ${SRCLIST} ${SRC_PATH})

        # Need to know what kind of file this is
        get_filename_component(SRC_EXT  ${SRC} EXT)
        get_filename_component(SRC_NAME ${SRC} NAME_WE)

        # Only interested in '.asm', '.s', '.S', '.c', '.cpp', '.cxx' and '.cl'
        if((${SRC_EXT} STREQUAL ".asm") OR (${SRC_EXT} STREQUAL ".s"))
            # Source files can be '.asm', but do not compile them with clang
            set(ASM ${SRC_PATH})
        else()
            # Locate where the generated '.s' file will be put
            set(ASM "${INTER_DIR}/${SRC_NAME}.s")

            # For each of the other relevant extensions, set the appropriate compilation flags
            if(${SRC_EXT} STREQUAL ".cl")
                set(FLAGS ${MCC_CLFLAGS})
            elseif(${SRC_EXT} STREQUAL ".c")
                set(FLAGS ${MCC_CFLAGS})
            elseif((${SRC_EXT} STREQUAL ".cpp") OR (${SRC_EXT} STREQUAL ".cxx"))
                set(FLAGS ${MCC_CXXFLAGS})
            elseif(${SRC_EXT} STREQUAL ".S")
                set(FLAGS ${MCC_CFLAGS})
            endif()

            # Compile this source to a '.s' file
            add_custom_command(
                OUTPUT  ${ASM}
                COMMAND clang ${FLAGS} -S ${INCLUDES} ${SRC_PATH} -o ${ASM}
                DEPENDS clang ${SRC_PATH}
                COMMENT "Compiling ${SRC_NAME}${SRC_EXT} for ${ARCH}/${LIBNAME}.a"
            )

            # Note that this is a generated file for cleanup
            set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES ${ASM})
        endif()
        
        # Add this '.asm' file to the list of files to be passed to the assembler
        set(ASMLIST ${ASMLIST} ${ASM})
    endforeach(SRC ${ARGN})
    
    # Commands to assemble '.s' files to '.o'
    foreach(ASM ${ASMLIST})
        get_filename_component(ASM_NAME ${ASM} NAME_WE)
        
        # Locate the generated '.o' file and add it to the set of '.o' files for this library
        set(OBJ "${INTER_DIR}/${ASM_NAME}.o")
        set(OBJLIST ${OBJLIST} ${OBJ})
        set(OBJLIST_REL ${OBJLIST_REL} "${ASM_NAME}.o")

        # Assemble this '.s' file to a '.o' file
        add_custom_command(
            OUTPUT  ${OBJ}
            COMMAND ${MCC_ASM_BIN} ${AFLAGS} ${ASM} -o:${OBJ}
            DEPENDS ${MCC_ASM_BIN} ${ASM}
            COMMENT "Assembling ${ASM_NAME}.s for ${ARCH}/${LIBNAME}.a"
        )

        # Note that this is a generated file for cleanup
        set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES ${OBJ})
    endforeach(ASM ${ASMLIST})
    
    # Command to link '.o' files into a '.a' library
    set(LIB "${LIB_DIR}/${LIBNAME}.a")

    # On Windows, the CMD.EXE command-shell has a limit of 8191 characters on the command-line.
    # We work around that by using relative file paths for object files (OBJLIST_REL).
    # This shortens the command-line quite a lot, from over 16,000 to under 2,000.
    add_custom_command(
        OUTPUT  ${LIB}
        COMMAND ${MCC_AR_BIN} cr ${LIB} ${OBJLIST_REL}
        DEPENDS ${MCC_AR_BIN} ${OBJLIST}
        WORKING_DIRECTORY ${INTER_DIR}
        COMMENT "Creating library ${ARCH}/${LIBNAME}.a"
    )

    # Register this as a project target in the 'Movidius libraries' category
    add_custom_target(Movidius_${LIBNAME}_${ARCH} DEPENDS ${LIB} SOURCES ${SRCLIST})
    set_target_properties(Movidius_${LIBNAME}_${ARCH} PROPERTIES FOLDER "Movidius libraries")
endfunction(add_shave_library_for_target)

function(add_shave_library LIBNAME)
#    add_shave_library_for_target(${LIBNAME} myriad2.1 ${ARGN})
#    add_shave_library_for_target(${LIBNAME} myriad2.2 ${ARGN})
#
#    add_custom_target(Movidius_${LIBNAME})
#
#    add_dependencies(Movidius_${LIBNAME} Movidius_${LIBNAME}_myriad2.1)
#    add_dependencies(Movidius_${LIBNAME} Movidius_${LIBNAME}_myriad2.2)
#    add_dependencies(Movidius_Libraries Movidius_${LIBNAME})
endfunction(add_shave_library LIBNAME ARCH)

function(copy_shave_headers LIBNAME)
#    set(out_files)
#    file(MAKE_DIRECTORY ${MCC_HEADER_INSTALL_DIR})
#    foreach(INCLUDE_DIR ${ARGN})
#        file(GLOB_RECURSE files RELATIVE ${INCLUDE_DIR} ${INCLUDE_DIR}/*)
#        foreach(f ${files})
#            set(dst ${MCC_HEADER_INSTALL_DIR}/${f})
#            set(src ${INCLUDE_DIR}/${f})
#            configure_file(${src} ${dst} COPYONLY)
#        endforeach()
#    endforeach()
endfunction(copy_shave_headers LIBNAME)

function(copy_shave_ldscripts LIBNAME)
#    set(out_files)
#    file(MAKE_DIRECTORY ${MCC_LDSCRIPT_INSTALL_DIR})
#    foreach(LDSCRIPT_DIR ${ARGN})
#        file(GLOB_RECURSE files RELATIVE ${LDSCRIPT_DIR} ${LDSCRIPT_DIR}/*)
#        foreach(f ${files})
#            set(dst ${MCC_LDSCRIPT_INSTALL_DIR}/${f})
#            set(src ${LDSCRIPT_DIR}/${f})
#            configure_file(${src} ${dst} COPYONLY)
#        endforeach()
#    endforeach()
endfunction(copy_shave_ldscripts LIBNAME)
