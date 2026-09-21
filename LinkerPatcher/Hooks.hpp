#pragma once

#include <LESDK/Headers.hpp>
#include <LESDK/Init.hpp>
#include "Common/Base.hpp"

#include <unordered_map>
#include <unordered_set>
#include <string>

namespace LinkerPatcher
{
	// ! Config loading
	// ========================================
	//
	// BioGame/LinkerPatches/CookedPCConsole is a custom, non-DLC folder (not "DLC_*"-prefixed), so the
	// engine's own DLC discovery (FindDLCFolders/AdditionalContent_MountContentForMode) will never find
	// or mount its config. Default_LinkerPatches.bin is force-loaded directly by calling the same
	// low-level primitive real DLC mounts use (FConfigCacheIni::CreateConfigFile), bypassing discovery
	// entirely. See the research report for how this was confirmed via Ghidra.

	// Resolves GConfig and the native config functions this ASI calls directly (not exposed as
	// BUILTIN_*_RVA constants elsewhere in the SDK, since nothing else in this repo needs them yet).
	void InitializeConfigGlobals(::LESDK::Initializer& Init);

	// Reads BioGame/LinkerPatches/CookedPCConsole/Default_LinkerPatches.bin (relative to the game's
	// working directory, matching how the engine resolves DLC-relative paths) and registers it into
	// GConfig under the display name "LinkerPatches", as if it were a real mounted DLC's config.
	// Returns false if the file couldn't be read.
	bool ForceLoadLinkerPatchesConfig();

	// Reads [LinkerPatchObjects] Redirects=(OriginalObject="...",PatchObject="...") entries out of the
	// now-loaded "LinkerPatches" config file and populates the in-memory redirect table. Safe to call
	// even if ForceLoadLinkerPatchesConfig() failed (the table will just end up empty).
	void BuildRedirectTable();

	// Calls ForceLoadLinkerPatchesConfig()+BuildRedirectTable() exactly once, the first time it's
	// called. GConfig is NOT yet initialized at SPI_IMPLEMENT_ATTACH time (confirmed: attempting this
	// at ASI attach logs "GConfig is null"), so this can't run from InitializeHooks() directly -- it's
	// instead called lazily from the LoadPackage hook below, by which point the config system is
	// guaranteed to be up.
	//
	// Called from LoadPackage_hook ONLY. This ASI attaches asynchronously, so package loading is
	// already in flight on other threads while the hooks are being installed; this is safe to call
	// concurrently, and the tables it fills are not visible to the other hooks until it has finished
	// building them.
	void EnsureConfigLoaded();

	// Original.leaf-qualified key (e.g. "Engine.Actor", case-insensitive) -> patch package name (e.g.
	// "Patch_Actor"). The patch package is expected to contain its own export using the SAME leaf
	// object name as the original (mirrors ME3's own DLC_TestPatch convention, e.g. Patch_Actor.pcc
	// contains a class also named "Actor").
	extern std::unordered_map<std::wstring, std::wstring> RedirectTable;

	// Lowercased patch package name -> that name in its original config casing (the distinct set of
	// RedirectTable's values). Populated alongside RedirectTable. The lowercased key is what
	// FindPackageFile_hook matches on, since package name lookups are case-insensitive; the original
	// spelling is what gets used to build paths and to preload.
	extern std::unordered_map<std::wstring, std::wstring> KnownPatchPackages;

	// Loads one patch package, at most once per process. Called on demand from CreateExport_hook,
	// *after* the original export has been created -- patch packages deliberately are NOT preloaded,
	// see the comment in CreateExport_hook for why preloading is circular.
	void EnsurePatchPackageLoaded(std::wstring const& PackageName);

	// ! FindPackageFile hook
	// ========================================
	//
	// BioGame/LinkerPatches/CookedPCConsole is not registered in PCConsoleTOC.bin (updating TOC-building
	// tooling to add it is explicitly out of scope), so the engine's normal package file lookup -- which
	// resolves names via the TOC cache rather than scanning the filesystem live -- will never find a
	// patch package there on its own. This hooks the same FindPackageFile function DebugLogger already
	// hooks (there, opt-in behind -findpackagefile) and, only when the original lookup fails AND the
	// requested name is one of our own known patch packages (from KnownPatchPackages), supplies the
	// direct BioGame/LinkerPatches/CookedPCConsole/<Name>.pcc path ourselves. Every other lookup is left
	// completely untouched.

	void InstallFindPackageFileHook(::LESDK::Initializer& Init);

	using tFindPackageFile = unsigned(void* This, wchar_t* InName, FGuid* Guid, FString* OutFileName, wchar_t const* Language, unsigned Unk);
	extern tFindPackageFile* FindPackageFile_orig;
	unsigned FindPackageFile_hook(void* This, wchar_t* InName, FGuid* Guid, FString* OutFileName, wchar_t const* Language, unsigned Unk);

	// ! LoadPackage hook
	// ========================================
	//
	// Purely a trigger point for EnsureConfigLoaded() -- package loading only starts once the config
	// system is fully initialized, matching how DebugLogger already hooks this same function.

	void InstallLoadPackageHook(::LESDK::Initializer& Init);

	using tLoadPackage = UPackage*(UPackage* Outer, wchar_t* PackageName, ELoadFlags LoadFlags);
	extern tLoadPackage* LoadPackage_orig;
	UPackage* LoadPackage_hook(UPackage* Outer, wchar_t* PackageName, ELoadFlags LoadFlags);

	// ! CreateExport hook
	// ========================================

	void InstallCreateExportHook(::LESDK::Initializer& Init);

	using tCreateEntry = UObject*(ULinkerLoad* Context, int i);
	extern tCreateEntry* CreateExport_orig;
	UObject* CreateExport_hook(ULinkerLoad* Context, int i);

	// Resolves the logical name of an export's outer, one level up, purely from data already parsed
	// into the linker's own Export/Import tables -- does not trigger any loading and does not depend on
	// Context->Filename. This is required because seekfree cooking force-exports commonly-referenced
	// classes into many unrelated .pcc files: the same logical class (e.g. "SFXGame.BioCustomAction")
	// can be independently instantiated from dozens of different physical files' export tables, so
	// matching on which file is currently loading would silently miss almost every real occurrence.
	// See the research report's "CRITICAL CORRECTION" section for the full reasoning.
	FString ResolveOuterName(ULinkerLoad* Context, int OuterIndex);
}
