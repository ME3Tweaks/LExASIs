#pragma once

// This file is used for shared version info in C++ and RC files.
// This file is effectively templated and should be located in all ASI mod projects.

// Todo: Move to a single version file for all asis
#define CURRENT_YEAR "2026"

#define DEVELOPER_A "ME3Tweaks"
#define DEVELOPER_W L"ME3Tweaks"
#define ASI_NAME_A "Linker Patcher"
#define ASI_NAME_W L"Linker Patcher"
#define ASI_NAME_NO_SPACE_A "LinkerPatcher"
#define ASI_NAME_NO_SPACE_W L"LinkerPatcher"


#define ASI_VERSION 1
#define ASI_DESCRIPTION "Linker Patcher force-loads a LinkerPatches config and redirects targeted object loads to replacement packages, similar to ME3's original DLC_TestPatch mechanism."




// === Identification from automated ME3Tweaks tooling ====
// TODO: Real ASI_GROUP_ID_RC values need to be assigned by ME3Tweaks tooling before release.
// 0 is used as a placeholder here, matching the convention other not-yet-registered ASIs in this
// repo use (e.g. ConfigDumper, ShaderClient, ControllerKeyboard).
#if defined(SDK_TARGET_LE1)
#define GAME_PREFIX_RC    "LE1"
#define ASI_GROUP_ID_RC 0
#define ASI_GAME_ID_RC 4
#endif

#if defined(SDK_TARGET_LE2)
#define GAME_PREFIX_RC    "LE2"
#define ASI_GROUP_ID_RC 0
#define ASI_GAME_ID_RC 5
#endif

#if defined(SDK_TARGET_LE3)
#define GAME_PREFIX_RC    "LE3"
#define ASI_GROUP_ID_RC 0
#define ASI_GAME_ID_RC 6
#endif


// Constants for all ASIs - DO NOT CHANGE BELOW
// Helper macros for converting version numbers to strings
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

// Comma-separated version for use in the .rc file's FILEVERSION/PRODUCTVERSION
// and logging in c++.
// Version.Unused.GameId.GroupId
#define APP_VERSION_RC ASI_VERSION, 0, ASI_GAME_ID_RC, ASI_GROUP_ID_RC
#define VERSION_STRING_A TOSTRING(ASI_VERSION) ".0." TOSTRING(ASI_GAME_ID_RC) "." TOSTRING(ASI_GROUP_ID_RC)
#define VERSION_STRING_W TOSTRING(ASI_VERSION) L".0." TOSTRING(ASI_GAME_ID_RC) L"." TOSTRING(ASI_GROUP_ID_RC)
