# SFXGame string↔ID lookup tables: `MPPlayerVariableMappings` and `SFXUniqueIDs`

Two related but distinct config-driven "give a string a small int ID" mechanisms in
`SFXGame.pcc`. Part 1 below is `SFXGameConfig.MPPlayerVariableMappings` (MP save-profile
setting IDs — store unlocks, purchasable items). Part 2 is `SFXEngine.SFXUniqueIDs` (network
replication IDs — MP lobby loadouts, power combos, accomplishments). They look superficially
similar (both are "config array that turns a string into an int") but work completely
differently under the hood and back completely different systems — see Part 2's "how this
differs from MPPlayerVariableMappings" for the comparison.

# Part 1: `SFXGame.SFXGameConfig.MPPlayerVariableMappings`

**Status: package-level investigation complete. Native call-site tracing not done for this one
specifically** (see Part 2 for the sibling mechanism, where it was).

## Question

The `sfxgame.sfxgameconfig` / `mpplayervariablemappings` config array shows up in coalesced
config like:

```
<Property name="mpplayervariablemappings">
    <Value type="4">(ClassName="recru_asa",ID=272)</Value>
    <Value type="4">(ClassName="recru_kro",ID=273)</Value>
    ...
    <Value type="4">(ClassName="SFXGameChoiceGUIData_StoreData_MPAssaultRifles_InventoryLocked",ID=56)</Value>
    <Value type="4">(ClassName="MatchConsumable_ActiveXPBonusMultiplierIndex_1",ID=132)</Value>
```

Question was: what is this, and how is it used, tracing through `SFXGame.pcc` and
`BIOP_MP_COMMON.pcc`?

## Method

Used **LECCLI** (`C:\Users\mgame\source\repos\ME3Tweaks\LECCLI`) — see
[CoverClient/Notes/07-lexcli.md](../CoverClient/Notes/07-lexcli.md) for the tool itself — to
read the packages directly rather than booting the game:

- `SFXGame.pcc`: `S:\SteamLibrary\steamapps\common\Mass Effect Legendary Edition\Game\LMP\BioGame\CookedPCConsole\SFXGame.pcc`
- `BIOP_MP_COMMON.pcc`: `S:\SteamLibrary\steamapps\common\Mass Effect Legendary Edition\Game\LMP\BioGame\DLC\DLC_MOD_ProjectMilan\CookedPCConsole\BIOP_MP_COMMON.pcc`

**Note the second path**: `BIOP_MP_COMMON.pcc` only exists as a loose file under
`DLC_MOD_ProjectMilan` — there is no loose copy in base `LMP\BioGame\CookedPCConsole` (base
game ships it packed in a DLC `.sfar`). So this is very likely **our own mod's copy**, not
stock BioWare content — worth double-checking before treating anything found in it as vanilla
behavior.

`list-properties --class SFXGameConfig --game LMP`, `decompile-class`, and `list-exports
--filter <text>` (a full-package name search, not a body-text grep) were the three commands
that did all the work here.

## Finding: it's a name↔ID lookup table, fully self-contained in `SFXGame.pcc`

`BIOP_MP_COMMON.pcc` does **not** reference this table at all — `list-exports --filter` for
`PurchasableItem`, `GetPurchasableItem`, and `recru` all returned zero matches in that
package. The original assumption (defined in SFXGame, used in BIOP_MP_COMMON) was wrong; it's
entirely a `SFXGame.pcc`-internal mechanism.

Definition, in `SFXGameConfig`:

```unrealscript
struct PurchasableItem
{
    var string className;
    var int Id;
};
var config array<PurchasableItem> MPPlayerVariableMappings;

static function int GetPurchasableItemID(string className)         // name -> ID, linear Find()
static function string GetPurchasableItemClassName(int Id)          // ID -> name, linear Find()
static function int GetMaxPurchasableItemID()                       // highest ID in table
static function bool VerifyPurchasableItems()                       // dupe-name / dupe-ID check, LogInternal on conflict
```

Despite the struct name, **most entries are not real UClasses**. Spot-checked several with
`find-export` and none resolved: `recru_asa`, `MatchConsumable_ActiveXPBonusMultiplierIndex_1`,
`AdeptAsari`, `SFXGameChoiceGUIData_StoreData_MPAssaultRifles_InventoryLocked` — none exist as
objects in `SFXGame.pcc`. They're arbitrary string *keys*. BioWare's own table already mixes
these synthetic keys (`starter`=232, `bf3`=269, `NewlyAffordableStoreItems`=271,
`LastPromoIDShown`=270) alongside entries that do name real content classes (weapons, weapon
mods, power classes, character-role classes). The `recru_asa`/`recru_kro`/`recru_qua`/
`recru_sal`/`recru_tur`/`recru_dre` (272–277) entries follow that same synthetic-key pattern —
whoever added them (presumably this mod) picked new unused IDs, same as BioWare did for
`starter`/`bf3`/etc.

