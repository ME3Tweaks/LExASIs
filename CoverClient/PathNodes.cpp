#include "CoverClient/PathNodes.hpp"
#include "CoverClient/FloorSampler.hpp"
#include "CoverClient/NavRuntime.hpp"
#include "CoverClient/Helpers.hpp"
#include "CoverClient/CoverEdges.hpp"
#include "CoverClient/ScoutHarness.hpp"
#include "Common/Base.hpp"
#include "Common/Objects.hpp"

#include <cmath>
#include <functional>
#include <map>
#include <algorithm>
#include <set>
#include <string>
#include <vector>

namespace CoverClient
{
	namespace
	{
		/// Path nodes we spawned, so cc.despawnpathnodes can take them away again.
		std::vector<APathNode*> g_SpawnedPathNodes;

		/// Fast "is this one of ours", for the spec predicates. A vector scan would be O(n)
		/// per spec across a network of thousands.
		std::set<AActor*> g_SpawnedPathNodeSet;

		constexpr float k_defaultRadius = 2048.0f;
		constexpr float k_minRadius = 256.0f;
		constexpr int   k_minNodeBudget = 200;
		constexpr int   k_nodeBudgetCeiling = 20000;

		/// Past this the pass is long enough that it needs to say so -- the whole thing runs
		/// inside one Exec call, so the game is frozen until it finishes.
		constexpr float k_longRadius = 8192.0f;

		/// How far apart two nodes may be and still be joined. A diagonal neighbour on the
		/// sample grid is spacing*1.41 away, so anything under that would silently drop every
		/// diagonal and leave a network that can only move on the compass points.
		constexpr float k_linkReachFactor = 1.5f;

		// ! Cover is human-sized, and only human-sized
		// ========================================
		//
		// Mgamerz: "I don't think anything can use cover that is larger than the default pawn
		// size - brutes are huge and won't be able to use cover - all non-human sized enemies
		// can't use cover."
		//
		// So the largest-pawn-that-fits sizing is right for ROUTING and wrong for anything
		// touching cover. A node-to-marker spec sized at Vehicle 140 x 195 tells a Brute it
		// may path onto a cover slot it can never occupy -- a route to a destination that
		// rejects it on arrival, which is worse than no route at all.
		//
		// 40 x 95 is not a guess: it is what every shipped SlotToSlotReachSpec on
		// BioD_CerMir_125Land measures, and it is what the within-link chain and the slip
		// specs already use. The cover network is one size throughout, so its approaches are
		// that size too.
		constexpr float k_coverSpecRadius = 40.0f;
		constexpr float k_coverSpecHeight = 95.0f;

		// ! How many path nodes one cover slot gets an approach to
		// ========================================
		//
		// Every node within reach used to get one, which in a node field this dense is about
		// three per marker and 2920 specs for 452 markers. Mgamerz, looking at it under
		// 'show paths': "the amount of reachspecs seems pretty high compared to vanilla."
		//
		// Measured rather than guessed. A 1000-spec sample of the plain ReachSpecs in
		// BioD_Kro001_605FourthPaths (note 06) breaks down by endpoint class, and the relevant
		// slice is CoverSlotMarker -> PathNode: 178 of 1000, scaling to ~448 specs over 540
		// markers -- **0.83 outgoing marker-to-node approaches per marker**. Ours was 3.2.
		//
		// One nearest node per marker gives 1.0, which is the closest a whole number gets, and
		// it puts the level's total at ~4.3 specs per nav point against a shipped 4.5 to 5.0.
		//
		// A slot is not cut off by this: it also carries its SlotToSlot chain to its
		// neighbours and its slip, which is how shipped cover is mostly reached. If cover ever
		// proves hard for the AI to path to, this is the constant to raise -- 2 would put
		// approaches at roughly 2.4x shipped, so raise it knowing that.
		constexpr int k_maxApproachNodesPerMarker = 1;

		// ! Trace at the pawn size, label at the shipped size
		// ========================================
		//
		// These are two different numbers and conflating them failed 250 of 448 approaches.
		//
		// The LABEL is 40 x 95, because that is what every shipped spec touching cover
		// carries and `UReachSpec::supports` reads it to decide who may use the spec.
		//
		// The TRACE cannot use 40. `AlignDist` -- how far a cover slot stands off its own wall
		// -- is **36**, so a 40-radius box centred on a marker reaches 4u INTO the wall behind
		// it. Every approach then fails at the marker end, for a reason that has nothing to do
		// with the approach. Scout `PathSizes(0)` is 34 x 90, which clears a 36u standoff by
		// 2u, and it is the size the engine's own cover analysis ran at -- so it is the pawn
		// that actually has to fit.
		//
		// Shipped data does exactly this: specs labelled 40 x 95 against slots standing 36u
		// off their walls. The label was never a claim about clearance.
		bool bUseScoutRadiusForCoverTrace = true;

		// ! Cover links are the routing highway
		// ========================================
		//
		// An ACoverLink is an ANavigationPoint sitting 128u off its own wall, out in the open,
		// and shipped levels route THROUGH it. Read straight out of BioD_CerMir_125Land -- all
		// 39 links, 176 outgoing specs, 4.51 per link:
		//
		//     link -> CoverSlotMarker   89   66 - 627u    mostly 40x95
		//     link -> CoverLink         50  105 - 1028u   105x145, 140x195, some 40x95
		//     link -> PathNode          35  251 - 1120u   mostly 140x195
		//
		// Two things fall out of that table. **The ranges are far longer than the node grid's**
		// (up to 1120u against our 384u reach), and **the sizes are the big ones** -- 140x195
		// is the Vehicle entry. Links sit in the open, so the route between two of them is open
		// floor and the widest pawn fits. This is the coarse network a large enemy actually
		// travels on, and we were generating none of it: links were spawned, spliced onto the
		// nav chain, and joined to nothing.
		//
		// Counts are capped to the shipped per-link figures rather than to whatever is in
		// range, the same way cover approaches are. A whole marker's worth of link specs is
		// not wanted; one good long-range hop is.
		constexpr float k_linkRouteReach = 1152.0f;
		constexpr int   k_maxLinkToLinkRoutes = 1;
		constexpr int   k_maxLinkToNodeRoutes = 1;

		/// How much the floor's slope has to change to count as a ramp break, as a
		/// rise-over-run difference. 0.25 is about 14 degrees -- steep enough that a pawn
		/// heading past it would push into the surface, shallow enough to catch a gentle ramp
		/// meeting a flat.
		constexpr float k_slopeBreakGradient = 0.25f;

		/// A node is not placed within this fraction of the spacing of an existing ROUTING nav
		/// point -- another path node, a spawn point, anything that stands on the floor.
		constexpr float k_exclusionFactor = 0.75f;

		/// How close a node may get to a COVER SLOT MARKER. Deliberately tiny, and the reason
		/// is the first run's numbers: excluding cover at half a spacing threw away 92 of 127
		/// samples and left 35 nodes, 8 of them connected to nothing at all.
		///
		/// The 132-cover-to-21-path-node ratio in shipped levels is not a licence to delete
		/// floor coverage near cover. A cover marker hugs a wall and faces into it; it is a
		/// firing position, not a route. Punching holes in the floor network around every one
		/// of them removes nodes exactly where the AI has to walk -- along the walls, and into
		/// the cover itself. All this needs to prevent is a node standing inside a marker.
		constexpr float k_coverExclusion = 48.0f;

		ABioWorldInfo* GetWorldInfo()
		{
			auto* const engine = ::Common::FindFirstObject<USFXEngine, true>();
			if (engine == nullptr) { return nullptr; }
			return static_cast<ABioWorldInfo*>(engine->GetCurrentWorldInfo());
		}

		/// Guard against a corrupt or cyclic nextNavigationPoint chain, which would otherwise
		/// hang the game inside a single console command.
		constexpr int k_maxChainWalk = 200000;

		/// The object's own name -- "PathNode_37" -- rather than its full path. The full path
		/// is the same 40-character prefix for every node in the level, and these are printed
		/// one per line in a list where only the tail differs.
		std::string NameOf(UObject* object)
		{
			if (object == nullptr) { return std::string{ "<null>" }; }
			const wchar_t* const data = *object->GetName();
			return (data != nullptr) ? ws2s(data) : std::string{};
		}

		/// The floor under a nav point, probed. Falls back to the placement formula only when
		/// there is nothing under it to measure.
		///
		/// Never derive this from the actor's Z: a spawned PathNode sits ~26u higher above its
		/// floor than the cylinder half-height it was placed with, and a walk started from the
		/// derived value ends on a step that exists nowhere in the level.
		float FloorUnder(AActor* tracer, ANavigationPoint* point, float halfHeight,
			float maxRise, float maxDrop)
		{
			if (point == nullptr) { return 0.0f; }
			float probed = point->LOCATION.Z - halfHeight;
			ProbeFloor(tracer, point->LOCATION, maxRise, maxDrop, probed);
			return probed;
		}

		/// Distance along the floor, ignoring height.
		float HorizontalDistance(const FVector& a, const FVector& b)
		{
			const float dx = b.X - a.X;
			const float dy = b.Y - a.Y;
			return std::sqrt((dx * dx) + (dy * dy));
		}

		float Distance(const FVector& a, const FVector& b)
		{
			const float dx = b.X - a.X;
			const float dy = b.Y - a.Y;
			const float dz = b.Z - a.Z;
			return std::sqrt((dx * dx) + (dy * dy) + (dz * dz));
		}

		/// How many cells either side have to be searched to be sure of finding everything
		/// within `range`.
		///
		/// One cell is NOT enough and getting this wrong is silent: a pair 1.5 spacings apart
		/// can straddle two cell boundaries, so a +/-1 search drops connections that exist and
		/// leaves holes in the network with nothing in the log to say so. The +1 is the
		/// rounding slack -- neither point sits at the centre of its own cell.
		int CellSpan(float range, float cell)
		{
			if (cell <= 0.0f) { return 1; }
			return static_cast<int>(std::ceil(range / cell)) + 1;
		}

