#pragma once
#include <iostream>

// This file is effectively templated and should be located in all ASI mod projects.

// Helper macros for converting version numbers to strings
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

// __DATE__ is "Mmm dd yyyy"
// sizeof(__DATE__) is 12.
// __DATE__ + 7 points to the start of the year (yyyy)
#define CURRENT_YEAR (__DATE__ + 7)


// Minor, revision, and build should all be 0, as Mod Manager only supports
// major version numbers.
#define APP_VERSION_MAJOR 3
#define APP_VERSION_MINOR 0
#define APP_VERSION_REVISION 0
#define APP_VERSION_BUILD 0

// Comma-separated version for use in the .rc file's FILEVERSION/PRODUCTVERSION
#define APP_VERSION_RC APP_VERSION_MAJOR, APP_VERSION_MINOR, APP_VERSION_REVISION, APP_VERSION_BUILD

// String that appears in the file description and part of SPI
#define APP_NAME "Texture Override"
#if defined(SDK_TARGET_LE1) || defined(SDK_TARGET_LE2)
#define APP_DESCRIPTION "Texture Override enables texture overrides at runtime via Binary Texture Packages."
#else
#define APP_DESCRIPTION "Texture Override enables texture overrides at runtime via Binary Texture Packages, as well as enabling multiple TFCs to load for each DLC."
#endif

#define GAME_PREFIX_RC SDK_TARGET_NAME_A
#define VERSION_STRING TOSTRING(APP_VERSION_MAJOR) ".0.0"
