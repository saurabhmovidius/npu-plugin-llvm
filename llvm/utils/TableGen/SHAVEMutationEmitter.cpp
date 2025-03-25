//===- SHAVEMutationmitter.cpp - Generate SHAVE Mutation mappings ---------===//
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
using namespace llvm;

namespace {
class SHAVEMutationMappingEmitter {
  RecordKeeper &Records;
public:
  explicit SHAVEMutationMappingEmitter(RecordKeeper &R) : Records(R) {}

  void run(raw_ostream &o);

private:
  void emitSHAVEMutations(raw_ostream &O,
                          std::vector<Record *> &mutations);
};
} // End anonymous namespace

void SHAVEMutationMappingEmitter::run(raw_ostream &O) {
  emitSourceFileHeader("SHAVE Mutation Descriptions", O);

  std::vector<Record *> Mutations = Records.getAllDerivedDefinitions("SHAVEMutation");
  emitSHAVEMutations(O, Mutations);
}

void SHAVEMutationMappingEmitter::emitSHAVEMutations(raw_ostream &O,
                                                     std::vector<Record *> &mutationRecords) {
  O << "SHAVESchedulerConflicts::Mutation"; // Return type (typedef to std::optional)
  O << " SHAVESchedulerConflicts::getMutation"; // Function name (declaration in SHAVEConflict.h)
  // The operands:
  //   - opcode - the opcode of the instruction we are mutating from
  //   - previous - the mutation that was previously returned by this function. If this is the first call then previous.position will be -1
  O << " (unsigned int opcode, const SHAVESchedulerConflicts::MutationInfo &previous)";
  O << " const {\n";

  std::string indent = "";
  auto incrementIndent = [&indent]() {
    indent += "  ";
  };
  auto decrementIndent = [&indent]() {
    indent.resize(indent.size() - 2);
  };

  auto outputMutationInfo = [&O](StringRef opcode, StringRef mutator, int position = 0) {
    O << "MutationInfo(SHAVE::" << opcode;
    if (!mutator.empty())
      O << ", &SHAVESchedulerConflicts::" << mutator;
    O << ", " << position << ")";
  };

  // Switch statement, this must return a value
  incrementIndent();
  O << indent << "switch(opcode) {\n";
  // Default return
  incrementIndent();
  O << indent << "default: return std::nullopt;\n";

  std::map<std::string, std::vector<Record *>> mutations;

  for (Record * mutationRecord : mutationRecords) {
    Record * from = mutationRecord->getValueAsDef("from");
    StringRef fromName = from->getName();

    Record * to = mutationRecord->getValueAsDef("to");

    mutations[fromName.str()].push_back(to);
  }

  for (auto &mutation : mutations) {
    // The "incoming" instruction opcode
    StringRef fromName = mutation.first;

    // List of "outgoing" instruction opcodes and function names
    std::vector<Record *> &toList = mutation.second;

    O << indent << "case SHAVE::" << fromName << ":\n";

    if (toList.size() > 1) {
      // More than one available mutation, so we define them as an array and return the next index to "previous"
      // on each call

      // Enter block
      O << indent << "{\n";
      incrementIndent();

      int position = 0;
      O << indent << "MutationInfo mutations[] = {\n";
      incrementIndent();
      for (Record * to : toList) {
        StringRef opcode = to->getValueAsDef("instruction")->getName();
        StringRef mutator = to->getValueAsString("mutator");
        O << indent;
        outputMutationInfo(opcode, mutator, position++);
        O << ",\n";
      }
      decrementIndent();
      O << indent << "};\n";

      O << indent << "if (previous.position >= " << (position-1) << ") return std::nullopt;\n";
      O << indent << "return mutations[previous.position + 1];\n";

      // Exit block
      decrementIndent();
      O << indent << "}\n";
    }
    else {
      // Only one available instruction, return it for the first call and then nullopt for all subsequent calls
      incrementIndent(); // Enter block

      Record * to = toList.front();
      StringRef opcode = to->getValueAsDef("instruction")->getName();
      StringRef mutator = to->getValueAsString("mutator");

      O << indent << "if (previous.position == -1) return ";
      outputMutationInfo(opcode, mutator);
      O << ";\n";

      // previous.position is >= 0, but there is only one mutation target so return nothing
      O << indent << "return std::nullopt;\n";

      decrementIndent(); // Exit block
    }
  }

  O << "  } // end of switch\n";
  O << "  llvm_unreachable(\"SHAVESchedulerConflicts::getMutation should have returned a value\");\n";

  O << "} // End of function SHAVESchedulerConflicts::getMutation\n\n";
}


static TableGen::Emitter::OptClass<SHAVEMutationMappingEmitter>
    X("gen-shave-mutation-mappings", "Generate SHAVE mutation mappings");
