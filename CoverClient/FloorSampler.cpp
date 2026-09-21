#include "CoverClient/FloorSampler.hpp"
#include "CoverClient/CoverEdges.hpp"

#include <cmath>
#include <map>
#include <set>

namespace CoverClient
{
	SampleKey FloorSampleKey(const FVector& v, float spacing)
	{
		return SampleKey{
			static_cast<int>(std::round(v.X / spacing)),
			static_cast<int>(std::round(v.Y / spacing)),
			static_cast<int>(std::round(v.Z / k_floorStoreyHeight))
		};
	}

	namespace
	{
		/// Pinned by default, because every comparison being made right now is between two
		/// runs. The coordinate is the bottom of the ramp in biop_covertest -- where the scout
		/// spawned on the 2026-09-11 16:40 run, which is the run whose numbers are written up
		/// in note 04. Seeding there again reproduces that run.
		///
		/// `cc.seed here` overwrites it from wherever the player is standing and prints the
		/// literal to paste back in here; `cc.seed player` turns pinning off.
		bool    g_bHasFloorSeed = true;
		FVector g_FloorSeed{ -8938.6f, 1307.8f, 803.2f };

		/// Shortest sub-step worth taking. Below this the probe count climbs fast and the
		/// answer stops changing.
		constexpr float k_minSubStep = 16.0f;

		/// Headroom above the previous sub-step's floor for the next probe to start from.
		/// ProbeFloor reports Blocked when its box begins inside solid, which is what a crate
		/// face gives us and exactly the answer wanted.
		constexpr float k_subStepClearance = 4.0f;

	}

	void SetFloorSeed(const FVector& at)
	{
		g_FloorSeed = at;
		g_bHasFloorSeed = true;
	}

	void ClearFloorSeed()
	{
		g_bHasFloorSeed = false;
	}

	bool HasFloorSeed()
	{
		return g_bHasFloorSeed;
	}

	FVector FloorSeedOr(const FVector& fallback)
	{
		return g_bHasFloorSeed ? g_FloorSeed : fallback;
	}

	FloorProfile WalkFloor(AActor* tracer, const FVector& from, float fromFloorZ,
		const FVector& to, float toFloorZ, float maxStepUp, float maxDrop)
	{
		FloorProfile profile{};
		profile.floorZ.push_back(fromFloorZ);

		const float dx = to.X - from.X;
		const float dy = to.Y - from.Y;
		const float run = std::sqrt((dx * dx) + (dy * dy));
		const float subStep = (maxStepUp > k_minSubStep) ? maxStepUp : k_minSubStep;
		const int steps = static_cast<int>(std::ceil(run / subStep));
		profile.runPerStep = (steps > 0) ? (run / static_cast<float>(steps)) : run;

		if (steps <= 1)
		{
			profile.floorZ.push_back(toFloorZ);
			const float rise = toFloorZ - fromFloorZ;
			if (rise > maxStepUp)
			{
				profile.result = FloorWalk::StepTooHigh;
				profile.failedAt = 1;
			}
			else if (-rise > maxDrop)
			{
				profile.result = FloorWalk::DropTooFar;
				profile.failedAt = 1;
			}
			return profile;
		}

		float previousZ = fromFloorZ;
		for (int i = 1; i <= steps; i++)
		{
			const float t = static_cast<float>(i) / static_cast<float>(steps);
			float floorZ = toFloorZ;
			if (i < steps)
			{
				// The probe starts a step height above the floor we are standing on, so a
				// surface higher than that begins inside solid and comes back Blocked --
				// which is the answer wanted for a crate face.
				const FVector at{ from.X + (dx * t), from.Y + (dy * t), previousZ };
				const FloorProbe probe = ProbeFloor(tracer, at,
					maxStepUp + k_subStepClearance, maxDrop, floorZ);
				if (probe != FloorProbe::Found)
				{
					profile.result = (probe == FloorProbe::None)
						? FloorWalk::NoFloor : FloorWalk::Blocked;
					profile.failedAt = i;
					return profile;
				}
			}

			profile.floorZ.push_back(floorZ);
			const float rise = floorZ - previousZ;
			if (rise > maxStepUp)
			{
				profile.result = FloorWalk::StepTooHigh;
				profile.failedAt = i;
				return profile;
			}
			if (-rise > maxDrop)
			{
				profile.result = FloorWalk::DropTooFar;
				profile.failedAt = i;
				return profile;
			}
			previousZ = floorZ;
		}
		return profile;
	}

