#!/usr/bin/env python3

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
#  File       :   intrinsicsGenerator.py
#  Description:   Script for adding intrinsic instructions to LLVM and clang
#  ---------------------------------------------------------------------------
#  This will automatically generate the relevant code for:
#         tools/clang/lib/CodeGen/CGBuiltin.cpp
#         tools/clang/include/clang/Basic/BuiltinsSHAVE.def
#         lib/Target/SHAVE/SHAVEInstrInfo.cpp
#         include/llvm/IR/IntrinsicsShave.td
#         lib/Target/SHAVE/SHAVEISelDAGtoDAG.cpp
#  New intrinsic functions can be added to the compiler by adding the
#  relevant information on the instruction to ./SHAVEIntrinsics.csv
#  ---------------------------------------------------------------------------
#  CSV Format by column:
#
#     0. Functional unit the instruction belongs to
#     1. Name of the instruction
#     2. Instruction suffix indicating type of instruction
#     3. Optional additional suffix for TD name
#     4. The return type of the instruction.
#     5. The number of arguments the instruction takes
#     6. The types of each argument; separated by '.'
#     7. Other: dot-separated set of additional attributes and properties:
#           '0x[a-fA-F0-9]*'  - a constant hex value for an immediate value added by the compiler.
#           'boolimm'         - for a number of immediates that are represented in the instruction as booleans: instead of i8 i8 i8 i8 => [DEED]
#           'colimm'          - for an immediate number which specifies a column of a vector: e.g. v4i32 i8 => v20.1.
#           'notype'          - the instruction is not prepended with type information.
#           'noreturn'        - the instruction ends execution.
#           'overload         - when a set of type differentiated intrinsics would otherwise have the same name
#           'srcdst           - the destination is also a source
#           'stoh'            - for an intrinsic which stores the higher half of the result.
#           'stol'            - for an intrinsic which stores the lower half of the result.
#     8. Overload discriminator - used when the Other column contains 'overload'
#     9. Dot separated list of subtarget features (empty implies all NPU versions are supported) matching those defined in SHAVE.td
#
#  Comments are denoted with // at the start of the line only
#  Public comments have only the characters '//' in the first column
#  ---------------------------------------------------------------------------

import csv
from optparse import OptionParser
import os
import re
import sys


# Use names to identify the columns - hard-wired numbers are difficult to understand and maintain
colFuncUnit       = 0
colOPName         = 1
colOPSuffix       = 2
colTDSuffix       = 3
colReturnType     = 4
colNumArgs        = 5
colArgTypes       = 6
colOtherFlags     = 7
colOverloadSuffix = 8
colTargetFeatures = 9


# Given a set of target features, return a comma separated list
def getTargetFeaturesString(targetFeatures):
  return targetFeatures.replace('.', ', ')

# Routine to write a public comment to the output file
def emitPublicComment( out, row, beginComment, endComment="" ):
  # Public comments have '//' alone in the first column, ignore otherwise
  if row[ colFuncUnit ] == '//':
    isEmpty = 1

    # Check the remaining columns to find if they are all empty or not
    for col in row[ colOPName: ]:
      if '' != col:
        isEmpty = 0
        break

    # If all the remaining columns in the row are blank or empty, then just write out a newline
    if isEmpty:
      out.write( '\n' )
    else:
      # Otherwise start the comment with the file type specific comment-to-eol prefix (includes trailing spaces if required, e.g. '//  ', '# ' or '/*' with '*/')
      out.write( beginComment )

      # Each empty or blank column from left to right represents a comment indentation of 4-spaces
      # The first non-empty, non-blank column is written literally to the output and all subsequent columns are ignored
      for col in row[ colOPName: ]:
        if '' != col:
          out.write( col + endComment + '\n' )
          break
        else:
          out.write( '    ' )


def getOperandKinds( row ):
  types = ''

  for c in row[ colArgTypes ]:
    if (c == 'R') or (c =='P'):
      types += 'r'
    elif c == '#':
      types += 'i'

  return types


def getOperandNames( row ):
  argNames = []
  ptrNo = 1
  varNo = 1
  immNo = 1

  for c in row[ colArgTypes ]:
    if c == 'P':
      argNames.append('pointer_' + str(ptrNo))
      ptrNo += 1
    elif c == 'R':
      argNames.append('variable_' + str(varNo))
      varNo += 1
    elif c == '#':
      argNames.append('immediate_' + str(immNo))
      immNo += 1

  return argNames


# Write intrinsics for 'tools/clang/lib/CodeGen/CGBuiltin.cpp'
def writeCGBuiltins( lines, CGBuiltinFilePath ):
  try:
    outFile = open( CGBuiltinFilePath, 'w' )
  except:
    print("Unable to open the file '" + CGBuiltinFilePath + "' for writing")
    sys.exit( 1 )

  # Write out the file preamble
  outFile.write( "// Auto-generated by 'utils/Target/SHAVE/intrinsicsGenerator.py'\n" )
  outFile.write( "// for inclusion by 'CGBuiltin.cpp'\n\n" )

  # Generate main code body
  for row in lines:
    try:
      if '//' in row[ colFuncUnit ]: # Copy comments across
        emitPublicComment( outFile, row, '//  ' )
      elif row[ colFuncUnit ] != "": # Insert builtin function definition for current intrinsic
        intrinsic =  row[ colFuncUnit ] + '_' + row[ colOPName ]
        if row[ colOPSuffix ] != '':
          intrinsic += '_' + row[ colOPSuffix ]

        # Add suffix to intrinsic based on operand types (register or immediate)
        if row[ colNumArgs ] != '0':
          intrinsic += '_' + getOperandKinds(row)

        # Does it need to handle overloaded builtins?
        other = row[ colOtherFlags ]
        if '.' in other:
          other = other.split( '.' )
        else:
          other = [ other ]

        # If this builtin needs to be overloaded, suffix with the value in the overload discrimination column
        if 'overload' in other:
          if row[ colOverloadSuffix ] != '':
            intrinsic += '_' + row[ colOverloadSuffix ]
          else:
            raise Exception ( 'Error in source CSV; discrimination value required for overload attribute' )

        outFile.write( '    case SHAVE::BI__builtin_shave_' + intrinsic.lower() + ':\n' )
        outFile.write( '        ID = Intrinsic::shave_' + intrinsic.lower() + ';\n' )
        outFile.write( '        break;\n' )
      else: # empty line
        outFile.write( '\n' )
    except Exception as e:
      print(e)
      pass

  # Tidy up
  outFile.close()


