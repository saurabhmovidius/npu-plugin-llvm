//===- SHAVEConflictEmitter.cpp - Generate SHAVE Conflict descriptions ----===//
//
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
//
//===----------------------------------------------------------------------===//

#include "CodeGenTarget.h"
#include "llvm/TableGen/Error.h"
#include "llvm/TableGen/Record.h"
#include "llvm/TableGen/TableGenBackend.h"
#include <cassert>
#include <set>
using namespace llvm;

namespace {
class SHAVEConflictDescriptionEmitter {
  RecordKeeper &Records;
public:
  explicit SHAVEConflictDescriptionEmitter(RecordKeeper &R) : Records(R) {}

  void run(raw_ostream &o);

private:
  void emitSHAVEConflictEnum(raw_ostream &O, std::vector<Record *> &vector, std::string name);
  void emitSHAVEConflicts(raw_ostream &O, std::vector<Record *> &conflicts);
  void emitStructs(raw_ostream &O);
  void emitSHAVEConflictFunctions(raw_ostream &O, std::vector<Record *> &instructionConflicts);
};
} // End anonymous namespace

void SHAVEConflictDescriptionEmitter::run(raw_ostream &O) {
  emitSourceFileHeader("SHAVE Conflict Descriptions", O);

  O << "#ifndef SHAVE_CONFLICTS_INC\n";
  O << "#define SHAVE_CONFLICTS_INC (1)\n";

  O << "namespace llvm {\n";
  O << "namespace SHAVEConflicts {\n";

  // Emit the conflict types enum and corresponding bitset typedef
  std::vector<Record *> conflictTypes = Records.getAllDerivedDefinitions("SHAVEConflictType");
  emitSHAVEConflictEnum(O, conflictTypes, "SHAVEConflictType");

  // Emit the conflict conditions enum and corresponding bitset
  std::vector<Record *> conflictConditions = Records.getAllDerivedDefinitions("SHAVEConflictCondition");
  emitSHAVEConflictEnum(O, conflictConditions, "SHAVEConflictCondition");

  // Emit the conflict names enum and corresponding bitset
  std::vector<Record *> conflictNames = Records.getAllDerivedDefinitions("SHAVEConflictName");
  emitSHAVEConflictEnum(O, conflictNames, "SHAVEConflict");

  // Emit the SHAVEConflictPredicate and SHAVEConflictDescription structs
  emitStructs(O);

  // Emit the conflict table and the  function "initConflictTable" which initialises the table at runtime
  std::vector<Record *> conflicts = Records.getAllDerivedDefinitions("SHAVEConflict");
  emitSHAVEConflicts(O, conflicts);  
  
  // Emit the functions which check each of the conflicts. This emits a function per SHAVEConflict.
  // Each function emitted is simply a switch statement with cases for every opcode affected by
  // that conflict.
  std::vector<Record *> instructionConflicts = Records.getAllDerivedDefinitions("SHAVEInstr");
  emitSHAVEConflictFunctions(O, instructionConflicts);

  O << "} // end namespace SHAVEConflicts\n";
  O << "} // end namespace llvm\n";
  O << "#endif // SHAVE_CONFLICTS_INC\n";
}

void SHAVEConflictDescriptionEmitter::emitSHAVEConflictEnum(raw_ostream &O, std::vector<Record*> &vector, std::string name) {
  assert(!vector.empty());
  std::set<std::string> alreadyInserted;

  auto outputName = [&](StringRef name) {
    if (alreadyInserted.find(name.str()) == alreadyInserted.end())
      O << ",\n  " << name;
    alreadyInserted.insert(name.str());
  };

  O << "enum " << name << " {\n";
  O << "  " << vector[0]->getValueAsString("name") << " = 0";
  alreadyInserted.insert(vector[0]->getValueAsString("name").str());

  for (unsigned int i = 1; i < vector.size(); ++i) {
    outputName(vector[i]->getValueAsString("name"));
  }

  O << "\n};\n\n";
  O << "typedef std::bitset<" << vector.size() << "> " << name <<  "Set;\n\n";
}

