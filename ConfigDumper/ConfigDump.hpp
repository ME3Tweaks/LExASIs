#pragma once

#include <LESDK/Headers.hpp>
#include <LESDK/Init.hpp>

namespace ConfigDumper
{
	// ! Initialization
	// ========================================

	// Resolves GConfig and the engine's own config-dump routine. Must run before DumpRuntimeConfig().
	void InitializeConfigDumpGlobals(::LESDK::Initializer& Init);

	// Resolves UObject::LoadConfig. Must run before DumpResolvedConfig().
	void InitializeResolvedConfigGlobals(::LESDK::Initializer& Init);

	// ! Dump entry points
	// ========================================

	// Writes GConfig's raw per-layer state to Logs/RuntimeConfigDump.txt, verbatim as the engine's
	// own FConfigFile::Dump() produces it (same code path as the built-in CONFIGHASH console command).
	// CONFIRMED this is NOT the merged/resolved config: each layer only shows that one source's own
	// unmerged contribution. Kept because it is still useful as "what was loaded, from where" -- see
	// DumpResolvedConfig() for the actual merged values. Must be called from the game thread and only
	// after DLC mount has completed.
	void DumpRuntimeConfig();

	// Writes the actual resolved/merged config to Logs/ResolvedConfigDump.txt by calling the real
	// UObject::LoadConfig() per loaded Config class and reading the resulting property values back
	// out via reflection. See the large comment above its definition in ConfigDump.cpp for exactly
	// what this does and does not cover. Must be called from the game thread and only after DLC mount
	// has completed.
	void DumpResolvedConfig();
}
