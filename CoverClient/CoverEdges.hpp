#pragma once

#include <LESDK/Headers.hpp>

#include <vector>

namespace CoverClient
{
	// ! FindCoverEdges -- Phase 2
	// ========================================
	//
	// A port of ACoverLink::FindCoverEdges (UE3 UnCover.cpp:82), the editor's slot placement
	// scan. LE3 ships AutoAdjustSlot / BuildSlotInfo / ABioScout::CanDoMove intact but this
	// function was stripped, so slot *placement* is the half we have to supply.
	//
	// The algorithm is two walks:
	//   1. From the click point, walk LEFT until an edge is found (a floor gap, a gap in the
	//      wall, or a corner that turns the axes). Place the first slot at that edge.
	//   2. Walk back RIGHT from that slot, laying slots down: intermediate ones every
	//      AutoCoverSlotInterval, PAIRED transition slots wherever cover changes between
	//      standing and mid-level, and a pair at inside corners. Stop at the right edge.
	//
	// This replaces the crude two-ended ScanToEdge in ScoutHarness, which measured the same
	// box as 144u wide from one side and 180u from the other and pushed slots off the end of
	// the geometry (where AutoAdjustSlot then disabled them).
	//
	// Two deliberate departures from UE3, both because UE3's version only ever ran on the
	// flat-floored test geometry its authors pointed it at. See Notes/03.
	//   * The walk rides a fixed height above the FLOOR, not at a fixed Z. UE3 steps
	//     sideways without re-grounding, so on a ramp every forward trace, every
	//     standing-vs-mid test and every transition seam drifts by however much the floor
	//     has moved -- and once the drift passes StandHeight the walk is tracing through the
	//     ramp itself and calls all of it standing cover.
	//   * The standing-vs-mid test is taken at floor+StandHeight, matching AutoAdjustSlot,
	//     rather than at walkZ+MidHeight. See SlotOrigin below and the comment at the trace.

	/// What a downward floor probe found.
	///
	/// Exposed because cc.sweep needs the same answer to "can a player stand here" that the
	/// placement walk uses. If the two disagreed, the sweep would seed cover on faces the walk
	/// then refuses to lay slots along, and every such seed costs a spawned link.
	enum class FloorProbe : unsigned char
	{
		/// Floor found in open air below the probe.
		Found,
		/// The probe started INSIDE geometry. Not a floor: something is standing here that is
		/// at least as tall as the caller's allowance. A box trace that starts in a solid
		/// reports the hit at its own start point, so a caller that treats this as Found will
		/// creep up the face of a ledge one allowance at a time.
		Blocked,
		/// Nothing under the probe at all.
		None,
	};

	/// Probes downward for the floor at `probe`, from `topLift` above it to `bottomDrop` below.
	/// Position the top relative to the floor you already know about, never relative to the
	/// point you are probing from -- that is what makes a step-up allowance mean what it says.
	FloorProbe ProbeFloor(AActor* tracer, const FVector& probe, float topLift,
		float bottomDrop, float& outFloorZ);

	/// Which branch of the walk placed a slot. Recorded per slot so a bad one can be traced
	/// back to the decision that produced it instead of guessed at from its final position --
	/// by the time you see a slot it has been ground-snapped, re-aimed and re-seated by
	/// AutoAdjustSlot, and none of that is recoverable from the result.
	enum class SlotOrigin : unsigned char
	{
		Unknown = 0,
		LeftEdgeDrop,          ///< left walk ran out of floor
		LeftEdgeWallGap,       ///< left walk ran out of wall
		IntervalStanding,      ///< AutoCoverSlotInterval reached along standing cover
		IntervalMid,           ///< AutoCoverSlotInterval reached along mid-level cover
		TransitionStandToMid,  ///< one of the pair straddling a standing -> mid seam
		TransitionMidToStand,  ///< one of the pair straddling a mid -> standing seam
		InsideCornerOld,       ///< corner pair, still facing the wall we walked in along
		InsideCornerNew,       ///< corner pair, facing the wall we turned onto
		RightEdge,             ///< right walk found the end cap of the cover
		RightEdgeDrop,         ///< right walk ran out of walkable floor
		RightEdgeNoLip,        ///< right walk found the end but no lip to measure against
		RightEdgeNotAWall,     ///< sideways trace hit a floor or ramp, not a wall to turn onto
		LeftEdgeOffSurface,    ///< left walk drifted too far in height from the seeded surface
		RightEdgeOffSurface,   ///< right walk drifted too far in height from the seeded surface
		SingleSlotRecentre,    ///< the one-slot fix-up at the end
		LeftEdgeInsideCorner,  ///< left walk reached an inside corner off the seeded face
	};