		/// Positions bucketed by quantised cell, so "is anything near here" is a handful of
		/// map lookups rather than a scan of the whole level.
		struct PositionGrid
		{
			float cell{ 1.0f };
			std::map<SampleKey, std::vector<FVector>> buckets{};

			void Add(const FVector& at)
			{
				buckets[FloorSampleKey(at, cell)].push_back(at);
			}

			/// Whether anything sits within `range` of `at`. Cells are searched, then the
			/// exact distance is measured -- the grid narrows the search, it does not decide
			/// the answer.
			bool AnyWithin(const FVector& at, float range) const
			{
				const SampleKey home = FloorSampleKey(at, cell);
				const int span = CellSpan(range, cell);
				// SampleKey quantises Z by storey, not by cell, so its span is its own.
				const int zSpan = CellSpan(range, k_floorStoreyHeight);
				for (int dz = -zSpan; dz <= zSpan; dz++)
				{
					for (int dy = -span; dy <= span; dy++)
					{
						for (int dx = -span; dx <= span; dx++)
						{
							const SampleKey key{ home.x + dx, home.y + dy, home.z + dz };
							const auto found = buckets.find(key);
							if (found == buckets.end()) { continue; }
							for (const FVector& other : found->second)
							{
								if (Distance(at, other) <= range) { return true; }
							}
						}
					}
				}
				return false;
			}
		};

		/// Whether `point` already has a spec leading to `target`.
		///
		/// cc.pathnodes is going to be run more than once -- after a wider sweep, after new
		/// cover -- and without this every re-run would stack a second copy of every spec it
		/// made last time.
		bool HasSpecTo(ANavigationPoint* point, AActor* target)
		{
			if (point == nullptr) { return false; }
			for (unsigned int i = 0; i < point->PathList.Count(); i++)
			{
				auto* const spec = point->PathList(i);
				if (spec != nullptr && spec->End.Actor == target) { return true; }
			}
			return false;
		}

		/// The straight line between two nav points at a given pawn width.
		///
		/// A zero extent answers "is there a wall in the way"; a pawn-sized box answers "can
		/// something this big actually get through", which is the question that decides who
		/// may use the spec.
		bool RouteClear(AActor* tracer, const FVector& from, const FVector& to,
			const FVector& extent)
		{
			if (tracer == nullptr) { return false; }
			FVector hitLocation{}, hitNormal{};
			return tracer->Trace(to, from, FALSE /*bTraceActors*/,
				extent, 0, &hitLocation, &hitNormal, nullptr) == nullptr;
		}

		// ! Sizing a spec to the largest pawn that can use it
		// ========================================
		//
		// UReachSpec::supports refuses a pawn wider or taller than the spec's
		// CollisionRadius / CollisionHeight, so a network built entirely at PathSizes(0)
		// (Common, 34 x 90) is a network only the smallest mooks may walk. Everything bigger --
		// LE3's scout table has Large 40x95, VeryLarge 105x145, Max 135x146, Vehicle 140x195 --
		// simply has no route, which is Mgamerz's "the enemies would never be able to traverse
		// the room".
		//
		// UE3's FPathBuilder::DefinePaths does not pick one size either: it walks Scout->
		// PathSizes from the LARGEST down and records the largest that got through. That is the
		// "passes from largest to smallest" this needs, and it belongs on the spec rather than
		// on the node -- one node network, each connection labelled with who may use it.
		//
		// The one deliberate difference from UE3: it moved the scout along the route and we
		// sweep a box. Walking the scout is far more faithful and far more expensive, and this
		// whole pass runs inside a single Exec call.

		struct SpecSize
		{
			std::string desc{};
			float radius{ 0.0f };
			float height{ 0.0f };
		};

		/// The scout's PathSizes, largest first. Empty if there is no scout.
		std::vector<SpecSize> ReadPathSizes(AScout* scout)
		{
			std::vector<SpecSize> sizes;
			if (scout == nullptr) { return sizes; }
			for (unsigned int i = 0; i < scout->PathSizes.Count(); i++)
			{
				const FPathSizeInfo& info = scout->PathSizes(i);
				if (info.Radius <= 0.0f || info.Height <= 0.0f) { continue; }
				const char* const desc = info.Desc.GetName();
				sizes.push_back(SpecSize{
					(desc != nullptr) ? std::string{ desc } : std::string{ "?" },
					info.Radius, info.Height });
			}
			std::sort(sizes.begin(), sizes.end(),
				[](const SpecSize& a, const SpecSize& b)
				{
					if (a.radius != b.radius) { return a.radius > b.radius; }
					return a.height > b.height;
				});
			return sizes;
		}

		// ! A pawn box rests on the floor; it does not float at the actor's own height
		// ========================================
		//
		// The box is centred on the traced line, so tracing between two ACTOR locations puts
		// it wherever those actors happen to sit -- and nav actors do not sit at a pawn's
		// centre. A CoverSlotMarker is about 59u above its floor; a 95u half-height box
		// centred there reaches 36u UNDER the floor and can never come back clear. That is
		// 1291 of 1460 cover approaches "too tight" on the first run that had cover in it:
		// not tight geometry, a box buried in the ground.
		//
		// The endpoints are therefore lifted to floor + halfHeight at each end, which is where
		// the cylinder's centre would be if a pawn of that size were standing there. Same
		// mistake as the derived floor baseline, in a different place: a height was taken from
		// an actor when it had to come from the geometry.
		bool PawnBoxFits(AActor* tracer, const FVector& from, float fromFloorZ,
			const FVector& to, float toFloorZ, float radius, float halfHeight)
		{
			const FVector a{ from.X, from.Y, fromFloorZ + halfHeight };
			const FVector b{ to.X,   to.Y,   toFloorZ   + halfHeight };
			return RouteClear(tracer, a, b, FVector{ radius, radius, halfHeight });
		}

		/// Index of the largest size that fits through, or -1 if not even the smallest does.
		int LargestSizeThatFits(AActor* tracer, const FVector& from, float fromFloorZ,
			const FVector& to, float toFloorZ, const std::vector<SpecSize>& sizes)
		{
			for (size_t i = 0; i < sizes.size(); i++)
			{
				if (PawnBoxFits(tracer, from, fromFloorZ, to, toFloorZ,
					sizes[i].radius, sizes[i].height))
				{
					return static_cast<int>(i);
				}
			}
			return -1;
		}

		/// Every enabled cover slot marker on the links this session spawned.
		///
		/// Deliberately ours only. The dedupe reads the level's own nav points because reading
		/// is free, but appending a spec to a SHIPPED marker's PathList would mutate cover
		/// BioWare built, in a session the user is going to keep playing.
		std::vector<ACoverSlotMarker*> OurCoverMarkers()
		{
			std::vector<ACoverSlotMarker*> markers;
			for (auto* const link : GetSpawnedLinks())
			{
				if (link == nullptr || ::Common::IsDestroyed(link)) { continue; }
				const int slots = static_cast<int>(link->Slots.Count());
				for (int i = 0; i < slots; i++)
				{
					if (link->Slots(i).bEnabled == 0) { continue; }
					auto* const marker = link->Slots(i).SlotMarker;
					if (marker == nullptr || ::Common::IsDestroyed(marker)) { continue; }
					markers.push_back(marker);
				}
			}
			return markers;
		}
	}

	const std::vector<APathNode*>& GetSpawnedPathNodes()
	{
		return g_SpawnedPathNodes;
	}

	int DropPathNodeSpecsToCover()
	{
		int dropped = 0;
		for (auto* const node : g_SpawnedPathNodes)
		{
			if (node == nullptr || ::Common::IsDestroyed(node)) { continue; }
			dropped += DropSpecsWhere(node, [](AActor* target)
				{
					return g_SpawnedPathNodeSet.find(target) == g_SpawnedPathNodeSet.end();
				});
		}
		return dropped;
	}

