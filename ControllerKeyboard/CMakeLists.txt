asi_target (ControllerKeyboard
  SHARED
  GAMES  "LE3"
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

if (TARGET ControllerKeyboard-LE3)
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
    set (LEASI_GDK_INCLUDE_DIR "${LEASI_GDK_CANDIDATE}/windows/include")
    set (LEASI_GDK_X64_LIB_DIR "${LEASI_GDK_CANDIDATE}/windows/lib/x64")

    if (EXISTS "${LEASI_GDK_INCLUDE_DIR}" AND EXISTS "${LEASI_GDK_X64_LIB_DIR}/xgameruntime.lib")
      set (LEASI_GDK_ROOT "${LEASI_GDK_CANDIDATE}")
      break ()
    endif ()

    get_filename_component (LEASI_GDK_PARENT "${LEASI_GDK_CANDIDATE}" DIRECTORY)
    set (LEASI_GDK_INCLUDE_DIR "${LEASI_GDK_PARENT}/windows/include")
    set (LEASI_GDK_X64_LIB_DIR "${LEASI_GDK_PARENT}/windows/lib/x64")

    if (EXISTS "${LEASI_GDK_INCLUDE_DIR}" AND EXISTS "${LEASI_GDK_X64_LIB_DIR}/xgameruntime.lib")
      set (LEASI_GDK_ROOT "${LEASI_GDK_PARENT}")
      break ()
    endif ()
  endforeach ()

  if (LEASI_GDK_ROOT)
    target_include_directories (ControllerKeyboard-LE3 PRIVATE "${LEASI_GDK_ROOT}/windows/include")
    target_link_directories (ControllerKeyboard-LE3 PRIVATE "${LEASI_GDK_ROOT}/windows/lib/x64")
    target_link_libraries (ControllerKeyboard-LE3 PRIVATE xgameruntime.lib)
  else ()
    message (WARNING "ControllerKeyboard: Windows GDK root not found. Set GameDKLatest or GRDKLatest.")
  endif ()
endif ()