	/// One past the last SlotOrigin, for tallies indexed by origin.
	///
	/// The sweep's per-branch counters guard their bounds and silently SKIP an origin past
	/// the end, so a hardcoded size does not crash -- it just quietly stops counting the
	/// newest branch, which is the one being added because it needs watching. Derived here so
	/// that cannot happen, provided new values are appended rather than inserted.
	constexpr size_t k_slotOriginCount =
		static_cast<size_t>(SlotOrigin::LeftEdgeInsideCorner) + 1;

	const char* SlotOriginName(SlotOrigin origin);

	/// Where a slot was put versus where the engine decided it belonged. A large `moved` or
	/// `turned` means AutoAdjustSlot's 128-trace sweep disagreed with the walk about which
	/// surface the slot is against -- which is a placement bug even when the slot ends up
	/// enabled, because the slot is no longer on the line the walk measured.
	struct SlotPlacement
	{
		int        slotIdx{ -1 };
		SlotOrigin origin{ SlotOrigin::Unknown };
		FVector    placedLocation{};
		int        placedYaw{ 0 };
		FVector    seatedLocation{};
		int        seatedYaw{ 0 };
		/// AutoAdjustSlot's cover-type pass cleared bEnabled, and whether the retry against
		/// the corrected facing brought it back. See ClassifySlot in the .cpp.
		bool       bDisabledFirstPass{ false };
		bool       bRecovered{ false };
		/// AutoAdjustSlot moved the slot vertically further than the walk's own slope limit,
		/// so it is no longer on the surface the walk measured. Disabled on the spot.
		bool       bReseatedOffSurface{ false };
	};

	struct FindCoverEdgesResult
	{
		bool  bFoundLeftEdge{ false };
		bool  bFoundRightEdge{ false };
		int   slotsPlaced{ 0 };
		int   leftWalkIterations{ 0 };
		int   rightWalkIterations{ 0 };
		/// Distance between the first and last slot, i.e. how wide we decided the cover is.
		/// The acceptance test for this port is that the same obstacle measures the same
		/// width from either approach angle.
		float span{ 0.0f };
		/// The vertical and horizontal extent of every slot, not just the first and last.
		///
		/// These exist because the floor-rise warning cannot see a wall climb. That warning
		/// measures how far the FLOOR moved between iterations, and a walk that climbs a wall
		/// places its slots off the floor entirely -- the floor beneath them never moves, so
		/// the rise stays at zero while the slots march straight up. A sweep in which seven
		/// links each rose 400u reported exactly one climber.
		///
		/// Measured across all slots rather than end to end, so a walk that goes up and comes
		/// back down is still counted as having climbed.
		float verticalExtent{ 0.0f };
		float horizontalExtent{ 0.0f };
		/// How many slots AutoAdjustSlot's cover-type pass disabled on the first attempt,
		/// and how many the retry brought back (see ClassifySlot). A run with
		/// `slotsDisabled == slotsRecovered` is healthy -- the retry is doing its job. A run
		/// where they diverge is placing slots against nothing, which is a placement bug and
		/// worth chasing; those slots stay disabled and would serialise as dead cover.
		int   slotsDisabled{ 0 };
		int   slotsRecovered{ 0 };
		/// 1 when the left walk stopped at an inside corner rather than turning onto the
		/// adjoining face. See the corner branch of the left walk for why that is the right
		/// answer: the left walk's only job is to find where THIS cover run starts.
		int   leftStoppedAtCorner{ 0 };
		/// Inside corners the right walk turned only because of the second probe in the
		/// "wall ran out" branch -- corners where the two faces do not meet exactly, so the
		/// front wall ran out a step before the ordinary side probe would have seen them.
		/// These were end caps before, which is why cover stopped dead at some corners and
		/// turned at others.
		int   rightCornersRecovered{ 0 };

		/// ! Why the right walk stopped, recorded rather than inferred.
		///
		/// Two theories about this have now been wrong -- a chamfered corner the probe missed
		/// by one step, and a nearby window link stealing the connection -- and both were
		/// argued from slot positions after the fact. These are what the walk actually saw at
		/// the moment it gave up.
		///
		/// `rightEndProbeDistance` is -1 when the sideways probe found nothing at all, which
		/// is the interesting case: it means there was no wall within 2*AlignDist along the
		/// direction of travel, so the inside-corner branch could not fire however tight the
		/// geometry looks on screen.
		bool  bRightEndProbed{ false };
		float rightEndProbeDistance{ -1.0f };
		/// dot(-hitNormal, axisX). Near 1 means the probe grazed the wall we were already
		/// following; near 0 means a genuine perpendicular face.
		float rightEndProbeAlignment{ 0.0f };
		/// The terminal slot was dropped for landing within half an AlignDist of the previous
		/// one -- UE3 does the same at a full AlignDist. On a pillar one pawn wide this is
		/// why a run can end on a transition slot with no edge slot at all.
		bool  bRightEndSlotSuppressed{ false };
		/// Which of the four right-edge branches actually ended the walk. Recorded inside
		/// EmitRightEdgeSlot so it is captured even when the slot is suppressed and therefore
		/// never appears in the per-branch tally -- which is exactly the case that has been
		/// invisible: a run ending on a transition slot looks, in every other output, like a
		/// walk that simply stopped.
		SlotOrigin rightEndOrigin{ SlotOrigin::Unknown };

