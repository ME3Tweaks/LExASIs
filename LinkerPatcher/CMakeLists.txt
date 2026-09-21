asi_target (LinkerPatcher
  SHARED
  GAMES "LE3" "LMP"
  SOURCES
    "Entry.cpp"
    "Entry.hpp"
    "Hooks.cpp"
    "Hooks.hpp"
    "SharedVersion.h"
    "LinkerPatcher.rc"
  VLINKS
    "Common"
    "LESDK"
  )
