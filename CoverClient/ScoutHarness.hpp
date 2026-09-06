#pragma once

#include <LESDK/Headers.hpp>
#include <LESDK/Init.hpp>
#include "CoverClient/CoverEdges.hpp"

#include <vector>

namespace CoverClient
{
	// ! Scout harness
	// ========================================
	//
	// LE3 ships with the whole per-slot cover analysis intact -- ACoverLink::AutoAdjustSlot,
	// BuildSlotInfo/BuildSlotInfoInternal, and ABioScout::CanDoMove (mantle / climb-up /
	// cover-slip / swat-turn / cover-turn). What was compiled out is FPathBuilder::GetScout's
	// "create one if there isn't one" path: it was reduced to a plain read of a global, and
	// the only writer left is FPathBuilder::DestroyScout, which zeroes it.
	//
	// Worse, AutoAdjustSlot calls DestroyScout() on the way out, so even a scout we poke into
	// the global would not survive a single call.
	//
	// So we supply our own scout and hook both functions: GetScout returns ours, and
	// DestroyScout becomes a no-op, for as long as the harness holds the scout.
	//
	// See Notes/02-le3-native-cover-functions.md for the addresses and the reasoning.

	/// Installs the FPathBuilder::GetScout / DestroyScout hooks. Call once from InitializeHooks.
	void InitializeScoutHooks(::LESDK::Initializer& Init);

	/// Spawns the harness scout if it does not exist yet, and takes ownership of
	/// FPathBuilder's scout global. Safe to call repeatedly.
	/// @returns The harness scout, or nullptr if one could not be created.
	AScout* AcquireScout();

	/// Releases the harness scout, letting the engine tear it down again.
	void ReleaseScout();

	/// Whether the harness currently owns the scout (and is therefore suppressing DestroyScout).
	bool IsScoutHeld();

	/// RAII guard around AcquireScout / ReleaseScout, so an early return or a throw inside a
	/// build cannot leave DestroyScout permanently stubbed out.
	class ScopedScout final
	{
	public:
		ScopedScout();
		~ScopedScout();

		ScopedScout(ScopedScout const&) = delete;
		ScopedScout& operator=(ScopedScout const&) = delete;

		AScout* Get() const { return Scout; }
		explicit operator bool() const { return Scout != nullptr; }

	private:
		AScout* Scout{ nullptr };
		bool    bOwnsRelease{ false };
	};

	// ! Spike commands
	// ========================================

	/// cc.testscout -- spawns the scout and dumps everything we need to know about it:
	/// whether it spawned at all, its class, its collision size, and its PathSizes table.
	/// The PathSizes dump answers the open question of whether a "Human" entry exists --
	/// ABioScout's CanDoMove leaves look it up by that name and fall back to PathSizes(0),
	/// which would fault on an empty array.
	void CmdTestScout();

	/// cc.testbuildslotinfo -- calls ACoverLink::BuildSlotInfo directly through the vtable on
	/// the nearest CoverLink, bypassing the bAutoAdjust gate that makes cc.testautoadjust skip
	/// it on shipped links. This is the only way to actually exercise ABioScout::CanDoMove and
	/// therefore the mantle / climb-up / cover-slip / swat-turn / cover-turn analysis.
	///
	/// WARNING: this mutates the live level's cover data. Reload the level afterwards.
	void CmdTestBuildSlotInfo();

	/// cc.spawnlink -- Phase 1 step 2. Traces from the camera, spawns a REAL ACoverLink the way
	/// ACoverLink::EditorAutoSetup did, adds slots along the surface, sets bAutoAdjust so the
	/// engine's full pass actually runs, then calls AutoAdjustSlot on every slot and reports
	/// what the game decided.
	///
	/// Slot placement here is deliberately crude -- this spike tests the engine's *analysis*,
	/// not our placement. AutoAdjustSlot re-seats and re-orients each slot anyway, which is
	/// exactly how the editor worked.
	void CmdSpawnLink();

	/// cc.buildlinks -- Phase 1 step 3. Re-runs BuildSlotInfo across every slot of every
	/// spawned link, now that all of them exist and are spliced into the world's cover list.
	///
	/// ABioScout::CanMantle searches that list for a landing slot, so a slot analysed before
	/// its opposite number was placed was analysed against an incomplete world. The editor
	/// avoided this by building every link before running the per-slot pass. Reports which
	/// slots gained, lost, or re-pointed their MantleTarget.
	void CmdBuildLinks();