		/// ! Did the walk keep a floor reference after each corner turn?
		///
		/// The drop-branch turn fires precisely BECAUSE a floor probe failed, and the turn
		/// then re-anchors and probes again. If that second probe also fails, the walk carries
		/// on along the new face with no floor -- and IsStandingHere falls back to an offset
		/// from the walk's own Z instead of measuring from the floor, so every stretch can
		/// classify as standing and neither the transition pair nor the interval slot fires.
		/// A 412u leg with two slots and nothing between them looks exactly like that.
		int cornerTurns{ 0 };
		int cornerTurnsWithoutFloor{ 0 };
		/// Right-walk iterations that ran with no floor reference at all.
		int iterationsWithoutFloor{ 0 };
		/// The largest gap between consecutive slots. Should never exceed the slot interval by
		/// much -- a leg far longer than AutoCoverSlotInterval with nothing on it means the
		/// interval rule never fired there.
		float widestSlotGap{ 0.0f };
		/// Times a transition was detected but its seam trace found nothing, so the paired
		/// slots could not be placed. Before this was handled the walk did NOTHING in that
		/// case -- no slot, no state change, no distance accumulated -- so `lastCoverType`
		/// stayed on the old value and every later iteration retried the same failed trace.
		/// A window immediately after a corner was skipped entirely that way: no transition
		/// pair, and no interval slot either, because the distance counter never advanced.
		int transitionSeamsMissed{ 0 };
		/// Iterations of the right-hand walk that ended up no further along the wall than
		/// they started, and had to be forced forward or stopped. Non-zero means a branch
		/// re-anchored behind the step it took; see the guard at the bottom of that loop.
		int   walkStalls{ 0 };
		/// Where the walk was told to start, and the floor height it found there. Filled in by
		/// the caller, because FindCoverEdges is handed a scan point rather than the surface
		/// hit that produced it. Without this, "the link is 300u up a wall" cannot be told
		/// apart from "the link was SEEDED 300u up a wall" -- the first is a walk defect, the
		/// second is a sweep defect, and they need opposite fixes.
		FVector seedPoint{};
		float   seedFloorZ{ 0.0f };
		bool    bSeedHadFloor{ false };
		/// 1 when the walk could not find a floor under its own starting point. Worth counting
		/// rather than shrugging at: the scan start is only 32u from the wall, so a downward
		/// box probe grazing the wall face is the likely cause, and everything the walk does
		/// with height is degraded until it finds one.
		int     seedFloorMissing{ 0 };
		/// Slots AutoAdjustSlot lifted or dropped off the measured surface -- see EmitSlot.
		/// These are disabled rather than shipped, because the engine marks them enabled and
		/// CT_Standing and nothing downstream would otherwise reject cover floating in air.
		int   slotsReseatedOffSurface{ 0 };
		float worstSeatLift{ 0.0f };
		/// Total climb along the right-hand walk, so a run over a ramp is obvious in the log.
		float floorRise{ 0.0f };
		/// Total climb along the LEFT walk, which sets where slot 0 lands and hence the height
		/// the whole link sits at. Measured separately because a link can be entirely in the
		/// wrong place with a right-hand rise of exactly zero.
		float leftFloorRise{ 0.0f };
		/// One entry per slot, in placement order.
		std::vector<SlotPlacement> placements{};
	};

	/// Runs the placement scan and fills link->Slots.
	///
	/// @param link     The freshly spawned ACoverLink. Its existing archetype slot is reused
	///                 as slot 0 rather than left stranded (see Notes/06).
	/// @param tracer   Actor to trace as (the player pawn works). Must not be the link, or
	///                 every trace hits the link's own collision cylinder.
	/// @param startLoc Where to begin. EditorAutoSetup uses
	///                 Location + AxisX*96 + AxisZ*16, which lands ~32u in front of the
	///                 surface and 16u up.
	/// @param axisX    Link forward (into the wall) -- i.e. -hitNormal.
	/// @param axisY    Link right.
	/// @param axisZ    Link up.
	FindCoverEdgesResult FindCoverEdges(ACoverLink* link, AActor* tracer,
		const FVector& startLoc, FVector axisX, FVector axisY, FVector axisZ);
}
