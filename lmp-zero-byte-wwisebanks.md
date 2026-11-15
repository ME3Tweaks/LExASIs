# LMP zero-byte WwiseBank audit

## Scope and method

- Scanned the installed LMP game tree at `S:\SteamLibrary\steamapps\common\Mass Effect Legendary Edition\Game\LMP` on 2026-09-28.
- Enumerated `.pcc`, `.upk`, and `.u` packages; loaded 793 LMP packages out of 794 package files, with zero package or export read errors.
- Checked each non-default `WwiseBank` export and measured the serialized `BnkFile` payload (`WwiseBank.BnkFile`). A listed payload is exactly 0 bytes.
- Searched all loaded LMP package imports for `WwiseBank` imports with the same object name. Importer entries below are direct package imports by name; where duplicate bank exports exist, the import does not identify a unique source package.

## Results

Found **40 zero-byte WwiseBank exports**, across **9 bank names**. No loose zero-byte files were found in the installed LMP tree; these are empty payloads embedded as exports in package files, not standalone empty bank files.

The CSV has one row per zero-byte export and includes each containing package and every matching inbound `WwiseBank` import entry.

| Bank export | Empty copies | Package files containing the export | Importing package files |
|---|---:|---|---|
| `Wwise__Placeholder_Music.Wwise__Placeholder_Music` | 2 | BioGame\DLC\DLC_MOD_ProjectMilan\CookedPCConsole\Levels\BioP_MPGeth\BioSnd_MPGeth.pcc<br>BioGame\DLC\DLC_MOD_ProjectMilan\CookedPCConsole\Levels\BioP_MPOcean\BioSnd_MPOcean.pcc | No matching imports found |
| `Wwise_Creature_Collector.Wwise_Creature_Collector` | 3 | BioGame\DLC\DLC_MOD_ProjectMilan\CookedPCConsole\Kits\Collector_Adept_MP.pcc<br>BioGame\DLC\DLC_MOD_ProjectMilan\CookedPCConsole\MP4Enemies\SFXPawn_CollectorTrooper.pcc<br>BioGame\DLC\DLC_MOD_ProjectMilan\CookedPCConsole\NonStartup\Pawns\SFXPawn_CollectorCaptain.pcc | No matching imports found |
| `Wwise_Creature_Praetorian.Wwise_Creature_Praetorian` | 11 | BioGame\DLC\DLC_MOD_ProjectMilan\CookedPCConsole\Kits\Asari_Vanguard_MP.pcc<br>BioGame\DLC\DLC_MOD_ProjectMilan\CookedPCConsole\Kits\Batarian_Vanguard_MP.pcc<br>BioGame\DLC\DLC_MOD_ProjectMilan\CookedPCConsole\Kits\Cerberus_Vanguard_MP.pcc<br>BioGame\DLC\DLC_MOD_ProjectMilan\CookedPCConsole\Kits\Drell_Vanguard_MP.pcc<br>BioGame\DLC\DLC_MOD_ProjectMilan\CookedPCConsole\Kits\HumanFemale_Vanguard_MP.pcc<br>BioGame\DLC\DLC_MOD_ProjectMilan\CookedPCConsole\Kits\HumanMale_Vanguard_MP.pcc<br>BioGame\DLC\DLC_MOD_ProjectMilan\CookedPCConsole\Kits\Krogan_Vanguard_MP.pcc<br>BioGame\DLC\DLC_MOD_ProjectMilan\CookedPCConsole\Kits\N7_Vanguard_MP.pcc<br>BioGame\DLC\DLC_MOD_ProjectMilan\CookedPCConsole\Kits\Volus_Vanguard_MP.pcc<br>BioGame\DLC\DLC_MOD_ProjectMilan\CookedPCConsole\NonStartup\Pawns\SFXPawn_Praetorian.pcc<br>BioGame\DLC\DLC_MOD_ProjectMilan\CookedPCConsole\NonStartup\Pawns\SFXPawn_Scion.pcc | No matching imports found |
| `Wwise_Creature_Red_Husk.Wwise_Creature_Red_Husk` | 1 | BioGame\DLC\DLC_MOD_ProjectMilan\CookedPCConsole\NonStartup\Pawns\SFXPawn_Abomination.pcc | No matching imports found |
| `Wwise_Creature_Scion.Wwise_Creature_Scion` | 1 | BioGame\DLC\DLC_MOD_ProjectMilan\CookedPCConsole\NonStartup\Pawns\SFXPawn_Scion.pcc | No matching imports found |
| `Wwise_Creature_Spider.Wwise_Creature_Spider` | 18 | BioGame\DLC\DLC_MOD_ProjectMilan\CookedPCConsole\Kits\Asari_Adept_MP.pcc<br>BioGame\DLC\DLC_MOD_ProjectMilan\CookedPCConsole\Kits\Asari_Infiltrator_MP.pcc<br>BioGame\DLC\DLC_MOD_ProjectMilan\CookedPCConsole\Kits\Asari_Sentinel_MP.pcc<br>BioGame\DLC\DLC_MOD_ProjectMilan\CookedPCConsole\Kits\AsariCommando_Adept_MP.pcc<br>BioGame\DLC\DLC_MOD_ProjectMilan\CookedPCConsole\Kits\Batarian_Adept_MP.pcc<br>BioGame\DLC\DLC_MOD_ProjectMilan\CookedPCConsole\Kits\Collector_Adept_MP.pcc<br>BioGame\DLC\DLC_MOD_ProjectMilan\CookedPCConsole\Kits\Drell_Adept_MP.pcc<br>BioGame\DLC\DLC_MOD_ProjectMilan\CookedPCConsole\Kits\HumanFemale_Adept_MP.pcc<br>BioGame\DLC\DLC_MOD_ProjectMilan\CookedPCConsole\Kits\HumanFemale_Sentinel_MP.pcc<br>BioGame\DLC\DLC_MOD_ProjectMilan\CookedPCConsole\Kits\HumanMale_Adept_MP.pcc<br>BioGame\DLC\DLC_MOD_ProjectMilan\CookedPCConsole\Kits\HumanMale_Sentinel_MP.pcc<br>BioGame\DLC\DLC_MOD_ProjectMilan\CookedPCConsole\Kits\HumanMale_SoldierBF3_MP.pcc<br>BioGame\DLC\DLC_MOD_ProjectMilan\CookedPCConsole\Kits\Krogan_Adept_MP.pcc<br>BioGame\DLC\DLC_MOD_ProjectMilan\CookedPCConsole\Kits\Krogan_Soldier_MP.pcc<br>BioGame\DLC\DLC_MOD_ProjectMilan\CookedPCConsole\Kits\Krogan_Vanguard_MP.pcc<br>BioGame\DLC\DLC_MOD_ProjectMilan\CookedPCConsole\Kits\N7_Adept_MP.pcc<br>BioGame\DLC\DLC_MOD_ProjectMilan\CookedPCConsole\Kits\Turian_Sentinel_MP.pcc<br>BioGame\DLC\DLC_MOD_ProjectMilan\CookedPCConsole\Kits\Vorcha_Soldier_MP.pcc | No matching imports found |
| `Wwise_Load_Audio_Random.Wwise_Load_Audio_Random` | 1 | BioGame\CookedPCConsole\EntryMenu.pcc | No matching imports found |
| `Wwise_VFX_DLC_Flashbang.Wwise_VFX_DLC_Flashbang` | 1 | BioGame\DLC\DLC_MOD_ProjectMilan\CookedPCConsole\BIOP_MP_COMMON.pcc | No matching imports found |
| `Wwise_VFX_Rockets.Wwise_VFX_Rockets` | 2 | BioGame\CookedPCConsole\SFXPawn_GethRocketTrooper.pcc<br>BioGame\DLC\DLC_MOD_ProjectMilan\CookedPCConsole\BIOP_MP_COMMON.pcc | BioGame\DLC\DLC_MOD_ProjectMilan\CookedPCConsole\Kits\N7_Soldier_MP.pcc<br>BioGame\DLC\DLC_MOD_ProjectMilan\CookedPCConsole\NonStartup\Pawns\SFXPawn_Scion.pcc |