	std::vector<FloorSample> SampleFloor(AActor* tracer, const FVector& origin,
		float radius, float spacing, float rideHeight, float maxRise, float maxDrop,
		int maxSamples, FloorSampleStats& stats, float maxStepHeight)
	{
		std::vector<FloorSample> samples{};

		float originFloorZ = 0.0f;
		if (ProbeFloor(tracer, origin, maxRise, maxDrop, originFloorZ) != FloorProbe::Found)
		{
			return samples;
		}

		std::set<SampleKey> visited{};

		FloorSample start{};
		start.floorZ = originFloorZ;
		start.location = { origin.X, origin.Y, originFloorZ + rideHeight };
		visited.insert(FloorSampleKey(start.location, spacing));
		samples.push_back(start);

		// Breadth-first, so the budget is spent on the ground nearest the player rather
		// than on one long tendril off into a corridor.
		for (size_t head = 0; head < samples.size(); head++)
		{
			if (static_cast<int>(samples.size()) >= maxSamples)
			{
				stats.sampleBudgetHit = 1;
				break;
			}

			const FloorSample current = samples[head];

			for (int dy = -1; dy <= 1; dy++)
			{
				for (int dx = -1; dx <= 1; dx++)
				{
					if (dx == 0 && dy == 0) { continue; }
					if (static_cast<int>(samples.size()) >= maxSamples) { break; }

					const FVector flat{
						current.location.X + static_cast<float>(dx) * spacing,
						current.location.Y + static_cast<float>(dy) * spacing,
						current.location.Z
					};

					const float rx = flat.X - origin.X;
					const float ry = flat.Y - origin.Y;
					if (rx * rx + ry * ry > radius * radius) { continue; }

					// Probe from maxRise above the floor we are standing on, not from the
					// walk line -- see ProbeFloor. A surface higher than that comes back
					// Blocked, which is correct: it is a wall, not a step.
					float nextFloorZ = 0.0f;
					const FloorProbe probe = ProbeFloor(tracer, flat,
						(current.floorZ + maxRise) - flat.Z, maxDrop + rideHeight,
						nextFloorZ);
					if (probe != FloorProbe::Found)
					{
						stats.neighboursNoFloor++;
						continue;
					}

					const FVector next{ flat.X, flat.Y, nextFloorZ + rideHeight };
					const SampleKey key = FloorSampleKey(next, spacing);
					if (visited.find(key) != visited.end()) { continue; }

					// Reachable, not merely present. Checked before the line trace because it
					// is the cheaper way to reject a surface you would have to climb.
					if (maxStepHeight > 0.0f)
					{
						const FloorProfile walk = WalkFloor(tracer, current.location,
							current.floorZ, next, nextFloorZ, maxStepHeight, maxDrop);
						if (walk.result != FloorWalk::Continuous)
						{
							switch (walk.result)
							{
							case FloorWalk::NoFloor:     stats.walkNoFloor++;     break;
							case FloorWalk::Blocked:     stats.walkBlocked++;     break;
							case FloorWalk::StepTooHigh: stats.walkStepTooHigh++; break;
							default:                     stats.walkDropTooFar++;  break;
							}
							continue;
						}
					}

					FVector hitLocation{}, hitNormal{};
					auto* const blocker = tracer->Trace(next, current.location,
						FALSE /*bTraceActors*/, FVector{ 1.0f, 1.0f, 1.0f },
						0, &hitLocation, &hitNormal, nullptr);
					if (blocker != nullptr)
					{
						stats.neighboursBlocked++;
						continue;
					}

					visited.insert(key);
					FloorSample sample{};
					sample.floorZ = nextFloorZ;
					sample.location = next;
					samples.push_back(sample);
				}
			}
		}

		stats.samples = static_cast<int>(samples.size());
		return samples;
	}

