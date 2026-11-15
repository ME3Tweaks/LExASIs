# WwiseIntercept: hooks every AK:: Wwise SDK export MassEffect3.exe has and
# redirects each into the matching export of WwiseDebugBridge.dll (built by
# the Bridge/ subdirectory), logging on entry. LE3-only. Hook declarations
# and definitions in Generated/ are produced by Tools/generate_exports.py and
# checked in - see that script and the WwiseIntercept plan for the full
# design (startup-timing / SEQATTACH reasoning, state-ownership reasoning,
# etc).

if (NOT DEFINED ENV{WWISESDK})
  message (FATAL_ERROR "!! WwiseIntercept requires the WWISESDK environment variable (Wwise SDK install) to be set")
endif ()

set (WWISESDK_ROOT "$ENV{WWISESDK}")

asi_target (WwiseIntercept
  SHARED
  GAMES "LE3"
  SOURCES
    "Entry.cpp"
    "Entry.hpp"
    "Hooks.cpp"
    "Hooks.hpp"
    "SharedVersion.h"
    "WwiseIntercept.rc"
    "Generated/WwiseExports.gen.hpp"
    "Generated/WwiseExports.gen.cpp"
  VLINKS
    "Common"
    "LESDK"
  )

target_include_directories (WwiseIntercept-LE3 PUBLIC "${WWISESDK_ROOT}/include")

# The Wwise SDK headers themselves (e.g. AkArray.h's template code) trip a
# few warnings under this repo's /W4 /WX - not our code to fix, so silence
# just for this target rather than loosening the project-wide warning set.
target_compile_options (WwiseIntercept-LE3 PRIVATE "/wd4100")

add_subdirectory ("Bridge")
