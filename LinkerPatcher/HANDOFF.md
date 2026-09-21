# LinkerPatcher handoff

## Current objective

`LinkerPatcher` is a new ASI for LE3 and LMP. It reads a compiled coalesced file from:

```text
BioGame/LinkerPatches/CookedPCConsole/Default_LinkerPatches.bin
```

It will eventually redirect object exports through the existing LE3/LMP `CreateExport` hook. LE3
and LMP use the exact same executable byte-for-byte; only the runtime game content/config context
differs. LE1 and LE2 are not currently in scope.

## Current implementation

The implementation is in `LinkerPatcher/Hooks.cpp` and currently hooks:

- `LoadPackage` to defer config setup until `GConfig` exists;
- `FindPackageFile` to resolve known patch packages outside `PCConsoleTOC`;
- `CreateExport` for the eventual object redirection.

The config path is intentionally relative to the process working directory:

```text
../../BioGame/LinkerPatches/CookedPCConsole/Default_LinkerPatches.bin
```

The config buffer is retained for the lifetime of the ASI because the native config layer owns or
references the supplied BINI data. `CreateConfigFile` now logs:

- byte count and first four bytes;
- returned `FConfigFile*`;
- the parsed-data pointer at `FConfigFile + 0x28`.

The observed `mrmf` magic (`6D 72 6D 66`) means the file is already in the decompressed internal
BINI format. Registration is succeeding; it no longer causes the previous stack overflow.

## Config format currently tested

The source XML contains a `BioEngine` asset with a `configuration/basedon` section and a
`linkerpatchobjects/redirects` property:

```xml
<CoalesceAsset id="BioEngine" name="bioengine.ini" source="..\\..\\biogame\\config\\bioengine.ini">
  <Sections>
    <Section name="configuration">
      <Property name="basedon" type="0">..\\..\\BIOGame\\DLC\\Test\\DLC_LinkerPatches\\Config\\DefaultEngine.ini</Property>
    </Section>
    <Section name="linkerpatchobjects">
      <Property name="redirects">
        <Value type="3">(OriginalObject="GUI_SF_Splash.Splash_I2",PatchObject="Patch_GUI_SF_Splash_Splash")</Value>
      </Property>
    </Section>
  </Sections>
</CoalesceAsset>
```

`confighash names` confirms the layer is registered and contains the data:

```text
BINI Layer: LinkerPatches
File: ..\\..\\biogame\\config\\bioengine.ini
[configuration]
basedon=(SET)..\\..\\BIOGame\\DLC\\Test\\DLC_LinkerPatches\\Config\\DefaultEngine.ini
[linkerpatchobjects]
redirects=(ADDUNIQUE)(OriginalObject="GUI_SF_Splash.Splash_I2",PatchObject="Patch_GUI_SF_Splash_Splash")
```

## Important Ghidra findings

For the LE3/LMP 11.3.2 executable:

- `GConfig` global: RVA `0x17CD5B8`.
- `FConfigCacheIni::CreateConfigFile`: RVA `0x91700`.
- `FConfig::GetArray`: RVA `0x94F60`.
- `GEngineIni`: RVA `0x17DEEF0`.
- `CreateExport`: use the existing SDK/hook definition already present in the ASI code; do not
  replace its target without re-validating the current LE3/LMP build.

`CreateConfigFile` takes the raw BINI data pointer, display name, flags, and registration boolean.
The DLC path uses flags `2`; the unrelated live-config path uses display name `LiveINI` and flags
`3`. Changing between `2` and `3` did not explain the previous failure.

The native `FConfigFile` array-layer method at `0x7FF74A9245C0` confirms the effective query shape:

```cpp
GetArray(GConfig, Section, Key, OutArray, Filename)
```

The layer implementation hashes the filename, section, and property key. The ASI now passes:

```cpp
GetArray(GConfig,
         L"linkerpatchobjects",
         L"redirects",
         &rawEntries,
         Filename);
```

