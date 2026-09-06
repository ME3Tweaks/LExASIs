#pragma once

namespace CoverClient
{
	// ! Engine actors -> client model
	// ========================================
	//
	// Two representations of cover exist in this client and they had never met.
	//
	//   * The SCOUT HARNESS builds real engine actors: ACoverLink, ACoverSlotMarker,
	//     UReachSpec. That is what makes cover enterable in a live session, and it is what
	//     cc.spawnlink and cc.sweep produce.
	//   * The CLIENT MODEL (CoverClient::CoverLink / CoverSlotMarker / NavigationPoint, owned
	//     by allNetworkNodes) is what ComputeFireLinks / ComputeExposedCover /
	//     ComputeDangerNavs operate on, and what WriteNodeToHost pipes to CoverHost.
	//
	// Everything downstream of the model is already written -- the compute passes, the
	// per-slot wire format, the whole FCoverSlot serialisation. What was missing was the step
	// that turns generated actors into model nodes. That is this file.
	//
	// The import COPIES each FCoverSlot rather than pointing at engine memory, even though
	// CoverSlotMarker::slot is a bare pointer and pointing would be one line. Three reasons,
	// all of which have bitten something already:
	//   1. cc.despawnlinks destroys the actors. A model holding pointers into a destroyed
	//      actor's Slots array is a use-after-free at send time.
	//   2. CoverLink::WriteNodeToHost WRITES LocationOffset and RotationOffset back through
	//      that pointer. Pointing at engine memory means sending to the host mutates the live
	//      level's cover.
	//   3. The compute passes append to ExposedCoverPackedProperties / DangerCover... A shallow
	//      struct copy would alias the engine's TArray Data pointer and hand two owners to one
	//      allocation.

	/// cc.importlinks -- copies every ACoverLink cc.spawnlink / cc.sweep created this session
	/// into the client model, so the compute passes and cc.sendtohost can see them.
	///
	/// Appends rather than clearing: cc.buildpathfinding puts path nodes in the same list, and
	/// a network needs both. Links already imported are skipped, so it is safe to re-run after
	/// another sweep.
	void CmdImportLinks();
}
