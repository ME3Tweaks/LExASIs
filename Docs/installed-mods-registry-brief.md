# Brief: installed-mods registry (live config section for DLC_MOD_* discovery)

**Standalone handoff.** Everything needed to start is in this file; it assumes no memory of the
ConfigDumper work that motivated it, though it reuses several of that work's findings directly.

---

## 1. What to build

A hook (in a new small ASI, or folded into an existing one) that maintains a **live, in-memory**
list of currently-mounted `DLC_MOD_*` folders inside the engine's own `Engine`-named config file
(the same live `FConfigFile` instance that backs `BioEngine.ini` / any UnrealScript class declared
`config(Engine)`), so **other mods can query "what's installed" through completely normal config
lookups**, at runtime, with no filesystem scanning of their own.

Concretely: a new section (e.g. `[Engine.InstalledMods]`) with a key holding the list of installed
mod folder names (e.g. `Mods=DLC_MOD_ProjectMilan`, repeated per mod, or however
`FConfig::SetArray`'s real semantics turn out to work — see §5), kept in sync every time the game's
own DLC mount state changes: added when a `DLC_MOD_*` folder mounts, removed when it unmounts.

**This must be runtime-only** (not written to disk) — it should reflect this session's actual live
mount state, not persist across restarts.

## 2. Why

Right now, if a mod wants to know "is `DLC_MOD_XYZ` installed", it has to either scan the filesystem
itself (fragile — different install layouts, permissions, DLC could be virtually mounted differently
in the future) or hardcode knowledge of a specific other mod. Exposing the *engine's own* live view
of what's mounted, through the *same config system every mod already reads from*, means any mod can
just do a normal config lookup instead of reinventing discovery.

## 3. What's already confirmed (from this session's Ghidra work)

All of this is also written directly into the Ghidra project as function renames and
`set_decompiler_comment` comments — read those first if picking this up from the binary rather than
this doc. RVAs are relative to the module's own base (LE3/LMP, same `MassEffect3.exe` /
`MassEffectMultiplayer.exe` binary as the rest of this repo's Ghidra work).

### The per-content-item struct: `SFXAdditionalContent`

Already a named struct type in this Ghidra project (`/auto_structs/SFXAdditionalContent`, size 220
bytes as currently defined — **treat this size as a lower bound**, not confirmed complete, since only
6 of 150 inferred component slots are actually named). Fields that matter here:

| Offset | Name | Type | Notes |
|---|---|---|---|
| `0x64` | `RelativeDLCPath` | `FString` | The folder name, e.g. `"DLC_MOD_ProjectMilan"`. **This is the value to expose.** |
| `0x84` | `ConfigFile` | `FString` (mistyped as `TArray<byte>` in Ghidra, same size so harmless) | Path/identifier for this DLC's own config source. |
| `0xC8` | `someFlags` | `int` | Bitflags controlling mount state (see below). |
| `0xD4` | `PointerToGameContent` | `void*` | Set to the mounted `FConfigFile*` once loaded; null when not mounted. |

`someFlags` bit meanings, reverse-engineered from `LoadDLCConfigFile_` (RVA `0xAF6C10`):
- Bit `0x20` = "still wanted" (driven externally, likely by the online/Blaze layer reconciling
  which DLC the current session needs).
- Bit `0x40` = "currently loaded" (set once mount succeeds, cleared on unmount).
- `(someFlags & 0x70) == 0x30` triggers the **mount** branch.
- `(someFlags & 0x20) == 0 && (someFlags & 0x40) != 0` triggers the **unmount** branch.

**`someFlags & 0x40` is the reliable "is this currently mounted" check** — use it to decide which
entries belong in the registry.

### The mount/unmount driver functions

- **`LoadDLCConfigFile_`** (RVA `0xAF6C10`) — per-content mount/unmount toggle. On mount, calls
  `FConfigCacheIni::CreateConfigFile(GConfig, Content->ConfigFile.Data, DLCName, flags, 1)`
  (RVA `0x91700`) and stores the result in `PointerToGameContent`. On unmount, calls
  `FConfigCacheIni::RemoveConfigFile(GConfig, Content->PointerToGameContent, 1)` (RVA `0x9F3E0`).

- **`AdditionalContent_ProcessMountStates`** (RVA `0xAF6DA0`) — **the recommended hook point.**
  Iterates an array of `SFXAdditionalContent*` (its own first parameter *is* the `TArray` — `Data` at
  `+0`, `Count` at `+8`, no need to separately locate an owning "content manager" object) and calls
  `LoadDLCConfigFile_` (plus a few sibling per-content steps: TFC registration, TOC, etc.) for each
  entry. **Confirmed this is a live, repeatable state machine — DLC mount/unmount is not a one-time
  boot action.** Hook this function: call the original first, then re-scan the *same* array parameter
  for entries with `someFlags & 0x40` set and rebuild the registry from scratch each time. Re-scanning
  fully (rather than tracking incremental add/remove yourself) is simpler and avoids any risk of your
  own tracking state drifting from the engine's.

- **`AdditionalContent_MountContentForMode`** (RVA `0xAF6900`) — one level up, recomputes each
  entry's "wanted" bit for the current mode (SP vs MP, matching `FindDLCFolders`' filtering) then
  calls `AdditionalContent_ProcessMountStates`. Not needed as a hook point if you hook
  `ProcessMountStates` directly, but useful context: DLC folders whose name contains `"MP"` are
  filtered out of `FindDLCFolders`' singleplayer-oriented scan and handled via
  `SFXOnlineComponentBlazeGame::ValidateDLCMasterListNative` instead — verify which path Project
  Milan's own `DLC_MOD_*` folders actually go through before assuming both hook points see every mod.