void SHAVEConflictDescriptionEmitter::emitSHAVEConflicts(raw_ostream &O, std::vector<Record*> &conflicts) {
  O << "static const unsigned int conflictTableSize = " << conflicts.size() << ";\n";
  O << "static SHAVEConflictDescription conflictTable[conflictTableSize];\n\n";

  O << "static inline void initConflictTable() {\n";
  for (unsigned int i = 0; i < conflicts.size(); ++i) {
    Record * conflict = conflicts[i];
    O << "  // Initialise conflict " << conflict->getName() << "\n";
    
    std::vector<Record *> pred1 = conflict->getValueAsListOfDefs("predicate1Flags");
    for (Record * pred : pred1)
      O << "  conflictTable[" << i << "].pred1.conflicts.set(" << pred->getValueAsString("name") << ");\n";

    Record * pred1Cond = conflict->getValueAsDef("predicate1Condition");
    O << "  conflictTable[" << i << "].pred1.condition = " << pred1Cond->getValueAsString("name") << ";\n\n";

    std::vector<Record *> pred2 = conflict->getValueAsListOfDefs("predicate2Flags");
    for (Record * pred : pred2)
      O << "  conflictTable[" << i << "].pred2.conflicts.set(" << pred->getValueAsString("name") << ");\n";

    Record * pred2Cond = conflict->getValueAsDef("predicate2Condition");
    O << "  conflictTable[" << i << "].pred2.condition = " << pred2Cond->getValueAsString("name") << ";\n\n";

    std::vector<Record *> types = conflict->getValueAsListOfDefs("types");
    for (Record * type : types)
      O << "  conflictTable[" << i << "].types.set(" << type->getValueAsString("name") << ");\n";
    O << "\n";

    if (conflict->getValueAsString("optionMin") != "")
      O << "  conflictTable[" << i << "].minDelay = SHAVEOptions::" << conflict->getValueAsString("optionMin") << ";\n";
    else
      O << "  conflictTable[" << i << "].minDelay = " << conflict->getValueAsInt("minimumDelay") << ";\n";

    if (conflict->getValueAsString("optionMax") != "")
      O << "  conflictTable[" << i << "].maxDelay = SHAVEOptions::" << conflict->getValueAsString("optionMax") << ";\n\n";
    else
      O << "  conflictTable[" << i << "].maxDelay = " << conflict->getValueAsInt("maximumDelay") << ";\n\n";
  }
  O << "}\n";
}

void SHAVEConflictDescriptionEmitter::emitStructs(raw_ostream &O) {
  O << "struct SHAVEConflictPredicate {\n";
  O << "  SHAVEConflictSet conflicts;\n";
  O << "  SHAVEConflictCondition condition;\n";
  O << "};\n\n";

  O << "struct SHAVEConflictDescription {\n";
  O << "  SHAVEConflictPredicate pred1;\n";
  O << "  SHAVEConflictPredicate pred2;\n";
  O << "  uint64_t port;\n";
  O << "  unsigned int minDelay;\n";
  O << "  unsigned int maxDelay;\n";
  O << "  SHAVEConflictTypeSet types;\n";
  O << "};\n\n";
}

void SHAVEConflictDescriptionEmitter::emitSHAVEConflictFunctions(raw_ostream &O, std::vector<Record *> &instructionConflicts) {
  std::map<std::string, std::vector<Record *> > conflictInstrLists;
  std::map<std::string, std::map<int, std::vector<Record *>>> conflictValueLists;

  // Generate a list of instructions for each conflict type
  for (Record * instr : instructionConflicts) {
    std::vector<Record *> conflicts = instr->getValueAsListOfDefs("conflicts");
    for (Record * conflict : conflicts) {
      std::string conflictName(conflict->getValueAsString("name"));
      if (conflictInstrLists.find(conflictName) == conflictInstrLists.end())
        conflictInstrLists[conflictName] = std::vector<Record *>();
      conflictInstrLists[conflictName].push_back(instr);

      if (conflict->getValueAsBit("hasValue"))
        conflictValueLists[conflictName][conflict->getValueAsInt("value")].push_back(instr);
    }
  }

  // Emit a function for each conflict
  for (auto &conflict : conflictInstrLists) {
    O << "static bool check_" << conflict.first << "(unsigned int opcode) {\n";
    O << "  switch(opcode) {\n";
    O << "  default:\n";
    O << "     return false;\n";
    for (Record * instr : conflict.second) {
      O << "  case SHAVE::" << instr->getName() << ":\n";
    }
    O << "    return true;\n";
    O << "  }\n";
    O << "}\n\n";
  }

  // Emit a function for each conflict with a value attached
  for (auto &conflict : conflictValueLists) {
    O << "static int getValueFor_" << conflict.first << "(unsigned int opcode) {\n";
    O << "  switch(opcode) {\n";
    O << "  default:\n";
    O << "     llvm_unreachable(\"getValueFor_" << conflict.first << " should not be called without first establishing the conflict type is valid\");\n";
    for (auto &value : conflict.second) {
      for (Record * instruction : value.second)
        O << "  case SHAVE::" << instruction->getName() << ":\n";
      O << "    return " << value.first << ";\n";
    }
    O << "  }\n";
    O << "}\n\n";
  }

  // Emit the function "getConflictSet" which uses all of the functions that have just
  // been generated to create a full SHAVEConflictSet bitset for any given opcode at runtime
  O << "static inline SHAVEConflictSet getConflictSet(unsigned int opcode) {\n";
  O << "  SHAVEConflictSet conflictSet;\n";
  for (auto &conflict : conflictInstrLists) {
    O << "  conflictSet.set(" << conflict.first << ", check_" << conflict.first << "(opcode));\n";
  }
  O << "  return conflictSet;\n";
  O << "}\n\n";
}

static TableGen::Emitter::OptClass<SHAVEConflictDescriptionEmitter>
    X("gen-shave-conflict-descriptions", "Generate SHAVE conflict descriptions");