# Write intrinsics for 'tools/clang/include/clang/Basic/BuiltinsSHAVE.def'
def writeBuiltinsSHAVE( lines, BuiltinsSHAVEFilePath ):
  print('Going to ' + BuiltinsSHAVEFilePath)
  try:
    outFile = open( BuiltinsSHAVEFilePath, 'w' )
  except:
    print("Unable to open the file '" + BuiltinsSHAVEFilePath + "' for writing")
    sys.exit( 1 )

  # Write out the file preamble
  outFile.write( "// Auto-generated by 'utils/Target/SHAVE/intrinsicsGenerator.py'\n" )
  outFile.write( "// for inclusion by 'BuiltinsSHAVE.def'\n" )
  outFile.write( "// The format of this database matches 'clang/Basic/Builtins.def'\n\n" )

  # Generate main code body
  for row in lines:
    try:
      if '//' in row[ colFuncUnit ]: # Copy comments across
        emitPublicComment( outFile, row, '//  ' )
      elif row[ colFuncUnit ] != "": # Insert builtin function definition for current intrinsic
        typeString = getClangTypes( row )
        intrinsic = row[ colFuncUnit ] + '_' + row[ colOPName ]
        if row[ colOPSuffix ] != '':
          intrinsic += '_' + row[ colOPSuffix ]

        # Add suffix to intrinsic based on operand types (register or immediate)
        if row[ colNumArgs ] != '0':
          intrinsic += '_' + getOperandKinds( row )

        # Does it need to handle overloaded builtins?
        other = row[ colOtherFlags ]
        if '.' in other:
          other = other.split( '.' )
        else:
          other = [ other ]

        # If this builtin needs to be overloaded, suffix with the value in the overload discrimination column
        if 'overload' in other:
          if row[ colOverloadSuffix ] != '':
            intrinsic += '_' + row[ colOverloadSuffix ]
          else:
            raise Exception ( 'Error in source CSV; discrimination value required for overload attribute' )

        # Allow the builtin modifier to handle intrinsics that do not return control
        defModifier = 'n'
        if 'noreturn' in other:
          defModifier += 'r'

        outFile.write( 'BUILTIN(__builtin_shave_' + intrinsic.lower() + ', "' + typeString + '", "' + defModifier + '")\n' )
      else: # empty line
        outFile.write( '\n' )
    except Exception as e:
      print(e)
      pass

  # Tidy up
  outFile.close()


# Helper function for writeBuiltinsSHAVE that converts the type signature
# in the CSV into the format required by clang
def getClangTypes( row ):
  if row[ colReturnType ] == '': # No return type, i.e. function return type is void
    clangString = 'v'
    typeString = row[ colArgTypes ] + '.'
  else:
    clangString = ''
    typeString = row[ colReturnType ] + '.' + row[ colArgTypes ] + '.'

  # Scan the type string to compose the CLang type for the intrinsic
  curChar = 0
  isPtr = 0
  cvPtr = 0

  while curChar < len( typeString ):
    # Is this a pointer?
    if typeString[ curChar ] == 'P':
      curChar += 1
      isPtr = 1

      # Handle the 'const' and 'volatile' type qualifiers
      while (typeString[ curChar ] == 'C') or (typeString[ curChar ] == 'V'):
        if typeString[ curChar ] == 'C':
          cvPtr |= 1
        else:
          cvPtr |= 2
        curChar += 1

    # If it a vector?
    if typeString[ curChar ] == 'v':
      clangString += 'V'
      curChar += 1
      while (curChar < len( typeString )) and typeString[ curChar ].isdigit():
        clangString += typeString[ curChar ]
        curChar += 1

    elif typeString[ curChar ] in ( 'u', 'i' ): # Signed and unsigned integers
      if typeString[ curChar ] == 'u':
        clangString += 'U'
      curChar += 1
      size = 0
      while (curChar < len( typeString )) and typeString[ curChar ].isdigit():
        size = size*10 + int( typeString[ curChar ] )
        curChar += 1

      if size == 0: # No size given
        instr = row[ colFuncUnit ] + '_' + row[ colOPName ]
        if row[ colOPSuffix ] != '':
          instr += '_' + row[ colOPSuffix ]
        raise Exception('No size given on integer operand for instruction ' + instr)

      if size <= 8:
        clangString += 'c'
      elif size <= 16:
        clangString += 's'
      elif size <= 32:
        clangString += 'i'
      elif size <= 64:
        clangString += 'LLi'
      else:
        instr = row[ colFuncUnit ] + '_' + row[ colOPName ]
        if row[ colOPSuffix ] != '':
          instr += '_' + row[ colOPSuffix ]
        raise Exception('Integer operand too large for instruction ' + instr)

    elif typeString[ curChar ] == 'f': # Float
      curChar += 1
      size = 0

      while (curChar < len( typeString )) and typeString[ curChar ].isdigit():
        size = size*10 + int( typeString[ curChar ] )
        curChar += 1

      if size == 32:
        clangString += 'f'
      elif size == 16:
        clangString += 'h'
      elif size == 8:
        clangString += 'c'
      else:
        instr = row[ colFuncUnit ] + '_' + row[ colOPName ]
        if row[ colOPSuffix ] != '':
          instr += '_' + row[ colOPSuffix ]
        raise Exception('Floating point operand with unsupported size in type string for instruction ' + instr)

    elif typeString[ curChar ] == 'R':
      curChar += 1

    elif typeString[ curChar ] == '#':
      curChar += 1
      # Immediate arguments are required to be constant-expressions
      clangString += 'I'

    elif typeString[ curChar ] == '.':
      # Start a new argument, so append any pending pointer information to the previous argument
      if isPtr:
        if (cvPtr & 1) == 1:  # Const
          clangString += 'C'
        if (cvPtr & 2) == 2:  # Volatile
          clangString += 'D'
        clangString += '*'

      # Reset for the next argument
      isPtr = 0
      cvPtr = 0
      curChar += 1

    else:
      instr = row[ colFuncUnit ] + '_' + row[ colOPName ]
      if row[ colOPSuffix ] != '':
        instr += '_' + row[ colOPSuffix ]
      raise Exception('Unrecognized symbol ' + typeString[ curChar ] + ' in type string for instruction ' + instr)

  return clangString


