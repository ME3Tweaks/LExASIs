# WwiseDebugBridge: thin re-export shim. Links the real Debug-config Wwise
# SDK static libraries and re-exports the exact same AK:: symbols
# MassEffect3.exe exports, via the generated Generated/WwiseDebugBridge.def -
# see WwiseIntercept/Tools/generate_exports.py. Building this successfully is
# itself the check that the SDK version/toolset is ABI-compatible with what
# the exe expects.
#
# LE3-only (this repo only cares about Wwise for LE3).

if (LEASI_BUILD_LE3)

  if (NOT DEFINED ENV{WWISESDK})
    message (FATAL_ERROR "!! WwiseDebugBridge requires the WWISESDK environment variable (Wwise SDK install) to be set")
  endif ()

  set (WWISESDK_ROOT "$ENV{WWISESDK}")
  # Debug (dynamic-CRT) variant, matching this repo's own /MDd Debug config -
  # NOT the "Debug(StaticCRT)" folder.
  set (WWISE_LIB_DIR "${WWISESDK_ROOT}/x64_vc150/Debug/lib")

  if (NOT EXISTS "${WWISE_LIB_DIR}")
    message (FATAL_ERROR "!! WwiseDebugBridge: Wwise Debug libs not found at ${WWISE_LIB_DIR} - check WWISESDK / SDK version")
  endif ()

  file (GLOB WWISE_DEBUG_LIBS LIST_DIRECTORIES false "${WWISE_LIB_DIR}/*.lib")

  set (BRIDGE_DEF_FILE "${CMAKE_CURRENT_SOURCE_DIR}/Generated/WwiseDebugBridge.def")
  if (NOT EXISTS "${BRIDGE_DEF_FILE}")
    message (FATAL_ERROR "!! WwiseDebugBridge: ${BRIDGE_DEF_FILE} not found - run WwiseIntercept/Tools/generate_exports.py first")
  endif ()

  add_library (WwiseDebugBridge SHARED
    "${CMAKE_CURRENT_SOURCE_DIR}/BridgeStub.cpp"
    "${BRIDGE_DEF_FILE}"
  )

  target_include_directories (WwiseDebugBridge PRIVATE "${WWISESDK_ROOT}/include")
  # ws2_32/dxguid/dsound satisfy Wwise's own Communication/DirectSound sink
  # libs (pulled in by the broad *.lib glob above even though nothing in our
  # hooked export set actually needs AK::Comm:: or the DirectSound sink).
  target_link_libraries (WwiseDebugBridge PRIVATE ${WWISE_DEBUG_LIBS} ws2_32.lib dxguid.lib dsound.lib)
  target_link_options (WwiseDebugBridge PRIVATE "/DEF:${BRIDGE_DEF_FILE}")

  set_target_properties (WwiseDebugBridge PROPERTIES OUTPUT_NAME "WwiseDebugBridge")

  source_group (TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES "${CMAKE_CURRENT_SOURCE_DIR}/BridgeStub.cpp" "${BRIDGE_DEF_FILE}")

  # WwiseIntercept-LE3 LoadLibraryW's this DLL from its own directory, so it
  # needs to land next to the .asi in the game's ASI folder too.
  if (DEFINED ENV{LEASI_GAME3_WIN64})
    set (LEASI_GAME3_WIN64 "$ENV{LEASI_GAME3_WIN64}")
    add_custom_command (TARGET WwiseDebugBridge POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:WwiseDebugBridge> "${LEASI_GAME3_WIN64}/ASI/$<TARGET_FILE_NAME:WwiseDebugBridge>")
  endif ()

endif ()
