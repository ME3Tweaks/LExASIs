#include "Hooks.hpp"
#include "Entry.hpp"

#include <algorithm>
#include <atomic>
#include <cwctype>
#include <fstream>
#include <mutex>
#include <vector>

namespace LinkerPatcher
{
	namespace
	{
		// FConfigCacheIni::CreateConfigFile(GConfig, RawFileBytes, DisplayName, Flags, RegisterFastLookup).
		// RawFileBytes must be the ENTIRE compiled coalesced .bin file, verbatim, exactly as read from
		// disk -- it starts with a "NIBC"-magic header followed by a compressed payload, and
		// CreateConfigFile's own internal decoder handles decompression itself.
		using tCreateConfigFile = void* (void* GConfigInstance, void* RawFileBytes, wchar_t const* DisplayName, unsigned long long Flags, int RegisterFastLookup);

		// FConfig::GetArray(GConfig, Section, Key, OutArray, Filename) -- thin thunk to
		// FConfigFile::MergeArrayPropertyAcrossLayers, which walks every registered layer newest-first
		// and calls each one's FConfigFile_GetArrayLayer (vtable +0x8).
		//
		// CONFIRMED (Ghidra, FConfigFile_GetArrayLayer @ 0x7ff74a9245c0): a layer lookup is a THREE
		// LEVEL descent through that layer's BINI trie -- Filename, then Section, then Key -- where
		// each of the three is looked up by *hash*, not by string compare. The hash is computed by
		// 0x7ff74a929a30, which lowercases the string (ASCII plus the Latin-1 accented ranges) and
		// UTF-8 encodes it before hashing: so the lookup is case-insensitive, but otherwise the text
		// must match the stored key EXACTLY -- separator direction and the leading "..\..\" included.
		// If any of the three hashes isn't present, the function silently contributes nothing (this is
		// why a wrong Filename produced zero entries rather than any kind of error).
		using tGetArray = void (void* GConfigInstance, wchar_t const* Section, wchar_t const* Key, TArray<FString>* OutArray, wchar_t const* Filename);

		// UObject::StaticFindObject(ObjectClass, InObjectPackage, OrigInName, ExactClass) -- a pure
		// in-memory lookup (its internal ResolveName call passes Create=0, Throw=0), so it never loads
		// anything and never re-enters the linker. Patch packages are preloaded up front instead, see
		// PreloadPatchPackages() below.
		//
		// ObjectClass may be null, meaning "any class": StaticFindObject only dereferences it behind an
		// explicit null check, and forwards it to StaticFindObjectFast which treats null as a wildcard.
		//
		// This replaced an earlier UObject::StaticLoadObject_ call, which CANNOT take a null class.
		// StaticLoadObject hands ObjectClass to ULinkerLoad::Create (0x7ff74a98b960), whose very first
		// act is to read `*(ObjectClass + 0x40) + 8` -- with a null class that faults reading address
		// 0x40 exactly, which is the access violation this path originally produced. The engine itself
		// always passes a real class there (UObject::StaticClass() when resolving intermediate outer
		// path segments), and the class filter it applies is an exact export ClassName match rather
		// than an IsA test, so there is no "match anything" class to pass either.
		using tStaticFindObject = UObject* (UClass* ObjectClass, UObject* InObjectPackage, wchar_t const* OrigInName, int ExactClass);

		void** GConfig_location = nullptr;
		tCreateConfigFile* CreateConfigFile_fn = nullptr;
		tGetArray* GetArray_fn = nullptr;
		tStaticFindObject* StaticFindObject_fn = nullptr;

		// DisplayName is just this load's own bookkeeping identity within GConfig (mirrors how a real
		// DLC mount passes its own folder name here, e.g. "DLC_MOD_ProjectMilan") -- it is NOT the
		// config family GetArray reads from, so it can be an arbitrary name unique to this ASI.
		constexpr wchar_t const* k_configDisplayName = L"LinkerPatches";
		constexpr char const* k_configDisplayNameA = "LinkerPatches"; // narrow copy, for use in "..." (not L"...") log format strings

		// The [LinkerPatchObjects] section must be authored under the "BioEngine" asset in the
		// compiled Default_LinkerPatches.bin (matching BioWare's own [SFXPatch]/[SFXPatchObjects]
		// precedent, and the only asset names this project's coalesced-compiling tooling supports).
		constexpr char const* k_configRelativePath = "../../BioGame/LinkerPatches/CookedPCConsole/Default_LinkerPatches.bin";
		constexpr wchar_t const* k_patchFolderW = L"../../BioGame/LinkerPatches/CookedPCConsole/";