# Write intrinsics for 'lib/Target/SHAVE/SHAVEInstrInfo.cpp'
def writeSHAVEIntrinsics( lines, SHAVEIntrinsicsFilePath ):
  try:
    outFile = open( SHAVEIntrinsicsFilePath, 'w' )
  except:
    print("Unable to open the file '" + SHAVEIntrinsicsFilePath + "' for writing")
    sys.exit( 1 )

  # Write out the file preamble
  outFile.write( "// Auto-generated by 'utils/Target/SHAVE/intrinsicsGenerator.py'\n" )
  outFile.write( "// for inclusion by 'SHAVEInstrInfo.cpp'\n\n" )

  # Generate main code body
  for row in lines:
    try:
      if '//' in row[ colFuncUnit ]: # Copy comment across
        emitPublicComment( outFile, row, '//  ' )
      elif row[ colFuncUnit ] != "": # Insert builtin function definition for current intrinsic
        instruction = row[ colFuncUnit ] + '_' + row[ colOPName ]
        intrinsic = instruction
        if row[ colOPSuffix ] != '':
          instruction += '_' + row[ colOPSuffix ]
          intrinsic += '_' + row[ colOPSuffix ]
        if row[ colTDSuffix ] != '':
          instruction += '_' + row[ colTDSuffix ]

        # Add suffix to intrinsic based on operand types (register or immediate)
        if row[ colNumArgs ] != '0':
          opTypes = getOperandKinds( row )
          intrinsic += '_' + opTypes
          if 'i' in opTypes:
            instruction = row[ colFuncUnit ] + '_' + row[ colOPName ]
            if row[ colOPSuffix ] != '':
              instruction += '_' + row[ colOPSuffix ]
            instruction += '_imm'
            if row[ colTDSuffix ] != '':
              instruction += '_' + row[ colTDSuffix ]

        # Does it need to handle overloaded builtins?
        other = row[ colOtherFlags ]
        if '.' in other:
          other = other.split( '.' )
        else:
          other = [ other ]

        # If this builtin needs to be overloaded, suffix with the value in the overload discrimination column
        if 'overload' in other:
          if row[ colOverloadSuffix ] != '':
            intrinsic += '_' + row[ colOverloadSuffix ]
          else:
            raise Exception ( 'Error in source CSV; discrimination value required for overload attribute' )

        if len( row ) <= colTargetFeatures:
          outFile.write( '    case Intrinsic::shave_' + intrinsic.lower() + ': { opcode = SHAVE::' + instruction + '; break; }\n' )
        else:
          # Get the set of target features (possibly empty)
          targetFeatures = row[ colTargetFeatures ]
          if "HasVRF512" in targetFeatures:
            instruction += "_VRF512" # Avoid any potential ambiguity with equivalent VRF128 instructions

          # Does this apply to all CPUs?
          if targetFeatures == "":
            outFile.write( '    case Intrinsic::shave_' + intrinsic.lower() + ': { opcode = SHAVE::' + instruction + '; break; }\n' )
          else:
            # Otherwise construct a decision tree based on CPUs supported
            if '.' in targetFeatures:
              targetFeatures = targetFeatures.split( '.' )
            else:
              targetFeatures = [ targetFeatures ]

            outFile.write( '    case Intrinsic::shave_' + intrinsic.lower() + ':\n' )
            outFile.write( '            builtinName << "shave_' + intrinsic.lower() + '";\n' )
            first = " "
            outFile.write( '            if (' )

            for targetFeature in targetFeatures:
              outFile.write( first + 'SHAVEST.hasFeature(SHAVE::' + targetFeature + '_Feature)' )
              first = " && "

            outFile.write( ")\n" )
            outFile.write( "              opcode = SHAVE::" + instruction + ";\n" )
            outFile.write( "            else { /* Diagnose that 'Intrinsic::shave_" + intrinsic.lower() + "' is unavailable for this NPU */ }\n" )
            outFile.write( "        break;\n" )
      else: # empty line
        outFile.write( '\n' )
    except Exception as e:
      print(e)
      pass

  # Tidy up
  outFile.close()


# Write intrinsics for 'include/llvm/IR/IntrinsicsShave.td'
def writeIntrinsicsShave( lines, IntrinsicsShaveFilePath ):
  try:
    outFile = open( IntrinsicsShaveFilePath, 'w' )
  except:
    print("Unable to open the file '" + IntrinsicsShaveFilePath + "' for writing")
    sys.exit( 1 )

  # Write out the file preamble
  outFile.write( "// Auto-generated by 'utils/Target/SHAVE/intrinsicsGenerator.py'\n" )
  outFile.write( "// for inclusion by 'IntrinsicsShave.td'\n\n" )

  # Generate main code body
  for row in lines:
    try:
      if '//' in row[ colFuncUnit ]: # Copy comments across
        emitPublicComment( outFile, row, '//  ' )
      elif row[ colFuncUnit ] != "": # Insert builtin function definition for current intrinsic
        other = row[ colOtherFlags ]
        if '.' in other:
          other = other.split( '.' )
        else:
          other = [ other ]

        args = row[ colArgTypes ].split('.')
        has_ret = (row[ colReturnType ] != '')
        ret = row[ colReturnType ]
        intrinsic = row[ colFuncUnit ] + "_" + row[ colOPName ]
        if row[ colOPSuffix ] != '':
          intrinsic += '_' + row[ colOPSuffix ]

        # Add suffix to intrinsic based on operand types (register or immediate)
        if row[ colNumArgs ] != '0':
          intrinsic += '_' + getOperandKinds( row )

        # If this builtin needs to be overloaded, suffix with the value in the overload discrimination column
        if 'overload' in other:
          if row[ colOverloadSuffix ] != '':
            intrinsic += '_' + row[ colOverloadSuffix ]
          else:
            raise Exception ( 'Error in source CSV; discrimination value required for overload attribute' )

        content = "    def int_shave_" + intrinsic.lower() + " : "
        content += "SHAVE_Intrinsic<\"" + intrinsic.lower() + "\", [ "
        if has_ret:
          ret = fixupType( ret )
          content += "llvm_" + ret + "_ty"
        content += " ], [ "

        accessesMem = False
        readsMem = False
        writesMem = False
        for a in args:
          if a[0] == 'P':
            accessesMem = True
            if a[1] == 'C':
              if a[2] != 'V':
                readsMem = True
            else:
              writesMem = True
          
          a = fixupType( a )
          content += "llvm_" + a + "_ty," # Need to strip the leading 'R' or '#'

        content = content[ :-1 ] + " ], ["
        if "noreturn" in other:
          content += "IntrNoReturn"
        elif not accessesMem and not ("sideeffects" in other):
          content += "IntrNoMem"
        elif accessesMem:
          if readsMem:
            content += "IntrReadMem"
          elif writesMem:
            content += "IntrWriteMem"
        content += "]>;\n"

        outFile.write( content )
      else: # empty line
        outFile.write( '\n' )
    except Exception as e:
      print(e)
      pass

  # Done
  outFile.close()


