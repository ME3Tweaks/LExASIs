asi_target (ControllerKeyboard
  SHARED
  GAMES  "LE3" "LMP"
  SOURCES
    "Entry.cpp"
    "Entry.hpp"
    "Hooks.cpp"
    "Hooks.hpp"
    "SharedVersion.h"
    "ControllerKeyboard.rc"
  VLINKS
    "Common"
    "LESDK"
  )

# Both keyboards are optional at build time; a missing SDK only drops that keyboard.

# Windows GDK: XGameUiShowTextEntryAsync. xgameruntime.lib loads XGameRuntime.dll itself at runtime, so a PC
# without the Gaming Runtime still loads the ASI and XGameRuntimeInitialize just fails.
set (LEASI_GDK_CANDIDATES "")

if (DEFINED ENV{GameDKLatest})
  list (APPEND LEASI_GDK_CANDIDATES "$ENV{GameDKLatest}")
endif ()

if (DEFINED ENV{GRDKLatest})
  list (APPEND LEASI_GDK_CANDIDATES "$ENV{GRDKLatest}")
endif ()

if (EXISTS "C:/Program Files (x86)/Microsoft GDK")
  file (GLOB LEASI_GDK_VERSIONS LIST_DIRECTORIES true "C:/Program Files (x86)/Microsoft GDK/*")
  list (SORT LEASI_GDK_VERSIONS COMPARE NATURAL ORDER DESCENDING)
  list (APPEND LEASI_GDK_CANDIDATES ${LEASI_GDK_VERSIONS})
endif ()

set (LEASI_GDK_ROOT "")

foreach (LEASI_GDK_CANDIDATE IN LISTS LEASI_GDK_CANDIDATES)
  if (EXISTS "${LEASI_GDK_CANDIDATE}/windows/include/XGameUI.h" AND EXISTS "${LEASI_GDK_CANDIDATE}/windows/lib/x64/xgameruntime.lib")
    set (LEASI_GDK_ROOT "${LEASI_GDK_CANDIDATE}")
    break ()
  endif ()

  get_filename_component (LEASI_GDK_PARENT "${LEASI_GDK_CANDIDATE}" DIRECTORY)

  if (EXISTS "${LEASI_GDK_PARENT}/windows/include/XGameUI.h" AND EXISTS "${LEASI_GDK_PARENT}/windows/lib/x64/xgameruntime.lib")
    set (LEASI_GDK_ROOT "${LEASI_GDK_PARENT}")
    break ()
  endif ()
endforeach ()

if (NOT LEASI_GDK_ROOT)
  message (WARNING "ControllerKeyboard: Windows GDK not found (set GameDKLatest or GRDKLatest); building without the Windows keyboard.")
endif ()

# Steamworks SDK: headers only. Nothing links steam_api64.lib, because the game does not ship steam_api64.dll and
# may not be a Steam copy; Hooks.cpp talks to the running Steam client's steamclient64.dll instead.
set (LEASI_STEAMWORKS_CANDIDATES "")

if (DEFINED ENV{STEAMWORKS_SDK_ROOT})
  list (APPEND LEASI_STEAMWORKS_CANDIDATES "$ENV{STEAMWORKS_SDK_ROOT}" "$ENV{STEAMWORKS_SDK_ROOT}/sdk")
endif ()

list (APPEND LEASI_STEAMWORKS_CANDIDATES
  "B:/UserProfile/Downloads/steamworks_sdk_165/sdk"
)

set (LEASI_STEAMWORKS_INCLUDE_DIR "")

foreach (LEASI_STEAMWORKS_CANDIDATE IN LISTS LEASI_STEAMWORKS_CANDIDATES)
  if (EXISTS "${LEASI_STEAMWORKS_CANDIDATE}/public/steam/steam_api.h")
    set (LEASI_STEAMWORKS_INCLUDE_DIR "${LEASI_STEAMWORKS_CANDIDATE}/public")
    break ()
  endif ()
endforeach ()

if (NOT LEASI_STEAMWORKS_INCLUDE_DIR)
  message (WARNING "ControllerKeyboard: Steamworks SDK not found (set STEAMWORKS_SDK_ROOT); building without the Steam keyboard.")
endif ()

foreach (GAME IN ITEMS "LE3" "LMP")
  if (NOT TARGET ControllerKeyboard-${GAME})
    continue ()
  endif ()

  if (LEASI_GDK_ROOT)
    target_include_directories (ControllerKeyboard-${GAME} PRIVATE "${LEASI_GDK_ROOT}/windows/include")
    target_link_directories (ControllerKeyboard-${GAME} PRIVATE "${LEASI_GDK_ROOT}/windows/lib/x64")
    target_link_libraries (ControllerKeyboard-${GAME} PRIVATE xgameruntime.lib advapi32)
    target_compile_definitions (ControllerKeyboard-${GAME} PRIVATE CONTROLLERKEYBOARD_HAS_GDK)
  endif ()

  if (LEASI_STEAMWORKS_INCLUDE_DIR)
    target_include_directories (ControllerKeyboard-${GAME} PRIVATE "${LEASI_STEAMWORKS_INCLUDE_DIR}")
    target_link_libraries (ControllerKeyboard-${GAME} PRIVATE advapi32)
    target_compile_definitions (ControllerKeyboard-${GAME} PRIVATE CONTROLLERKEYBOARD_HAS_STEAMWORKS)
  endif ()
endforeach ()