		constexpr wchar_t const* k_configSection = L"linkerpatchobjects";
		constexpr wchar_t const* k_configKey = L"redirects";

		// The BINI's first trie level is keyed on the *source path* each coalesced asset declares --
		// i.e. the CoalesceAsset XML's `source` attribute, verbatim -- NOT on the asset id, the asset
		// `name`, its `configuration/basedon` value, or the engine's own GEngineIni. Confirmed both
		// from FConfigFile::Dump (@ 0x7ff74a921f20: its "File: %s" line prints exactly this level-1
		// key) and from a live `confighash names` dump of this ASI's own layer, which printed
		// "File: ..\..\biogame\config\bioengine.ini" for a BioEngine asset whose source was that path.
		//
		// The previous zero-entry result came from querying the engine's Engine-family filename
		// (..\..\BIOGame\Config\DefaultEngine.ini) instead, which no layer in our .bin declares.
		//
		// Tried in order, first one that yields entries wins. Entry 0 is the confirmed-correct value
		// for the standard BioEngine asset; the rest only exist so a differently-configured coalesced
		// source still resolves without a code change (the matched one is logged either way).
		constexpr wchar_t const* k_configFilenameCandidates[] = {
			L"..\\..\\biogame\\config\\bioengine.ini",
			L"..\\..\\BIOGame\\Config\\DefaultEngine.ini",
			L"bioengine.ini",
		};

		// The engine normally receives config bytes owned by the DLC/live-config object. Keep our
		// equivalent allocation alive for as long as the registered layer exists.
		std::vector<uint8_t> LinkerPatchesConfigBuffer;
		void* LinkerPatchesConfigFile = nullptr;