# Helper function for writeIntrinsicsShave
# Re-writes a type string into the format required by 'include/llvm/IntrinsicsShave.td'
def fixupType( typ ):
  typ = typ.replace( 'u', 'i' )

  if typ[ 0 ] == 'P':
    return 'ptr'
  elif typ == 'Rf32':
    return 'float'
  elif typ == 'Rf16':
    return 'half'

  i = 0
  newTyp = ''
  imm = False
  skip = False
  # Iterate through the type string searching for immediate values to fix
  # All else is simply copied over to the result string (Except for 'R' and '#')
  while i < len( typ ):
    skip = False
    if typ[ i ] == '#':
      imm = True
      i += 1
    elif typ[ i ] == 'R':
      imm = False
      i += 1

    if typ[ i ] == 'i' and imm: # immediate value may need to be resized
      skip = True
      newTyp += 'i'
      i += 1
      if i <= len( typ )-2: # Potential 2 digit sized immediate
        if typ[i].isdigit( ) and typ[i+1].isdigit( ):
          size = int( typ[ i:i+2 ] )
          i += 2
        elif typ[ i ].isdigit( ):
          size = int( typ[ i:i+1 ] )
          i += 1
        else:
          raise Exception( 'Could not parse type string ' + typ )
      elif i <= len( typ )-1 and typ[ i ].isdigit( ): # 1 digit sized immediate
        size = int( typ[ i:i+1 ] )
        i += 1
      else:
        raise Exception( 'Could not parse type string ' + typ )

      if size <= 8:
        newTyp += '8'
      elif size <= 16:
        newTyp += '16'
      elif size <= 32:
        newTyp += '32'
      elif size <= 64:
        newTyp += '64'
      else:
        raise Exception( 'Immediate value greater than 64 bits in type string ' + typ )

    if not skip: # Do not copy next character if we just saw an immediate
      newTyp += typ[ i ]
      i += 1

  return newTyp


# Write C header containing function prototypes for all generated builtin functions
def writePseudoHeader( lines, CHeaderFilePath ):
  try:
    outFile = open( CHeaderFilePath, 'w' )
  except:
    print("Unable to open the file '" + CHeaderFilePath + "' for writing")
    sys.exit( 1 )

  # Write out the file preamble
  outFile.write( "/*\n" )
  outFile.write( " *  Auto-generated for 'moviCompile'\n" )
  outFile.write( " *\n" )
  outFile.write( " *  ---------------------------------------------------------------------------\n" )
  outFile.write( " *  {% copyright %}\n" )
  outFile.write( " *  ---------------------------------------------------------------------------\n" )
  outFile.write( " *  File       :   shave-builtin-pseudo-declarations.h\n" )
  outFile.write( " *  Description:   This header contains pseudo function prototypes for all SHAVE\n" )
  outFile.write( " *                 ISA builtins, it is provided for informative reasons for SHAVE\n" )
  outFile.write( " *                 programmers and does not need to be included by an actual C\n" )
  outFile.write( " *                 source file.\n" )
  outFile.write( " *  ---------------------------------------------------------------------------\n" )
  outFile.write( " *\n" )
  outFile.write( " *  Each builtin has an exact correspondence to an instruction in the SHAVE ISA.\n" )
  outFile.write( " *\n" )
  outFile.write( " *  Naming convention:\n" )
  outFile.write( " *        __builtin_shave_<functional-unit>_<instruction>_<instruction-type>_<operand-types>\n" )
  outFile.write( " *\n" )
  outFile.write( " *      For example: __builtin_shave_vau_add_i32s_ri\n" )
  outFile.write( " *        This is the builtin for VAU.ADD.i32s <vrf:Dst> <vrf:srcA> <immu5:B>\n" )
  outFile.write( " *  ---------------------------------------------------------------------------\n" )
  outFile.write( " */\n\n" )

  outFile.write( "#ifndef __SHAVE_BUILTIN_PSEUDO_DECLARATIONS__\n" )
  outFile.write( "#define __SHAVE_BUILTIN_PSEUDO_DECLARATIONS__ (1)\n\n" )

  outFile.write( "#ifndef __shave__\n\n" )

  outFile.write( "#include <moviVectorUtils.h>\n\n" )

  outFile.write( "_MV_EXTERNC_BEGIN\n" )

  for row in lines:
    try:
      # Don't attempt to parse empty lines or comments
      if '//' in row[ colFuncUnit ]: # Copy comments across
        emitPublicComment( outFile, row, '/*  ', ' */' )
      elif ( row[ colFuncUnit ] != '' ):
        instruction = row[ colFuncUnit ] + '.' + row[ colOPName ]
        intrinsic = row[ colFuncUnit ] + '_' + row[ colOPName ]
        if row[ colOPSuffix ] != '':
          instruction += '.' + row[ colOPSuffix ].replace( '_', '.' )
          intrinsic += '_' + row[ colOPSuffix ]
        numberOfArgs = int( row[ colNumArgs ] )

        # Add suffix to intrinsic based on operand types (register or immediate)
        if numberOfArgs != 0:
          intrinsic += '_' + getOperandKinds( row )

        # Does it need to handle overloaded builtins?
        other = row[ colOtherFlags ]
        if '.' in other:
          other = other.split( '.' )
        else:
          other = [ other ]

        # For builtins that do not directly correspond to an instruction, suppress the 'instruction' comment
        if 'meta' in other:
          instruction = 'Meta-Instruction'

        # If this builtin needs to be overloaded, suffix with the value in the overload discrimination column
        if 'overload' in other:
          if row[ colOverloadSuffix ] != '':
            intrinsic += '_' + row[ colOverloadSuffix ]
          else:
            raise Exception ( 'Error in source CSV; discrimination value required for overload attribute' )

        builtin = '__builtin_shave_' + intrinsic.lower()

        # Get return type of the function
        if row[ colReturnType ] == '':
          returnType = 'void'
        else:
          returnType = getCType( row[ colReturnType ], 0 )

        # Get the argument types for the function
        argTypes = []
        for i in range( numberOfArgs ):
          argTypes.append(getCType( row[ colArgTypes ], i ))

        # Handle the 'noreturn' attribute
        if 'noreturn' in other:
          line = '__attribute__((noreturn))\t' + returnType + '\t' + builtin + ' ( '
        else:
          line = returnType + '\t' + builtin + ' ( '

        argNames = []
        argNames = getOperandNames( row )

        for i in range( len( argTypes ) ):
          line += argTypes[ i ] + ' ' + argNames[ i ] + ', '

        if len( row ) <= colTargetFeatures:
          line = line[ :-2 ] + ' );\t/* ' + instruction + ' */\n'
        else:
          # Get the set of CPU types (possibly empty)
          targetFeatures = row[ colTargetFeatures ]

          # Does this apply to all CPUs?
          if targetFeatures == "":
            line = line[ :-2 ] + ' );\t/* ' + instruction + ' */\n'
          else:
            # Otherwise construct a decision tree based on CPUs supported
            if '.' in targetFeatures:
              targetFeatures = targetFeatures.split( '.' )
            else:
              targetFeatures = [ targetFeatures ]

            line = line[ :-2 ] + ' );\t/* ' + instruction

            first = '  : Only NPUs with feature(s) '
            for targetFeature in targetFeatures:
              line += first + targetFeature
              first = ', '

            line += ' */\n'

        outFile.write( line )
    except Exception as e:
      print(e)
      pass

  # Write out the file postamble
  outFile.write( "\n\n_MV_EXTERNC_END\n\n" )

  outFile.write( "#endif /* __shave__ */\n" )
  outFile.write( "#endif /* __SHAVE_BUILTIN_PSEUDO_DECLARATIONS__ */\n" )
  outFile.close()