	/// cc.sweep [radius] -- automatic cover discovery. Walks the floor outward from the player
	/// to find where a person could stand, fans traces from each such place to find the
	/// vertical faces, discards the ones already covered by our links or the level's own, and
	/// runs the ordinary cc.spawnlink pipeline on the rest -- then cc.buildlinks over the lot.
	///
	/// This works only because FindCoverEdges converges: seeds 343u apart on one face produce
	/// byte-identical slots, so the sweep never has to find the middle or the ends of a wall.
	/// It needs enough seeds, not good ones.
	///
	/// @param radius     Sweep radius in world units. <= 0 uses the default (2048). There is
	///                   no upper limit -- a whole-level sweep is a supported thing to ask for
	///                   -- but the whole command runs inside one Exec call, so the game is
	///                   frozen for the duration and the log carries the progress.
	/// @param maxSamples Floor sample budget. <= 0 derives it from the radius (one per grid
	///                   cell inside it, floored at 400).
	/// @param maxLinks   Link budget. <= 0 means one per sample, which in practice never binds
	///                   because the dedupe limits links long before any cap does.
	void CmdSweep(float radius, int maxSamples, int maxLinks);

	/// The links cc.spawnlink created this session, in spawn order. Exposed so DebugTools can
	/// draw them -- without a visualisation there is no way to judge whether a placement is
	/// any good, and no way to notice you have seeded the same wall twice.
	const std::vector<ACoverLink*>& GetSpawnedLinks();

	/// cc.despawnlinks -- destroys every ACoverLink cc.spawnlink created this session, so the
	/// level can be iterated on without accumulating junk cover. Unlinks each one from the
	/// world's nav/cover chains first, or those chains would keep dangling pointers.
	/// cc.enemyspawnpoint [count] -- puts real SFXEnemySpawnPoint actors on the `count` cover
	/// slots nearest the player, out of the links cc.spawnlink / cc.sweep made. Default 1.
	///
	/// Everything verified so far has been verified by the PLAYER walking into a slot, which
	/// only exercises the player's cover acquisition. Getting an enemy onto generated cover is
	/// what tests the AI path -- FindPlayerCover, the octree query, the slot-to-slot specs.
	///
	/// This places the points and registers them; it does not spawn anyone. Whatever normally
	/// drives enemy spawning still has to reference them.
	/// The walk's record for a link the harness built, or null if it has none.
	const FindCoverEdgesResult* GetLinkEdges(ACoverLink* link);
	const SlotPlacement* GetSlotPlacement(ACoverLink* link, int slotIdx);

	void CmdSpawnEnemySpawnPoints(int count);

	/// cc.despawnenemyspawnpoints -- removes what the above made.
	void CmdDespawnEnemySpawnPoints();

	/// cc.explainlink -- dumps the walk's own account of the link nearest the player: every
	/// slot, the branch that placed it, where it was put versus where AutoAdjustSlot seated
	/// it, the yaw it was given versus the yaw it ended up with, and whether it survived.
	///
	/// This exists because the alternative was re-running a whole sweep with a new log line
	/// added every time a screenshot showed something wrong. Point at the bad link and ask.
	/// `index` is the L number the in-world labels show; pass -1 to pick by aim instead.
	/// `index` is the spawn-list index (the L number). `objectNumber`, when >= 0, is the
	/// game's own CoverLink_N number and wins over `index`. Both -1 means pick by aim.
	void CmdExplainLink(int index, int objectNumber = -1);

	/// Reports one link, however it was chosen.
	void ExplainOneLink(ACoverLink* link, int linkIndex);

	void CmdDespawnLinks();

	/// cc.covercheck -- diagnostic for "the cover draws but I cannot enter it". Reports
	/// whether each spawned link is threaded onto WorldInfo.CoverList and each marker onto
	/// NavigationPointList (Spawn puts an actor in Level->Actors and nothing else), and runs
	/// the engine's own ACoverLink::FindSlots against the player's position to see whether the
	/// slots themselves are acceptable.
	/// The number the GAME shows for a link -- 12 for "CoverLink_12", -1 if unparseable.
	///
	/// This exists because the console's own `profile cover self` names cover as
	/// "Cover Link: CoverLink_12 / Cover Slot: CoverSlotMarker_57", while our overlay and
	/// cc.explainlink used the spawn-list index. Two numbering schemes for the same object,
	/// neither convertible to the other by eye, and aiming was the only bridge between them.
	int LinkObjectNumber(ACoverLink* link);

	/// The spawn-list index of a link, or -1 if it is not one of ours.
	int LinkSpawnIndex(ACoverLink* link);

	void CmdBuildSlips();
	/// cc.slips [radius] -- every edge slot near the player and what it connects to.
	///
	/// Exists because identifying the right link to interrogate was the bottleneck: aiming
	/// picks the wrong thing in dense cover, and a corner is exactly where several links are
	/// close together. This needs no aim and no link number -- it prints every edge slot within
	/// the radius, ordered nearest first, with its slip or the reason it has none.
	void CmdSlips(float radius);
	/// Clears every SlipTarget this session created, so cc.buildslips can be re-run.
	void CmdClearSlips();

	void CmdCoverCheck();

	/// cc.testautoadjust -- the real proof. Finds the CoverLink nearest the player, snapshots
	/// every slot's analysis flags, runs the game's own AutoAdjustSlot over them, and reports
	/// what changed. Because the target is cover BioWare themselves built and shipped, an
	/// empty diff means our invocation reproduces the editor's output exactly.
	void CmdTestAutoAdjust();
}