	void CmdBuildPathNodes(float radius, float spacing, int maxNodes)
	{
		LEASI_INFO("=== cc.pathnodes ===");

		auto* const wi = GetWorldInfo();
		if (wi == nullptr)
		{
			LEASI_ERROR("No WorldInfo.");
			return;
		}
		auto* const pc = wi->GetALocalPlayerController();
		if (pc == nullptr || pc->Pawn == nullptr)
		{
			LEASI_ERROR("Need a player controller with a pawn.");
			return;
		}

		auto* const linkClass = ACoverLink::StaticClass();
		auto* const linkDefaults = (linkClass != nullptr)
			? static_cast<ACoverLink*>(linkClass->ClassDefaultObject) : nullptr;
		if (linkDefaults == nullptr)
		{
			LEASI_ERROR("Could not reach the ACoverLink CDO for its spacing constants.");
			return;
		}

		auto* const nodeClass = APathNode::StaticClass();
		auto* const nodeDefaults = (nodeClass != nullptr)
			? static_cast<APathNode*>(nodeClass->ClassDefaultObject) : nullptr;
		if (nodeDefaults == nullptr)
		{
			LEASI_ERROR("Could not find class Engine.PathNode.");
			return;
		}

		if (radius <= 0.0f) { radius = k_defaultRadius; }
		if (radius < k_minRadius) { radius = k_minRadius; }
		// AutoCoverSlotInterval is what cc.sweep samples at, and matching it is what makes the
		// exclusion test meaningful: a node interval finer than the cover interval would place
		// gap-fillers between slots of one cover run.
		if (spacing <= 0.0f) { spacing = linkDefaults->AutoCoverSlotInterval; }
		if (spacing < 32.0f) { spacing = 32.0f; }

		const float standHeight = linkDefaults->StandHeight;
		const float maxRise = standHeight;
		const float maxDrop = standHeight * 2.0f;

		// A nav point's cylinder rests ON the floor, so its Location sits a half-height above
		// it. Read rather than assumed -- placing the node at the sampler's own walk height
		// would bury or float every cylinder by whatever the difference happened to be.
		float nodeHalfHeight = 40.0f;
		float nodeRadius = 40.0f;
		if (nodeDefaults->CylinderComponent != nullptr)
		{
			nodeHalfHeight = nodeDefaults->CylinderComponent->CollisionHeight;
			nodeRadius = nodeDefaults->CylinderComponent->CollisionRadius;
		}
		else
		{
			LEASI_WARN("The PathNode CDO has no CylinderComponent, so node heights are a "
				"guess -- and AddToNavigationOctree early-outs without one, which would "
				"leave every node invisible to radius queries.");
		}

		auto* const tracer = pc->Pawn;
		const FVector playerLoc = pc->Pawn->location;

		// Acquired before sampling, not after: the scout's MaxStepHeight is what bounds the
		// fill, and its PathSizes table sizes the specs later on.
		ScopedScout guard{};

		// How high a pawn can actually STEP. maxRise alone was StandHeight (130), which let
		// one 256u sample jump climb anything cover-height -- crate tops, window sills, the
		// top of a low wall -- and put nodes on geometry nothing can walk onto. The scout is
		// the pawn UE3's own path building measured with; the player is the fallback.
		float maxStepHeight = 0.0f;
		const char* stepSource = "none";
		if (guard && guard.Get() != nullptr && guard.Get()->MaxStepHeight > 0.0f)
		{
			maxStepHeight = guard.Get()->MaxStepHeight;
			stepSource = "the scout";
		}
		else if (pc->Pawn->MaxStepHeight > 0.0f)
		{
			maxStepHeight = pc->Pawn->MaxStepHeight;
			stepSource = "the player pawn";
		}
		else
		{
			LEASI_WARN("Neither the scout nor the player pawn reports a MaxStepHeight, so the "
				"fill falls back to a single {:.0f}u rise per step and will climb onto "
				"anything that tall.", standHeight);
		}

		if (maxNodes <= 0)
		{
			const float cellsPerSide = radius / spacing;
			const float cells = 3.14159265f * cellsPerSide * cellsPerSide;
			maxNodes = static_cast<int>(cells) + 1;
			if (maxNodes < k_minNodeBudget) { maxNodes = k_minNodeBudget; }
			if (maxNodes > k_nodeBudgetCeiling) { maxNodes = k_nodeBudgetCeiling; }
		}

		LEASI_INFO("Radius {:.0f}u, spacing {:.0f}u, budget {} sample(s) -- fewer nodes than "
			"that, since the ones the nav network already reaches are dropped. Cylinder "
			"{:.0f} x {:.0f} from the PathNode CDO.",
			radius, spacing, maxNodes, nodeRadius, nodeHalfHeight);
		if (maxStepHeight > 0.0f)
		{
			LEASI_INFO("Step height {:.0f}u from {}: the floor between two samples is walked "
				"in sub-steps that long and every rise has to be within it, so a surface is "
				"only reached if it can be walked to.", maxStepHeight, stepSource);
		}
		if (radius > k_longRadius)
		{
			LEASI_WARN("This is a big pass and all of it runs inside one Exec call, so the "
				"game will be frozen until it finishes. It is not hung.");
		}
		if (GetSpawnedLinks().empty())
		{
			LEASI_WARN("No cover links spawned this session. Path nodes are meant to fill the "
				"gaps BETWEEN cover, and with no cover to defer to this will carpet the floor "
				"instead. Run cc.sweep first.");
		}

		// ! Pass 1 -- where can a person stand
		FloorSampleStats floorStats{};
		// Pinned by default while runs are being compared -- see FloorSampler.hpp. The player
		// can still walk anywhere; only the fill's ORIGIN is held still.
		const FVector seed = FloorSeedOr(playerLoc);
		if (HasFloorSeed())
		{
			LEASI_INFO("Seeding from the PINNED origin ({:.0f}, {:.0f}, {:.0f}), not from you "
				"({:.0f}, {:.0f}, {:.0f}). 'cc.seed player' uses your position instead.",
				seed.X, seed.Y, seed.Z, playerLoc.X, playerLoc.Y, playerLoc.Z);
		}

		const auto samples = SampleFloor(tracer, seed, radius, spacing, nodeHalfHeight,
			maxRise, maxDrop, maxNodes, floorStats, maxStepHeight);
		if (samples.empty())
		{
			LEASI_ERROR("No floor under the player, so there is nothing to sample from.");
			return;
		}
		LEASI_INFO("Floor sampling: {} standable point(s){}. Rejected {} neighbour(s) with no "
			"floor and {} with no clear line to them.",
			floorStats.samples,
			floorStats.sampleBudgetHit != 0 ? " (NODE BUDGET HIT -- pass is truncated)" : "",
			floorStats.neighboursNoFloor, floorStats.neighboursBlocked);
		if (floorStats.WalkRejected() > 0)
		{
			// Broken out by reason, because they are four different pieces of geometry with
			// four different fixes and the single lumped count could not tell a crate from a
			// ramp the probe would not follow.
			LEASI_INFO("  {} more the floor does not continuously reach: {} step up too high "
				"(a crate, a sill, a low wall), {} blocked (a wall face, or a slope past "
				"MINFLOORZ that nothing may stand on), {} with a gap in the floor, {} with a "
				"drop too far.",
				floorStats.WalkRejected(), floorStats.walkStepTooHigh, floorStats.walkBlocked,
				floorStats.walkNoFloor, floorStats.walkDropTooFar);
		}

		// ! Pass 1b -- put a node on every ramp's break line
		//
		// The grid cannot land on one by chance, and a pawn heading for a node partway down a
		// slope walks into the slope instead of onto it.
		std::vector<FloorSample> candidates{};
		int breakPairsWalked = 0;
		size_t breakCount = 0;
		if (maxStepHeight > 0.0f)
		{
			candidates = FindSlopeBreaks(tracer, samples, spacing, nodeHalfHeight,
				maxStepHeight, maxDrop, k_slopeBreakGradient, breakPairsWalked);
			breakCount = candidates.size();
			LEASI_INFO("Slope breaks: {} found over {} neighbouring pair(s) -- ramp tops and "
				"bottoms, where the grid cannot put a point of its own.",
				static_cast<int>(breakCount), breakPairsWalked);
		}
		// Breaks are placed BEFORE the grid points and hold the grid off, rather than the
		// other way round. A break is a specific spot the geometry chose; a grid point is one
		// of many interchangeable ones, and letting the grid claim the ground first would let
		// a node 192u away veto the node that actually has to exist.
		candidates.insert(candidates.end(), samples.begin(), samples.end());

		// ! Pass 2 -- refuse the places the nav network already reaches
		//
		// This is what keeps cover the primary network and path nodes the gap filler, which is
		// the shape shipped levels actually have. It reads the level's own nav points as well
		// as ours: a level with shipped pathing should get nodes only where it is missing
		// some.
		const float exclusion = spacing * k_exclusionFactor;
		PositionGrid existingRoutes{};
		existingRoutes.cell = exclusion;
		PositionGrid existingCover{};
		existingCover.cell = k_coverExclusion;
		int navCounted = 0, markerCount = 0, linkCount = 0, routeCount = 0;
		for (auto* nav = wi->NavigationPointList; nav != nullptr;
			nav = nav->nextNavigationPoint)
		{
			// ACoverLink is an ANavigationPoint too, and testing only for ACoverSlotMarker
			// dropped every link into the ROUTING bucket -- where each one punched a full
			// 192u hole in the floor network.
			//
			// A link is worse there than a marker would be: EditorAutoSetup places it 128u
			// off its own wall, out in the open floor, which is exactly where the routes are.
			// A sweep of 112 links cost about 118 path nodes, and a room ringed with cover
			// came back with almost nothing in it.
			//
			// None of these three is a place anything walks TO. They are cover infrastructure,
			// and the rule already written for markers is the rule for all of them: cover is a
			// firing position, not a route, so it does not get to erase floor coverage.
			const bool bIsMarker = nav->Cast<ACoverSlotMarker>() != nullptr;
			const bool bIsLink = nav->Cast<ACoverLink>() != nullptr;
			const bool bIsMantle = nav->Cast<AMantleMarker>() != nullptr;
			if (bIsMarker || bIsLink || bIsMantle)
			{
				existingCover.Add(nav->LOCATION);
				if (bIsLink) { linkCount++; } else { markerCount++; }
			}
			else
			{
				existingRoutes.Add(nav->LOCATION);
				routeCount++;
			}
			if (++navCounted >= k_maxChainWalk) { break; }
		}
		// Broken down by kind, because the whole defect above was one class sitting in the
		// wrong bucket and a single total could never have shown it.
		LEASI_INFO("Nav network holds {} point(s): {} cover marker(s), {} cover link(s), {} "
			"routing point(s). Nodes keep {:.0f}u from a routing point and only {:.0f}u from "
			"anything belonging to cover -- cover is a firing position, not a route, so it "
			"does not get to erase floor coverage.",
			navCounted, markerCount, linkCount, routeCount, exclusion, k_coverExclusion);

		// ! Pass 3 -- spawn
		//
		// ANavigationPoint defaults bStatic and bNoDelete to true and SpawnActor refuses a
		// class with either, so the CDO has to be opened up across every Spawn call.
		const size_t firstNew = g_SpawnedPathNodes.size();
		int spawned = 0, failed = 0, rejectedRoutes = 0, rejectedCover = 0, noCylinder = 0;
		int breaksPlaced = 0;
		{
			ScopedSpawnableNav spawnable{ nodeClass };
			for (size_t c = 0; c < candidates.size(); c++)
			{
				if (spawned >= maxNodes) { break; }
				const FloorSample& sample = candidates[c];
				const bool bIsBreak = c < breakCount;

				const FVector at{ sample.location.X, sample.location.Y,
					sample.floorZ + nodeHalfHeight };
				// A break answers to a far smaller exclusion than a grid point: it is where a
				// node HAS to be, not merely where one would be convenient.
				const float against = bIsBreak ? k_coverExclusion : exclusion;
				if (existingRoutes.AnyWithin(at, against))
				{
					rejectedRoutes++;
					continue;
				}
				if (existingCover.AnyWithin(at, k_coverExclusion))
				{
					rejectedCover++;
					continue;
				}

				auto* const actor = wi->Spawn(nodeClass, nullptr, SFXName{}, at,
					FRotator{}, nullptr, TRUE /*bNoCollisionFail*/, FALSE);
				if (actor == nullptr)
				{
					failed++;
					continue;
				}

				auto* const node = static_cast<APathNode*>(actor);
				node->bStatic = 0;
				node->bNoDelete = 0;
				if (node->CylinderComponent == nullptr) { noCylinder++; }

				g_SpawnedPathNodes.push_back(node);
				g_SpawnedPathNodeSet.insert(node);
				if (bIsBreak) { breaksPlaced++; }
				// A node placed now is a node the next candidate has to keep away from, or
				// the exclusion test only ever sees the network as it was before this pass.
				existingRoutes.Add(at);
				spawned++;
			}
		}

		LEASI_INFO("Spawned {} node(s), {} of them on slope breaks. Rejected {} already "
			"served by a routing point and {} standing on cover.",
			spawned, breaksPlaced, rejectedRoutes, rejectedCover);
		if (breakCount > 0 && breaksPlaced == 0)
		{
			LEASI_WARN("Every slope break was rejected, so ramps still have no node at their "
				"top or bottom -- which is the case where a pawn walks into the slope.");
		}
		if (failed > 0)
		{
			LEASI_ERROR("{} Spawn call(s) returned null even with bStatic and bNoDelete "
				"cleared on the CDO.", failed);
		}
		if (g_SpawnedPathNodes.empty())
		{
			LEASI_INFO("Nothing to connect. If everything was rejected, the level is already "
				"pathed here and there is no gap for a path node to fill.");
			return;
		}
		if (spawned == 0)
		{
			// Deliberately NOT a return. Rebuilding cover -- cc.despawnlinks, cc.sweep -- takes
			// the path network's cover specs with it, and the nodes that were there are exactly
			// the reason nothing new gets placed. Bailing out here would leave the two networks
			// permanently disconnected with no way back short of despawning the nodes too.
			LEASI_INFO("No new nodes: the nav network already reaches everywhere sampled. "
				"Carrying on to the spec passes, which is what rejoins the existing nodes to "
				"cover that has been rebuilt since.");
		}

		std::vector<SpecSize> sizes = ReadPathSizes(guard ? guard.Get() : nullptr);
		if (sizes.empty())
		{
			// PathSizes(0) is Common, 34 x 90, and it is what the cover analysis itself ran
			// at -- so this fallback is the old single-size behaviour, not a new guess.
			LEASI_WARN("No scout PathSizes, so every spec is sized 34 x 90 and only the "
				"smallest pawns will be able to use this network.");
			sizes.push_back(SpecSize{ "Common", 34.0f, 90.0f });
		}
		else
		{
			std::string listed;
			for (const auto& size : sizes)
			{
				if (!listed.empty()) { listed += ", "; }
				listed += size.desc + " " + std::to_string(static_cast<int>(size.radius))
					+ "x" + std::to_string(static_cast<int>(size.height));
			}
			LEASI_INFO("Sizing every spec by the largest pawn that fits, largest first: {}.",
				listed);
		}
		const size_t smallest = sizes.size() - 1;

		/// Counts per size index, plus the ones nothing fit through.
		std::vector<int> sizeHistogram(sizes.size(), 0);
		int sizedByFallback = 0;

		// ! Pass 4 -- node to node
		//
		// Both directions per pair: a ReachSpec is directed, and a network built one way round
		// is a set of one-way streets.
		const float linkReach = spacing * k_linkReachFactor;
		const int span = CellSpan(linkReach, spacing);
		const int zSpan = CellSpan(linkReach, k_floorStoreyHeight);

		// Every node we own, not only this pass's, so a second cc.pathnodes over newly opened
		// ground joins onto the network already there instead of building an island beside it.
		std::map<SampleKey, std::vector<size_t>> nodeBuckets;
		for (size_t i = 0; i < g_SpawnedPathNodes.size(); i++)
		{
			nodeBuckets[FloorSampleKey(g_SpawnedPathNodes[i]->LOCATION, spacing)]
				.push_back(i);
		}

		int nodeSpecs = 0, nodePairsBlocked = 0, nodeSpecsFailed = 0;
		/// Pairs in reach whose floor does not continuously connect them.
		int nodePairsUnwalkable = 0;

		// ! Each node's floor is PROBED, not derived from where we meant to put it
		//
		// Deriving it as `Z - cylinder half-height` looked safe -- that is the formula the
		// nodes were placed with -- and it was wrong by a consistent 26u: PathNode_0 sits at
		// Z 833 over a floor that probes at 717, not 743. Something between the requested
		// location and the spawned actor adds that offset, and it does not matter what,
		// because the walk should be asking the geometry either way.
		//
		// It mattered because WalkFloor takes the DESTINATION's floor from its caller rather
		// than probing the last sub-step. The walk climbed the real ramp to +102 and then its
		// final entry jumped to the assumed floor at +144 -- a 42u step that exists nowhere in
		// the level, over a 35u limit, on a ramp every other sub-step had just cleared at 16u
		// a time. Both nodes up the ramp were rejected by an artefact of the baseline.
		//
		// Probed once per node here rather than per pair: pass 4 is O(pairs) and this is
		// O(nodes).
		std::vector<float> nodeFloor(g_SpawnedPathNodes.size(), 0.0f);
		int floorAssumed = 0;
		float worstFloorError = 0.0f;
		for (size_t i = 0; i < g_SpawnedPathNodes.size(); i++)
		{
			auto* const node = g_SpawnedPathNodes[i];
			const float assumed = (node != nullptr)
				? (node->LOCATION.Z - nodeHalfHeight) : 0.0f;
			float probed = assumed;
			if (node == nullptr
				|| ProbeFloor(tracer, node->LOCATION, maxRise, maxDrop, probed)
					!= FloorProbe::Found)
			{
				// Nothing under it to measure against, so the placement formula is all there
				// is. Counted, because a node with no floor under it is worth knowing about.
				probed = assumed;
				floorAssumed++;
			}
			else
			{
				const float error = std::abs(probed - assumed);
				if (error > worstFloorError) { worstFloorError = error; }
			}
			nodeFloor[i] = probed;
		}
		if (worstFloorError > 1.0f)
		{
			LEASI_INFO("Node floors are probed, not derived: they sit up to {:.0f}u off the "
				"placement formula (Z minus the {:.0f}u cylinder). Deriving them put a step "
				"that big at the end of every walk.", worstFloorError, nodeHalfHeight);
		}
		if (floorAssumed > 0)
		{
			LEASI_WARN("{} node(s) have no floor under them to probe, so their placement "
				"height was used instead.", floorAssumed);
		}
		for (size_t i = 0; i < g_SpawnedPathNodes.size(); i++)
		{
			auto* const from = g_SpawnedPathNodes[i];
			const SampleKey home = FloorSampleKey(from->LOCATION, spacing);

			for (int dz = -zSpan; dz <= zSpan; dz++)
			{
				for (int dy = -span; dy <= span; dy++)
				{
					for (int dx = -span; dx <= span; dx++)
					{
						const auto found = nodeBuckets.find(
							SampleKey{ home.x + dx, home.y + dy, home.z + dz });
						if (found == nodeBuckets.end()) { continue; }

						for (const size_t j : found->second)
						{
							// Each unordered pair is handled once, by its lower index. Both
							// specs are made here, so the higher index has nothing to do --
							// which also stops the pair being traced twice.
							if (j <= i) { continue; }
							// Both already existed, so they were joined by the run that made
							// them; nothing about them has changed.
							if (i < firstNew && j < firstNew) { continue; }
							auto* const to = g_SpawnedPathNodes[j];

							// Horizontal, not straight-line. `linkReach` is derived from the
							// sample spacing, which is a distance along the FLOOR -- and on a
							// ramp the straight-line distance between two grid-adjacent nodes
							// is longer than the spacing by however much the floor climbed.
							// Measuring through the air quietly shortens the reach on exactly
							// the geometry that needs it most, and puts diagonal pairs on a
							// slope out of range altogether.
							const float run = HorizontalDistance(from->LOCATION, to->LOCATION);
							if (run > linkReach) { continue; }

							// Whether the floor between them is continuously walkable, asked
							// the same way the sampler asks it.
							//
							// This replaces a flat rise limit of StandHeight, which was
							// inherited from the sampler (where it bounds how far a PROBE
							// looks for floor) and had no business deciding whether two nodes
							// may be joined. It rejected the two nodes up the ramp at dZ +144
							// and +141 while the sampler was calling that same step walkable:
							// a 26-degree ramp climbs more than a pawn is tall in well under
							// two sample steps, and there is nothing wrong with that. The rise
							// between two points on a ramp is bounded by the SLOPE, not by
							// anyone's height.
							//
							// Asking the floor directly also makes the two passes agree by
							// construction rather than by two constants happening to line up.
							if (maxStepHeight > 0.0f)
							{
								const FloorProfile walk = WalkFloor(tracer,
									from->LOCATION, nodeFloor[i],
									to->LOCATION, nodeFloor[j],
									maxStepHeight, maxDrop);
								if (walk.result != FloorWalk::Continuous)
								{
									nodePairsUnwalkable++;
									continue;
								}
							}
							else if (std::abs(to->LOCATION.Z - from->LOCATION.Z) > maxRise)
							{
								// No step height to walk with, so fall back to the old gate
								// rather than joining everything in reach.
								nodePairsUnwalkable++;
								continue;
							}
							if (HasSpecTo(from, to)) { continue; }

							const int fit = LargestSizeThatFits(tracer, from->LOCATION,
								nodeFloor[i], to->LOCATION, nodeFloor[j], sizes);
							size_t pick = smallest;
							if (fit >= 0) { pick = static_cast<size_t>(fit); }
							else
							{
								// Not even the smallest box fits. A zero-extent line still
								// decides whether there is a WALL in the way, and the engine
								// re-checks reachability when the move is attempted, so this
								// keeps a route the pawn may have to squeeze along rather
								// than dropping it -- counted, so it stays visible.
								if (!RouteClear(tracer, from->LOCATION, to->LOCATION,
									FVector{ 1.0f, 1.0f, 1.0f }))
								{
									nodePairsBlocked++;
									continue;
								}
								sizedByFallback++;
							}
							sizeHistogram[pick]++;

							auto* const cls = UReachSpec::StaticClass();
							if (CreateSpec(cls, from, to, sizes[pick].radius,
								sizes[pick].height, k_reachFlagWalk) != nullptr)
							{
								nodeSpecs++;
							}
							else { nodeSpecsFailed++; }

							if (CreateSpec(cls, to, from, sizes[pick].radius,
								sizes[pick].height, k_reachFlagWalk) != nullptr)
							{
								nodeSpecs++;
							}
							else { nodeSpecsFailed++; }
						}
					}
				}
			}
		}

		// Counted only now: a node's PathList keeps growing while later pairs are made, so
		// asking mid-loop would report nodes as isolated that are about to be joined.
		int isolated = 0;
		for (size_t i = firstNew; i < g_SpawnedPathNodes.size(); i++)
		{
			if (g_SpawnedPathNodes[i]->PathList.Count() == 0) { isolated++; }
		}

		LEASI_INFO("Node network: {} new spec(s) across {} node(s) in total. {} neighbour "
			"pair(s) had no clear line, {} had no continuously walkable floor between them.",
			nodeSpecs, static_cast<int>(g_SpawnedPathNodes.size()), nodePairsBlocked,
			nodePairsUnwalkable);
		if (nodeSpecsFailed > 0)
		{
			LEASI_ERROR("{} spec(s) failed to construct.", nodeSpecsFailed);
		}
		if (isolated > 0)
		{
			LEASI_WARN("{} node(s) ended up with no connection at all. A node nothing can "
				"reach is worse than no node -- it is a destination the AI will consider and "
				"never arrive at.", isolated);
		}

		// ! Pass 5 -- node to cover (roadmap step 5)
		//
		// Without this the two networks sit side by side and never touch: the AI can stand in
		// cover it happens to be next to, and cannot path to any of it.
		const auto markers = OurCoverMarkers();
		// Probed once each, for the same reason the nodes are: a marker's Z is where
		// AutoAdjustSlot seated it, not a fixed height above the ground.
		std::vector<float> markerFloor(markers.size(), 0.0f);
		for (size_t m = 0; m < markers.size(); m++)
		{
			float probed = markers[m]->LOCATION.Z;
			ProbeFloor(tracer, markers[m]->LOCATION, maxRise, maxDrop, probed);
			markerFloor[m] = probed;
		}
		// Traced at the pawn that has to fit, labelled at the size shipped data uses.
		const float coverTraceRadius = bUseScoutRadiusForCoverTrace
			? sizes[smallest].radius : k_coverSpecRadius;
		const float coverTraceHeight = bUseScoutRadiusForCoverTrace
			? sizes[smallest].height : k_coverSpecHeight;

		int coverSpecs = 0, coverBlocked = 0, coverSpecsFailed = 0, markersJoined = 0;
		/// Approaches a human-sized box will not fit down, kept on a clear line alone.
		int coverTooTight = 0;
		for (size_t m = 0; m < markers.size(); m++)
		{
			auto* const marker = markers[m];
			const SampleKey home = FloorSampleKey(marker->LOCATION, spacing);
			bool bJoined = false;
			int approaches = 0;

			// Gathered and sorted before anything is built, because the cap has to spend its
			// budget on the NEAREST nodes. Creating them as they are found spends it on
			// whichever bucket the lookup happened to visit first.
			std::vector<std::pair<float, size_t>> nearby;
			for (int dz = -zSpan; dz <= zSpan; dz++)
			{
				for (int dy = -span; dy <= span; dy++)
				{
					for (int dx = -span; dx <= span; dx++)
					{
						const auto found = nodeBuckets.find(
							SampleKey{ home.x + dx, home.y + dy, home.z + dz });
						if (found == nodeBuckets.end()) { continue; }

						for (const size_t j : found->second)
						{
							auto* const node = g_SpawnedPathNodes[j];
							const float dist = Distance(marker->LOCATION, node->LOCATION);
							if (dist > linkReach) { continue; }
							if (HasSpecTo(marker, node))
							{
								// Built by an earlier run, and it still counts against the
								// cap -- otherwise a second cc.pathnodes stacks another set
								// on top of the first.
								bJoined = true;
								approaches++;
								continue;
							}
							nearby.emplace_back(dist, j);
						}
					}
				}
			}
			std::sort(nearby.begin(), nearby.end(),
				[](const std::pair<float, size_t>& a, const std::pair<float, size_t>& b)
				{ return a.first < b.first; });

			for (const auto& candidate : nearby)
			{
				if (approaches >= k_maxApproachNodesPerMarker) { break; }
				const size_t j = candidate.second;
				auto* const node = g_SpawnedPathNodes[j];

				// Fixed at the cover size, NOT sized to the largest pawn that fits: only
				// human-sized pawns use cover at all, so a wider spec here would advertise a
				// route to a destination that rejects whoever followed it.
				if (!PawnBoxFits(tracer, marker->LOCATION, markerFloor[m],
					node->LOCATION, nodeFloor[j], coverTraceRadius, coverTraceHeight))
				{
					if (!RouteClear(tracer, marker->LOCATION, node->LOCATION,
						FVector{ 1.0f, 1.0f, 1.0f }))
					{
						// Does not consume the budget: a blocked candidate is not an
						// approach, and the next nearest should get its turn.
						coverBlocked++;
						continue;
					}
					coverTooTight++;
				}

				// A plain ReachSpec, which is what shipped markers carry to ordinary nav
				// points -- SlotToSlot is the cover chain and means something narrower.
				auto* const cls = UReachSpec::StaticClass();
				if (CreateSpec(cls, node, marker, k_coverSpecRadius, k_coverSpecHeight,
					k_reachFlagWalk) != nullptr)
				{
					coverSpecs++;
				}
				else { coverSpecsFailed++; }

				if (CreateSpec(cls, marker, node, k_coverSpecRadius, k_coverSpecHeight,
					k_reachFlagWalk) != nullptr)
				{
					coverSpecs++;
					bJoined = true;
				}
				else { coverSpecsFailed++; }
				approaches++;
			}

			if (bJoined) { markersJoined++; }
		}

		LEASI_INFO("Cover to paths: {} spec(s) joining {} of {} cover marker(s) to the node "
			"network, all at the cover size {:.0f} x {:.0f}. {} pair(s) had no clear line.",
			coverSpecs, markersJoined, static_cast<int>(markers.size()),
			k_coverSpecRadius, k_coverSpecHeight, coverBlocked);
		if (!markers.empty())
		{
			// Outgoing only, to compare like with like: the shipped figure counts specs whose
			// Start is a CoverSlotMarker and whose End is a PathNode.
			LEASI_INFO("  traced at {:.0f} x {:.0f} (the pawn that has to fit past a {:.0f}u "
				"slot standoff), labelled {:.0f} x {:.0f} (what shipped specs carry).",
				coverTraceRadius, coverTraceHeight, 36.0f,
				k_coverSpecRadius, k_coverSpecHeight);
			LEASI_INFO("  {:.2f} outgoing approach(es) per marker, at most {} node(s) each. "
				"Shipped BioD_Kro001_605FourthPaths runs 0.83 -- see note 06.",
				(static_cast<float>(coverSpecs) * 0.5f)
					/ static_cast<float>(markers.size()),
				k_maxApproachNodesPerMarker);
		}
		if (coverTooTight > 0)
		{
			LEASI_WARN("{} approach(es) a human-sized box will not fit down were kept on a "
				"clear line alone. A cover slot nothing can walk up to is cover nothing can "
				"use.", coverTooTight);
		}
		if (coverSpecsFailed > 0)
		{
			LEASI_ERROR("{} cover-to-path spec(s) failed to construct.", coverSpecsFailed);
		}
		if (!markers.empty() && markersJoined == 0)
		{
			LEASI_WARN("Not one cover marker reached a path node. Either the nodes are all "
				"further than {:.0f}u from any cover, or the exclusion in pass 2 rejected "
				"every candidate near cover -- which it will if the markers themselves "
				"already fill the space.", linkReach);
		}

		// Which pawns can actually use this network. The point of sizing every spec was that
		// one number -- if everything lands on the smallest entry, only the smallest mooks can
		// move, whatever the network looks like on screen.
		LEASI_INFO("--- who can use these ROUTES (cover approaches are excluded and fixed at "
			"{:.0f} x {:.0f}, because cover is human-only) ---",
			k_coverSpecRadius, k_coverSpecHeight);
		for (size_t i = 0; i < sizes.size(); i++)
		{
			if (sizeHistogram[i] == 0) { continue; }
			LEASI_INFO("  {:>10} {:.0f} x {:.0f}: {} connection(s), usable by anything this "
				"size or smaller.", sizes[i].desc, sizes[i].radius, sizes[i].height,
				sizeHistogram[i]);
		}
		if (sizedByFallback > 0)
		{
			LEASI_WARN("{} connection(s) fit no pawn box at all and were sized {:.0f} x "
				"{:.0f} on a clear line alone. They may be routes nothing can physically "
				"walk.", sizedByFallback, sizes[smallest].radius, sizes[smallest].height);
		}

		// ! Pass 6 -- the octree, last
		//
		// AddToNavigationOctree registers the point AND the specs on its PathList, so this
		// cannot run before the spec passes. Cover markers registered by cc.buildlinks have to
		// be re-registered too: their PathList grew in pass 5, and the entries added after
		// their original registration are not in the octree.
		int registered = 0, stillOut = 0, reRegistered = 0;
		for (auto* const node : g_SpawnedPathNodes)
		{
			if (node->CylinderComponent == nullptr) { continue; }
			// Every node, not only this pass's: one from an earlier run can have gained specs
			// in pass 4, and the octree only ever saw the PathList it had at the time.
			if (node->NavOctreeObject.OctreeNode != nullptr)
			{
				CallRemoveFromNavigationOctree(node);
			}
			CallAddToNavigationOctree(node);
			if (node->NavOctreeObject.OctreeNode != nullptr) { registered++; }
			else { stillOut++; }
		}
		for (auto* const marker : markers)
		{
			if (marker->CylinderComponent == nullptr) { continue; }
			if (marker->NavOctreeObject.OctreeNode != nullptr)
			{
				CallRemoveFromNavigationOctree(marker);
			}
			CallAddToNavigationOctree(marker);
			if (marker->NavOctreeObject.OctreeNode != nullptr) { reRegistered++; }
		}

		LEASI_INFO("Octree: {} node(s) registered, {} cover marker(s) re-registered so their "
			"new specs are indexed.", registered, reRegistered);
		if (noCylinder > 0)
		{
			LEASI_WARN("{} node(s) have no CylinderComponent, so AddToNavigationOctree "
				"early-outs and they stay out of every radius query.", noCylinder);
		}
		if (stillOut > 0)
		{
			LEASI_WARN("{} node(s) still have a null OctreeNode after the add.", stillOut);
		}
		// ! Pass 6 -- cover links as routing hubs (see below for pass 7)
		//
		// Runs before the octree, like every other spec pass: AddToNavigationOctree walks
		// PathList at registration time, so a spec added afterwards is never indexed.
		int linkMarkerSpecs = 0, linkLinkSpecs = 0, linkNodeSpecs = 0, linkSpecsFailed = 0;
		int linksRegistered = 0, linksNoCylinder = 0;
		{
			auto* const cls = UReachSpec::StaticClass();
			std::vector<ACoverLink*> links;
			for (auto* const link : GetSpawnedLinks())
			{
				if (link != nullptr && !::Common::IsDestroyed(link)) { links.push_back(link); }
			}

			// Probed, not derived -- the same rule the nodes and markers follow.
			std::vector<float> linkFloor(links.size(), 0.0f);
			for (size_t i = 0; i < links.size(); i++)
			{
				float probed = links[i]->LOCATION.Z;
				ProbeFloor(tracer, links[i]->LOCATION, maxRise, maxDrop, probed);
				linkFloor[i] = probed;
			}

			for (size_t i = 0; i < links.size(); i++)
			{
				auto* const link = links[i];

				// --- the link and its own slots, both ways, at the cover size ---
				//
				// This is how a slot reaches the highway at all. Shipped markers carry exactly
				// one such spec each, to the link named in their own OwningSlot.
				const int slotCount = static_cast<int>(link->Slots.Count());
				for (int sIdx = 0; sIdx < slotCount; sIdx++)
				{
					auto* const marker = link->Slots(sIdx).SlotMarker;
					if (marker == nullptr || link->Slots(sIdx).bEnabled == 0) { continue; }
					if (HasSpecTo(link, marker)) { continue; }

					if (CreateSpec(cls, link, marker, k_coverSpecRadius, k_coverSpecHeight,
						k_reachFlagWalk) != nullptr) { linkMarkerSpecs++; }
					else { linkSpecsFailed++; }
					if (CreateSpec(cls, marker, link, k_coverSpecRadius, k_coverSpecHeight,
						k_reachFlagWalk) != nullptr) { linkMarkerSpecs++; }
					else { linkSpecsFailed++; }
				}

				// --- link to link, and link to path node, long range and wide ---
				std::vector<std::pair<float, size_t>> otherLinks;
				for (size_t j = 0; j < links.size(); j++)
				{
					if (j == i) { continue; }
					const float d = Distance(link->LOCATION, links[j]->LOCATION);
					if (d <= k_linkRouteReach) { otherLinks.emplace_back(d, j); }
				}
				std::sort(otherLinks.begin(), otherLinks.end(),
					[](const std::pair<float, size_t>& a, const std::pair<float, size_t>& b)
					{ return a.first < b.first; });

				int made = 0;
				for (const auto& cand : otherLinks)
				{
					if (made >= k_maxLinkToLinkRoutes) { break; }
					auto* const other = links[cand.second];
					if (HasSpecTo(link, other)) { made++; continue; }
					const int fit = LargestSizeThatFits(tracer, link->LOCATION, linkFloor[i],
						other->LOCATION, linkFloor[cand.second], sizes);
					if (fit < 0) { continue; }
					const SpecSize& size = sizes[static_cast<size_t>(fit)];
					if (CreateSpec(cls, link, other, size.radius, size.height,
						k_reachFlagWalk) != nullptr) { linkLinkSpecs++; }
					else { linkSpecsFailed++; }
					if (CreateSpec(cls, other, link, size.radius, size.height,
						k_reachFlagWalk) != nullptr) { linkLinkSpecs++; }
					else { linkSpecsFailed++; }
					made++;
				}

				std::vector<std::pair<float, size_t>> nodes;
				for (size_t j = 0; j < g_SpawnedPathNodes.size(); j++)
				{
					auto* const node = g_SpawnedPathNodes[j];
					if (node == nullptr || ::Common::IsDestroyed(node)) { continue; }
					const float d = Distance(link->LOCATION, node->LOCATION);
					if (d <= k_linkRouteReach) { nodes.emplace_back(d, j); }
				}
				std::sort(nodes.begin(), nodes.end(),
					[](const std::pair<float, size_t>& a, const std::pair<float, size_t>& b)
					{ return a.first < b.first; });

				made = 0;
				for (const auto& cand : nodes)
				{
					if (made >= k_maxLinkToNodeRoutes) { break; }
					auto* const node = g_SpawnedPathNodes[cand.second];
					if (HasSpecTo(link, node)) { made++; continue; }
					const int fit = LargestSizeThatFits(tracer, link->LOCATION, linkFloor[i],
						node->LOCATION, nodeFloor[cand.second], sizes);
					if (fit < 0) { continue; }
					const SpecSize& size = sizes[static_cast<size_t>(fit)];
					if (CreateSpec(cls, link, node, size.radius, size.height,
						k_reachFlagWalk) != nullptr) { linkNodeSpecs++; }
					else { linkSpecsFailed++; }
					if (CreateSpec(cls, node, link, size.radius, size.height,
						k_reachFlagWalk) != nullptr) { linkNodeSpecs++; }
					else { linkSpecsFailed++; }
					made++;
				}
			}

			// The links themselves have to be in the octree or none of the above is visible to
			// a radius query -- the same gate the markers hit.
			for (size_t i = 0; i < links.size(); i++)
			{
				auto* const link = links[i];
				if (link->CylinderComponent == nullptr) { linksNoCylinder++; continue; }
				if (link->NavOctreeObject.OctreeNode != nullptr)
				{
					CallRemoveFromNavigationOctree(link);
				}
				CallAddToNavigationOctree(link);
				if (link->NavOctreeObject.OctreeNode != nullptr) { linksRegistered++; }
			}

			const int routedLinkCount = static_cast<int>(links.size());
			if (routedLinkCount > 0)
			{
				LEASI_INFO("Cover links as routing hubs: {} spec(s) to their own slots, {} "
					"link-to-link, {} link-to-node = {:.2f} outgoing per link. Shipped "
					"BioD_CerMir_125Land runs 4.51 -- see note 06.",
					linkMarkerSpecs, linkLinkSpecs, linkNodeSpecs,
					static_cast<float>(linkMarkerSpecs + linkLinkSpecs + linkNodeSpecs)
						* 0.5f / static_cast<float>(routedLinkCount));
				LEASI_INFO("  {} link(s) registered in the navigation octree.",
					linksRegistered);
			}
			if (linkSpecsFailed > 0)
			{
				LEASI_WARN("{} link spec(s) could not be constructed.", linkSpecsFailed);
			}
			if (linksNoCylinder > 0)
			{
				LEASI_WARN("{} link(s) have no CylinderComponent and stay out of the octree.",
					linksNoCylinder);
			}
		}

		// ! Pass 7 -- the path-build metadata nothing was setting
		//
		// A trooper spawned on this network fired, faced the player, and would not move. Its AI
		// was trying: "move to goal", "find cover". Two fields on every ANavigationPoint are
		// filled in by FPathBuilder at build time and were still at their defaults on ours:
		//
		//   MaxPathSize (FCylinder)  the largest pawn any path touching this node can carry.
		//                            Shipped PathNode_0 in CerMir: **140 x 195**. Ours: 0 x 0.
		//                            FindPath early-rejects a node whose MaxPathSize is
		//                            smaller than the pawn, so at 0 it rejects EVERY pawn --
		//                            a network that is fully built, correctly sized, octree
		//                            registered, and closed to everything alive.
		//
		//   NetworkID                which connected component the node belongs to. Shipped
		//                            PathNode_0: 431. Pathing early-outs when start and goal
		//                            disagree, so a default on both sides is at best luck.
		//
		// Both are derived from the specs, so they have to come last -- after every pass that
		// can add one.
		{
			// Everything we own that can carry a spec.
			std::vector<ANavigationPoint*> navActors;
			for (auto* const node : g_SpawnedPathNodes)
			{
				if (node != nullptr && !::Common::IsDestroyed(node)) { navActors.push_back(node); }
			}
			for (auto* const marker : markers) { navActors.push_back(marker); }
			for (auto* const link : GetSpawnedLinks())
			{
				if (link != nullptr && !::Common::IsDestroyed(link)) { navActors.push_back(link); }
			}

			std::map<ANavigationPoint*, size_t> indexOf;
			for (size_t i = 0; i < navActors.size(); i++) { indexOf[navActors[i]] = i; }

			// --- MaxPathSize: the widest spec on the node's own list ---
			//
			// Taken from outgoing specs. Ours are built as bidirectional pairs, so the
			// incoming set is the same; a one-way spec would under-report at its destination,
			// which errs toward refusing a pawn rather than promising a route it cannot take.
			int sized = 0;
			float widest = 0.0f;
			for (auto* const nav : navActors)
			{
				float widestSpecRadius = 0.0f, tallestSpecHeight = 0.0f;
				for (unsigned int i = 0; i < nav->PathList.Count(); i++)
				{
					auto* const spec = nav->PathList(i);
					if (spec == nullptr) { continue; }
					widestSpecRadius = (std::max)(widestSpecRadius,
						static_cast<float>(spec->CollisionRadius));
					tallestSpecHeight = (std::max)(tallestSpecHeight,
						static_cast<float>(spec->CollisionHeight));
				}
				nav->MaxPathSize.Radius = widestSpecRadius;
				nav->MaxPathSize.Height = tallestSpecHeight;
				if (widestSpecRadius > 0.0f) { sized++; }
				widest = (std::max)(widest, widestSpecRadius);
			}

			// --- NetworkID: one per connected component ---
			//
			// Union-find over the spec graph, treated as undirected -- which is what
			// FPathBuilder::BuildNetworkIDs does, since a component is about reachability in
			// principle rather than in one direction.
			std::vector<size_t> parent(navActors.size());
			for (size_t i = 0; i < parent.size(); i++) { parent[i] = i; }
			const std::function<size_t(size_t)> findRoot = [&parent, &findRoot](size_t x)
			{
				while (parent[x] != x) { parent[x] = parent[parent[x]]; x = parent[x]; }
				return x;
			};
			for (size_t i = 0; i < navActors.size(); i++)
			{
				auto* const nav = navActors[i];
				for (unsigned int k = 0; k < nav->PathList.Count(); k++)
				{
					auto* const spec = nav->PathList(k);
					if (spec == nullptr || spec->End.Actor == nullptr) { continue; }
					auto* const end = spec->End.Actor->Cast<ANavigationPoint>();
					if (end == nullptr) { continue; }
					const auto found = indexOf.find(end);
					if (found == indexOf.end()) { continue; }
					const size_t a = findRoot(i);
					const size_t b = findRoot(found->second);
					if (a != b) { parent[a] = b; }
				}
			}

			// Numbered from a high base so an id can never collide with a shipped level's own
			// -- equal ids on two genuinely separate networks would claim a route that is not
			// there.
			constexpr int k_networkIdBase = 10000;
			std::map<size_t, int> idOfRoot;
			for (size_t i = 0; i < navActors.size(); i++)
			{
				const size_t root = findRoot(i);
				auto found = idOfRoot.find(root);
				if (found == idOfRoot.end())
				{
					found = idOfRoot.emplace(root,
						k_networkIdBase + static_cast<int>(idOfRoot.size())).first;
				}
				navActors[i]->NetworkID = found->second;
			}

			LEASI_INFO("Path metadata: MaxPathSize set on {} of {} nav point(s), widest {:.0f}; "
				"{} network component(s) numbered from {}. Both were left at their defaults "
				"until now, and MaxPathSize 0 makes a node reject every pawn alive.",
				sized, static_cast<int>(navActors.size()), widest,
				static_cast<int>(idOfRoot.size()), k_networkIdBase);
			if (idOfRoot.size() > 1)
			{
				LEASI_WARN("{} separate network(s) means nothing can path between them. One is "
					"the healthy answer for a single connected room.",
					static_cast<int>(idOfRoot.size()));
			}
		}

		{
			// The one figure the shipped survey found stable across a 5x range of level size,
			// so it is the one worth printing every run.
			const int navPoints = static_cast<int>(g_SpawnedPathNodes.size())
				+ static_cast<int>(markers.size());
			const int routingSpecs = nodeSpecs + coverSpecs
				+ linkMarkerSpecs + linkLinkSpecs + linkNodeSpecs;
			if (navPoints > 0)
			{
				LEASI_INFO("Routing density: {} spec(s) from this pass over {} nav point(s) = "
					"{:.2f} per nav point. Shipped levels run 2.6 to 3.3 for the equivalent "
					"classes, plus their cover chains. Note 06 has the survey.",
					routingSpecs, navPoints,
					static_cast<float>(routingSpecs) / static_cast<float>(navPoints));
			}
		}

		LEASI_INFO("'show paths' draws these: they are real APathNodes with real ReachSpecs, "
			"so the game's own path debug is the check on whether the network looks right.");
	}