**Only caller found**, a thin wrapper on the MP save-profile object:

```unrealscript
// SFXOnlineSaveGameMP
public event function int GetPurchasableItemID(const out string className)
{
    return Class'SFXGameConfig'.static.GetPurchasableItemID(className);
}
```

`SFXOnlineSaveGameMP` holds `CachedOnlinePlayerStorage` of type `SFXOnlinePlayerStorage`, whose
`ProfileSettings` is `array<OnlineProfileSetting>` — Unreal's generic ID→value profile-settings
store. Working theory: the `int Id` this table returns is used as the property/setting ID into
that store, i.e. this table exists to give a stable, save-compatible numeric ID to anything
("player owns weapon X", "player unlocked recruit Y", "consumable counter Z") that needs to be
persisted in the MP profile, without hardcoding magic numbers at every call site.

## Open thread: who actually calls this?

`GetPurchasableItemID` is declared `event function` (UE3: native-callable/overridable). No
UnrealScript caller was found anywhere in either package (`list-exports --filter
GetPurchasableItemID` across both files → only the definition + the `SFXOnlineSaveGameMP`
wrapper, no third caller). That strongly suggests the real call sites — where the game decides
"the player just unlocked `recru_asa`, go get its ID and set the profile bit" — live in native
C++ in `MassEffectMultiplayer.exe`, not script.

**Not yet done**: Ghidra tracing of `MassEffectMultiplayer.exe` for xrefs to
`GetPurchasableItemID`/`GetPurchasableItemClassName`/`ProfileSettings` to find the native
caller(s) and confirm the "ID = profile-setting property ID" theory. The Ghidra MCP in this
session was loaded against LE3's `MassEffect3.exe` (per
[CoverClient project notes](../CoverClient/Notes/00-INDEX.md)), not LMP's
`MassEffectMultiplayer.exe` — would need a separate Ghidra project/load for that binary.

## Reusable bits

- LECCLI build/usage: [CoverClient/Notes/07-lexcli.md](../CoverClient/Notes/07-lexcli.md).
- `list-exports --file-id N --filter <text> --max-results 20000` against a *class-scoped*
  `--game LMP` package is the fast way to search **export paths** (class/function/property
  names) across a whole `.pcc` without decompiling everything — but it only matches names, not
  script body text, so it can't find e.g. a string literal `"recru_asa"` passed as a runtime
  argument.
- `decompile-class`/`decompile-function --uindex <function-uIndex>` (not the class uIndex) to
  get a single function's script text.

# Part 2: `SFXGame.SFXEngine.SFXUniqueIDs`

**Status: complete, including native implementation and every script call site (game-wide).**

## Question

Same shape of question as Part 1, but for a different config array: `sfxgame.sfxengine` /
`sfxuniqueids`, also declared in `SFXGame.pcc`. Wanted: what it is, its native implementation,
and — the actual motivating question — **where in the game is it used**, so it's possible to
tell what kind of new content (recruits, weapons, powers, etc.) would actually need a new entry
added to it, versus what doesn't.

## Definition

```unrealscript
Class SFXEngine extends GameEngine
    ...
    config(Engine);

var const config array<string> SFXUniqueIDs;

static final native function int GetSFXUniqueIDFromStr(string Str);
static final native function string GetStrFromSFXUniqueID(int UniqueId);
```

Unlike `MPPlayerVariableMappings` (explicit `(ClassName, ID)` struct pairs, IDs hand-assigned),
this is a flat `array<string>`, `const` (script can't write to it), and both accessors are
**native** — all the actual lookup logic lives in C++, not script.

## Native implementation (Ghidra, `MassEffect3.exe` — LE3, `MassEffect3.exe-11.3.2` in the
project; this is core `SFXEngine`, shared with LMP, not MP-specific code)

Findings are written into the Ghidra project itself (function rename + `set_decompiler_comment`
on all three), read those first if picking this up from the binary:

- **`SFXEngine::GetSFXUniqueIDFromStrNative`** (`7ff74b3dd800`) — pure function, does **not**
  consult the config array or any table. Just returns `appStrCrcCaps(Str)`: UE3's standard
  case-insensitive CRC32 (uppercases the string, then runs it through `GCRCTable`). Any string
  produces a deterministic ID; the ID is **not** an index into `SFXUniqueIDs`.
