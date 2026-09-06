#include "CoverClient/CoverEdges.hpp"
#include "CoverClient/Utilities/PathUtilities.hpp"
#include "Common/Base.hpp"

#include <algorithm>
#include <cmath>

namespace CoverClient
{
	namespace
	{
		/// UE3 traces with a 1-unit box rather than a ray, so thin geometry still registers.
		const FVector k_traceExtent{ 1.0f, 1.0f, 1.0f };

		/// How closely a wall the left walk turns onto has to match the seeded facing to
		/// count as the same surface: cos(45 degrees). Above this the wall is bending and the
		/// walk follows it; below, it is a different face and the run starts here.
		constexpr float k_sameSurfaceCos = 0.707f;

		/// How many times the right walk may turn an inside corner from the drop branch.
		constexpr int k_maxCornerTurnsAtDrop = 2;

		/// Both walks bail after this many steps. UE3 used 150 for each.
		constexpr int k_maxIterations = 150;

		/// How far the floor may move in one step before the walk stops calling it a slope,
		/// as a multiple of AlignDist. A step is 2*AlignDist along the surface, so 2.0 is
		/// exactly a 45-degree slope in either direction. The sandbox ramp climbs 36u per
		/// step (26 degrees) and is followed; the sandbox ledge climbs 100u in one step (54
		/// degrees) and is not.
		///
		/// This is measured from the FLOOR, never from the walk line. The walk rides
		/// rideHeight (a CoverLink cylinder, ~44u) above the floor, so probing from the line
		/// silently adds that to the allowance -- 116u instead of 72u, which is how a 100u
		/// ledge got walked over.
		constexpr float k_maxSlopeFactor = 2.0f;

		/// Headroom above the slope limit, so a step of exactly the limit is found in open
		/// air rather than reported as Blocked.
		constexpr float k_probeClearance = 4.0f;

		/// Smallest Z component of a surface normal the walk will stand on. UE3's MINFLOORZ.
		/// ~45.5 degrees, which is also what k_maxSlopeFactor allows, so the two agree.
		constexpr float k_minFloorZ = 0.7f;

		/// Reach of the anchor probe, which asks where the floor under the walk is rather
		/// than whether a step is allowed. UE3 has no allowance at all: it traces straight
		/// down from wherever the walk line happens to be, so a floor that has risen above
		/// the line reads as a drop.
		constexpr float k_anchorProbeFactor = 2.0f;

		FVector Add(const FVector& a, const FVector& b)
		{
			return { a.X + b.X, a.Y + b.Y, a.Z + b.Z };
		}

		FVector Sub(const FVector& a, const FVector& b)
		{
			return { a.X - b.X, a.Y - b.Y, a.Z - b.Z };
		}

		FVector Scale(const FVector& v, float s)
		{
			return { v.X * s, v.Y * s, v.Z * s };
		}

		float Size(const FVector& v)
		{
			return std::sqrt(v.X * v.X + v.Y * v.Y + v.Z * v.Z);
		}

		FVector Negate(const FVector& v)
		{
			return { -v.X, -v.Y, -v.Z };
		}

		FVector Lift(const FVector& v, float dz)
		{
			return { v.X, v.Y, v.Z + dz };
		}

		/// FRotationMatrix(rot).GetAxes(). Matches CoverGeneration's GetSlotAxes.
		void RotatorToAxes(const FRotator& rot, FVector& axisX, FVector& axisY, FVector& axisZ)
		{
			const float pitchRad = UnrealRotationUnitsToRadians(rot.Pitch);
			const float yawRad = UnrealRotationUnitsToRadians(rot.Yaw);
			const float rollRad = UnrealRotationUnitsToRadians(rot.Roll);
			const float SP = std::sin(pitchRad), CP = std::cos(pitchRad);
			const float SY = std::sin(yawRad), CY = std::cos(yawRad);
			const float SR = std::sin(rollRad), CR = std::cos(rollRad);
			axisX = { CP * CY,                    CP * SY,                   SP };
			axisY = { SR * SP * CY - CR * SY,     SR * SP * SY + CR * CY,   -SR * CP };
			axisZ = { -(CR * SP * CY + SR * SY),  CY * SR - CR * SP * SY,    CR * CP };
		}

		/// Mirrors UE3's `GWorld->SingleLineCheck(...)` return convention: TRUE means the
		/// trace hit NOTHING. Reads as a double negative but keeps every branch below
		/// line-for-line comparable with UnCover.cpp, which matters far more here than
		/// prettier naming -- this function is nothing but nested trace branches.
		///
		/// Uses AActor::Trace rather than the raw SingleLineCheck because the SDK's
		/// FCheckResult is an opaque 160-byte blob, and every branch here needs Hit.Location
		/// and Hit.Normal. Trace hands both back as out-params.
		bool TraceClear(AActor* tracer, const FVector& start, const FVector& end,
			FVector& outHitLocation, FVector& outHitNormal)
		{
			outHitLocation = FVector{};
			outHitNormal = FVector{};
			auto* const hit = tracer->Trace(end, start, FALSE /*bTraceActors*/, k_traceExtent,
				0, &outHitLocation, &outHitNormal, nullptr);
			return hit == nullptr;
		}

		/// The anchor probe, for a point the walk is already standing at rather than stepping
		/// to. No slope limit applies: we are asking where the floor under us is, not whether
		/// we are allowed to go there.
		bool FindFloorZ(AActor* tracer, const FVector& probe, float alignDist, float& outFloorZ)
		{
			return ProbeFloor(tracer, probe, alignDist * k_anchorProbeFactor,
				alignDist * 4.0f, outFloorZ) == FloorProbe::Found;
		}

		struct StepFloor
		{
			bool bWalkable{ false };
			/// Nothing to stand on, or a drop too steep to follow. The walk edge condition.
			bool bDropsAway{ false };
		};

		/// One step of floor-following. `floorZ` is the floor under where the walk is now and
		/// is advanced on a walkable step. `end` is lifted onto the new floor on a walkable
		/// step and left alone otherwise -- so that a ledge is still standing in the way when
		/// the caller runs its sideways trace, and the corner logic sees it.
		StepFloor FollowFloor(AActor* tracer, FVector& end, float alignDist, bool bHaveFloor,
			float rideHeight, float& floorZ)
		{
			StepFloor step{};
			if (!bHaveFloor)
			{
				float unused = 0.0f;
				step.bDropsAway = ProbeFloor(tracer, end, alignDist * k_anchorProbeFactor,
					alignDist * 4.0f, unused) == FloorProbe::None;
				return step;
			}

			const float maxSlope = alignDist * k_maxSlopeFactor;
			const float topLift = (floorZ + maxSlope + k_probeClearance) - end.Z;

			float stepFloorZ = 0.0f;
			const FloorProbe probe =
				ProbeFloor(tracer, end, topLift, alignDist * 4.0f, stepFloorZ);
			if (probe == FloorProbe::None)
			{
				step.bDropsAway = true;
				return step;
			}
			if (probe == FloorProbe::Blocked)
			{
				// A ledge. Leave `end` where it is and let the sideways trace hit its face.
				return step;
			}

			if (stepFloorZ - floorZ < -maxSlope)
			{
				// Cover that keeps going over a drop this steep lets the player pop up and
				// fall off the far side, so the cover ends here.
				step.bDropsAway = true;
				return step;
			}

			end.Z = stepFloorZ + rideHeight;
			floorZ = stepFloorZ;
			step.bWalkable = true;
			return step;
		}

		/// Z step of AutoAdjustSlot's standing sweep, and the length of each of its traces
		/// as a multiple of AlignDist. Both read out of the binary at 0x7FF74AD30870:
		/// DAT_7ff74b95e2c0 = 16.0 and DAT_7ff74b91d1fc = 4.0.
		constexpr float k_classifyStep = 16.0f;
		constexpr float k_classifyTraceFactor = 4.0f;