	void CmdSeed(bool bHasArgument, bool bPinHere)
	{
		LEASI_INFO("=== cc.seed ===");

		auto* const wi = GetWorldInfo();
		auto* const pc = (wi != nullptr) ? wi->GetALocalPlayerController() : nullptr;
		const bool bHavePawn = (pc != nullptr && pc->Pawn != nullptr);

		if (bHasArgument)
		{
			if (bPinHere)
			{
				if (!bHavePawn)
				{
					LEASI_ERROR("Need a player pawn to take a position from.");
					return;
				}
				SetFloorSeed(pc->Pawn->location);
			}
			else
			{
				ClearFloorSeed();
			}
		}

		if (!HasFloorSeed())
		{
			LEASI_INFO("Seeding from the player. Runs from different spots are NOT comparable "
				"-- the placement rules are order-dependent, so the seed decides which nodes "
				"exist. 'cc.seed here' pins it.");
			return;
		}

		const FVector seed = FloorSeedOr(FVector{});
		LEASI_INFO("Pinned origin: ({:.1f}, {:.1f}, {:.1f}). cc.sweep and cc.pathnodes both "
			"start here regardless of where you stand.", seed.X, seed.Y, seed.Z);
		LEASI_INFO("To make it the default, put this in FloorSampler.cpp's g_FloorSeed:");
		LEASI_INFO("    FVector g_FloorSeed{{ {:.1f}f, {:.1f}f, {:.1f}f }};",
			seed.X, seed.Y, seed.Z);
		if (bHavePawn)
		{
			const FVector at = pc->Pawn->location;
			LEASI_INFO("You are {:.0f}u from it.", Distance(at, seed));
		}
	}