	// ! Slope breaks -- the top and bottom of a ramp
	// ========================================
	//
	// Mgamerz: "A node for ramps needs one at the top otherwise the pawn may push into the
	// floor if it isn't at the top of the ramp." A pawn heading for a node partway down a slope
	// walks into the slope rather than onto it, so the break line itself has to carry a node.
	//
	// The sampler cannot produce one. Its points sit on a fixed grid stepped from the player,
	// and a ramp's break lands wherever the geometry puts it -- the odds of a grid point
	// falling on it are the odds of the ramp being an exact multiple of the spacing away.
	//
	// So the breaks are found rather than hoped for: the floor between neighbouring samples is
	// already walked in sub-steps for the reachability test, and that walk is a height profile.
	// Where its gradient changes sharply is the break.

	std::vector<FloorSample> FindSlopeBreaks(AActor* tracer,
		const std::vector<FloorSample>& samples, float spacing, float rideHeight,
		float maxStepUp, float maxDrop, float gradientChange, int& outPairsWalked)
	{
		std::vector<FloorSample> breaks{};
		outPairsWalked = 0;
		if (maxStepUp <= 0.0f) { return breaks; }

		std::map<SampleKey, size_t> byKey{};
		for (size_t i = 0; i < samples.size(); i++)
		{
			byKey[FloorSampleKey(samples[i].location, spacing)] = i;
		}

		// Breaks are deduped much finer than the samples themselves. Two adjacent pairs can
		// find the same break from either side, and the whole point is a node ON the line.
		std::set<SampleKey> placed{};
		const float dedupe = maxStepUp;

		for (size_t i = 0; i < samples.size(); i++)
		{
			const FloorSample& from = samples[i];
			const SampleKey home = FloorSampleKey(from.location, spacing);

			for (int dy = -1; dy <= 1; dy++)
			{
				for (int dx = -1; dx <= 1; dx++)
				{
					if (dx == 0 && dy == 0) { continue; }
					// Each pair once. Diagonals are included: a ramp running diagonally to the
					// grid has its break line found by no other pair.
					if (dy < 0 || (dy == 0 && dx < 0)) { continue; }

					const auto found = byKey.find(
						SampleKey{ home.x + dx, home.y + dy, home.z });
					if (found == byKey.end()) { continue; }

					const FloorSample& to = samples[found->second];
					const FloorProfile walk = WalkFloor(tracer, from.location, from.floorZ,
						to.location, to.floorZ, maxStepUp, maxDrop);
					outPairsWalked++;
					if (walk.result != FloorWalk::Continuous) { continue; }
					if (walk.floorZ.size() < 3 || walk.runPerStep <= 0.0f) { continue; }

					const float ddx = to.location.X - from.location.X;
					const float ddy = to.location.Y - from.location.Y;
					const int steps = static_cast<int>(walk.floorZ.size()) - 1;

					// The sharpest gradient change along the segment, if it is sharp enough.
					int bestAt = -1;
					float bestChange = gradientChange;
					for (int k = 1; k < steps; k++)
					{
						const float before =
							(walk.floorZ[k] - walk.floorZ[k - 1]) / walk.runPerStep;
						const float after =
							(walk.floorZ[k + 1] - walk.floorZ[k]) / walk.runPerStep;
						const float change = std::abs(after - before);
						if (change > bestChange)
						{
							bestChange = change;
							bestAt = k;
						}
					}
					if (bestAt < 0) { continue; }

					const float t = static_cast<float>(bestAt) / static_cast<float>(steps);
					FloorSample brk{};
					brk.floorZ = walk.floorZ[bestAt];
					brk.location = {
						from.location.X + (ddx * t),
						from.location.Y + (ddy * t),
						walk.floorZ[bestAt] + rideHeight
					};

					const SampleKey key = FloorSampleKey(brk.location, dedupe);
					if (placed.find(key) != placed.end()) { continue; }
					placed.insert(key);
					breaks.push_back(brk);
				}
			}
		}
		return breaks;
	}
}
