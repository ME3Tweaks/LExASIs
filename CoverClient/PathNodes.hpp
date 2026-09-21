#pragma once

#include <LESDK/Headers.hpp>

#include <vector>

namespace CoverClient
{
	// ! Path nodes -- roadmap step 3, and step 5 with it
	// ========================================
	//
	// Cover tells the AI where to shoot from. Path nodes are how it gets there, and how
	// everything computed against the nav network (DangerNavs, roadmap step 4) has anything to
	// be computed against: BuildOtherLinks literally iterates nextNavigationPoint, so with only
	// our own cover markers in the world every answer would be measured against an incomplete
	// level.
	//
	// This replaces FloodFillPathfindingNetwork's client-side structs on a fixed 280u
	// world-aligned grid with real APathNode actors placed by the same floor sampler cc.sweep
	// uses. Three separate reasons the old one had to go, all recorded in note 04:
	//
	//   * a world-aligned grid cannot follow a ramp ("struggles on slopes");
	//   * its visited key had no Z component, so the two storeys of a room merged;
	//   * client-side structs are invisible to the running game, which is the whole point of
	//     generating cover you can walk into and test.
	//
	// Shipped levels have SIX TIMES as many cover slots as path nodes (132 vs 21 in
	// BioD_CerMir_125Land): in ME3 the cover network IS the navigation network and path nodes
	// fill the gaps between it. So this pass deliberately refuses to place a node anywhere the
	// nav network already reaches -- cover markers included -- rather than blanketing the floor
	// and burying the cover in grid points.

	/// cc.pathnodes [radius] [spacing] -- spawn a path-node network around the player.
	///
	/// Run it AFTER cc.sweep. The dedupe against existing nav points is what keeps cover
	/// primary, and it can only see the cover that already exists.
	///
	/// Passes, in order, because the order is load-bearing:
	///   1. sample the floor (shared with cc.sweep -- same question, same answer)
	///   2. drop samples the nav network already reaches
	///   3. spawn the APathNodes
	///   4. node <-> node ReachSpecs
	///   5. node <-> cover marker ReachSpecs (roadmap step 5: without these the AI can stand
	///      in cover but cannot path TO it)
	///   6. register everything in the navigation octree
	///
	/// Six is last because AddToNavigationOctree registers a point's PathList along with the
	/// point. A spec created afterwards is never registered and the move stays invisible to
	/// every radius query -- the same trap that made cover slips do nothing.
	///
	/// @param radius   World units around the player. <= 0 uses the default (2048).
	/// @param spacing  Distance between nodes. <= 0 uses the CoverLink CDO's
	///                 AutoCoverSlotInterval, which is what cc.sweep samples at.
	/// @param maxNodes Budget. <= 0 derives one from the radius.
	void CmdBuildPathNodes(float radius, float spacing, int maxNodes);

	/// cc.seed [here|player] -- where cc.sweep and cc.pathnodes start their fill.
	///
	/// With no argument it reports the current origin. `here` pins it to where the player is
	/// standing and prints the literal to paste into FloorSampler.cpp; `player` turns pinning
	/// off and goes back to seeding wherever the player happens to be.
	///
	/// Exists because the placement rules are order-dependent, so the seed decides the whole
	/// network -- two runs from two spots are not comparable, and a fix cannot be told apart
	/// from a different starting point.
	void CmdSeed(bool bHasArgument, bool bPinHere);

	/// cc.floorcheck [spacing] -- why the fill will or will not leave where you are standing.
	///
	/// Stand on the thing that is not being covered -- the top of the ramp, the doorway -- and
	/// run it. It repeats the sampler's own neighbour test in all eight directions and prints
	/// the floor height profile of each, with the sub-step any failure happened at.
	///
	/// Exists because "the fill does not go down the ramp" has at least four causes that look
	/// identical from the finished node layout, and reading them backwards off that layout is
	/// how four theories in a row turned out wrong. This asks the sampler directly.
	void CmdFloorCheck(float spacing);

	/// cc.despawnpathnodes -- destroys every path node this session made, and takes the specs
	/// pointing at them off our cover markers first.
	void CmdDespawnPathNodes();

	/// The path nodes cc.pathnodes created this session, in spawn order.
	const std::vector<APathNode*>& GetSpawnedPathNodes();

	/// Drops every spec on our path nodes that does NOT lead to another of our path nodes --
	/// i.e. every node-to-cover spec.
	///
	/// cc.despawnlinks destroys the cover markers, and nothing in the engine removes INCOMING
	/// specs when a nav point is destroyed. Without this the path network is left holding
	/// dangling End.Actors that the octree still indexes.
	///
	/// @returns How many specs were dropped.
	int DropPathNodeSpecsToCover();
}