### Writing into GConfig at runtime: the safe primitive

Rather than hand-constructing `FConfigSection`'s internal multimap (not reverse-engineered, and the
kind of thing that's easy to get subtly wrong in a way that corrupts memory rather than just
producing a wrong value — this is exactly the risk `ConfigDumper`'s design doc explicitly steered
around for reading), **use the engine's own write-side accessors**, found by decompiling
`UObject::SaveConfig` (RVA `0x12DFC0`, the write-side mirror of `UObject::LoadConfig` used
throughout `ConfigDumper`):

- **`FConfig::SetString(GConfig, Filename, Section, Value)`** (RVA `0x9DF30`) — for scalar values.
- **`FConfig::SetArray(GConfig, Filename, Section, TArray<FString>*)`** (RVA `0x9CD40`) — for array
  values. Likely **SET/replace semantics** (see below), which is convenient here: rebuild the full
  mod list and call this once per registry rebuild rather than trying to diff and incrementally
  add/remove.

**These are the two functions to hook-and-log first**, before writing any registry code, to nail
down what's not yet confirmed (§5).

## 4. Recommended design

1. Resolve `AdditionalContent_ProcessMountStates` (RVA `0xAF6DA0`) and install a hook.
2. In the hook: call the original, then walk its `TArray<SFXAdditionalContent*>` parameter, collect
   `RelativeDLCPath` for every entry with `someFlags & 0x40` set.
3. Resolve `GConfig` (RVA `0x17CD5B8`, already used by `ConfigDumper`) and `FConfig::SetArray`
   (RVA `0x9CD40`). Call it with `Filename="Engine"`, `Section="InstalledMods"` (or similar — pick
   something unlikely to collide with a real BioWare section), and the collected list.
4. That's it for the write side — no unmount-specific code needed if you rebuild the full list every
   time `ProcessMountStates` runs (which fires for both mount and unmount, since it drives
   `LoadDLCConfigFile_` for both branches).