		/// The engine's own standing-vs-mid test, asked at a point the walk is passing through.
		///
		/// AutoAdjustSlot does not ask one question at one height. It starts at floor+MidHeight,
		/// traces AlignDist*4 forward along the slot facing, steps up 16u, and repeats while it
		/// has not passed floor+StandHeight -- four traces for the shipped 70/130 pair, at
		/// +70, +86, +102 and +118. The FIRST one that comes back clear makes the slot
		/// mid-level; it is standing only if every one of them is blocked.
		///
		/// The walk has to ask exactly that question, because it acts on the answer and
		/// AutoAdjustSlot then overwrites CoverType with its own. One trace at floor+StandHeight
		/// disagrees with the sweep on any wall with a gap BELOW its top -- a window, a railing,
		/// a crate stack with a hole in it -- and the disagreement does not stay cosmetic. The
		/// walk reads "standing" and emits a transition; AutoAdjustSlot writes "mid"; the next
		/// iteration reads the engine's answer off the slot, sees the disagreement again, and
		/// emits another pair from the same place. Nothing breaks the tie, so it runs to the
		/// iteration cap. A 64000u sweep of BioP_MPPSWarehouse produced thirteen links like
		/// that, 111 to 297 slots each -- 2664 of the run's 3918 slots, one link fitting 297
		/// into 119u of wall.
		bool IsStandingHere(AActor* tracer, const FVector& at, const FVector& axisX,
			float alignDist, float midHeight, float standHeight,
			bool bHaveFloor, float floorZ, float fallbackOffset)
		{
			// Measured from the FLOOR, as the engine measures it. Falling back to the walk
			// line costs us rideHeight of accuracy, but that only happens where there is no
			// floor to find, and then nothing here is trustworthy anyway.
			const float baseZ = bHaveFloor ? (floorZ + midHeight) : (at.Z + fallbackOffset);
			const float band = standHeight - midHeight;
			const float traceLength = alignDist * k_classifyTraceFactor;

			FVector hit{}, normal{};
			for (float remaining = band; remaining > 0.0f; remaining -= k_classifyStep)
			{
				FVector from = at;
				from.Z = baseZ + (band - remaining);
				const FVector to = Add(from, Scale(axisX, traceLength));
				if (TraceClear(tracer, from, to, hit, normal))
				{
					return false;
				}
			}
			return true;
		}

		/// Places a slot, reusing the inherited archetype slot for the first one.
		///
		/// UE3's FindCoverEdges opens with Slots.Empty(), which we cannot safely do to an
		/// engine-owned TArray from outside. Instead the first placement overwrites slot 0
		/// in place -- which is the slot Spawn inherited from the CDO -- and the rest append.
		/// Same end state, no array surgery. See Notes/06 on the archetype slot.
		int PlaceSlot(ACoverLink* link, const FVector& location, const FRotator& rotation,
			bool& bFirstSlotUsed)
		{
			if (!bFirstSlotUsed && link->Slots.Count() > 0)
			{
				bFirstSlotUsed = true;

				FVector axisX{}, axisY{}, axisZ{};
				RotatorToAxes(link->Rotation, axisX, axisY, axisZ);
				const FVector rel = Sub(location, link->location);

				FCoverSlot& slot = link->Slots(0);
				// AddCoverSlot's own conversion, UnCover.cpp:37-38.
				slot.LocationOffset = {
					rel.X * axisX.X + rel.Y * axisX.Y + rel.Z * axisX.Z,
					rel.X * axisY.X + rel.Y * axisY.Y + rel.Z * axisY.Z,
					rel.X * axisZ.X + rel.Y * axisZ.Y + rel.Z * axisZ.Z
				};
				slot.RotationOffset = {
					rotation.Pitch - link->Rotation.Pitch,
					rotation.Yaw - link->Rotation.Yaw,
					rotation.Roll - link->Rotation.Roll
				};
				return 0;
			}

			bFirstSlotUsed = true;
			return link->AddCoverSlot(location, rotation, -1, FALSE);
		}

		/// The walk reads LastSlot.CoverType to decide standing<->mid transitions. In the
		/// editor that field was already populated, because AddCoverSlot runs
		/// AutoAdjustSlot + BuildSlotInfo inline when !GIsGame (UnCover.cpp:47-53). In-game
		/// that branch is dead, so a naive port would read CoverType off every slot as
		/// CT_None and never emit a single transition pair. Run the cover-type pass by hand.
		///
		/// The retry is not belt-and-braces, it is load-bearing. LE3's AutoAdjustSlot
		/// (0x7FF74AD30870) runs its cover-type pass BEFORE the bAutoAdjust re-seat, and the
		/// cover-type pass clears bEnabled whenever the forward trace along the slot's
		/// *placed* facing finds nothing within 4*AlignDist. The re-seat's 128-trace circle
		/// sweep then corrects the yaw -- but the slot is already disabled by then, and the
		/// tail of the function skips BuildSlotInfo on a disabled slot. So a slot placed
		/// facing even slightly wrong comes back dead despite the engine having worked out
		/// where its wall is. Running the pass a second time re-tests against the corrected
		/// facing. A slot that disables twice is genuinely against nothing and stays off.
		void ClassifySlot(ACoverLink* link, int slotIdx, SlotPlacement& record,
			FindCoverEdgesResult& result)
		{
			link->AutoAdjustSlot(slotIdx, FALSE);
			if (link->Slots(slotIdx).bEnabled != 0)
			{
				return;
			}

			record.bDisabledFirstPass = true;
			result.slotsDisabled++;
			link->Slots(slotIdx).bEnabled = 1;
			link->Slots(slotIdx).bFailedToFindSurface = 0;
			link->AutoAdjustSlot(slotIdx, FALSE);
			if (link->Slots(slotIdx).bEnabled != 0)
			{
				record.bRecovered = true;
				result.slotsRecovered++;
			}
		}

		/// Place, classify and record in one step. Every placement in the walk goes through
		/// here so that provenance cannot be forgotten at a call site -- there are eleven of
		/// them and the whole point is being able to attribute a bad slot to its branch.
		int EmitSlot(ACoverLink* link, const FVector& location, const FRotator& rotation,
			SlotOrigin origin, bool& bFirstSlotUsed, FindCoverEdgesResult& result)
		{
			const int slotIdx = PlaceSlot(link, location, rotation, bFirstSlotUsed);
			if (slotIdx < 0 || slotIdx >= static_cast<int>(link->Slots.Count()))
			{
				return slotIdx;
			}

			SlotPlacement record{};
			record.slotIdx = slotIdx;
			record.origin = origin;
			record.placedLocation = location;
			record.placedYaw = rotation.Yaw;

			ClassifySlot(link, slotIdx, record, result);

			record.seatedLocation = link->GetSlotLocation(slotIdx, FALSE);
			record.seatedYaw = link->GetSlotRotation(slotIdx, FALSE).Yaw;

			// ! A slot the engine moved vertically is not on the surface we measured.
			//
			// AutoAdjustSlot does not only ground-snap. Its tail runs UWorld::FindSpot up to
			// sixteen times, stepping back along -axisX, and if the downward trace from the
			// result comes back clear it sets the slot to `lastHit.Z + CollisionHeight` --
			// the Z of whatever the previous trace happened to touch. Against a tall wall
			// that can be a long way up, and the slot lands in mid-air with nothing under it.
			//
			// This is the second, separate cause of cover climbing a wall. The first --
			// ProbeFloor taking a grazing wall hit for ground -- made the WALK ascend a step
			// at a time. This one relocates a SINGLE slot in one jump, which is why the
			// remaining floaters appear at "the next node" with no ladder of slots leading up
			// to them, and why fixing ProbeFloor did not touch them.
			//
			// The engine will happily mark such a slot CT_Standing and enabled, so nothing
			// downstream rejects it: it draws as ordinary cover and would serialise as
			// ordinary cover. Disabling it is the honest outcome -- the placement failed --
			// and every later pass (markers, ReachSpecs, the export) already skips disabled
			// slots.
			//
			// Threshold is the walk's own slope allowance: if the walk would not have STEPPED
			// to that height, a slot has no business being SEATED there. Ground-snapping a
			// correctly placed slot moves it by almost nothing, so this is generous.
			const float seatLift = std::abs(record.seatedLocation.Z - record.placedLocation.Z);
			if (seatLift > link->AlignDist * k_maxSlopeFactor)
			{
				link->Slots(slotIdx).bEnabled = 0;
				record.bReseatedOffSurface = true;
				result.slotsReseatedOffSurface++;
				if (seatLift > result.worstSeatLift) { result.worstSeatLift = seatLift; }
			}

			// The archetype-slot reuse means slot 0 can be written twice (the second time by
			// the one-slot recentre at the end); keep the later record, not both.
			for (auto& existing : result.placements)
			{
				if (existing.slotIdx == slotIdx)
				{
					existing = record;
					return slotIdx;
				}
			}
			result.placements.push_back(record);
			return slotIdx;
		}