- **`SFXEngine::GetStrFromSFXUniqueIDNative`** (`7ff74b3dd8d0`) — the reverse direction, and the
  one that actually needs the array. Looks the incoming ID up in an in-memory chained hash table
  (buckets + entries, classic separate-chaining hash map) that maps `appStrCrcCaps(entry) ->
  index into SFXUniqueIDs`. If found, returns `SFXUniqueIDs[index]`; if not found (i.e. no
  config entry hashes to that ID), returns `""`.
- **`SFXEngine::RebuildSFXUniqueIDsHashMap`** (`7ff74b420de0`, renamed from `FUN_7ff74b420de0`)
  — builds that reverse-lookup hash table: walks `self->SFXUniqueIDs`, computes
  `appStrCrcCaps(entry)` per string, inserts `(crc, index)`. Called from
  `USFXEngine::PostReloadConfig` (`7ff74b4293d0`) whenever the `SFXUniqueIDs` config array is
  (re)loaded.

**Practical implication**: the forward direction (string→ID) works for *any* string, config or
not — it's just a hash function. Only the **reverse** direction (ID→string) requires the string
to actually be listed in the `SFXUniqueIDs` config array. So the question "does X need an entry
in `SFXUniqueIDs`" is really "does anything need to reconstruct X's name *from* a replicated
int" — see call sites below for what that means concretely.

## Call sites — found game-wide via LECCLI `find-cross-package-usages`

`list-exports --filter` (used in Part 1) only searches **export names**, not script body text —
it cannot find `Class'SFXEngine'.static.GetSFXUniqueIDFromStr(...)` call expressions inside a
function. LECCLI has a purpose-built command for this: **`find-cross-package-usages --uindex
<uIndex> --max-files 8000`**, run against the exported `SFXEngine.GetSFXUniqueIDFromStr` /
`GetStrFromSFXUniqueID` function uIndex — it scans every `.pcc` in the game's import tables for
FinalFunctionCall references (these are `static final` functions, so calls compile to direct/
structural references, not virtual dispatch — `--uindex` is the right mode, see `LECCLI
find-cross-package-usages help` for when to use `--uindex` vs `--name`). Scanned all 742
candidate files in the `LMP` game; two packages actually reference these functions: `SFXGame.pcc`
itself and — confirming the original hunch — **`BIOP_MP_COMMON.pcc`** (`DLC_MOD_ProjectMilan`'s
copy).

**Forward direction (`GetSFXUniqueIDFromStr`, string→ID, 14 call sites) — all of these are "I'm
about to replicate/store this identifier as a compact int":**

| Caller | What it does |
|---|---|
| `SFXPRIMP.SetCharacterKit(Name)` | `CharacterData.CharacterKitID = GetSFXUniqueIDFromStr(string(CharacterKit))` |
| `SFXPRIMP.SetPower(PowerIndex, Name PowerClassPath, ...)` | `CharacterPowers[i].PowerClassPathID = ...` (6 power slots, each with evolved-choice ranks) |
| `SFXPRIMP.SetWeapon(WeaponIndex, Name WeaponClassPath, ...)` | `CharacterWeapons[i].WeaponClassPathID = ...` (2 weapon slots) |
| `SFXPRIMP.SetWeaponMod(WeaponIndex, ModIndex, Name WeaponModClassPath, ...)` | `CharacterWeapons[i].WeaponMods[j].WeaponModClassPathID = ...` (2 mod slots per weapon) |
| `SFXPRIMP.GetWeaponLevel(Name WeaponClassPath)` | hashes the query name to compare against the already-replicated `WeaponClassPathID` |
| `SFXPowerCustomAction.GetPowerComboTypeUniqueIDFromClass(className)` | power-combo-impact replication (e.g. Warp detonating Singularity) |
| `BioPlayerController.{UnlockAccomplishment,LockAccomplishment,UpdateAccomplishmentProgression,SetAccomplishmentProgression}(Name AccomplishmentName)` | accomplishment name → id, fed into the matching `Client*` RPC |
| `SFXGAWReinforcementBase.GenerateCardUniqueID(idx)` | `CardList[idx].UniqueId = GetSFXUniqueIDFromStr(CardList[idx].UniqueName)` — Galaxy-at-War reinforcement cards |
| `SFXOperationObjective.SetObjectiveData(...)` | `ReplicatedMeshInfo.MeshUniqueID` / `.ObjectiveTypeID` — Horde-mode objective mesh/type, server (`ROLE_Authority`) only |

**Reverse direction (`GetStrFromSFXUniqueID`, ID→string, 24 call sites) — the receiving side,
reconstructing the name from a replicated int:**

