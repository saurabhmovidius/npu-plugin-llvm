//===- SHAVETestTargetFeaturesEmitter.cpp - Generate SHAVE Target Features -===//
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
#include "llvm/TableGen/Record.h"
#include "llvm/TableGen/TableGenBackend.h"
#include <regex>

using namespace llvm;

namespace {
class SHAVETestTargetFeaturesEmitter {
  RecordKeeper &Records;
public:
  explicit SHAVETestTargetFeaturesEmitter(RecordKeeper &R) : Records(R) {}

  void run(raw_ostream &o);

private:
  void emitHeader(raw_ostream& O);
  std::vector<const Record*> getNPUs();
  void emitFeatures(raw_ostream& O, std::vector<const Record*>& NPUs);
};
} // End anonymous namespace

void SHAVETestTargetFeaturesEmitter::run(raw_ostream &O) {
  emitHeader(O);

  auto NPUs = getNPUs();
  emitFeatures(O, NPUs);
}

void SHAVETestTargetFeaturesEmitter::emitHeader(raw_ostream& O) {
  O << "# This file has been auto-generated as part of the moviCompile build process\n";
  O << "# It specifies a list of target features which are supported on each NPU platform\n";
  O << "# This is used by the test-runner to enable/disable tests based on the FEATURE tag in test CONFIGs\n";
  O << "\n";
}

std::vector<const Record*> SHAVETestTargetFeaturesEmitter::getNPUs() {
  std::vector<Record*> processors = Records.getAllDerivedDefinitions("ProcessorModel");

  std::vector<const Record*> NPUs;
  for (Record* processor : processors) {
    Record* model = processor->getValueAsDef("SchedModel");
    if (model && model->getName() == "SHAVEModel")
      NPUs.push_back(processor);
  }

  return NPUs;
}

void SHAVETestTargetFeaturesEmitter::emitFeatures(raw_ostream& O, std::vector<const Record*>& NPUs) {
  O << "targetFeatures = {\n";

  for (const Record* npu : NPUs) {
    O << "  \"" << npu->getValueAsString("Name") << "\" :\n"; // Start of new dict entry for target
    O << "  [\n"; // Start of list of features

    std::vector<Record*> features = npu->getValueAsListOfDefs("Features");
    for (Record* feature : features) {
      std::string featureName = feature->getName().str();
      featureName = std::regex_replace(featureName, std::regex("_Feature$"), "");
      O << "    \"" << featureName << "\",\n";
    }

    O << "  ]"; // End of list of features
    O << ",\n"; // End of dict entry
  }

  O << "}\n";
}

static TableGen::Emitter::OptClass<SHAVETestTargetFeaturesEmitter>
    X("gen-shave-test-target-features", "Generate SHAVE test target features");