# Helper function for 'writePseudoHeader' and 'writeCTests'
# Converts a type string in the CSV format into the corresponding C type
def getCType( typeStrings, index ):
  typeStr = typeStrings.split( '.' )[ index ]

  i = 0
  isPtr = 0
  cvPtr = 0

  # It does not matter if this is an immediate or a register
  if (typeStr[ i ] == 'R') or (typeStr[ i ] == '#'):
    i += 1
  elif typeStr[ i ] == 'P':
    # But pointers are interesting
    i += 1
    isPtr = 1;

    # Handle the 'const' and 'volatile' type qualifiers
    while (typeStr[ i ] == 'C') or (typeStr[ i ] == 'V'):
      if typeStr[ i ] == 'C':
        cvPtr |= 1
      else:
        cvPtr |= 2
      i += 1

  if typeStr[ i ] == 'v': # Vector
    vecSize = ''
    i += 1
    while typeStr[ i ].isdigit():
      vecSize += typeStr[ i ]
      i += 1

    prefix = ''
    if typeStr[ i ] == 'u':
      prefix = 'u'

    if typeStr[ i ] == 'i' or typeStr[ i ] == 'u':
      i += 1
      size = int( typeStr[ i: ] )
      if size <= 8:
        if prefix == '':
          cType = 'schar' + vecSize
        else:
          cType = prefix + 'char' + vecSize
      elif size <= 16:
        cType = prefix + 'short' + vecSize
      elif size <= 32:
        cType = prefix + 'int' + vecSize
      else:
        cType = prefix + 'longlong' + vecSize
    elif typeStr[ i ] == 'f':
      i += 1
      size = int( typeStr[ i: ] )
      if size == 8:
        cType = 'char' + vecSize
      elif size == 16:
        cType = 'half' + vecSize
      else:
        cType = 'float' + vecSize
  else:
    prefix = ''
    if typeStr[ i ] == 'u':
      prefix = 'unsigned '

    if typeStr[ i ] == 'i' or typeStr[ i ] == 'u':
      i += 1
      size = int( typeStr[ i: ] )
      if size <= 8:
        if prefix == '':
          cType = 'signed char'
        else:
          cType = prefix + 'char'
      elif size <= 16:
        cType = prefix + 'short'
      elif size <= 32:
        cType = prefix + 'int'
      else:
        cType = prefix + 'long long'
    elif typeStr[ i ] == 'f':
      i += 1
      size = int( typeStr[ i: ] )
      if size == 8:
        cType = 'char'
      elif size == 16:
        cType = 'half'
      else:
        cType = 'float'

  # If it is a pointer, then modify the type string
  if isPtr:
    if (cvPtr & 2) == 2:
      cType = 'volatile ' + cType;
    if (cvPtr & 1) == 1:
      cType = 'const ' + cType;

    cType = cType + ' *'

  return cType


# Helper function for 'writeIRTests'
# Converts a type string in the CSV format into the corresponding IR type
def getIRType( typeStrings, index ):
  typeStr = typeStrings.split( '.' )[ index ]

  i = 0
  isPtr = 0
  cvPtr = 0

  # It does not matter if this is an immediate or a register
  if (typeStr[ i ] == 'R') or (typeStr[ i ] == '#'):
    i += 1
  elif typeStr[ i ] == 'P':
    # But pointers are interesting
    i += 1
    isPtr = 1;

    # Handle the 'const' and 'volatile' type qualifiers
    while (typeStr[ i ] == 'C') or (typeStr[ i ] == 'V'):
      if typeStr[ i ] == 'C':
        cvPtr |= 1
      else:
        cvPtr |= 2
      i += 1

  if typeStr[ i ] == 'v': # Vector
    vecSize = ''
    i += 1
    while typeStr[ i ].isdigit():
      vecSize += typeStr[ i ]
      i += 1

    if (typeStr[ i ] == 'i') or (typeStr[ i ] == 'u'):
      i += 1
      size = int(typeStr[ i: ])
      if size <= 8:
        size = 8
      elif size <= 16:
        size = 16
      elif size <= 32:
        size = 32
      elif size <= 64:
        size = 64
      irType = '<' + vecSize + ' x i' + str(size) + '>'
    elif typeStr[ i ] == 'f':
      i += 1
      size = int( typeStr[ i: ] )
      if size == 8:
        irType = '<' + vecSize + ' x i8>'
      elif size == 16:
        irType = '<' + vecSize + ' x half>'
      elif size == 32:
        irType = '<' + vecSize + ' x float>'
  else:
    if (typeStr[ i ] == 'i') or (typeStr[ i ] == 'u'):
      i += 1
      size = int(typeStr[ i: ])
      if size <= 8:
        size = 8
      elif size <= 16:
        size = 16
      elif size <= 32:
        size = 32
      elif size <= 64:
        size = 64
      irType = 'i' + str(size)
    elif typeStr[ i ] == 'f':
      i += 1
      size = int( typeStr[ i: ] )
      if size == 8:
        irType = 'i8'
      elif size == 16:
        irType = 'half'
      else:
        irType = 'float'

  # If it is a pointer, then modify the type string
  # FIXME: Movidius - What is the IR encoding for pointers and CV-qualifiers?