		std::wstring ToLower(std::wstring value)
		{
			std::transform(value.begin(), value.end(), value.begin(), [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
			return value;
		}

		std::wstring SafeChars(FString const& Value)
		{
			return Value.Chars() != nullptr ? Value.Chars() : L"";
		}

		// Parses one "(OriginalObject="...",PatchObject="...")" struct-literal entry -- the config
		// array-of-structs format chosen so each redirect is one self-contained, named record instead
		// of a string we'd have to split positionally. Returns false (and leaves the outputs untouched)
		// if either field is missing, so callers can skip and log a malformed entry instead of silently
		// misreading it.
		bool ParseRedirectEntry(std::wstring const& Raw, std::wstring& OutOriginal, std::wstring& OutPatch)
		{
			auto const extractField = [&Raw](wchar_t const* FieldName, std::wstring& OutValue) -> bool
			{
				std::wstring const needle = std::wstring(FieldName) + L"=\"";
				size_t const start = Raw.find(needle);
				if (start == std::wstring::npos)
					return false;
				size_t const valueStart = start + needle.size();
				size_t const valueEnd = Raw.find(L'"', valueStart);
				if (valueEnd == std::wstring::npos)
					return false;
				OutValue = Raw.substr(valueStart, valueEnd - valueStart);
				return !OutValue.empty();
			};

			return extractField(L"OriginalObject", OutOriginal) && extractField(L"PatchObject", OutPatch);
		}

		// The same async package loading that made the trampoline race real also means
		// EnsureConfigLoaded() can be entered by more than one thread, and that RedirectTable /
		// KnownPatchPackages can be read by a hook on one thread while BuildRedirectTable() is still
		// filling them on another -- a data race on the containers themselves, not just a lost update.
		//
		// bRedirectsReady is the publication barrier: it is stored (release) only after every
		// mutation of both containers is complete, and the reader hooks load it (acquire) before
		// touching either one. After that point both containers are immutable for the rest of the
		// process, so the read path stays lock-free.
		std::mutex ConfigInitMutex;

		// Published once RedirectTable and KnownPatchPackages are both complete and immutable.
		std::atomic<bool> bRedirectsReady{ false };

		bool bConfigLoadAttempted = false;

		// Patch packages are loaded on demand from CreateExport_hook (see the long comment there for
		// why they cannot be preloaded). This is just a "don't ask the engine twice" cache; a package
		// name is added once its load has been attempted, successfully or not.
		std::mutex PatchLoadMutex;
		std::unordered_set<std::wstring> AttemptedPatchLoads;

		// Non-zero while this thread is inside a patch package load. Redirects are suppressed for the
		// duration, so a patch package that somehow contains an export matching a redirect key cannot
		// recurse into loading itself. Thread-local because package loading happens on more than one
		// thread and this is about re-entrancy, not shared state.
		thread_local int PatchLoadDepth = 0;

		// Installs a hook in a DISABLED state, publishes its trampoline into OutOriginal, and only
		// then enables the detour.
		//
		// Init.InstallHook() enables the detour before it returns, so there is a window between the
		// detour going live and the caller's `X_orig = InstallHook(...)` assignment landing. Any
		// thread entering the hooked function inside that window runs our hook body with X_orig still
		// nullptr and calls straight through address 0. That is not theoretical here: this ASI is
		// SPI_PLUGINSIDE_ASYNCATTACH, so the game is already loading packages on another thread while
		// InitializeHooks() runs -- observed as an access violation executing 0x0 inside
		// CreateExport_hook, reached via LoadPackage -> Preload -> IndexToObject. DebugLogger hit the
		// identical race on OutputDebugStringW and fixes it exactly this way.
		//
		// Enabling only on success also means a failed install leaves the detour disabled rather than
		// live-but-broken, so the hook bodies never need their own null-trampoline guards.
		template <typename TFunc>
		bool InstallDeferredHook(::LESDK::Initializer& Init, char const* const Name, TFunc* const Target, TFunc* const Detour, TFunc*& OutOriginal)
		{
			OutOriginal = reinterpret_cast<TFunc*>(Init.InstallHookDeferred(
				Name,
				reinterpret_cast<void*>(Target),
				reinterpret_cast<void*>(Detour)));

			if (OutOriginal == nullptr)
			{
				LEASI_ERROR("LinkerPatcher: failed to install the {} hook -- leaving it disabled", Name);
				return false;
			}

			// The trampoline is published, so the detour is safe to go live now.
			Init.EnableHook(Name);
			return true;
		}
	}

	std::unordered_map<std::wstring, std::wstring> RedirectTable;

	void InitializeConfigGlobals(::LESDK::Initializer& Init)
	{
		GConfig_location = Init.ResolveTyped<void*>(LEASI_RVA(0x17CD5B8));
		CHECK_RESOLVED(GConfig_location);

		CreateConfigFile_fn = Init.ResolveTyped<tCreateConfigFile>(LEASI_RVA(0x91700));
		CHECK_RESOLVED(CreateConfigFile_fn);

		GetArray_fn = Init.ResolveTyped<tGetArray>(LEASI_RVA(0x94f60));
		CHECK_RESOLVED(GetArray_fn);


		StaticFindObject_fn = Init.ResolveTyped<tStaticFindObject>(LEASI_RVA(0x134510));
		CHECK_RESOLVED(StaticFindObject_fn);
	}

	bool ForceLoadLinkerPatchesConfig()
	{
		if (!GConfig_location || !CreateConfigFile_fn)
		{
			LEASI_ERROR("LinkerPatcher: config globals were not resolved, cannot force-load config");
			return false;
		}

		void* const gConfigInstance = *GConfig_location;
		if (!gConfigInstance)
		{
			LEASI_ERROR("LinkerPatcher: GConfig is null (config system not initialized yet)");
			return false;
		}

		std::ifstream file(k_configRelativePath, std::ios::binary | std::ios::ate);
		if (!file.is_open())
		{
			LEASI_WARN("LinkerPatcher: {} not found -- no LinkerPatches config to load", k_configRelativePath);
			return false;
		}

		auto const size = static_cast<size_t>(file.tellg());
		file.seekg(0, std::ios::beg);

		LinkerPatchesConfigBuffer.resize(size);
		if (size > 0 && !file.read(reinterpret_cast<char*>(LinkerPatchesConfigBuffer.data()), static_cast<std::streamsize>(size)))
		{
			LEASI_ERROR("LinkerPatcher: failed to read {} ({} bytes)", k_configRelativePath, size);
			LinkerPatchesConfigBuffer.clear();
			return false;
		}

		if (size < 4)
		{
			LEASI_ERROR("LinkerPatcher: {} is too short to be a compiled coalesced file ({} bytes)", k_configRelativePath, size);
			LinkerPatchesConfigBuffer.clear();
			return false;
		}

		LEASI_INFO(
			"LinkerPatcher: config buffer size={} magic={:02X} {:02X} {:02X} {:02X}",
			size,
			LinkerPatchesConfigBuffer[0],
			LinkerPatchesConfigBuffer[1],
			LinkerPatchesConfigBuffer[2],
			LinkerPatchesConfigBuffer[3]);

		// Real DLC config layers use a low dword of 2. LiveINI uses 3 for a different layer type.
		constexpr unsigned long long flags = 2;
		LinkerPatchesConfigFile = CreateConfigFile_fn(
			gConfigInstance,
			LinkerPatchesConfigBuffer.data(),
			k_configDisplayName,
			flags,
			1);

		if (LinkerPatchesConfigFile == nullptr)
		{
			LEASI_ERROR("LinkerPatcher: CreateConfigFile returned null for {}", k_configDisplayNameA);
			return false;
		}

		// FConfigFile stores the parsed coalesced representation at +0x28. The native merge code
		// dereferences this field, so log and validate it before allowing GetArray to run.
		void* const parsedConfig = *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(LinkerPatchesConfigFile) + 0x28);
		LEASI_INFO(
			"LinkerPatcher: CreateConfigFile returned FConfigFile={} parsedData={}",
			LinkerPatchesConfigFile,
			parsedConfig);
		if (parsedConfig == nullptr)
		{
			LEASI_ERROR("LinkerPatcher: CreateConfigFile produced an unusable FConfigFile; redirects will not be read");
			return false;
		}

		LEASI_INFO("LinkerPatcher: force-loaded {} ({} bytes) into GConfig as \"{}\"", k_configRelativePath, size, k_configDisplayNameA);
		return true;
	}