		/// A cover facing, flattened to yaw.
		///
		/// Every facing in the walk is derived from a surface normal or an axis, and a normal
		/// that is not perfectly horizontal gives the rotator a pitch. Feed that back through
		/// RotatorToAxes and the walk's own axes tilt: axisX stops meaning "into the wall" and
		/// starts meaning "into the wall and downwards", so the forward cover trace, the
		/// standing test and the next step all run at an angle. Hit something near-vertical --
		/// the top of a plinth at the foot of a wall, say -- and axisX points almost straight
		/// down, at which point the walk follows the wall UP. That is the long-range vertical
		/// climb: not a floor bug at all, an axis bug.
		///
		/// Yaw-only is not a simplification, it is what the engine does. AutoAdjustSlot
		/// (0x7FF74AD30870) recomputes a slot's facing from its 128-trace circle sweep and
		/// takes ONLY the yaw out of the result (`FVector::Rotation(...)` then reading +4);
		/// pitch and roll are passed straight through from whatever we set. So a slot with
		/// pitch is nonsense we injected and the engine faithfully preserves.
		FRotator Facing(const FRotator& rotation)
		{
			return FRotator{ 0, rotation.Yaw, 0 };
		}

		FRotator FacingFrom(const FVector& direction)
		{
			return Facing(FVectorToFRotator(direction));
		}

		/// Whether a traced surface is a wall rather than a floor or ceiling.
		///
		/// The inside-corner branch turns the walk onto whatever its sideways trace hit. That
		/// is only meaningful if the thing hit is something you could take cover against. When
		/// FollowFloor lifts the step onto a higher floor the sideways trace becomes diagonal,
		/// and at the foot of a wall it lands on the top face of the plinth -- a normal
		/// pointing straight up, from which a facing is meaningless.
		bool IsWallSurface(const FVector& normal)
		{
			return std::abs(normal.Z) < k_minFloorZ;
		}

		/// Where the walk should continue from after emitting `slotIdx`.
		///
		/// Re-anchoring on the slot the engine actually seated is right when the engine merely
		/// ground-snapped it, and wrong when it relocated the slot somewhere the walk never
		/// measured. Following a slot that was lifted off the surface is how one bad seat
		/// turns into a whole chain of cover in the wrong place.
		FVector AnchorAfterSlot(ACoverLink* link, int slotIdx, const FVector& fallback)
		{
			if (slotIdx < 0 || slotIdx >= static_cast<int>(link->Slots.Count()))
			{
				return fallback;
			}
			if (link->Slots(slotIdx).bEnabled == 0)
			{
				return fallback;
			}
			return link->GetSlotLocation(slotIdx, FALSE);
		}

		/// Places a right-edge slot unless it would land on top of the one already there.
		///
		/// All three right-edge branches end the walk at the same wall end, and on a narrow
		/// object -- a pillar, a crate, a box corner -- the right walk reaches that end on its
		/// FIRST iteration, before `start` has moved off slot 0. The result is a link made of
		/// two coincident slots.
		///
		/// The sandbox sweep put 92 of 314 links (29%) in exactly that state, and the origin
		/// tally is unambiguous: every one is one left-edge slot plus one right-edge slot, in
		/// a perfect 1:1 split (LeftEdgeDrop 48 + LeftEdgeWallGap 44 = 92, RightEdgeDrop 52 +
		/// RightEdge 36 + RightEdgeNoLip 4 = 92). No corner slots, no interval slots. 56 of
		/// them came from the two branches that had no guard at all; RightEdge already had
		/// this test inline, which is why its links are small rather than coincident.
		///
		/// A suppressed slot is REMEMBERED, not discarded: the one-slot recentre at the bottom
		/// of FindCoverEdges uses it to put the single remaining slot in the middle of the
		/// cover instead of at its left edge, which is what a pillar should get.
		int EmitRightEdgeSlot(ACoverLink* link, const FVector& location,
			const FRotator& rotation, SlotOrigin origin, float alignDist, int lastSlotIdx,
			bool& bFirstSlotUsed, FVector& lastPotentialSlotLocation, float& lastSlotDist,
			FindCoverEdgesResult& result)
		{
			result.rightEndOrigin = origin;

			// Half of AlignDist, not AlignDist: neither left-edge branch has a guard at all,
			// so at AlignDist the same wall end gets a slot walking one way and none walking
			// the other. Measured on the sandbox -- the +X end of the test wall is 32u from
			// the last transition slot, so walking -X placed a slot there and walking +X
			// dropped it, leaving the last 32u of that pillar with no edge slot. AlignDist is
			// the slot standoff FROM the wall; it was never a minimum spacing ALONG it.
			if (Size(Sub(location, link->GetSlotLocation(lastSlotIdx, FALSE)))
				<= alignDist * 0.5f)
			{
				lastPotentialSlotLocation = location;
				result.bRightEndSlotSuppressed = true;
				return lastSlotIdx;
			}
			lastSlotDist = 0.0f;
			return EmitSlot(link, location, rotation, origin, bFirstSlotUsed, result);
		}
	}

	FloorProbe ProbeFloor(AActor* tracer, const FVector& probe, float topLift,
		float bottomDrop, float& outFloorZ)
	{
		const FVector top = Lift(probe, topLift);
		const FVector bottom = Lift(probe, -bottomDrop);
		FVector hitLocation{}, hitNormal{};
		if (TraceClear(tracer, top, bottom, hitLocation, hitNormal))
		{
			return FloorProbe::None;
		}
		if (hitLocation.Z >= top.Z - 1.0f)
		{
			return FloorProbe::Blocked;
		}

		// A hit is not a floor just because it is below us. This probe is a 1-unit BOX cast
		// straight down, so anywhere the walk line sits against a wall the box clips the wall
		// FACE on the way past and reports it as ground at whatever height it grazed.
		//
		// That is where cover climbing a flat wall comes from, and it compounds: the walk
		// rides rideHeight above that bogus floor, the next step probes from
		// floorZ + maxSlope + clearance, grazes the same wall higher up, and each iteration
		// lifts the line again -- a staircase of slots straight up a surface with nothing
		// under it, which then ends the left-to-right chain in the wrong place.
		//
		// Rejecting on the normal is the whole fix. 0.7 is UE3's MINFLOORZ, the same
		// threshold its pathing uses to decide a surface can be stood on; it is ~45.5
		// degrees, which is also what k_maxSlopeFactor = 2.0 already allows, so the two
		// limits agree rather than contradicting each other.
		//
		// Blocked, not None: something IS there, it is simply not standable. That keeps the
		// walk at its current height and lets the sideways trace run into the face, which is
		// exactly the ledge behaviour Blocked was introduced for. Returning None instead
		// would end the cover at every wall the probe brushed.
		if (hitNormal.Z < k_minFloorZ)
		{
			return FloorProbe::Blocked;
		}

		outFloorZ = hitLocation.Z;
		return FloorProbe::Found;
	}