#  if isPtr:
#    if (cvPtr & 2) == 2:
#      irType = 'volatile ' + irType;
#    if (cvPtr & 1) == 2:
#      irType = 'const ' + irType;
#
#    irType = irType + ' *'

  return irType


# Write the function body for static function GetIntrinsicInfo in 'lib/Target/SHAVE/SHAVEISelDAGtoDAG.cpp'
# GetIntrinsicInfo is responsible for providing information on where immediate operands exist in a function prototype
def writeSHAVEGetIntrinsicInfo( lines, SHAVEISelDAGtoDAGFilePath ):
  try:
    outFile = open( SHAVEISelDAGtoDAGFilePath, 'w' )
  except:
    print("Unable to open the file '" + SHAVEISelDAGtoDAGFilePath + "' for writing")
    sys.exit( 1 )

  # Write out the file preamble
  outFile.write( "// This file has been automatically generated by 'utils/Target/SHAVE/intrinsicsGenerator.py'\n" )
  outFile.write( "// It has been generated for inclusion in 'lib/Target/SHAVE/SHAVEISelDAGtoDAG.cpp'\n\n" )

  for row in lines:
    try:
      # Don't attempt to parse empty lines or comments
      if '//' in row[ colFuncUnit ]: # Copy comments across
        emitPublicComment( outFile, row, '//  ' )
      elif ( row[ colFuncUnit ] != '' ) and ( '//' not in row[ colFuncUnit ] ):
        if '#' in row[ colArgTypes ]:
          intrinsic = row[ colFuncUnit ] + '_' + row[ colOPName ]
          if row[ colOPSuffix ] != '':
            intrinsic += '_' + row[ colOPSuffix ]

          # Add suffix to intrinsic based on operand types (register or immediate)
          opTypes = getOperandKinds( row )
          intrinsic += '_' + opTypes

          # Does it need to handle overloaded builtins?
          other = row[ colOtherFlags ]
          if '.' in other:
            other = other.split( '.' )
          else:
            other = [ other ]

          # If this builtin needs to be overloaded, suffix with the value in the overload discrimination column
          if 'overload' in other:
            if row[ colOverloadSuffix ] != '':
              intrinsic += '_' + row[ colOverloadSuffix ]
            else:
              raise Exception ( 'Error in source CSV; discrimination value required for overload attribute' )

          firstImmIdx = opTypes.find( 'i' )
          lastImmIdx = opTypes.rfind( 'i' )

          outFile.write( '    case Intrinsic::shave_' + intrinsic.lower() + ':\n' )
          outFile.write( '      firstImmIdx = ' + str(firstImmIdx) + ';\n' )
          outFile.write( '      lastImmIdx = ' + str(lastImmIdx) + ';\n' )
          outFile.write( '      break;\n' )
    except Exception as e:
      print(e)
      pass

  # Done
  outFile.close()


# returns a formatted instruction name as it is described in the ISA
def getInstrName( instr, typstr ):

  # build instruction name
  name = instr[ colFuncUnit ].lower( ) + '.' + instr[ colOPName ].lower( )
  if instr[ colOPSuffix ] != '':
    name += '.' + instr[ colOPSuffix ].replace( '_', '.' )

  # append typestr to name
  if not typstr == '':
    name += "." + typstr

  # If this builtin needs to be overloaded, suffix with the value in the overload discrimination column
  if 'overload' in instr[colOtherFlags]:
    if instr[ colOverloadSuffix ] != '':
      name += '_' + instr[ colOverloadSuffix ]
    else:
      raise Exception ( 'Error in source CSV; discrimination value required for overload attribute' )

  return name


# write IR tests for the intrinsics
def writeIRTests( instrs, outputPath ):

  for instr in instrs:

    # ignore comments etc.
    if instr[ colFuncUnit ] == '' or '//' in instr[ colFuncUnit ]:
      continue

    # get args
    numberOfArgs = int( instr[ colNumArgs ] )

    # Get the argument types for the function
    args = []
    for i in range( numberOfArgs ):
      args.append( getIRType( instr[ colArgTypes ], i ) )

    # get argument types r/i and ISA name
    typstr = getOperandKinds( instr )
    name = getInstrName( instr, typstr ).lower()
    typstr = list( typstr )

    # get return type
    if instr[ colReturnType ] == '':
      ret = "void"
    else:
      ret = getIRType( instr[ colReturnType ], 0 )
    args.insert( 0, ret )

    # get 'other' field
    other = instr[ colOtherFlags ].rstrip('\r\n')

    if '.' in other:
      other = other.split( '.' )
    else:
      other = [ other ]

    # remove vector part of type
    # FIXME: Movidius - this doesn't look right, and looked wrong even before my recent changes to add 'colTDSuffix'
    typ = instr[ colOPSuffix ]
    if "v" in typ:
      while True:
        typ = typ[ 1: ]
        try:
          integer = int( typ[ :1 ] )
        except:
          break

    # dummy registers
    regs = [ "%a", "%b", "%c", "%d", "%e", "%f", "%g" ]

    # write declare
    ret = args.pop( 0 )
    decl = "declare " + ret + " @llvm.shave." + name + "(";
    i = 0
    for a in args:
      decl += " " + a + " " + regs[ i ] + ","
      i += 1
    if len( args ) != 0:
      decl = decl[ :-1 ]
    decl += " )"

    # copy decl to create definition
    content = decl + "\n" + decl.replace( "declare", "define" ).replace( "llvm.shave.", "" )
    content += "{\n"
    content += "\t; CHECK: " + instr[ colFuncUnit ] + "." + instr[ colOPName ]

    # check if type should be appended
    if not "notype" in other:
      n = typ

      # remove _l (lower), _h (higher)
      if ( "stol" in other or "stoh" in other ) and '_' in n:
        n = n.split( '_' )[ 0 ]

      content += "." + n

    # if OTHER field has "srcdst" source and "dst" are the same
    if not "srcdst" in other and not ret == "void":
      typstr.insert( 0, ' ' )
      args.insert( 0, ret )

    # write CHECK regex for generated instruction
    i = 0
    for a in args:
      if a.startswith( "<" ):
        content += " {{v[0-9]+"
      elif a == "half" or a == "float" :
        content += " {{s[0-9]+"
      elif typstr[ i ] == "i":
        # boolimm option
        if "boolimm" in other:
          content += " {{.*}}"
          break
        else:
          # colimm option
          if "colimm" in other:
            content += "."
          else:
            content += " "
          content += "{{[0-9]+"
      else:
        content += " {{i[0-9]+"
      # stol, stoh option
      if i == 0:
        if "stol" in other:
          content += ".l"
        elif "stoh" in other:
          content += ".h"
      content += "}}"
      i += 1

    if not "srcdst" in other and not ret == "void":
      typstr.pop( 0 )
      args.pop( 0 )

    # if OTHER field has constant value, add to instruction
    for o in other:
      try:
        hx = int( o, 16 )
        content += " " + o + "\n"
        break
      except:
        pass
    content += "\n"

    # copy decl to create call
    call = "call"
    if not ret == "void":
      call = "%ret = " + call
    declare = "\t" + decl.replace( "declare", call ) + "\n"
    i = 0
    for a in args:
      if typstr[ i ] == "i":
        declare = declare.replace( regs[ i ], "1" )
      i += 1
    content += declare + "\tret " + ret
    if not ret == "void":
      content += " %ret"
    content += "\n}\n"

    # open LL file for writing
    outputName = name.replace( '.', '_' )
    fn = outputPath + '/' + outputName + ".ll"
    print("writing IR test .. " + fn)
    try:
      fh = open( fn, 'w' )
    except:
      raise Exception( "Could not open file for writing: " + fn )

    # write the test meta-data at the head of the file
    fh.write( "; RUN: llc -march shave < %s | FileCheck %s\n\n" )
    fh.write( ";==> Test Meta-Data\n" )
    fh.write( "; Auto-generated by 'utils/Target/SHAVE/intrinsicsGenerator.py'\n" )
    fh.write( ";\n" )
    fh.write( "; CATEGORY intrinsics\n" )
    fh.write( "; RUN[elf-ld] False\n" )
    fh.write( "; RUN[FileCheck] True\n" )
    if not instr[ colTargetFeatures ] == '':
      targetFeatures = getTargetFeaturesString(instr[ colTargetFeatures ])
      fh.write( "; FEATURE " + targetFeatures + '\n' )
    fh.write( ";\n" )
    fh.write( ";<==\n\n" )

    # write the test itself
    fh.write( content )
    fh.close( )


