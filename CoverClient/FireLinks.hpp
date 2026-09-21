#pragma once

#include <LESDK/Headers.hpp>

namespace CoverClient
{
	// ! Fire links on the LIVE cover slots
	// ========================================
	//
	// A trooper spawned on the finished network moves along the path nodes, fires, and never
	// enters cover. The combat profiler shows `SFXAICmd_AcquireCover_316:FindingCover` with a
	// three-digit instance number, so the command is being pushed, failing, and popped over
	// and over.
	//
	// `Goal_AtCover::RateSlotMarker` (LE3 0x7FF74B3A1630, vtable slot 92) is where every slot
	// dies. Its early-outs, in order, against `Marker->OwningSlot`:
	//
	//   1. OwningSlot.Link null, SlotIdx out of range
	//   2. `Slot.bEnabled` (0xDC bit 0x4000) clear
	//   3. `Link.bCircular | Link.bPlayerOnly` (0x404 bits 0x20|0x80) set
	//   4. `AI.bHighCoverOnly` set and `Slot.CoverType != CT_Standing`
	//   5. `ACoverSlotMarker::IsValidClaim(AI.Pawn)` false
	//   6. **`Slot.FireLinks.Count == 0` while `AI.EnemyList.Count > 0`**
	//
	// Only then does it count the marker as tested and hand it to the constraints. Gate 6 is
	// ours: the profiler showed `EnemyList: Pawn: SFXPawn_PlayerSoldier_0 Visible: True`, so
	// the trooper always has an enemy, and every slot we generate has an empty FireLinks --
	// nothing in the shipping binary fills it, because LE3 stripped `BuildFireLinks` along
	// with `GetFireActions` (note 02).
	//
	// So the AI's question is not "is this cover any good", it is "can anything be shot from
	// here at all", and an empty FireLinks answers no before any rating happens.
	//
	// The geometry half of this already existed -- `CoverGeneration.cpp`'s
	// `ComputeFireLinksForSlot` implements UE3's `GetFireActions` gate order faithfully. But
	// it runs over the C++ wrapper graph (`allNetworkNodes`) on its way to MilanCoverHost and
	// never touches an `FCoverSlot`, so the work had been done in the one place that could not
	// affect the game. This is the same shape as the `Actions` bug fixed just before it.

	/// Fills `FCoverSlot::FireLinks` on every link `cc.sweep` spawned. Call after the
	/// `Actions` pass (the source action list is read from `Slot.Actions`) and after
	/// `BuildSlotInfo`, so `CoverType` and the lean flags are settled.
	///
	/// Returns the number of FFireLink entries written.
	int BuildFireLinksOnSpawnedLinks();

	/// cc.firelinkstats -- reports FireLinks coverage on the spawned links and re-resolves a
	/// sample of the packed refs through the engine's own decoder, so a wrong level offset
	/// shows up as a mismatch rather than as silently dead cover. Also covers the two
	/// packed-properties arrays below.
	void CmdFireLinkStats();

	// ! The other two packed arrays
	// ========================================
	//
	// Counted across 1077 shipped slots in three levels (note 06), the per-slot fill rates are:
	//
	//     ExposedCoverPackedProperties  99%   mean 18.4, median 16, max 51
	//     DangerCoverPackedProperties   90%   mean 33.0, median 32, max 137
	//     FireLinks                     79%   mean 24.6, median 21, max 139
	//     Actions                       92%
	//
	// So exposed cover is the single most consistently populated thing on a cover slot --
	// more so than fire links -- and the live path wrote neither of these. Both reference the
	// level's ref tables the same way FireLinks does, but through *different* tables:
	// exposed cover indexes CoverIndexPairs (link + slot), danger cover indexes
	// NavRefs/NavRefIndicies (plain nav points). Crossing them produces a network that loads
	// without complaint and behaves wrongly.

	/// Fills `FCoverSlot::ExposedCoverPackedProperties` -- for each slot, which other cover
	/// slots can see it from an angle its cover does not protect against, and how badly.
	/// Port of UE3's `GetExposedInfo` onto the live slots.
	int BuildExposedCoverOnSpawnedLinks();

	/// Fills `FCoverSlot::DangerCoverPackedProperties` -- for each slot, which nav points it
	/// can be shot from. Port of `ACoverLink::BuildOtherLinks` (note 08).
	///
	/// Reproduces BioWare's scaling bug deliberately; see the comment on the implementation.
	int BuildDangerCoverOnSpawnedLinks();

	/// Sets the handful of scalar slot fields that shipped data sets universally but the CDO
	/// does not: `bForceNoGroundAdjust` (TRUE on 1077 of 1077) and `ForceCoverType`
	/// (equal to the computed `CoverType` on 1077 of 1077).
	int ApplyShippedSlotDefaults();
}
