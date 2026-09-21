#pragma once

#include <LESDK/Headers.hpp>

#include <vector>

namespace CoverClient
{
	// ! Floor sampling
	// ========================================
	//
	// "Where could a person stand?" -- asked by walking the floor outward from a point rather
	// than by snapping a grid over the world.
	//
	// cc.sweep uses it to decide where to fire its cover-finding fans; cc.pathnodes uses it to
	// decide where to put nav points. They are the same question and they must not answer it
	// differently: if the sampler the sweep uses admits a spot the path builder rejects, the
	// level gets cover with no route to it, and vice versa. That is why this is a shared
	// function rather than the two copies note 04's Phase 3 found.
	//
	// It also replaces the old fixed 280u world-aligned grid, whose two failures were the
	// "struggles on slopes" complaint (a grid cannot follow a ramp) and a visited key with no
	// Z component (which merged the two storeys of a multi-level room into one).

	struct FloorSample
	{
		/// At walk height, i.e. rideHeight above floorZ.
		FVector location{};
		float   floorZ{ 0.0f };
	};

	/// Quantised sample position. The Z component is what stops the two floors of a
	/// multi-level room merging into one.
	struct SampleKey
	{
		int x{ 0 }, y{ 0 }, z{ 0 };

		bool operator<(const SampleKey& other) const
		{
			if (x != other.x) { return x < other.x; }
			if (y != other.y) { return y < other.y; }
			return z < other.z;
		}
	};

	/// Vertical quantum for SampleKey. Two walkable surfaces closer together than a slope step
	/// is are the same surface; anything further apart is a different storey.
	constexpr float k_floorStoreyHeight = 72.0f;

	SampleKey FloorSampleKey(const FVector& v, float spacing);

	/// Why a floor walk between two points stopped.
	///
	/// Split out because "the floor does not continuously reach" covers four completely
	/// different pieces of geometry and lumping them together made the ramp problem
	/// undiagnosable: a step up onto a crate and a probe that came back Blocked on a slope
	/// steeper than MINFLOORZ are not the same defect and do not have the same fix.
	enum class FloorWalk
	{
		Continuous,
		/// A sub-step found nothing below it at all -- a hole, or a gap in the geometry.
		NoFloor,
		/// A sub-step's probe began inside solid, or hit a surface too steep to stand on.
		/// This is what a wall face gives, and also what a ramp steeper than MINFLOORZ gives.
		Blocked,
		/// A sub-step rose more than a pawn can step.
		StepTooHigh,
		/// A sub-step fell further than the drop allowance.
		DropTooFar,
	};

	/// The floor profile along a segment, and where it stopped being walkable.
	struct FloorProfile
	{
		FloorWalk result{ FloorWalk::Continuous };
		/// Sub-step the walk failed at, or -1.
		int failedAt{ -1 };
		/// Floor height at each sub-step, index 0 being the start point. Only filled as far
		/// as the walk got.
		std::vector<float> floorZ{};
		/// Horizontal distance covered by one sub-step.
		float runPerStep{ 0.0f };
	};

	/// Walks the floor from one point to another in sub-steps of `maxStepUp`.
	///
	/// This is the test that separates a ramp from a crate. The rise between two points 256u
	/// apart says nothing about whether the surface between them is continuous -- a ramp and a
	/// crate of the same height look identical from the endpoints alone -- so the space
	/// between has to be looked at. Sub-stepping by the step height makes one rule do both
	/// jobs: a ~45-degree slope limit at fine granularity, the step height at coarse.
	FloorProfile WalkFloor(AActor* tracer, const FVector& from, float fromFloorZ,
		const FVector& to, float toFloorZ, float maxStepUp, float maxDrop);