	void CmdFloorCheck(float spacing)
	{
		LEASI_INFO("=== cc.floorcheck ===");

		auto* const wi = GetWorldInfo();
		auto* const pc = (wi != nullptr) ? wi->GetALocalPlayerController() : nullptr;
		if (pc == nullptr || pc->Pawn == nullptr)
		{
			LEASI_ERROR("Need a player controller with a pawn.");
			return;
		}

		auto* const linkClass = ACoverLink::StaticClass();
		auto* const linkDefaults = (linkClass != nullptr)
			? static_cast<ACoverLink*>(linkClass->ClassDefaultObject) : nullptr;
		if (linkDefaults == nullptr)
		{
			LEASI_ERROR("Could not reach the ACoverLink CDO.");
			return;
		}
		if (spacing <= 0.0f) { spacing = linkDefaults->AutoCoverSlotInterval; }

		const float standHeight = linkDefaults->StandHeight;
		const float maxRise = standHeight;
		const float maxDrop = standHeight * 2.0f;

		auto* const nodeClass = APathNode::StaticClass();
		auto* const nodeDefaults = (nodeClass != nullptr)
			? static_cast<APathNode*>(nodeClass->ClassDefaultObject) : nullptr;
		float rideHeight = 40.0f;
		if (nodeDefaults != nullptr && nodeDefaults->CylinderComponent != nullptr)
		{
			rideHeight = nodeDefaults->CylinderComponent->CollisionHeight;
		}

		ScopedScout guard{};
		float maxStepHeight = 0.0f;
		const char* stepSource = "nothing -- falling back to the rise allowance";
		if (guard && guard.Get() != nullptr && guard.Get()->MaxStepHeight > 0.0f)
		{
			maxStepHeight = guard.Get()->MaxStepHeight;
			stepSource = "the scout";
		}
		else if (pc->Pawn->MaxStepHeight > 0.0f)
		{
			maxStepHeight = pc->Pawn->MaxStepHeight;
			stepSource = "the player pawn";
		}

		auto* const tracer = pc->Pawn;
		const FVector playerLoc = pc->Pawn->location;

		float floorZ = 0.0f;
		if (ProbeFloor(tracer, playerLoc, maxRise, maxDrop, floorZ) != FloorProbe::Found)
		{
			LEASI_ERROR("No floor under you at all, so the fill could not start here either.");
			return;
		}

		const FVector here{ playerLoc.X, playerLoc.Y, floorZ + rideHeight };
		LEASI_INFO("Standing at ({:.0f}, {:.0f}), floor Z {:.0f}. Spacing {:.0f}u, step "
			"height {:.0f}u from {}, drop allowance {:.0f}u.",
			playerLoc.X, playerLoc.Y, floorZ, spacing, maxStepHeight, stepSource, maxDrop);

		static const char* const k_names[8] = {
			"+X    ", "+X +Y ", "+Y    ", "-X +Y ", "-X    ", "-X -Y ", "-Y    ", "+X -Y " };
		const int dxs[8] = { 1, 1, 0, -1, -1, -1,  0,  1 };
		const int dys[8] = { 0, 1, 1,  1,  0, -1, -1, -1 };

		for (int d = 0; d < 8; d++)
		{
			const FVector flat{ here.X + (static_cast<float>(dxs[d]) * spacing),
				here.Y + (static_cast<float>(dys[d]) * spacing), here.Z };

			float nextFloorZ = 0.0f;
			const FloorProbe probe = ProbeFloor(tracer, flat,
				(floorZ + maxRise) - flat.Z, maxDrop + rideHeight, nextFloorZ);
			if (probe != FloorProbe::Found)
			{
				LEASI_INFO("  {}: no floor at the neighbour ({}). The fill stops here.",
					k_names[d],
					(probe == FloorProbe::None) ? "nothing below" : "probe began in solid");
				continue;
			}

			const FVector next{ flat.X, flat.Y, nextFloorZ + rideHeight };
			const FloorProfile walk = WalkFloor(tracer, here, floorZ, next, nextFloorZ,
				(maxStepHeight > 0.0f) ? maxStepHeight : maxRise, maxDrop);

			std::string profile;
			for (size_t i = 0; i < walk.floorZ.size(); i++)
			{
				if (!profile.empty()) { profile += " "; }
				profile += std::to_string(static_cast<int>(walk.floorZ[i] - floorZ));
			}

			const char* verdict = "walkable";
			switch (walk.result)
			{
			case FloorWalk::NoFloor:     verdict = "GAP IN THE FLOOR"; break;
			case FloorWalk::Blocked:     verdict = "BLOCKED -- a wall face, or a slope past "
				"MINFLOORZ that nothing may stand on"; break;
			case FloorWalk::StepTooHigh: verdict = "STEP TOO HIGH"; break;
			case FloorWalk::DropTooFar:  verdict = "DROP TOO FAR"; break;
			default: break;
			}

			FVector hitLocation{}, hitNormal{};
			const bool bLineClear = tracer->Trace(next, here, FALSE,
				FVector{ 1.0f, 1.0f, 1.0f }, 0, &hitLocation, &hitNormal, nullptr) == nullptr;

			LEASI_INFO("  {}: floor {:+.0f}u away, {}{}. Line of sight {}. Profile "
				"(dZ per sub-step of {:.0f}u): {}",
				k_names[d], nextFloorZ - floorZ, verdict,
				(walk.failedAt >= 0)
					? (" at sub-step " + std::to_string(walk.failedAt)) : std::string{},
				bLineClear ? "clear" : "BLOCKED",
				walk.runPerStep, profile);
		}

		LEASI_INFO("A ramp should read as a long profile climbing or falling a few units per "
			"sub-step. One big jump is a step. 'BLOCKED' partway along a ramp means the slope "
			"is past MINFLOORZ (0.7, about 45 degrees) and ProbeFloor will not call it "
			"standable at all -- which would be a limit to revisit, not a bug in the walk.");

		// ! Why the nearest node did or did not join its neighbours
		//
		// The sampling test above and the spec pass are two different questions, and the ramp
		// showed why they have to be asked separately: the floor walk called the step up the
		// ramp walkable, and the node there still came out unconnected. Whatever rejected it
		// did so in the spec pass, which until now reported only totals.
		//
		// This repeats that pass's candidate loop against the node nearest the player and
		// gives the verdict for every candidate, in the same order the pass applies them.
		if (g_SpawnedPathNodes.empty())
		{
			LEASI_INFO("No path nodes spawned this session, so there is nothing to say about "
				"connections. Run cc.pathnodes first.");
			return;
		}

		APathNode* nearest = nullptr;
		float nearestDist = 0.0f;
		for (auto* const node : g_SpawnedPathNodes)
		{
			if (node == nullptr || ::Common::IsDestroyed(node)) { continue; }
			const float d = Distance(node->LOCATION, here);
			if (nearest == nullptr || d < nearestDist) { nearest = node; nearestDist = d; }
		}
		if (nearest == nullptr) { return; }

		const float linkReach = spacing * k_linkReachFactor;
		std::vector<SpecSize> sizes = ReadPathSizes(guard ? guard.Get() : nullptr);
		if (sizes.empty()) { sizes.push_back(SpecSize{ "Common", 34.0f, 90.0f }); }

		LEASI_INFO("--- nearest node: {} at ({:.0f}, {:.0f}, {:.0f}), {:.0f}u from you ---",
			NameOf(nearest), nearest->LOCATION.X, nearest->LOCATION.Y, nearest->LOCATION.Z,
			nearestDist);
		LEASI_INFO("It holds {} outgoing spec(s). Reach is {:.0f}u measured along the floor, "
			"and the floor between two nodes has to be continuously walkable at a {:.0f}u "
			"step height.",
			static_cast<int>(nearest->PathList.Count()), linkReach, maxStepHeight);

		// Everything reasonably close, not only what the pass would have looked at -- a
		// candidate rejected for being just out of reach is exactly what wants to be visible.
		struct Candidate
		{
			float dist{ 0.0f };
			float run{ 0.0f };
			float dz{ 0.0f };
			std::string path{};
			std::string verdict{};
		};
		std::vector<Candidate> candidates;
		for (auto* const other : g_SpawnedPathNodes)
		{
			if (other == nullptr || other == nearest || ::Common::IsDestroyed(other))
			{
				continue;
			}
			const float dist = Distance(nearest->LOCATION, other->LOCATION);
			const float run = HorizontalDistance(nearest->LOCATION, other->LOCATION);
			if (run > linkReach * 2.0f) { continue; }
			const float dz = other->LOCATION.Z - nearest->LOCATION.Z;

			std::string verdict;
			if (HasSpecTo(nearest, other))
			{
				verdict = "JOINED";
			}
			else if (run > linkReach)
			{
				verdict = "out of reach";
			}
			else if (maxStepHeight > 0.0f
				&& WalkFloor(tracer, nearest->LOCATION, FloorUnder(tracer, nearest, rideHeight,
						maxRise, maxDrop),
					other->LOCATION, FloorUnder(tracer, other, rideHeight, maxRise, maxDrop),
					maxStepHeight, maxDrop).result != FloorWalk::Continuous)
			{
				const FloorProfile walk = WalkFloor(tracer,
					nearest->LOCATION,
					FloorUnder(tracer, nearest, rideHeight, maxRise, maxDrop),
					other->LOCATION,
					FloorUnder(tracer, other, rideHeight, maxRise, maxDrop),
					maxStepHeight, maxDrop);

				const char* why = "?";
				switch (walk.result)
				{
				case FloorWalk::NoFloor:     why = "gap in the floor";    break;
				case FloorWalk::Blocked:     why = "BLOCKED (wall face, or a slope past "
					"MINFLOORZ)"; break;
				case FloorWalk::StepTooHigh: why = "step too high";       break;
				default:                     why = "drop too far";        break;
				}

				// The floor each end is ASSUMED to be the node's Z less its cylinder
				// half-height, because that is how the node was placed. If the engine moved
				// the actor after Spawn that assumption is wrong and the walk starts from a
				// height the geometry does not have -- so print the assumption next to what
				// the floor actually probes at.
				float fromProbed = 0.0f, toProbed = 0.0f;
				const bool bFromOk = ProbeFloor(tracer, nearest->LOCATION, maxRise, maxDrop,
					fromProbed) == FloorProbe::Found;
				const bool bToOk = ProbeFloor(tracer, other->LOCATION, maxRise, maxDrop,
					toProbed) == FloorProbe::Found;

				std::string profile;
				for (size_t k = 0; k < walk.floorZ.size(); k++)
				{
					if (!profile.empty()) { profile += " "; }
					profile += std::to_string(
						static_cast<int>(walk.floorZ[k] - walk.floorZ[0]));
				}

				verdict = std::string{ "floor not walkable: " } + why
					+ " at sub-step " + std::to_string(walk.failedAt)
					+ " | assumed floors " + std::to_string(
						static_cast<int>(nearest->LOCATION.Z - rideHeight))
					+ " -> " + std::to_string(
						static_cast<int>(other->LOCATION.Z - rideHeight))
					+ ", probed "
					+ (bFromOk ? std::to_string(static_cast<int>(fromProbed))
						: std::string{ "none" })
					+ " -> "
					+ (bToOk ? std::to_string(static_cast<int>(toProbed))
						: std::string{ "none" })
					+ " | profile " + profile;
			}
			else
			{
				const int fit = LargestSizeThatFits(tracer,
					nearest->LOCATION, FloorUnder(tracer, nearest, rideHeight, maxRise, maxDrop),
					other->LOCATION, FloorUnder(tracer, other, rideHeight, maxRise, maxDrop),
					sizes);
				if (fit >= 0)
				{
					verdict = "NOT JOINED, though a " + sizes[static_cast<size_t>(fit)].desc
						+ " box fits -- so nothing here explains it";
				}
				else if (RouteClear(tracer, nearest->LOCATION, other->LOCATION,
					FVector{ 1.0f, 1.0f, 1.0f }))
				{
					verdict = "NOT JOINED, no pawn box fits but the line is clear";
				}
				else
				{
					verdict = "not joined: the line is blocked";
				}
			}

			candidates.push_back(Candidate{ dist, run, dz, NameOf(other), verdict });
		}

		std::sort(candidates.begin(), candidates.end(),
			[](const Candidate& a, const Candidate& b) { return a.dist < b.dist; });
		for (const auto& candidate : candidates)
		{
			LEASI_INFO("  {:>6.0f}u away ({:>5.0f}u along the floor), dZ {:+5.0f}  {}  {}",
				candidate.dist, candidate.run, candidate.dz, candidate.path,
				candidate.verdict);
		}
		if (candidates.empty())
		{
			LEASI_WARN("Nothing else within {:.0f}u. This node is isolated by distance alone.",
				linkReach * 2.0f);
		}
	}

