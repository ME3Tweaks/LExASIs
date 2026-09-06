asi_target (PNGScreenshots
  SHARED
  GAMES "LE1" "LE2" "LE3" "LMP"
  EXTRA_DEBUGGER_ARGS "-transparentrender"
  SOURCES
    "Entry.cpp"
    "Entry.hpp"
    "Hooks.cpp"
    "Hooks.hpp"
    "TransparentRender.cpp"
    "TransparentRender.hpp"
    "fpng/fpng.h"
    "fpng/fpng.cpp"
    "SharedVersion.h"
    "PNGScreenshots.rc"
  VLINKS
    "Common"
    "LESDK"
  )