| Caller | Role |
|---|---|
| `SFXPRIMP.{GetCharacterKit,GetPower,GetWeapon,GetWeaponMod}` | client-side: turn a teammate's replicated loadout ints back into class names, to spawn/display their pawn, weapon and power actors in the MP lobby/match |
| `SFXPRIMP.SendTelemetryForWeapons` | reads the class names back out for analytics |
| `SFXPRIMP.VerifyPawnPowers` | validates the pawn's actual powers against the replicated `PowerClassPathID`s |
| `SFXPRIMP.ApplyMatchConsumableGameEffects` | resolves a consumable's effect class from its id |
| `SFXPRI.GetActiveMatchConsumable` | same, on the base (non-MP-specific) PRI |
| `SFXPowerCustomAction.GetPowerComboClassFromUniqueID` | reverse of the forward-direction combo-impact case above |
| `BioPlayerController.{ClientUnlockAccomplishment,ClientLockAccomplishment,ClientUpdateAccomplishmentProgression,ClientSetAccomplishmentProgression}` | client-side accomplishment RPC handlers, turning the id back into a name for UI/localization |
| `SFXGAWReinforcementBase.{GetPlayerVariableName,GetPlayerVariableNameFromCard}`, `SFXGAWReinforcementMatchConsumable_NonGameplay.{Activate,Deactivate,IsActive}` | reinforcement-card bookkeeping |
| `SFXOperationObjective.ClientFindObjectiveData`, `SFXObjective_Retrieve_PickupObject.ReplicatedEvent` | Horde objective/mesh reconstruction, client side |
| `SFXCheatManagerNonNativeMP.{DebugMatchConsumables,GiveAllMatchConsumables}` | debug/cheat console commands |

## Answering the actual question: what needs an entry?

`SFXUniqueIDs` is **not** the store/unlock-purchase mechanism — that's `MPPlayerVariableMappings`
(Part 1), a completely separate table with hand-assigned IDs. Nothing in the call-site list above
touches store purchases, unlock state, or `SFXOnlinePlayerStorage`'s `ProfileSettings`.

`SFXUniqueIDs` exists purely to make **network-replicated identifier ints** reconstructable back
into strings. Concretely, in `BIOP_MP_COMMON.pcc` (i.e. relevant to this mod), a string needs an
entry here if and only if it is:

- a **character kit name**, **power class path**, **weapon class path**, or **weapon mod class
  path** that gets set via `SFXPRIMP.Set{CharacterKit,Power,Weapon,WeaponMod}` — i.e. anything
  that can be part of a player's equipped MP loadout, since other clients need to reconstruct it
  client-side via `Get{CharacterKit,Power,Weapon,WeaponMod}` to render that player;
- a **power class name that participates in a power combo** (via
  `GetPowerComboTypeUniqueIDFromClass`/`GetPowerComboClassFromUniqueID`) — i.e. only powers whose
  detonation needs to trigger a combo VFX/damage on another client's screen;
- an **accomplishment name** passed to `UnlockAccomplishment`/etc.;
- a **GAW reinforcement card's `UniqueName`**, or a **Horde objective type/mesh identifier**
  (`SFXOperationObjective`) — only relevant if adding new Horde/Operation content, not applicable
  to most MP content additions.

If a new string isn't consumed by one of the functions above, it doesn't need an `SFXUniqueIDs`
entry — the "huge list of names" already in the config is exactly BioWare's accumulated set of
every kit/power/weapon/weapon-mod/accomplishment/reinforcement-card/objective string that has
ever needed to survive a round trip through one of these specific replication paths, not a
general-purpose registry. Adding something *not* consumed by these paths (e.g. a pure store SKU)
costs nothing if omitted, and adding it wouldn't do anything either, since nothing would ever
call `GetSFXUniqueIDFromStr`/`GetStrFromSFXUniqueID` on it.

## How this differs from `MPPlayerVariableMappings` (Part 1)

| | `MPPlayerVariableMappings` | `SFXUniqueIDs` |
|---|---|---|
| Shape | `array<struct{className, Id}>`, explicit pairs | `array<string>`, flat |
| ID origin | hand-assigned by whoever wrote the config entry | `appStrCrcCaps(string)` — computed, not stored |
| Collision risk you control | yes — must pick an unused int (`VerifyPurchasableItems` checks this) | no — determined entirely by the string's own content |
| Backing system | `SFXOnlineSaveGameMP` / `SFXOnlinePlayerStorage.ProfileSettings` — persistent MP save profile | ad hoc, per-feature network replication (`SFXPRIMP`, power combos, accomplishments, GAW cards, Horde objectives) |
| What needs an entry | anything that must persist a purchase/unlock/counter across sessions | only strings some specific replication path needs to reverse an int back into a name (see table above) |
