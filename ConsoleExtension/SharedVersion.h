#pragma once

// This file is used for shared version info in C++ and RC files.
// This file is effectively templated and should be located in all ASI mod projects.

// Todo: Move to a single version file for all asis
#define CURRENT_YEAR "2026"

#define DEVELOPER_A "SirCxyrtyx"
#define DEVELOPER_W L"SirCxyrtyx"
#define ASI_NAME_A "Console Extension"
#define ASI_NAME_W L"Console Extension"
#define ASI_NAME_NO_SPACE_A "ConsoleExtension"
#define ASI_NAME_NO_SPACE_W L"ConsoleExtension"

#define ASI_VERSION 2
#define ASI_DESCRIPTION "Console Extension adds new console commands to the game."


// === Identification from automated ME3Tweaks tooling ====
#if defined(SDK_TARGET_LE1)
#define GAME_PREFIX_RC    "LE1"
#define ASI_GROUP_ID_RC 75
#define ASI_GAME_ID_RC 4
#endif

#if defined(SDK_TARGET_LE2)
#define GAME_PREFIX_RC    "LE2"
#define ASI_GROUP_ID_RC 76
#define ASI_GAME_ID_RC 5
#endif

#if defined(SDK_TARGET_LE3)
#define GAME_PREFIX_RC    "LE3"
#define ASI_GROUP_ID_RC 77
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