	const char* SlotOriginName(SlotOrigin origin)
	{
		switch (origin)
		{
		case SlotOrigin::LeftEdgeDrop:         return "LeftEdgeDrop";
		case SlotOrigin::LeftEdgeWallGap:      return "LeftEdgeWallGap";
		case SlotOrigin::IntervalStanding:     return "IntervalStanding";
		case SlotOrigin::IntervalMid:          return "IntervalMid";
		case SlotOrigin::TransitionStandToMid: return "TransitionStandToMid";
		case SlotOrigin::TransitionMidToStand: return "TransitionMidToStand";
		case SlotOrigin::RightEdgeNotAWall:    return "RightEdgeNotAWall";
		case SlotOrigin::LeftEdgeOffSurface:   return "LeftEdgeOffSurface";
		case SlotOrigin::RightEdgeOffSurface:  return "RightEdgeOffSurface";
		case SlotOrigin::LeftEdgeInsideCorner: return "LeftEdgeInsideCorner";
		case SlotOrigin::InsideCornerOld:      return "InsideCornerOld";
		case SlotOrigin::InsideCornerNew:      return "InsideCornerNew";
		case SlotOrigin::RightEdge:            return "RightEdge";
		case SlotOrigin::RightEdgeDrop:        return "RightEdgeDrop";
		case SlotOrigin::RightEdgeNoLip:       return "RightEdgeNoLip";
		case SlotOrigin::SingleSlotRecentre:   return "SingleSlotRecentre";
		case SlotOrigin::Unknown:              break;
		}
		return "Unknown";
	}