	std::unordered_map<std::wstring, std::wstring> KnownPatchPackages;

	void BuildRedirectTable()
	{
		RedirectTable.clear();
		KnownPatchPackages.clear();

		if (!GConfig_location || !GetArray_fn)
		{
			LEASI_ERROR("LinkerPatcher: config globals were not resolved, cannot read redirect table");
			return;
		}

		void* const gConfigInstance = *GConfig_location;
		if (!gConfigInstance)
		{
			LEASI_ERROR("LinkerPatcher: GConfig is null, cannot read redirect table");
			return;
		}

		// GetArray empties OutArray itself before merging, so each candidate is a clean attempt and a
		// miss is indistinguishable from a legitimately-empty section -- both just yield zero entries.
		TArray<FString> rawEntries;
		for (wchar_t const* const candidate : k_configFilenameCandidates)
		{
			GetArray_fn(gConfigInstance, k_configSection, k_configKey, &rawEntries, candidate);
			if (rawEntries.Count() > 0)
			{
				LEASI_INFO(L"LinkerPatcher: [{}] {} resolved under config filename '{}'", k_configSection, k_configKey, candidate);
				break;
			}

			LEASI_TRACE(L"LinkerPatcher: no entries under config filename '{}', trying the next candidate", candidate);
		}

		LEASI_INFO("LinkerPatcher: GetArray returned {} raw redirect entrie(s)", rawEntries.Count());
		if (rawEntries.Count() == 0)
		{
			LEASI_WARN(
				L"LinkerPatcher: no [{}] {} entries found under any known config filename -- check that the "
				L"CoalesceAsset's `source` attribute matches one of them (run `confighash names` in the "
				L"console to see the layer's actual File: path)",
				k_configSection,
				k_configKey);
			return;
		}

		for (UINT i = 0; i < rawEntries.Count(); i++)
		{
			std::wstring const raw = SafeChars(rawEntries(i));

			std::wstring original, patch;
			if (!ParseRedirectEntry(raw, original, patch))
			{
				LEASI_WARN(L"LinkerPatcher: skipping malformed [LinkerPatchObjects] Redirects entry: {}", raw);
				continue;
			}

			RedirectTable.insert_or_assign(ToLower(original), patch);
			KnownPatchPackages.insert_or_assign(ToLower(patch), patch);
			LEASI_INFO(L"LinkerPatcher: registered redirect {} -> {}", original, patch);
		}

		LEASI_INFO("LinkerPatcher: {} redirect(s) active", RedirectTable.size());
	}