# write C tests for the intrinsic
def writeCTests( instrs, outputPath ):

  # ensure the output directory actually exists
  if not os.path.exists(outputPath):
    os.mkdir(outputPath)

  for instr in instrs:
    # ignore empty lines, comments etc.
    if not instr or instr[ colFuncUnit ] == '' or '//' in instr[ colFuncUnit ]:
      continue

    # Breakout addition flags
    other = instr[ colOtherFlags ]
    if '.' in other:
      other = other.split( '.' )
    else:
      other = [ other ]

    # Get args
    numberOfArgs = int( instr[ colNumArgs ] )

    # Get the argument types for the function
    args = []
    for i in range( numberOfArgs ):
      cType = getCType( instr[ colArgTypes ], i )
      args.append( cType )

    # Get argument kinds r/i and ISA name
    typstr = getOperandKinds( instr )
    name = getInstrName( instr, typstr ).lower()

    # Handle the 'noreturn' attribute
    if 'noreturn' in other:
      ret = '__attribute__((noreturn)) '
    else:
      ret = ''

    # Get the return type
    if instr[ colReturnType ] == '':
      ret += "void"
    else:
      ret += getCType( instr[ colReturnType ], 0 )

    # write function
    content = ret + " test_" + instr[ colFuncUnit ] + "_" + instr[ colOPName ]
    if instr[ colOPSuffix ] != '':
      content += '_' + instr[ colOPSuffix ]
    content += "("

    # params
    i = 0
    nRegArgs = 0
    for a in args:
      if typstr[i] == 'r':
        content += a + " var_" + str( i ) + ', '
        nRegArgs += 1
      i += 1
    if nRegArgs != 0:
      content = content[ :-2 ]  # Remove the trailing comma and space after the final argument (if present)
    content += "){\n"

    # checks
    # LSU instructions will get emitted as LSU0 or LSU1. Check with a regexp
    instrStr = instr[ colFuncUnit ]
    if instrStr  == "LSU":
      instrStr += "{{[01]}}"


    # MOVIDIUS_FIXME: The builtins should be renamed to avoid special handling like this
    # Map instructions where the builtin name deviates from the naming protocol:
    #     1. Instructions like LD128 and ST128 get output as LD.128 and ST.128
    #     2. CMBBYTE and CMBWORD get output as CMB.BYTE and CMB.WORD
    #     3. VSZMBYTE and VSZMWORD get output as VSZM.BYTE and VSZM.WORD
    OPNameStr = instr[ colOPName ].replace("LD", "LD.")
    OPNameStr= OPNameStr.replace("ST", "ST.")
    OPNameStr= OPNameStr.replace("CMB", "CMB.")
    OPNameStr= OPNameStr.replace("VSZM", "VSZM.")
    instrStr += '.' + OPNameStr

    # MOVIDIUS_FIXME: The builtins should be renamed to avoid special handling like this
    # Handle suffixes in Intrinsics like:
    #         bitwise ops which do not require the suffix
    #         bsf_32/bsfinv_32 which do not require the suffix (the .32 is not a part of the mnemonics)
    #         sau_atn_f16_l which map to SAU.ATN <reg> <reg>.L
    #         sau_atn_f16_h which map to SAU.ATN <reg> <reg>.H
    #         vau_abs_v2f32 which maps to VAU.ABS32.f32
    # Finally Convert  _ in suffixes to .
    OPSuffixStr = instr[ colOPSuffix ]
    if OPNameStr == "SQT" and OPSuffixStr == "f16":
       OPSuffixStr = ""

    # MOVIDIUS_FIXME: This should ideally only tests for other != "notype"
    # but many builtins are not marked with notype while they should be
    if not "notype" in other and OPSuffixStr != "":
       if not re.search(r"f16_([lh])", OPSuffixStr):
            instrStr+="."
       OPSuffixStr = OPSuffixStr.replace("f16_l", "{{[ i0-9]+}}.L")
       OPSuffixStr = OPSuffixStr.replace("f16_h", "{{[ i0-9]+}}.H")
       OPSuffixStr = re.sub(r'v[0-9]+', '', OPSuffixStr)
       instrStr +=  OPSuffixStr.replace( '_', '.' )

    content += "// CHECK: " + instrStr + '\n\t';

    # call intrinsic
    if not instr[ colReturnType ] == '':
      content += "return "
    content += "__builtin_shave_" + name.replace( '.', '_' ) + '(';
    i = 0
    for a in args:
      if typstr[i] == 'r':
        content += "var_" + str( i ) + ', '
      else:
        content += "1, "
      i += 1
    if i != 0:
      content = content[ :-2 ]
    content += ");\n}\n"

    # open C file for writing
    outputName = name.replace( '.', '_' )
    fn = outputPath + '/' + outputName + ".c"
    print("Writing C test .. " + fn)
    try:
      fh = open( fn, 'w' )
    except:
      raise Exception( "Could not open file for writing: " + fn )

    # write the test meta-data at the head of the file
    fh.write( "//==> Test Meta-Data\n" )
    fh.write( "// Auto-generated by 'utils/Target/SHAVE/intrinsicsGenerator.py'\n" )
    fh.write( "//\n" )
    fh.write( "// CATEGORY intrinsics\n" )
    fh.write( "// RUN[elf-ld] False\n" )
    fh.write( "// RUN[FileCheck] True\n" )
    if not instr[ colTargetFeatures ] == '':
      targetFeatures = getTargetFeaturesString(instr[ colTargetFeatures ])
      fh.write( "// FEATURE " + targetFeatures + '\n' )
    fh.write( "//\n" )
    fh.write( "//<==\n\n" )

    # write the test itself
    fh.write( "#include <moviVectorTypes.h>\n\n" )
    fh.write( content + '\n' )
    fh.close()