	FindCoverEdgesResult FindCoverEdges(ACoverLink* link, AActor* tracer,
		const FVector& startLoc, FVector axisX, FVector axisY, FVector axisZ)
	{
		FindCoverEdgesResult result{};

		if (link == nullptr || tracer == nullptr)
		{
			return result;
		}

		auto* const cls = ACoverLink::StaticClass();
		auto* const defaults = (cls != nullptr)
			? static_cast<ACoverLink*>(cls->ClassDefaultObject) : nullptr;
		if (defaults == nullptr)
		{
			return result;
		}

		const float alignDist = defaults->AlignDist;
		const float slotInterval = defaults->AutoCoverSlotInterval;
		const float standHeight = defaults->StandHeight;
		const float midHeight = defaults->MidHeight;
		// UE3 names this StandingOffset but reads MidHeight into it. Only a fallback now,
		// for the case where the floor cannot be found at all -- see tallLift below.
		const float standingOffset = defaults->MidHeight;

		bool bFirstSlotUsed = false;
		FVector hitLocation{}, hitNormal{};

		// ! Pass 1 -- walk left until we find an edge
		// ========================================

		bool bFoundEdge = false;
		FVector start = startLoc;
		FVector lastHitNormal = Negate(axisX);

		// The face this link was seeded against, kept so the left walk can tell "still the
		// wall I started on, bending a bit" from "a different wall entirely". `axisX` points
		// into the wall, so this is the seeded facing and the comparison below is a dot
		// product against it.
		const FVector seedAxisX = axisX;

		// The walk rides a fixed height above the FLOOR rather than at a fixed Z, so it
		// follows ramps and steps instead of sinking into them or floating off them.
		float floorZ = 0.0f;
		bool bHaveFloor = FindFloorZ(tracer, start, alignDist, floorZ);
		float rideHeight = bHaveFloor ? (start.Z - floorZ) : 0.0f;

		// The left walk decides where slot 0 goes, and therefore the height the right walk
		// then runs at. A left walk that climbs a wall puts the whole link up there and the
		// right walk lays a perfectly ordinary horizontal row of slots at altitude -- which
		// is what the screenshots show, and which `floorRise` could never see because it only
		// ever measured the right walk.
		const float leftStartFloorZ = floorZ;

		// ! A cover run is ONE surface.
		//
		// Both walks follow the floor so that cover can run up a ramp, and the slope limit is
		// per-step. Per-step is not enough: a staircase of legal steps adds up to anything.
		// The sandbox produced a link seeded at Z 1084 whose LEFT walk descended to Z 772
		// before it placed slot 0, after which the right walk climbed the whole 300u back --
		// every individual step inside the limit, the finished link nowhere near the surface
		// that seeded it.
		//
		// So bound the total as well: once the floor has moved more than a man's height from
		// where the link was seeded, the walk has left the surface the seed found -- stepped
		// down off a ledge, or up a flight -- and anything it lays past that point belongs to
		// different cover.
		//
		// This does split cover on a long ramp into more than one link. That is the right
		// trade: several short links along a ramp are all valid cover and the sweep re-seeds
		// along it anyway, whereas one link spanning 300u of height is never correct.
		// NOT const, and not simply `floorZ`: floorZ is 0 when FindFloorZ fails, and comparing
		// a real floor at Z 1084 against a reference of 0 makes every walk look like it has
		// drifted a thousand units on its first grounded step. That is not hypothetical -- it
		// is what killed both walks at the sandbox's inside corner, reported as
		// LeftEdgeOffSurface / RightEdgeOffSurface, and it became reachable the moment
		// ProbeFloor started rejecting non-walkable hits: the scan start sits only 32u from
		// the wall, so its downward box probe can graze the wall face, which now returns
		// Blocked rather than a bogus floor.
		//
		// So the reference is adopted from the FIRST floor actually found, whenever that is.
		// A walk that never finds one has nothing to drift from and is simply not bounded.
		float seedFloorZ = floorZ;
		bool bHaveSeedFloor = bHaveFloor;
		const float maxFloorDrift = standHeight;

		result.seedPoint = startLoc;
		result.seedFloorZ = floorZ;
		result.bSeedHadFloor = bHaveFloor;
		if (!bHaveFloor) { result.seedFloorMissing++; }

		while (!bFoundEdge && result.leftWalkIterations++ < k_maxIterations)
		{
			// ! Stay on the surface that seeded this link.
			//
			// Checked at the TOP of the loop, not only after a step, because the corner
			// re-anchors reset floorZ through FindFloorZ -- which by design applies no slope
			// limit at all. It answers "where is the floor under me", and after a turn that
			// can be a different storey. A link seeded at Z 917 reported an 852u left-hand
			// rise with a single slot to show for it: every one of those units arrived through
			// an anchor, not through a step, so a per-step bound never saw them.
			if (bHaveFloor && !bHaveSeedFloor)
			{
				seedFloorZ = floorZ;
				bHaveSeedFloor = true;
			}
			if (bHaveSeedFloor && bHaveFloor
				&& std::abs(floorZ - seedFloorZ) > maxFloorDrift)
			{
				bFoundEdge = true;
				EmitSlot(link, start, FacingFrom(axisX), SlotOrigin::LeftEdgeOffSurface,
					bFirstSlotUsed, result);
				break;
			}

			const FVector initialStart = start;

			// Step left, following the floor into the step. Grounding the destination BEFORE
			// the sideways trace matters: on a rising ramp a level trace hits the floor, and
			// the walk reads that as a side wall and turns a corner that is not there.
			FVector end = Add(start, Scale(axisY, alignDist * -2.0f));
			StepFloor step =
				FollowFloor(tracer, end, alignDist, bHaveFloor, rideHeight, floorZ);
			const bool bLeftSurface = step.bWalkable && bHaveSeedFloor
				&& std::abs(floorZ - seedFloorZ) > maxFloorDrift;

			// Level probe, floor-followed step -- see the right-hand walk for why.
			const FVector levelEnd = Add(start, Scale(axisY, alignDist * -2.0f));
			if (TraceClear(tracer, start, levelEnd, hitLocation, hitNormal))
			{
				start = end;
				if (step.bWalkable)
				{
					result.leftFloorRise = floorZ - leftStartFloorZ;
				}
				if (bLeftSurface)
				{
					// Walked off the seeded surface -- see maxFloorDrift. The slot goes at
					// initialStart, the last position that was still on it.
					bFoundEdge = true;
					EmitSlot(link, initialStart, FacingFrom(axisX),
						SlotOrigin::LeftEdgeOffSurface, bFirstSlotUsed, result);
				}
				else if (step.bDropsAway)
				{
					// No floor -- the cover ends at a drop.
					bFoundEdge = true;
					const FVector below = Lift(start, alignDist * -4.0f);
					const FVector back = Add(below, Scale(axisY, alignDist * 4.0f));

					FVector slotLocation = initialStart;
					if (!TraceClear(tracer, below, back, hitLocation, hitNormal))
					{
						slotLocation = Add(slotLocation, Scale(Negate(axisY),
							(alignDist * 2.0f) - Size(Sub(hitLocation, below)) - alignDist));
					}
					EmitSlot(link, slotLocation, FacingFrom(axisX),
						SlotOrigin::LeftEdgeDrop, bFirstSlotUsed, result);
				}
				else
				{
					// Floor is there. Is the wall?
					end = Add(start, Scale(axisX, alignDist * 2.0f));
					if (TraceClear(tracer, start, end, hitLocation, hitNormal))
					{
						// Wall ran out -- that is the edge.
						bFoundEdge = true;
						const FVector lipStart = Add(start, Scale(axisX, alignDist * 1.5f));
						const FVector lipEnd = Add(lipStart, Scale(axisY, alignDist * 4.0f));

						FVector slotLocation = initialStart;
						if (!TraceClear(tracer, lipStart, lipEnd, hitLocation, hitNormal))
						{
							slotLocation = Add(slotLocation, Scale(Negate(axisY),
								(alignDist * 2.0f) - Size(Sub(hitLocation, lipStart)) - alignDist));
						}
						EmitSlot(link, slotLocation, FacingFrom(Negate(lastHitNormal)),
							SlotOrigin::LeftEdgeWallGap, bFirstSlotUsed, result);
					}
					else
					{
						// Still wall. Re-derive the axes from it and keep walking -- this is
						// what lets the scan follow a curved or angled surface instead of
						// assuming one flat plane, which is exactly what ScanToEdge got wrong.
						RotatorToAxes(FacingFrom(Negate(hitNormal)), axisX, axisY, axisZ);
						lastHitNormal = hitNormal;
						start = Add(hitLocation, Scale(axisX, -alignDist));
						bHaveFloor = FindFloorZ(tracer, start, alignDist, floorZ);
					}
				}
			}
			else
			{
				// ! Ran into a wall going left -- an inside corner.
				//
				// UE3 turns here and keeps walking, and that is right for the editor: a
				// designer placed the link on the surface they meant, so wherever the walk
				// wanders it comes back to cover that surface. Our seeds come from a ray fan,
				// and the difference is not academic. CoverLink_10 was seeded at
				// (-7122, 869) on a wall running in Y and put all four of its slots at
				// Y=536 on the perpendicular wall running in X: the left walk rounded the
				// corner, found an edge 500u down the adjoining face, and the right walk laid
				// the whole link there. The seeded face got no cover at all.
				//
				// Worse, it repeats. LinkCoversSurface asks whether an existing link covers
				// the CANDIDATE's seed point, and a link that walked off its own seed answers
				// no -- so the next seed along that same wall does the identical thing. Seven
				// links at X=-8118 came out 5 slots and 400u span each, to the unit, because
				// they are all the same link recomputed from seven different seeds.
				//
				// So the left walk stops at an inside corner instead of turning. Its only job
				// is to find where THIS run starts, and an inside corner is a legitimate
				// place for a run to start -- you cannot go further left, the wall is there.
				// The RIGHT walk keeps its corner behaviour, because cover continuing around
				// a corner in the direction of travel is wanted, and is what puts slots round
				// the turn.
				//
				// Bounded by angle rather than by corner count, so a wall that bends a few
				// degrees is still one surface and only a real turn stops the walk.
				const FVector turnedAxisX = Negate(hitNormal);
				const float alignment = (turnedAxisX.X * seedAxisX.X)
					+ (turnedAxisX.Y * seedAxisX.Y) + (turnedAxisX.Z * seedAxisX.Z);
				if (alignment < k_sameSurfaceCos)
				{
					bFoundEdge = true;
					result.leftStoppedAtCorner = 1;

					// ! Back off the wall we ran into, exactly as the right walk does.
					//
					// `start` is wherever the walk happened to be standing when the side
					// probe hit, which at an inside corner is hard against the perpendicular
					// wall -- and a pawn is wider than nothing, so it cannot physically reach
					// a slot placed there. In game this shows up as being unable to slide the
					// last few units into slot 0: the cover profile reads
					// "Slot Percent (from left to right): 0.05" and will not go lower.
					//
					// The right walk's corner branch has always measured from the wall it hit
					// rather than from where it was standing -- hitLocation + normal *
					// (AlignDist + 16) -- so mirror that. AlignDist is how far cover stands
					// off its own wall; the extra 16 is the same margin the corner pair uses.
					const FVector cornerBase = Add(hitLocation, Scale(hitNormal, alignDist));
					EmitSlot(link, Add(cornerBase, Scale(hitNormal, 16.0f)),
						FacingFrom(axisX), SlotOrigin::LeftEdgeInsideCorner,
						bFirstSlotUsed, result);
					break;
				}

				RotatorToAxes(FacingFrom(turnedAxisX), axisX, axisY, axisZ);
				start = Add(hitLocation, Scale(axisX, -alignDist));
				bHaveFloor = FindFloorZ(tracer, start, alignDist, floorZ);
			}
		}

		result.bFoundLeftEdge = bFoundEdge;
		if (!bFoundEdge)
		{
			// UE3 gives up entirely here and destroys the link.
			result.slotsPlaced = static_cast<int>(link->Slots.Count());
			return result;
		}

		// ! Pass 2 -- walk right, laying slots and transitions
		// ========================================

		bFoundEdge = false;
		int lastSlotIdx = 0;
		/// Inside-corner turns taken from the drop branch. Capped because that turn skips the
		/// loop's stall guard for its own iteration, so it needs a bound of its own.
		int cornerTurnsAtDrop = 0;
		RotatorToAxes(link->GetSlotRotation(lastSlotIdx, FALSE), axisX, axisY, axisZ);
		start = link->GetSlotLocation(lastSlotIdx, FALSE);

		// Re-anchor on slot 0. AutoAdjustSlot has ground-snapped it, so the gap between it
		// and the floor is exactly the height the right-hand walk should ride at.
		bHaveFloor = FindFloorZ(tracer, start, alignDist, floorZ);
		if (bHaveFloor)
		{
			rideHeight = start.Z - floorZ;
		}
		const float startFloorZ = floorZ;

		// How far to lift a trace from the walk line to the top of the engine's standing
		// test. AutoAdjustSlot (0x7FF74AD30870) sweeps from floor+MidHeight up to
		// floor+StandHeight and calls the slot mid-level if ANY of those traces miss, so the
		// seam the walk is looking for is where the wall stops reaching floor+StandHeight.
		// UE3's walk instead tests at walkZ+MidHeight -- about a CoverLink cylinder height
		// low -- so it calls walls standing that the engine then classes as mid, and puts the
		// transition pair in the wrong place. That is where a slot standing in front of a
		// window while its neighbour crouches against solid wall comes from.
		const float tallLift = bHaveFloor ? (standHeight - rideHeight) : standingOffset;

		FVector lastPotentialSlotLocation = start;
		float lastSlotDist = 0.0f;
		int stalls = 0;

		while (!bFoundEdge && result.rightWalkIterations++ < k_maxIterations)
		{
			// ! Stay on the surface that seeded this link.
			//
			// Checked at the TOP of the loop, not only after a step, because the corner
			// re-anchors reset floorZ through FindFloorZ -- which by design applies no slope
			// limit at all. It answers "where is the floor under me", and after a turn that
			// can be a different storey. A link seeded at Z 917 reported an 852u left-hand
			// rise with a single slot to show for it: every one of those units arrived through
			// an anchor, not through a step, so a per-step bound never saw them.
			if (bHaveFloor && !bHaveSeedFloor)
			{
				seedFloorZ = floorZ;
				bHaveSeedFloor = true;
			}
			if (bHaveSeedFloor && bHaveFloor
				&& std::abs(floorZ - seedFloorZ) > maxFloorDrift)
			{
				bFoundEdge = true;
				lastSlotIdx = EmitRightEdgeSlot(link, start, FacingFrom(axisX),
					SlotOrigin::RightEdgeOffSurface, alignDist, lastSlotIdx, bFirstSlotUsed,
					lastPotentialSlotLocation, lastSlotDist, result);
				break;
			}

			const unsigned char lastCoverType = link->Slots(lastSlotIdx).CoverType;
			const FVector initialStart = start;

			FVector end = Add(start, Scale(axisY, alignDist * 2.0f));
			const StepFloor step =
				FollowFloor(tracer, end, alignDist, bHaveFloor, rideHeight, floorZ);
			if (step.bWalkable)
			{
				result.floorRise = floorZ - startFloorZ;
			}
			if (step.bWalkable && bHaveSeedFloor
				&& std::abs(floorZ - seedFloorZ) > maxFloorDrift)
			{
				// Walked off the seeded surface -- see maxFloorDrift.
				bFoundEdge = true;
				lastSlotIdx = EmitRightEdgeSlot(link, start, FacingFrom(axisX),
					SlotOrigin::RightEdgeOffSurface, alignDist, lastSlotIdx, bFirstSlotUsed,
					lastPotentialSlotLocation, lastSlotDist, result);
				continue;
			}
			if (!bHaveFloor) { result.iterationsWithoutFloor++; }

			if (step.bDropsAway)
			{
				// ! Is this a drop, or the corner wall's footprint?
				//
				// The drop test runs BEFORE the level side probe below, so at an inside
				// corner the walk stops here and corner detection never executes at all.
				// That is why the identical probe added to the wall-ran-out branch never
				// fired once across a whole sweep: the walk was never reaching it.
				//
				// Measured, not guessed -- `cc.explainlink` on the corner that would not turn
				// reported "Right end branch: RightEdgeDrop (the wall-ran-out path never
				// ran)". Stepping toward a corner puts the destination hard against the
				// perpendicular wall, where a downward probe has no walkable floor to find:
				// it is either inside the wall's footprint or grazing its face. "No floor
				// ahead" there means a corner, not a cliff.
				//
				// Bounded by a small count rather than the loop's stall guard, because the
				// turn re-anchors and then skips that guard for this iteration.
				bool bTurnedAtDrop = false;
				if (cornerTurnsAtDrop < k_maxCornerTurnsAtDrop)
				{
					const FVector dropProbeEnd =
						Add(start, Scale(axisY, alignDist * 2.0f));
					if (!TraceClear(tracer, start, dropProbeEnd, hitLocation, hitNormal)
						&& IsWallSurface(hitNormal))
					{
						const FVector turnedAxisX = Negate(hitNormal);
						const float alignment = (turnedAxisX.X * axisX.X)
							+ (turnedAxisX.Y * axisX.Y) + (turnedAxisX.Z * axisX.Z);
						if (alignment < k_sameSurfaceCos)
						{
							const FVector cornerBase =
								Add(hitLocation, Scale(hitNormal, alignDist));
							EmitSlot(link, Add(cornerBase, Scale(hitNormal, 16.0f)),
								FacingFrom(axisX), SlotOrigin::InsideCornerOld,
								bFirstSlotUsed, result);
							lastSlotIdx = EmitSlot(link,
								Add(cornerBase, Scale(axisX, -16.0f)),
								FacingFrom(turnedAxisX), SlotOrigin::InsideCornerNew,
								bFirstSlotUsed, result);

							RotatorToAxes(link->GetSlotRotation(lastSlotIdx, FALSE),
								axisX, axisY, axisZ);
							start = AnchorAfterSlot(link, lastSlotIdx, start);
							bHaveFloor = FindFloorZ(tracer, start, alignDist, floorZ);
							lastSlotDist = 0.0f;
							cornerTurnsAtDrop++;
							result.cornerTurns++;
							if (!bHaveFloor) { result.cornerTurnsWithoutFloor++; }
							result.rightCornersRecovered++;
							bTurnedAtDrop = true;
						}
					}
				}

				if (!bTurnedAtDrop)
				{
					// UE3 right-hand walk never looks at the floor, so it runs cover out over
					// whatever is beyond the drop. Stop at the lip instead.
					//
					// `start` has not moved yet this iteration -- the drop is detected before
					// the step is taken -- so on iteration 1 this is slot 0's own location.
					// That is why it needs the suppression guard even more than the branches
					// below.
					bFoundEdge = true;
					lastSlotIdx = EmitRightEdgeSlot(link, start, FacingFrom(axisX),
						SlotOrigin::RightEdgeDrop, alignDist, lastSlotIdx, bFirstSlotUsed,
						lastPotentialSlotLocation, lastSlotDist, result);
				}
				continue;
			}

			// ! The side-wall probe runs LEVEL, the step follows the floor.
			//
			// FollowFloor lifts `end` onto a walkable step before this trace, which made the
			// trace diagonal -- and a diagonal trace asks two questions at once: "is a wall in
			// my way" and "is the floor rising". At the foot of a wall those answers differ.
			// Walking toward an inside corner whose wall has a plinth, the step is lifted onto
			// the plinth top and the diagonal trace clips that top FACE, so the walk reads a
			// floor as a side wall.
			//
			// Probing level separates them: the obstruction test asks only about geometry at
			// the height the walk is at, and the position update still uses the floor-followed
			// `end`, so ramps are followed exactly as before. UE3 traced level too -- it just
			// never lifted `end` at all, which is what broke ramps in the first place.
			const FVector levelEnd = Add(start, Scale(axisY, alignDist * 2.0f));
			if (TraceClear(tracer, start, levelEnd, hitLocation, hitNormal))
			{
				const float sideDist = Size(Sub(end, start));

				// Side is clear; is there still cover in front?
				const FVector stepped = end;
				start = stepped;
				end = Add(start, Scale(axisX, alignDist * 3.0f));
				if (!TraceClear(tracer, start, end, hitLocation, hitNormal))
				{
					const FRotator hitRotation = FacingFrom(Negate(hitNormal));

					// The seam traces below run at the top of the classification band, which is
					// the height that separates a tall wall from a low one most sharply.
					const FVector tallStart = Lift(start, tallLift);

					// Classify this stretch the way AutoAdjustSlot will -- see IsStandingHere.
					if (!IsStandingHere(tracer, start, axisX, alignDist, midHeight,
						standHeight, bHaveFloor, floorZ, standingOffset))
					{
						// Nothing up high -- this stretch is mid-level.
						if (lastCoverType == 1 /*CT_Standing*/)
						{
							// Standing -> mid. Trace back for the seam and pair the slots.
							const FVector seamStart =
								Add(tallStart, Scale(axisX, alignDist * 1.5f));
							const FVector seamEnd =
								Add(seamStart, Scale(axisY, alignDist * -2.0f));
							if (!TraceClear(tracer, seamStart, seamEnd, hitLocation, hitNormal))
							{
								// Measured back from the step we actually took, so the pair
								// inherits the stepped Z. UE3 rebuilds this from the previous
								// position and a flat 2*AlignDist, which is the same point
								// only while the floor is level.
								const FVector mid = Sub(stepped,
									Scale(axisY, Size(Sub(hitLocation, seamStart))));
								EmitSlot(link, Sub(mid, Scale(axisY, alignDist)), hitRotation,
									SlotOrigin::TransitionStandToMid, bFirstSlotUsed, result);
								lastSlotIdx = EmitSlot(link, Add(mid, Scale(axisY, alignDist)),
									hitRotation, SlotOrigin::TransitionStandToMid,
									bFirstSlotUsed, result);

								RotatorToAxes(link->GetSlotRotation(lastSlotIdx, FALSE),
									axisX, axisY, axisZ);
								start = AnchorAfterSlot(link, lastSlotIdx, start);
								bHaveFloor = FindFloorZ(tracer, start, alignDist, floorZ);
								lastSlotDist = 0.0f;
							}
							else
							{
								// ! Seam not found -- still mid cover, so still place a slot.
								//
								// Doing nothing here is a trap. `lastCoverType` is read from
								// the last slot placed, so leaving it on standing makes the
								// next iteration retry this same failing trace, and the one
								// after that, for the whole mid stretch -- while
								// `lastSlotDist` never accumulates, so the interval slot never
								// fires either. A window immediately after a corner came out
								// as 412u of nothing between two standing slots.
								//
								// One slot rather than the pair: without the seam we do not
								// know where the boundary is, and a single slot inside the
								// mid stretch is honest about that while still letting the
								// walk carry on in the right cover type.
								lastSlotIdx = EmitSlot(link, start, hitRotation,
									SlotOrigin::TransitionStandToMid, bFirstSlotUsed, result);
								lastSlotDist = 0.0f;
								result.transitionSeamsMissed++;
							}
						}
						else
						{
							// Still mid-level; advance and drop a slot every interval.
							RotatorToAxes(hitRotation, axisX, axisY, axisZ);

							lastSlotDist += sideDist;
							if (lastSlotDist >= slotInterval)
							{
								lastSlotIdx = EmitSlot(link, start, hitRotation,
									SlotOrigin::IntervalMid, bFirstSlotUsed, result);
								lastSlotDist = 0.0f;
							}
						}
					}
					else
					{
						// Wall continues up high -- standing cover.
						if (lastCoverType == 1 /*CT_Standing*/)
						{
							RotatorToAxes(FacingFrom(Negate(hitNormal)),
								axisX, axisY, axisZ);

							lastSlotDist += sideDist;
							if (lastSlotDist >= slotInterval)
							{
								lastSlotIdx = EmitSlot(link, start, hitRotation,
									SlotOrigin::IntervalStanding, bFirstSlotUsed, result);
								lastSlotDist = 0.0f;
							}
						}
						else
						{
							// Mid -> standing. The tall wall starts somewhere between where we
							// were and where we are, so trace forward from the previous
							// position to find it, and pair the slots at the seam.
							const FVector seamStart = Lift(
								Add(initialStart, Scale(axisX, alignDist * 1.5f)), tallLift);
							const FVector seamEnd =
								Add(seamStart, Scale(axisY, alignDist * 2.0f));
							if (!TraceClear(tracer, seamStart, seamEnd, hitLocation, hitNormal))
							{
								const FVector mid = Add(initialStart,
									Scale(axisY, Size(Sub(hitLocation, seamStart))));
								EmitSlot(link, Sub(mid, Scale(axisY, alignDist)), hitRotation,
									SlotOrigin::TransitionMidToStand, bFirstSlotUsed, result);
								lastSlotIdx = EmitSlot(link, Add(mid, Scale(axisY, alignDist)),
									hitRotation, SlotOrigin::TransitionMidToStand,
									bFirstSlotUsed, result);

								RotatorToAxes(link->GetSlotRotation(lastSlotIdx, FALSE),
									axisX, axisY, axisZ);
								start = AnchorAfterSlot(link, lastSlotIdx, start);
								bHaveFloor = FindFloorZ(tracer, start, alignDist, floorZ);
								lastSlotDist = 0.0f;
							}
							else
							{
								// Mirror of the stand -> mid case above, and the same trap.
								lastSlotIdx = EmitSlot(link, start, hitRotation,
									SlotOrigin::TransitionMidToStand, bFirstSlotUsed, result);
								lastSlotDist = 0.0f;
								result.transitionSeamsMissed++;
							}
						}
					}
				}
				else
				{
					// ! Did the wall run out, or did we just arrive at a corner?
					//
					// The side-wall probe that detects an inside corner runs at the TOP of the
					// loop, before the step. By the time we get here we have stepped, and
					// nothing has asked whether a perpendicular wall is now beside us. Where
					// two faces meet exactly that costs nothing -- the side probe catches the
					// corner on the iteration before the front wall runs out. Where they do
					// not, which is a chamfer, a small gap, or simply two brushes that abut
					// without touching, the front wall runs out one step early and the walk
					// calls it an end cap.
					//
					// That is the corner that would not turn. 46 corners in the same sweep
					// turned cleanly, so the corner branch works; it was never being reached
					// here. Run the probe the next iteration would have run, and if there is a
					// genuine wall beside us facing a different way, turn instead of ending.
					bool bTurnedAtCorner = false;
					const FVector cornerProbeEnd = Add(start, Scale(axisY, alignDist * 2.0f));
					result.bRightEndProbed = true;
					result.rightEndProbeDistance = -1.0f;
					if (!TraceClear(tracer, start, cornerProbeEnd, hitLocation, hitNormal))
					{
						result.rightEndProbeDistance = Size(Sub(hitLocation, start));
						const FVector probedAxisX = Negate(hitNormal);
						result.rightEndProbeAlignment = (probedAxisX.X * axisX.X)
							+ (probedAxisX.Y * axisX.Y) + (probedAxisX.Z * axisX.Z);
					}
					if (result.rightEndProbeDistance >= 0.0f && IsWallSurface(hitNormal))
					{
						const FVector turnedAxisX = Negate(hitNormal);
						const float alignment = result.rightEndProbeAlignment;
						if (alignment < k_sameSurfaceCos)
						{
							// Same corner pair the ordinary branch emits: one slot still
							// facing the wall we came along, one facing the wall we turn onto.
							const FVector cornerBase =
								Add(hitLocation, Scale(hitNormal, alignDist));
							EmitSlot(link, Add(cornerBase, Scale(hitNormal, 16.0f)),
								FacingFrom(axisX), SlotOrigin::InsideCornerOld,
								bFirstSlotUsed, result);
							lastSlotIdx = EmitSlot(link,
								Add(cornerBase, Scale(axisX, -16.0f)),
								FacingFrom(turnedAxisX), SlotOrigin::InsideCornerNew,
								bFirstSlotUsed, result);

							RotatorToAxes(link->GetSlotRotation(lastSlotIdx, FALSE),
								axisX, axisY, axisZ);
							start = AnchorAfterSlot(link, lastSlotIdx, start);
							bHaveFloor = FindFloorZ(tracer, start, alignDist, floorZ);
							lastSlotDist = 0.0f;
							result.rightCornersRecovered++;
							// Deliberately NOT `continue`: the stall guard at the bottom of
							// the loop is what stops a re-anchoring branch running to the
							// iteration cap, and this branch re-anchors exactly like the
							// ordinary corner one. Skipping it is how a 297-slot link happens.
							bTurnedAtCorner = true;
						}
					}

					if (!bTurnedAtCorner)
					{
						// No cover in front any more -- right edge.
						bFoundEdge = true;
						const FVector lipStart = Add(start, Scale(axisX, alignDist * 1.5f));
						const FVector lipEnd = Add(lipStart, Scale(axisY, alignDist * -2.0f));
						if (!TraceClear(tracer, lipStart, lipEnd, hitLocation, hitNormal))
						{
							const FVector newSlotLocation = Sub(stepped, Scale(axisY,
								Size(Sub(hitLocation, lipStart)) + alignDist));

							// axisX, NOT -hitNormal. The trace above ran back along the wall at
							// 1.5*AlignDist of depth, so what it hit is the wall's END CAP --
							// whose normal points along the walk direction. Facing the slot that
							// way aims it down the open side of the cover instead of into it,
							// which is what was killing the rightmost slot of every walk:
							// AutoAdjustSlot's forward trace found nothing and cleared bEnabled.
							// The end cap tells us WHERE the edge is; the wall we have been
							// following the whole time tells us where to face. (The mirrored
							// left-edge branch already gets this right -- it uses the saved wall
							// normal, not the fresh end-cap one.)
							lastSlotIdx = EmitRightEdgeSlot(link, newSlotLocation,
								FacingFrom(axisX), SlotOrigin::RightEdge, alignDist,
								lastSlotIdx, bFirstSlotUsed, lastPotentialSlotLocation,
								lastSlotDist, result);
						}
						else
						{
							lastSlotIdx = EmitRightEdgeSlot(link, initialStart,
								FacingFrom(axisX), SlotOrigin::RightEdgeNoLip, alignDist,
								lastSlotIdx, bFirstSlotUsed, lastPotentialSlotLocation,
								lastSlotDist, result);
						}
					}
				}
			}
			else if (!IsWallSurface(hitNormal))
			{
				// The sideways trace hit something, but not a wall -- a floor, a ramp, the
				// top face of a plinth at the foot of the wall we are following. FollowFloor
				// lifts `end` onto a walkable step BEFORE this trace runs, which makes the
				// trace diagonal, and a diagonal trace at the base of a wall lands on exactly
				// that. Turning the walk onto it would aim the whole coordinate frame at a
				// surface with no facing worth having.
				//
				// This is what was stopping cover turning an inside corner and sending it up
				// the wall instead: the corner branch fired, the "new wall" was the floor, and
				// the walk followed it. End the cover here instead -- honest, and a corner
				// that genuinely continues is one where the trace hits an actual wall.
				bFoundEdge = true;
				lastSlotIdx = EmitRightEdgeSlot(link, start, FacingFrom(axisX),
					SlotOrigin::RightEdgeNotAWall, alignDist, lastSlotIdx, bFirstSlotUsed,
					lastPotentialSlotLocation, lastSlotDist, result);
			}
			else
			{
				// Hit a side wall -- an inside corner. Two slots: one still facing the way we
				// came, one facing the new wall.
				const FVector cornerBase = Add(hitLocation, Scale(hitNormal, alignDist));
				EmitSlot(link, Add(cornerBase, Scale(hitNormal, 16.0f)),
					FacingFrom(axisX), SlotOrigin::InsideCornerOld,
					bFirstSlotUsed, result);
				lastSlotIdx = EmitSlot(link, Add(cornerBase, Scale(axisX, -16.0f)),
					FacingFrom(Negate(hitNormal)), SlotOrigin::InsideCornerNew,
					bFirstSlotUsed, result);

				RotatorToAxes(link->GetSlotRotation(lastSlotIdx, FALSE), axisX, axisY, axisZ);
				start = AnchorAfterSlot(link, lastSlotIdx, start);
				bHaveFloor = FindFloorZ(tracer, start, alignDist, floorZ);
				lastSlotDist = 0.0f;
				result.cornerTurns++;
				if (!bHaveFloor) { result.cornerTurnsWithoutFloor++; }
			}

			// ! The walk must move.
			//
			// Three branches re-anchor `start` onto the slot AutoAdjustSlot actually placed
			// rather than onto the step the loop took -- both transitions and the inside
			// corner -- and the engine is free to put that slot behind where we stepped. If
			// the same branch then fires again from the same place, nothing breaks the tie
			// and the walk runs to k_maxIterations laying slots on top of each other. That
			// is what a 297-slot, 119u-wide link is.
			//
			// IsStandingHere removed the reason the transition branches disagreed with the
			// engine, and this makes it structurally impossible for any future disagreement
			// to cost 300 slots instead of one. One forced step is forgiving -- a re-anchor
			// legitimately lands near where we started; a second stall in a row means the
			// walk is stuck and the cover ends here.
			//
			// DISPLACEMENT, not progress along the direction we set out in. The first version
			// of this guard measured Dot(start - initialStart, stepAxisY) and was wrong for
			// the very reason inside corners exist: at a corner the walk deliberately TURNS,
			// so its movement along the OLD axis is near zero however far it actually went.
			// On the box-filled sandbox that flagged 158 of 461 links -- 34% -- and the
			// forced step then shoved the walk along the axis it had just abandoned, so the
			// next iteration failed too and the walk ended AT the corner. Cover stopping dead
			// at every corner is a worse bug than the one being guarded against.
			//
			// Distance moved is direction-agnostic, so a corner satisfies it for free, and it
			// is the exact thing the runaway failed to do: 297 slots in 119u is a walk whose
			// position did not change. Pure oscillation would slip through; k_maxIterations
			// is still the backstop, and nothing observed has oscillated.
			if (!bFoundEdge && Size(Sub(start, initialStart)) < alignDist * 0.5f)
			{
				stalls++;
				result.walkStalls++;
				if (stalls >= 2)
				{
					bFoundEdge = true;
				}
				else
				{
					// axisY as it stands NOW, not as it was: if a branch turned the walk,
					// the current axis is the direction it means to go.
					start = Add(initialStart, Scale(axisY, alignDist * 2.0f));
				}
			}
			else
			{
				stalls = 0;
			}
		}

		result.bFoundRightEdge = bFoundEdge;
		result.slotsPlaced = static_cast<int>(link->Slots.Count());

		// A one-slot link means both edges landed in nearly the same place; UE3 recentres it
		// between the two candidate positions rather than leaving it at the left edge.
		if (result.slotsPlaced == 1 && bFoundEdge)
		{
			const FVector oldSlotLocation = link->GetSlotLocation(0, FALSE);
			const FVector delta = Sub(lastPotentialSlotLocation, oldSlotLocation);
			const float halfLen = Size(delta) * 0.5f;
			const float len = Size(delta);
			if (len > 0.001f)
			{
				const FVector centred = Add(oldSlotLocation, Scale(delta, halfLen / len));
				bool bUnused = false;
				EmitSlot(link, centred, link->GetSlotRotation(0, FALSE),
					SlotOrigin::SingleSlotRecentre, bUnused, result);
			}
		}

		if (result.slotsPlaced > 1)
		{
			result.span = Size(Sub(link->GetSlotLocation(result.slotsPlaced - 1, FALSE),
				link->GetSlotLocation(0, FALSE)));

			// Cover is a horizontal thing. A link whose slots are stacked vertically is a
			// walk that climbed a wall, and nothing else in the result can say so: `span` is
			// direction-blind, and the floor-rise counters read zero for exactly this failure
			// because the climbing slots have left the floor behind.
			FVector lo = link->GetSlotLocation(0, FALSE);
			FVector hi = lo;
			for (int i = 1; i < result.slotsPlaced; i++)
			{
				const FVector at = link->GetSlotLocation(i, FALSE);
				lo.X = std::min(lo.X, at.X); hi.X = std::max(hi.X, at.X);
				lo.Y = std::min(lo.Y, at.Y); hi.Y = std::max(hi.Y, at.Y);
				lo.Z = std::min(lo.Z, at.Z); hi.Z = std::max(hi.Z, at.Z);
			}
			for (int i = 1; i < result.slotsPlaced; i++)
			{
				const float gap = Size(Sub(link->GetSlotLocation(i, FALSE),
					link->GetSlotLocation(i - 1, FALSE)));
				if (gap > result.widestSlotGap) { result.widestSlotGap = gap; }
			}

			result.verticalExtent = hi.Z - lo.Z;
			const float dx = hi.X - lo.X;
			const float dy = hi.Y - lo.Y;
			result.horizontalExtent = std::sqrt(dx * dx + dy * dy);
		}

		return result;
	}
}
