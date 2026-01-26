asi_target (DebugLogger
  SHARED
  GAMES "LE1" "LE2" "LE3"
  SOURCES
    "Entry.cpp"
    "Entry.hpp"
    "Hooks.cpp"
    "Hooks.hpp"
    "LE1Hooks.cpp"
    "LE1Hooks.hpp"
    "LE2Hooks.cpp"
    "LE2Hooks.hpp"
    "LE3Hooks.cpp"
    "LE3Hooks.hpp"
    "VerboseLogger.hpp"
    "VerboseLogger.cpp"
    "PropertyLogger.hpp"
    "PropertyLogger.cpp"
    "SharedVersion.h"
    "DebugLogger.rc"
  VLINKS
    "Common"
    "LESDK"
  )