5. Verify by having something else (another small ASI hook, or the existing `ConfigDumper`'s
   `DUMPRESOLVEDCONFIG`/raw dump) query `GConfig` for `Engine`/`InstalledMods` and confirm the
   injected values show up and update correctly when a mod is mounted/unmounted mid-session (you can
   force this in Project Milan's multiplayer flow by joining/leaving a session with a different
   mounted-DLC set, per `AdditionalContent_MountContentForMode`'s SP/MP mode-recompute behavior).

## 5. What's NOT yet confirmed — verify before shipping

- **`FConfig::SetString`/`SetArray`'s exact parameter split.** The call sites inside
  `UObject::SaveConfig` strongly suggest `(GConfig, Filename, Section, Value)` in that order, mirrored
  from how `FConfig::GetArray`'s confirmed call sites look, but this was not independently verified
  the way `GetArray`'s merge behavior was (that one was checked against a real dump; this one wasn't
  checked against a real write). **First implementation step should be hooking both functions
  read-only (log args, call through to original unchanged) and triggering an ordinary in-game config
  save** (e.g. any settings change that persists) to see real argument values before writing anything.

- **Whether `SetArray` truly does SET/replace, or whether it adds another per-source layer** the way
  `LoadDLCConfigFile_`'s mount path does. If it's SET/replace (as the decompile suggests — it looks
  like a straightforward "find-or-create this key, overwrite its value" operation, not a
  layer-pushing operation like `CreateConfigFile`), rebuilding the full list on every call (as
  recommended above) is exactly the right approach and there's nothing further to worry about here.
  If it turns out to push a new layer each call instead, values would accumulate/duplicate across
  repeated mount/unmount cycles — the same failure mode `ConfigDumper`'s whole existence was about
  catching in `ConfigMerge.cs`, so building a tiny throwaway test (write, write again with a
  different list, dump and check with `ConfigDumper`) before relying on this is cheap insurance.

- **Whether a freshly-`SetArray`'d value is visible to a normal `UObject::LoadConfig()` call** made
  by some *other* mod's UnrealScript class declared `config(Engine)`. `LoadConfig`'s scalar path reads
  via `GetStringRefInternal`'s backward layer-scan; confirm the section/value written by `SetArray`
  actually gets picked up by that same read path (rather than, say, living in a write-only cache that
  only `SaveConfig`'s own round-trip sees). This is the single most important thing to verify, since
  it's the entire point of the feature.

- **`SFXAdditionalContent`'s full layout.** Only 6 of ~150 inferred fields are named. There's no
  reason to believe more fields are needed for this feature (`RelativeDLCPath` and `someFlags` are
  enough), but if the struct's true size turns out to be larger than the 220 bytes currently declared,
  don't let that block anything here — it only matters if you need to read *other* fields.

- **SP-vs-MP DLC discovery split.** `FindDLCFolders` explicitly filters out any DLC folder whose name
  contains `"MP"`, routing MP content through the Blaze online validation path instead
  (`SFXOnlineComponentBlazeGame::ValidateDLCMasterListNative`). None of this session's observed
  `DLC_MOD_*` folder names happen to contain the substring `"MP"`, so they likely go through the
  ordinary filesystem-scan path — but this hasn't been independently confirmed, and Project Milan
  being multiplayer-only makes it worth double-checking that `AdditionalContent_ProcessMountStates`
  really does see every mod folder and not just a singleplayer-oriented subset.

## 6. Threading / trigger considerations

`AdditionalContent_ProcessMountStates` is called from game-logic code (`AdditionalContent_MountContentForMode`), which should mean it runs on the game thread like everything else this repo's ASIs hook — but this hasn't been explicitly stack-traced up to a top-level tick/callback the way, say, `UEngine::Exec` obviously runs on the game thread. Worth a sanity check (a simple log line with a thread ID) before assuming it's safe to touch `GConfig` synchronously from inside the hook without any locking beyond what `FConfig::SetArray` already does internally.

## 7. Scope boundary

This is a **write** feature, unlike `ConfigDumper` (read-only, §8 of its own brief). Keep it narrowly
scoped to publishing the installed-mod-folder list; don't grow it into a general "let mods write
arbitrary config" facility without a separate design pass — the uncertainty in §5 around `SetArray`'s
exact semantics means every additional use of that primitive is another thing to independently
verify.
