asi_target (LinkerPrinter
  SHARED
  GAMES "LE1" "LE2" "LE3"
  SOURCES
    "Entry.cpp"
    "Entry.hpp"
    "Hooks.cpp"
    "Hooks.hpp"
    "SharedVersion.h"
    "LinkerPrinter.rc"
  VLINKS
    "Common"
    "LESDK"
  )
