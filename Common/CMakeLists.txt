asi_target (Common
  STATIC
  GAMES "LE1" "LE2" "LE3"
  SOURCES
    "Base.cpp"
    "Base.hpp"
    "Memory.cpp"
    "Memory.hpp"
    "Objects.cpp"
    "Objects.hpp"
    "ME3TweaksLogger.cpp"
    "ME3TweaksLogger.hpp"
    "Utils.cpp"
    "Utils.hpp"
    "json.hpp"
    "Mount.cpp"
    "Mount.hpp"
    "DefaultLogger.hpp"
    "DefaultLogger.cpp"
  LINKS
    "spdlog"
  VLINKS
    "LESDK"
)

asi_target (Common
  STATIC
  GAMES "LEL"
  SOURCES
    "Base.cpp"
    "Base.hpp"
    "DefaultLogger.hpp"
    "DefaultLogger.cpp"
  LINKS
    "spdlog"
  VLINKS
    "LESDK"
)