	// ! A fixed sampling origin, so two runs are the same run
	// ========================================
	//
	// Both cc.sweep and cc.pathnodes start their fill at the player, and the placement rules
	// are ORDER-DEPENDENT: a node placed early holds every later candidate off by its
	// exclusion radius, so a seed a few hundred units away walks the floor in a different
	// order and ends up with a different set of nodes and different connections. Mgamerz:
	// "it sometimes goes up the ramp, other times it doesn't".
	//
	// That is a real defect and not only a testing nuisance -- a generator whose output
	// depends on where the player happened to be standing cannot produce stable package data,
	// and two runs cannot be diffed. Pinning the origin does not fix it; it makes it hold
	// still, so a change in the output can be attributed to a change in the code.

	/// Pins the sampling origin. Both cc.sweep and cc.pathnodes use it instead of the player.
	void SetFloorSeed(const FVector& at);

	/// Goes back to seeding from wherever the player is standing.
	void ClearFloorSeed();

	/// Whether an origin is pinned.
	bool HasFloorSeed();

	/// The pinned origin, or `fallback` when there is none.
	FVector FloorSeedOr(const FVector& fallback);

	struct FloorSampleStats
	{
		int samples{ 0 };
		int sampleBudgetHit{ 0 };
		int neighboursNoFloor{ 0 };
		int neighboursBlocked{ 0 };
		/// Neighbours the floor does not continuously reach, by reason. Only counted when
		/// maxStepHeight is supplied.
		int walkNoFloor{ 0 };
		int walkBlocked{ 0 };
		int walkStepTooHigh{ 0 };
		int walkDropTooFar{ 0 };

		int WalkRejected() const
		{
			return walkNoFloor + walkBlocked + walkStepTooHigh + walkDropTooFar;
		}
	};

	/// Points where the floor's slope changes sharply -- the top and bottom of a ramp.
	///
	/// A pawn heading for a node partway down a ramp walks into the slope, so the break itself
	/// needs a node. The sampler cannot produce one on its own: its points sit on a fixed
	/// grid, and a ramp's break line lands wherever the geometry puts it.
	///
	/// @param gradientChange  How much the slope must change to count, as a rise-over-run
	///                        difference. 0.25 is about 14 degrees.
	std::vector<FloorSample> FindSlopeBreaks(AActor* tracer,
		const std::vector<FloorSample>& samples, float spacing, float rideHeight,
		float maxStepUp, float maxDrop, float gradientChange, int& outPairsWalked);

	/// Enumerates the places a player could stand within `radius` of `origin`.
	///
	/// The neighbour test is two traces and both matter. The floor has to be there, AND the
	/// straight line from here to there has to be clear -- without the second the fill leaks
	/// through walls, out of the play space, and samples the far side of the level.
	///
	/// @param tracer      Actor to trace with; the player's pawn in practice.
	/// @param rideHeight  How far above the floor a returned sample's `location` sits.
	/// @param maxRise     Largest step up. A surface higher than this reads as a wall.
	/// @param maxDrop     Largest step down.
	/// @param maxSamples  Budget. Hitting it is reported, not silently truncated.
	/// @param maxStepHeight  A pawn's MaxStepHeight. When > 0 the floor between a sample and
	///                   its neighbour is walked in sub-steps this long and each rise must be
	///                   within it, so a surface is only reached if it can actually be walked
	///                   to. 0 keeps the old single-jump test.
	///
	/// The sub-stepping is what separates a ramp from a crate, and without it `maxRise` alone
	/// cannot: one 256u jump with a 130u rise allowance climbs onto anything cover-height,
	/// which is where "nodes on areas you cannot go" came from. Sub-stepping by the step
	/// height makes the same rule a ~45-degree slope limit at fine granularity -- a ramp
	/// passes every sub-step, a crate fails the one that crosses its edge.
	std::vector<FloorSample> SampleFloor(AActor* tracer, const FVector& origin,
		float radius, float spacing, float rideHeight, float maxRise, float maxDrop,
		int maxSamples, FloorSampleStats& stats, float maxStepHeight = 0.0f);
}