	void EnsurePatchPackageLoaded(std::wstring const& PackageName)
	{
		if (LoadPackage_orig == nullptr)
		{
			LEASI_ERROR("LinkerPatcher: LoadPackage trampoline is unavailable, cannot load patch packages");
			return;
		}

		{
			std::lock_guard<std::mutex> const guard{ PatchLoadMutex };
			if (!AttemptedPatchLoads.insert(ToLower(PackageName)).second)
				return;
		}

		// LoadPackage_orig rather than LoadPackage_hook: the config is already loaded by the time any
		// redirect can fire, so there is nothing for the hook to add here, and going through it would
		// only risk re-entering EnsureConfigLoaded.
		++PatchLoadDepth;
		UPackage* const package = LoadPackage_orig(nullptr, const_cast<wchar_t*>(PackageName.c_str()), LOAD_None);
		--PatchLoadDepth;

		if (package == nullptr)
		{
			LEASI_WARN(
				L"LinkerPatcher: patch package {} failed to load -- redirects targeting it will fall back "
				L"to the original object",
				PackageName);
			return;
		}

		LEASI_INFO(L"LinkerPatcher: loaded patch package {}", PackageName);
	}

	void EnsureConfigLoaded()
	{
		if (bRedirectsReady.load(std::memory_order_acquire))
			return;

		std::lock_guard<std::mutex> const guard{ ConfigInitMutex };
		if (bConfigLoadAttempted)
			return;
		bConfigLoadAttempted = true;

		if (ForceLoadLinkerPatchesConfig())
		{
			BuildRedirectTable();
		}
		else
		{
			LEASI_WARN("LinkerPatcher: failed to force-load Default_LinkerPatches.bin -- no redirects will be active");
		}

		// Published last, and unconditionally: on failure both containers are simply empty, which the
		// reader hooks already treat as "nothing to do". Nothing mutates them after this store.
		bRedirectsReady.store(true, std::memory_order_release);
	}

	tFindPackageFile* FindPackageFile_orig = nullptr;

	unsigned FindPackageFile_hook(void* This, wchar_t* InName, FGuid* Guid, FString* OutFileName, wchar_t const* Language, unsigned Unk)
	{
		unsigned const result = FindPackageFile_orig(This, InName, Guid, OutFileName, Language, Unk);
		if (result != 0 || InName == nullptr || !bRedirectsReady.load(std::memory_order_acquire))
			return result;

		if (KnownPatchPackages.find(ToLower(InName)) == KnownPatchPackages.end())
			return result;

		// The TOC lookup failed (as expected -- this folder isn't registered in it), but this is one of
		// our own known patch packages, so we know exactly where it lives. Supply the path directly.
		std::wstring const path = std::wstring(k_patchFolderW) + InName + L".pcc";
		*OutFileName = path.c_str();

		LEASI_INFO(L"LinkerPatcher: resolved patch package {} -> {} (not in PCConsoleTOC)", InName, path);
		return 1;
	}

	void InstallFindPackageFileHook(::LESDK::Initializer& Init)
	{
		auto const target = Init.ResolveTyped<tFindPackageFile>(BUILTIN_FINDPACKAGEFILE_RVA);
		CHECK_RESOLVED(target);
		InstallDeferredHook(Init, "FindPackageFile", target, FindPackageFile_hook, FindPackageFile_orig);
	}

	tLoadPackage* LoadPackage_orig = nullptr;

	UPackage* LoadPackage_hook(UPackage* Outer, wchar_t* PackageName, ELoadFlags LoadFlags)
	{
		// GConfig is only guaranteed initialized once package loading actually starts -- attempting
		// this at SPI_IMPLEMENT_ATTACH time is too early (GConfig is still null then).
		EnsureConfigLoaded();
		return LoadPackage_orig(Outer, PackageName, LoadFlags);
	}

	void InstallLoadPackageHook(::LESDK::Initializer& Init)
	{
		auto const target = Init.ResolveTyped<tLoadPackage>(BUILTIN_LOADPACKAGE_RVA);
		CHECK_RESOLVED(target);
		InstallDeferredHook(Init, "LoadPackage", target, LoadPackage_hook, LoadPackage_orig);
	}

