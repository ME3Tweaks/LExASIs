#pragma once

#include <LESDK/Headers.hpp>

namespace CoverClient
{
	// ! Navigation runtime primitives
	// ========================================
	//
	// The three things every runtime navigation pass in this client needs, and all three are
	// traps rather than conveniences:
	//
	//   1. ANavigationPoint defaults bStatic AND bNoDelete to true, and SpawnActor refuses a
	//      class with either (UnLevAct.cpp:406). Every nav actor we spawn -- CoverLink,
	//      CoverSlotMarker, MantleMarker, SFXEnemySpawnPoint, PathNode -- hits this, and the
	//      failure is a null return with no message.
	//   2. Nothing in the shipping exe builds a ReachSpec. FPathBuilder kept only GetScout and
	//      DestroyScout, so a level-loaded marker has a PathList purely because the specs were
	//      serialised. Live-spawned navigation has no paths at all until we construct them.
	//   3. Every radius query reads the navigation octree, and Spawn does not put anything in
	//      it. An unregistered nav point exists, draws, and is invisible to the game.
	//
	// These lived in ScoutHarness.cpp while cover was the only thing being generated. Path
	// nodes need all three as well, so they are here rather than copied -- note 04's Phase 3
	// asks for one implementation of exactly this kind of thing, not two that can drift.

	/// Clears bStatic / bNoDelete on a class's CDO for as long as it is alive, then puts them
	/// back exactly as they were.
	///
	/// The instance copies its property values out of CDO memory at construction, so the CDO
	/// is the only place to intervene -- including for spawns the ENGINE performs on our
	/// behalf, like ACoverLink::AddMyMarker creating its own markers.
	class ScopedSpawnableNav final
	{
	public:
		explicit ScopedSpawnableNav(UClass* Class);
		~ScopedSpawnableNav();

		ScopedSpawnableNav(ScopedSpawnableNav const&) = delete;
		ScopedSpawnableNav& operator=(ScopedSpawnableNav const&) = delete;

	private:
		ANavigationPoint* Defaults{ nullptr };
		unsigned long     SavedStatic{ 0 };
		unsigned long     SavedNoDelete{ 0 };
	};

	/// UE3 EReachSpecFlags. Walking is the only one we emit; jump / swim / fly are moves we
	/// do not yet detect.
	constexpr int k_reachFlagWalk = 1;

	/// Creates one directed spec of `specClass` and appends it to `start`'s PathList.
	///
	/// The class is a parameter because the class IS the move: a cover chain is a
	/// USlotToSlotReachSpec, a plain walk between path nodes is a UReachSpec, and a mantle or
	/// slip has its own again. Passing it explicitly at the call site keeps that visible.
	///
	/// @returns The spec, or nullptr if it could not be constructed.
	UReachSpec* CreateSpec(UClass* specClass, ANavigationPoint* start, ANavigationPoint* end,
		float radius, float height, int reachFlags);

	/// ANavigationPoint::AddToNavigationOctree, vtable slot 265 (0x7FF74AE88C20).
	///
	/// It early-outs unless CylinderComponent is set and OctreeNode is null, then registers the
	/// point AND every ReachSpec on its PathList. That last part is why this must run AFTER the
	/// spec passes -- a spec added afterwards is never registered, and the move it describes
	/// stays invisible to anything that searches the octree.
	void CallAddToNavigationOctree(ANavigationPoint* point);

	/// ANavigationPoint::RemoveFromNavigationOctree, vtable slot 266 (0x7FF74AE90EF0). Takes
	/// the point and every ReachSpec on its PathList back out.
	///
	/// PostScriptDestroyed calls this itself, so destroying a nav point cleans up after
	/// itself; this is for tearing down a live one, or for editing a PathList that is already
	/// registered.
	void CallRemoveFromNavigationOctree(ANavigationPoint* point);

	/// The pawn size a spec should be built for. ABioScout::CanDoMove looks up "Human" and
	/// falls back to PathSizes(0); retail Default__BioScout ships Common/Large/VeryLarge/Max/
	/// Vehicle and no "Human", so entry 0 is what the cover analysis actually used and it is
	/// what these specs should be sized for too.
	bool GetScoutPathSize(AScout* scout, float& outRadius, float& outHeight);

	/// Drops every spec on `point`'s PathList whose End.Actor satisfies `predicate`, keeping
	/// the octree consistent while it does so.
	///
	/// Needed because nothing in the engine removes INCOMING specs when a nav point is
	/// destroyed -- it unregisters itself and leaves every other point's PathList holding a
	/// dangling End.Actor. Two independently despawnable networks (cover and path nodes) that
	/// reference each other cannot be torn down safely without this.
	///
	/// @returns How many specs were dropped.
	template <typename Predicate>
	int DropSpecsWhere(ANavigationPoint* point, Predicate predicate)
	{
		if (point == nullptr) { return 0; }

		// The octree indexes the specs themselves, so it has to let go of them before the
		// list they live on is rewritten. Re-registration is the caller's business: it knows
		// whether more spec passes are still to come.
		const bool bWasRegistered = point->NavOctreeObject.OctreeNode != nullptr;
		if (bWasRegistered)
		{
			CallRemoveFromNavigationOctree(point);
		}

		int dropped = 0;
		for (int i = static_cast<int>(point->PathList.Count()) - 1; i >= 0; i--)
		{
			auto* const spec = point->PathList(i);
			if (spec == nullptr || predicate(spec->End.Actor))
			{
				point->PathList.Erase(static_cast<unsigned int>(i), 1);
				dropped++;
			}
		}

		if (bWasRegistered)
		{
			CallAddToNavigationOctree(point);
		}
		return dropped;
	}
}
