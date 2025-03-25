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

#ifndef SHAVEVERSIONINFO_H
#define SHAVEVERSIONINFO_H (1)


#include <sstream>
#include <string>

#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"

//  Include the Version information from the compiler's 'common' folder
#include "../../../../common/include/moviCompileVersion.h"


// Ensure that the restricted distribution banner is always displayed for non-release builds
#if defined(NDEBUG) && !defined(MOVI_CUSTOM) && !defined(MOVI_ALPHA) && !defined(MOVI_BETA) && !defined(MOVI_INTERNAL) && !defined(MOVI_HOTFIX) && !defined(MOVI_RC) && !defined(__CYGWIN__)
# define MOVI_RELEASE (1)
#endif // NDEBUG && !MOVI_CUSTOM && !MOVI_ALPHA && !MOVI_BETA && !MOVI_INTERNAL && !MOVI_HOTFIX && !MOVI_RC && !__CYGWIN__

//  The Movidius tools version triplet
namespace SHAVEVersionInfo {
  class VersionTriplet {
  public: //  User interface
    unsigned int getMajorRevision() const { return MOVI_MAJOR_REVISION; }
    unsigned int getMinorRevision() const { return MOVI_MINOR_REVISION; }
    unsigned int getPatchRevision() const { return MOVI_PATCH_REVISION; }

    const std::string &getVersionString() const { return versionString; }
    const std::string &getLongVersionString() const { return longVersionString; }

    //  The VersionTriplet object is a singleton
  static inline
    const VersionTriplet &getVersionTriplet() {
      static  VersionTriplet versionTriplet;

      return versionTriplet;
    }

  private:  //  Copying is restricted
    VersionTriplet(const VersionTriplet &);
    VersionTriplet &operator = (const VersionTriplet &);
    //  Destruction too
    ~VersionTriplet() {}

  private:  //  Implementation interface
    VersionTriplet() {
      //  Handle internal versions differently; make it clear they are not for customers
#ifdef MOVI_INTERNAL
      versionString = "0.0.0";
      longVersionString = "0.0.0 !!! NOTE: Unversioned Movidius internal build. Do NOT distribute under any circumstances";
#else //  MOVI_INTERNAL
      //  Convert the triplet into a string
      std::stringstream tmpVer;
      tmpVer << std::dec << getMajorRevision();
      tmpVer << "." << std::dec << getMinorRevision();
      tmpVer << "." << std::dec << getPatchRevision();

      versionString = tmpVer.str();
# ifdef __CYGWIN__
      tmpVer << " [Cygwin]";
# endif // __CYGWIN__

# ifndef NDEBUG
      tmpVer << " DEBUG version. Do not distribute.";
# elif defined(MOVI_CUSTOM)
      tmpVer << " Custom Build. Do not distribute.";
# elif defined(MOVI_ALPHA)
      tmpVer << " Alpha #" << MOVI_ALPHA << ". Restricted distribution.";
# elif defined(MOVI_BETA)
      tmpVer << " Beta #" << MOVI_BETA << ". Not for general distribution.";
# elif defined(MOVI_HOTFIX)
      tmpVer << " HotFix #" << MOVI_HOTFIX;
# elif defined(MOVI_RC)
      tmpVer << " RC" << MOVI_RC << ". Not for general distribution.";
# endif
      longVersionString = tmpVer.str();
#endif //  MOVI_INTERNAL
    }

  private:  //  Implementation data
      std::string versionString;
      std::string longVersionString;
  };

  inline unsigned int getMajorRevision() { return VersionTriplet::getVersionTriplet().getMajorRevision(); }
  inline unsigned int getMinorRevision() { return VersionTriplet::getVersionTriplet().getMinorRevision(); }
  inline unsigned int getPatchRevision() { return VersionTriplet::getVersionTriplet().getPatchRevision(); }

  inline const std::string &getVersionString () { return VersionTriplet::getVersionTriplet().getVersionString(); }
  inline const std::string &getLongVersionString () { return VersionTriplet::getVersionTriplet().getLongVersionString(); }
}


#endif  // SHAVEVERSIONINFO_H