	FString ResolveOuterName(ULinkerLoad* Context, int OuterIndex)
	{
		if (OuterIndex > 0)
		{
			return Context->ExportMap(static_cast<UINT>(OuterIndex - 1)).ObjectName.ToString();
		}
		if (OuterIndex < 0)
		{
			return Context->ImportMap(static_cast<UINT>(-OuterIndex - 1)).ObjectName.ToString();
		}
		return Context->LinkerRoot != nullptr ? Context->LinkerRoot->GetName() : FString{};
	}

	tCreateEntry* CreateExport_orig = nullptr;

	UObject* CreateExport_hook(ULinkerLoad* Context, int i)
	{
		// Deliberately does NOT call EnsureConfigLoaded(): an export cannot be created before some
		// package has started loading, so LoadPackage_hook has always run it first. Gating on the
		// published barrier instead keeps this path lock-free and, more importantly, removes any
		// possibility of re-entering the config init from inside itself.
		if (!bRedirectsReady.load(std::memory_order_acquire) || RedirectTable.empty()
			|| StaticFindObject_fn == nullptr || PatchLoadDepth > 0)
		{
			return CreateExport_orig(Context, i);
		}

		FObjectExport& exp = Context->ExportMap(i);

		// Only worth checking if nothing has already created/cached this export -- once cached,
		// CreateExport's own early-return short-circuits before any of this would matter anyway.
		if (exp.Object != nullptr)
			return CreateExport_orig(Context, i);

		std::wstring const outerName = SafeChars(ResolveOuterName(Context, exp.OuterIndex));
		std::wstring const leafName = SafeChars(exp.ObjectName.ToString());
		std::wstring const key = ToLower(outerName + L"." + leafName);

		auto const it = RedirectTable.find(key);
		if (it == RedirectTable.end())
			return CreateExport_orig(Context, i);

		// The original export is created FIRST, deliberately, and the patch package is only loaded
		// afterwards. This ordering is load-bearing, not incidental.
		//
		// A patch object is by definition an instance of the same class as the thing it replaces, and
		// that class very often lives in the package being patched (patching an object in SFXGame.pcc
		// whose class is also defined in SFXGame.pcc is the ordinary case). Loading the patch package
		// before that class exists is circular: resolving the patch export's class import drags in
		// SFXGame.pcc, whose own exports then stream past this hook -- including the very export we
		// were trying to patch, whose patch object does not exist yet. The redirect silently misses,
		// the original is created and cached, and it can never be revisited.
		//
		// Running CreateExport_orig first breaks that cycle. ULinkerLoad::CreateExport resolves the
		// export's class via IndexToObject and Preloads it (confirmed in Ghidra @ 0x7ff74a98bdd0)
		// before constructing anything, so once it returns, every class the patch package imports is
		// guaranteed resident and the patch package can load cleanly. Nested loading from inside
		// CreateExport is ordinary engine behaviour -- it is exactly what that IndexToObject call
		// does for every import.
		//
		// The cost is that the original object is built and then discarded, left for the GC. That is
		// the price of not needing to resolve exp.ClassIndex ourselves.
		UObject* const original = CreateExport_orig(Context, i);

		EnsurePatchPackageLoaded(it->second);

		// Null ObjectClass means "any class", which is safe for StaticFindObject (unlike
		// StaticLoadObject -- see the tStaticFindObject comment). A patch object is therefore matched
		// purely by name; revisit if a stricter class check turns out to be needed in practice.
		std::wstring const findPath = it->second + L"." + leafName;
		UObject* const patched = StaticFindObject_fn(nullptr, nullptr, findPath.c_str(), 0);
		if (patched == nullptr)
		{
			LEASI_WARN(
				L"LinkerPatcher: redirect target {} for {} was not found after loading its package, "
				L"falling back to the original object",
				findPath,
				key);
			return original;
		}

		// Overwrite the export's own cache slot, so later references to this export index resolve
		// straight to the patched object without re-entering this hook.
		exp.Object = patched;
		LEASI_INFO(L"LinkerPatcher: redirected {} -> {} (in {})", key, findPath, Context->Filename);
		return patched;
	}

	void InstallCreateExportHook(::LESDK::Initializer& Init)
	{
		auto const target = Init.ResolveTyped<tCreateEntry>(BUILTIN_CREATEEXPORT_RVA);
		CHECK_RESOLVED(target);
		if (InstallDeferredHook(Init, "CreateExport", target, CreateExport_hook, CreateExport_orig))
		{
			LEASI_INFO("LinkerPatcher: CreateExport hook installed");
		}
	}
}