	void CmdDespawnPathNodes()
	{
		LEASI_INFO("=== cc.despawnpathnodes ===");
		if (g_SpawnedPathNodes.empty())
		{
			LEASI_INFO("Nothing to remove.");
			return;
		}

		// Incoming specs first. Destroying a nav point unregisters IT, and leaves every other
		// point's PathList holding a dangling End.Actor that the octree still indexes.
		int coverSpecsDropped = 0;
		for (auto* const link : GetSpawnedLinks())
		{
			if (link == nullptr || ::Common::IsDestroyed(link)) { continue; }
			const int slots = static_cast<int>(link->Slots.Count());
			for (int i = 0; i < slots; i++)
			{
				auto* const marker = link->Slots(i).SlotMarker;
				if (marker == nullptr || ::Common::IsDestroyed(marker)) { continue; }
				coverSpecsDropped += DropSpecsWhere(marker, [](AActor* target)
					{
						return g_SpawnedPathNodeSet.find(target)
							!= g_SpawnedPathNodeSet.end();
					});
			}
		}

		int destroyed = 0, specsCleared = 0;
		for (auto* const node : g_SpawnedPathNodes)
		{
			if (node == nullptr || ::Common::IsDestroyed(node)) { continue; }
			if (node->NavOctreeObject.OctreeNode != nullptr)
			{
				CallRemoveFromNavigationOctree(node);
			}
			specsCleared += static_cast<int>(node->PathList.Count());
			node->PathList.Clear();
			node->bStatic = 0;
			node->bNoDelete = 0;
			node->Destroy();
			destroyed++;
		}

		LEASI_INFO("Destroyed {} of {} path node(s), clearing {} outgoing spec(s) and {} "
			"incoming one(s) from cover markers.",
			destroyed, static_cast<int>(g_SpawnedPathNodes.size()), specsCleared,
			coverSpecsDropped);
		g_SpawnedPathNodes.clear();
		g_SpawnedPathNodeSet.clear();
	}
}