def main( argv ):

  # arguments
  usage  = "intrinsicsGenerator.py -h \n"
  usage += "   {-s|--csv-source} <csvfile> {-o|--output} <output-file> --gen-{SHAVECGBuiltins|SHAVEIntrinsics|IntrinsicsShave|pseudo-header|BuiltinsSHAVE|SHAVEGetIntrinsicInfo|C-tests|IR-tests}"

  # Initialise the option parser
  parser = OptionParser(usage=usage)
  parser.add_option("-o", "--output", dest="output", \
                    help="path to where the output for the selected option should be located. For tests this names a directory", default=False)
  parser.add_option("-s", "--csv-source", dest="source", \
                    help="path to the source CSV containing the intrinsic definitions", default=False)
  parser.add_option(      "--gen-SHAVECGBuiltins", dest="cgbuiltins", help="create the file 'SHAVECGBuiltins.inc' required by 'tools/clang/lib/CodeGen/CGBuiltin.cpp'", \
                    action="store_true", default=False)
  parser.add_option(      "--gen-SHAVEIntrinsics", dest="shaveintrinsics", help="create the file 'SHAVEIntrinsics.inc' required by 'lib/Target/SHAVE/SHAVEInstrInfo.cpp'", \
                    action="store_true", default=False)
  parser.add_option(      "--gen-IntrinsicsShave", dest="intrinsicsshave", help="create the file 'IntrinsicsShave.inc' required by 'include/llvm/IR/IntrinsicsShave.td'", \
                    action="store_true", default=False)
  parser.add_option(      "--gen-pseudo-header", dest="pseudoheader", help="create the file 'shave-builtin-pseudo-declarations.h'", \
                    action="store_true", default=False)
  parser.add_option(      "--gen-BuiltinsSHAVE", dest="builtinsshave", help="create the file 'BuiltinsSHAVE.inc' required by 'tools/clang/include/clang/Basic/BuiltinsSHAVE.def'", \
                    action="store_true", default=False)
  parser.add_option(      "--gen-SHAVEGetIntrinsicInfo", dest="shavegetintrinsicinfo", help="create the file 'SHAVEGetIntrinsicInfo.inc' required by 'lib/Target/SHAVE/SHAVEISelDAGtoDAG.cpp'", \
                    action="store_true", default=False)
  parser.add_option(      "--gen-IR-tests", dest="irtests", help="create the LLVM IR tests for the intrinsics {'-o' names the directory}", \
                    action="store_true", default=False)
  parser.add_option(      "--gen-C-tests", dest="ctests", help="create the C tests for the intrinsics {'-o' names the directory}", \
                    action="store_true", default=False)
  (options, args) = parser.parse_args()


  # Check for mutually exclusive arguments
  exclusive = [ "cgbuiltins", "shaveintrinsics", "intrinsicsshave", "pseudoheader", "builtinsshave", "shavegetintrinsicinfo", "irtests", "ctests" ]
  arg = False
  for e in exclusive:
    if getattr(options, e, False) == True:
      if arg == True:
        print("USAGE: " + usage)
        sys.exit( 1 )
      arg = True

  # Check that all the necessary arguments have been provided
  if not options.source:
    print("ERROR: no CSV source file provided; use '-s' or '--csv-source=' to specify")
    print("Usage: " + usage)
    sys.exit(1)
  if not options.output:
    print("ERROR: no output file provided; use '-o' or '--output=' to specify")
    print("Usage: " + usage)
    sys.exit(1)
  if arg == False:
    print("ERROR: no '--gen-*' option selected")
    print("Usage: " + usage)
    sys.exit(1)
  else:
    # Attempt to open the CSV file
    try:
      csvFile = open( options.source, "r" )
      csvReader = csv.reader( csvFile )
    except:
      print("ERROR: Unable to open CSV source file \"" + options.source + "\" for reading")
      sys.exit( 1 );

    # Read in the CSV source file
    csvLines = []
    for line in csvReader:
      csvLines.append( line )

    # Generate the selected type of file
    if options.cgbuiltins:
      writeCGBuiltins( csvLines, options.output )
    elif options.shaveintrinsics:
      writeSHAVEIntrinsics( csvLines, options.output )
    elif options.intrinsicsshave:
      writeIntrinsicsShave( csvLines, options.output )
    elif options.pseudoheader:
      writePseudoHeader( csvLines, options.output )
    elif options.builtinsshave:
      writeBuiltinsSHAVE( csvLines, options.output )
    elif options.shavegetintrinsicinfo:
      writeSHAVEGetIntrinsicInfo( csvLines, options.output )
    elif options.irtests:
      writeIRTests( csvLines, options.output )
    elif options.ctests:
      writeCTests( csvLines, options.output )


if __name__ == "__main__":
  sys.exit( main( sys.argv[1:]))