## Resolved: the zero-entry `GetArray` blocker

The blocker described below was a **wrong filename key**, not a timing, format or registration
problem.

`FConfigFile_GetArrayLayer` (`0x7FF74A9245C0`, the vtable `+0x8` slot
`MergeArrayPropertyAcrossLayers` calls on every registered layer) is a **three-level descent through
the layer's BINI trie: Filename → Section → Key**. All three levels are looked up by *hash*, via
`0x7FF74A929A30`, which lowercases the string (ASCII plus the Latin-1 accented ranges) and UTF-8
encodes it before hashing. Consequences:

- The lookup is **case-insensitive**, but otherwise the text must match the stored key exactly —
  separator direction and the leading `..\..\` included.
- A missing hash at any level makes the function **contribute nothing, silently**. There is no error
  path, which is exactly why a wrong filename looked like an empty section.

The level-1 key is the **`source` attribute of the CoalesceAsset XML, verbatim** — not the asset
`id`, not its `name`, not `configuration/basedon`, and not `GEngineIni`. Confirmed twice over:
`FConfigFile::Dump` (`0x7FF74A921F20`) prints exactly this level-1 key as its `File: %s` line, and
the live `confighash names` dump showed `File: ..\..\biogame\config\bioengine.ini` for a `BioEngine`
asset whose `source` was that path.

The ASI was querying `..\..\BIOGame\Config\DefaultEngine.ini`, which no layer in the `.bin`
declares. `GEngineIni` was a red herring and the dependency on it has been removed entirely; the
`DefaultTest.ini` value it produced never needed explaining. `[linkerpatchobjects]` is authored and
read only by this ASI, so it is queried under its own layer's filename and never needs to merge with
the base game's Engine family.

`BuildRedirectTable` now tries an ordered candidate list and logs which one matched
(`k_configFilenameCandidates`), with `..\..\biogame\config\bioengine.ini` first. `GetArray` empties
its out-array on entry, so each candidate is an independent clean attempt.

## Resolved: null-trampoline crash on attach

Once the redirect table loaded, the next crash was an access violation executing `0x0` inside
`CreateExport_hook`, reached via `LoadPackage -> Preload -> IndexToObject`.

`Initializer::InstallHook()` enables the detour *before* it returns, so there is a window between
the detour going live and the caller's `X_orig = InstallHook(...)` assignment landing. This ASI is
`SPI_PLUGINSIDE_ASYNCATTACH`, so the game is already loading packages on other threads while
`InitializeHooks()` runs; a thread entering the hooked function inside that window runs the hook
body with `X_orig` still `nullptr` and calls straight through address 0.

All three hooks now go through `InstallDeferredHook()`, which wraps
`InstallHookDeferred()` + `EnableHook()`: the hook is created disabled, the trampoline is published
into `X_orig`, and only then is the detour enabled. This mirrors the existing fix on DebugLogger's
`OutputDebugStringW` hook. It also only enables on success, so a failed install leaves the detour
disabled rather than live-but-broken — the hook bodies therefore need no null-trampoline guards.

### Related concurrency hardening (done at the same time)

The same async loading means the config init could be entered by several threads, and
`RedirectTable` / `KnownPatchPackages` could be read by a hook on one thread while
`BuildRedirectTable()` was still filling them on another — a data race on the containers, not just a
lost update. Now:

- `EnsureConfigLoaded()` takes `ConfigInitMutex` and is called from `LoadPackage_hook` **only**.
- An `std::atomic<bool> bRedirectsReady` is stored (release) after both containers are complete;
  `CreateExport_hook` and `FindPackageFile_hook` load it (acquire) before touching either.
- `CreateExport_hook` no longer calls `EnsureConfigLoaded()` defensively. An export cannot exist
  before a package load, so `LoadPackage_hook` has always run it first; gating on the barrier keeps
  the read path lock-free and removes any chance of re-entering the config init from inside itself.

After the barrier is published both containers are immutable for the life of the process.

## Resolved: `StaticLoadObject` crash reading `0x40`

The redirect then fired and crashed inside `UObject::StaticLoadObject_` with an access violation
reading `0x0000000000000040`.

The ASI was passing `ObjectClass = nullptr`, on the assumption that null meant "any class".
**`StaticLoadObject` cannot take a null class.** It forwards `ObjectClass` to `ULinkerLoad::Create`
(`0x7FF74A98B960`), whose first act is to read `*(ObjectClass + 0x40) + 8` — with a null class that
faults reading address `0x40` exactly, matching the reported fault address.

There is also no "match anything" class to substitute: the filter `ULinkerLoad::Create` applies is
an **exact export ClassName match** (via `FindExportIndex`), not an `IsA` test. The engine passes
`UObject::StaticClass()` only when resolving *intermediate outer path segments*, where the outers
genuinely are plain packages/objects — never for a leaf of arbitrary class.

Rather than resolve `exp.ClassIndex` to a real `UClass*` (which risks recursively triggering
`CreateImport`/`CreateExport`), the load was split into two phases:

- **`PreloadPatchPackages()`** loads every distinct patch package exactly once, from
  `EnsureConfigLoaded()`, via `LoadPackage_orig`.
- **`CreateExport_hook`** now uses **`UObject::StaticFindObject`** (`0x7FF74A9C4510`, RVA
  `0x134510`) instead. That *does* accept a null class — it only dereferences `ObjectClass` behind
  an explicit null check and forwards it to `StaticFindObjectFast`, which treats null as a wildcard.
  It is also a pure in-memory lookup (its internal `ResolveName` passes `Create=0, Throw=0`), so it
  never loads anything and never re-enters the linker.

This removes the deeper hazard too: the old call re-entered `GetPackageLinker`/`BeginLoad`/`EndLoad`
partway through serializing *another* package's export (the crash arrived via
`UArrayProperty -> UObjectProperty -> IndexToObject`), which was liable to corrupt state even once
the class argument was fixed.

### Supporting changes

- `KnownPatchPackages` is now a `lowercased -> original casing` map. The lowercased key is what
  `FindPackageFile_hook` matches on; the original spelling is used to build paths and to load.
- Hook install order in `Entry.cpp` is now FindPackageFile, CreateExport, **LoadPackage last**.
  LoadPackage triggers `EnsureConfigLoaded()`, after which redirects can fire and start loading patch
  packages out of their non-TOC folder, so FindPackageFile must already be live.

## Resolved: preloading patch packages is circular

The first version of the above preloaded every patch package once, up front, so that
`CreateExport_hook` could get away with a pure in-memory lookup. **That does not work**, and the
case that breaks it is the ordinary one: patching an object in `SFXGame.pcc` whose class is also
defined in `SFXGame.pcc`.

A patch object is by definition an instance of the same class as the thing it replaces, so the patch
package imports that class. Preloading `Patch_SFXGame_Foo.pcc` therefore resolves a class import
into `SFXGame.pcc` and drags that package in — during the preload, before `bRedirectsReady` is
published. `SFXGame.pcc`'s exports stream past `CreateExport_hook` with redirects still disabled,
the target export is created and cached unpatched, and it can never be revisited (the hook's
`exp.Object == nullptr` check fails from then on). The redirect silently never fires.

Publishing the barrier earlier does not help — it just moves the failure: the redirect then fires
while `Patch_SFXGame_Foo.pcc` is still mid-load, the lookup finds nothing, and it falls back to the
original anyway.

### The fix: create the original first, load the patch package second

`CreateExport_hook` is now a **post-hook** for the matched case:

1. Match the redirect key from the linker tables (no loading).
2. Call `CreateExport_orig` to create the original export.
3. `EnsurePatchPackageLoaded()` — load the patch package on demand, at most once.
4. `StaticFindObject` the patch object and overwrite `exp.Object`.

Step 2 is what breaks the cycle. `ULinkerLoad::CreateExport` (Ghidra @ `0x7FF74A98BDD0`) resolves
the export's class via `IndexToObject` and `Preload`s it *before* constructing anything, so by the
time it returns, every class the patch package imports is resident and the patch package loads
cleanly. Nested loading from inside `CreateExport` is ordinary engine behaviour — that
`IndexToObject` call does it for every import.

The cost is that the original object is constructed and then discarded for the GC. That is the price
of not resolving `exp.ClassIndex` to a `UClass*` ourselves.

`PatchLoadDepth` (thread-local) suppresses redirects while a patch package is loading, so a patch
package containing an export that matches a redirect key cannot recurse into loading itself.

### Still open on this path

Patch packages are loaded but never explicitly rooted, so nothing guarantees they survive a garbage
collection. They are reachable through the export `Object` slots the hook writes, which should be
enough in practice, but if patched objects ever come back stale or null after a GC, adding the
loaded packages to the root set is the first thing to try.

## Correction to an existing Ghidra comment

The comment on `FConfigFile::MergeArrayPropertyAcrossLayers` documents the parse-action codes as
`3 = REMOVE, 4 = ADDUNIQUE`. That is **backwards**. `FConfigFile::Dump` selects its action text
literally and unambiguously:

```text
0 = SET   1 = CLEAR   2 = ADD   3 = ADDUNIQUE   4 = REMOVEALL
```

which also matches the merge code's own bodies (case 3 skips when an equal entry is already present
and otherwise appends; case 4 is a run-length compaction that removes every matching entry). This
matters beyond this ASI: that comment is cited as the ground truth ME3TweaksModManager's
`ConfigMerge.cs` is validated against. The Ghidra database has **not** been edited — the comment is
still wrong in the project.

The user's XML uses `<Value type="3">` and the dump shows `(ADDUNIQUE)`, which is consistent.

## Previous blocker (historical, now explained)

The ASI resolved the inline `GEngineIni` buffer and logged this in LMP:

```text
..\\..\\BIOGame\\Config\\DefaultTest.ini
```

This value comes from the game’s runtime config initialization; it is not read from
`Default_LinkerPatches.bin` and is not caused by the XML source. Because LE3 and LMP use the same
byte-identical executable, the `DefaultTest.ini` value must be explained by the runtime content,
launch/config context, or selected game identity—not by a binary difference.

The ASI currently tries both:

1. the live `GEngineIni` value (`DefaultTest.ini` in LMP);
2. a hardcoded `..\\..\\BIOGame\\Config\\DefaultEngine.ini` fallback.

Both currently return zero entries even though `confighash names` displays the layer and property.
The fallback does **not** yet use the exact `basedon` value from the user’s XML, which is:

```text
..\\..\\BIOGame\\DLC\\Test\\DLC_LinkerPatches\\Config\\DefaultEngine.ini
```

## Recommended next steps

1. Live-test: confirm the log now reports a non-zero redirect count and names the matched config
   filename. If it still reports zero, run `confighash names` and compare its `File:` line against
   `k_configFilenameCandidates`.
2. With the table populated, verify `FindPackageFile` resolves the patch package outside
   `PCConsoleTOC`, and that `CreateExport` substitutes the object.
3. Decide whether to correct the `MergeArrayPropertyAcrossLayers` action-code comment in Ghidra.

## Build procedure

Use the Visual Studio CMake executable selected through `vswhere` on `A:` and configure with the
repository’s `VisualStudio` preset:

```powershell
cmake --preset VisualStudio
cmake --build Build --target LinkerPatcher-LMP --config Release
cmake --build Build --target LinkerPatcher-LE3 --config Release
```

Current successful outputs are:

```text
Build\\RELEASE\\LMPLinkerPatcher.asi
Build\\RELEASE\\LE3LinkerPatcher.asi
```
