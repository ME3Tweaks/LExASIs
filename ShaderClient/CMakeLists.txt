asi_target (ShaderClient
  SHARED
  GAMES "LE1" "LE2" "LE3"
  SOURCES
    "Entry.cpp"
    "Entry.hpp"
    "Hooks.cpp"
    "Hooks.hpp"
    "Client.cpp"
    "Client.hpp"
    "PipeHandler.cpp"
    "PipeHandler.hpp"
    "Helpers.cpp"
    "Helpers.hpp"
    "MilanShaderCompiler.hpp"
    "MilanShaderCompiler.cpp"
    "MilanShaderCompilerShared.h"
    "MilanShaderCompilerShared.cpp"
    "SharedVersion.h"
    "ShaderClient.rc"
  VLINKS
    "Common"
    "LESDK"
  )