## Notable entries

- `Wwise_VFX_DLC_Flashbang` is a 0-byte export in `BioGame\DLC\DLC_MOD_ProjectMilan\CookedPCConsole\BIOP_MP_COMMON.pcc`. No matching inbound `WwiseBank` imports were found anywhere in the scanned LMP packages.
- `Wwise_VFX_Rockets` has 0-byte exports in both `BioGame\CookedPCConsole\SFXPawn_GethRocketTrooper.pcc` and the ProjectMilan `BIOP_MP_COMMON.pcc`. Two package files import a bank with that object name: `Kits\N7_Soldier_MP.pcc` and `NonStartup\Pawns\SFXPawn_Scion.pcc`. Because there are two matching exports, those imports are name matches and do not resolve uniquely to either containing file.
- `Wwise_Creature_Spider` has 18 empty copies and `Wwise_Creature_Praetorian` has 11. The per-file locations are listed in the CSV.

## Trimming implications

The bank payload itself contributes no audio bytes. Removing a `WwiseBank` export from a package could save only its export/property metadata; deleting the containing `.pcc` would also remove every other asset in that package. The audit reports package-level import references, but it does not prove the absence of runtime name lookups or native loading behavior. Treat zero-byte exports with no inbound imports as trim candidates for a controlled package rebuild and runtime check, not as proof that the whole containing package is safe to delete.

## Machine-readable detail

- [lmp-zero-byte-wwisebanks.csv](lmp-zero-byte-wwisebanks.csv)
