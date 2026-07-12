#pragma once
#include "LESDK/Headers.hpp"
#include "CoverClient/Classes/NavigationPoint.hpp"

namespace CoverClient {

	/// The distance in Unreal units that we should separate pathfinding nodes when flood filling the pathfinding network.
	const float NODE_SEPARATION_DISTANCE = 280;
	const float MAX_PATHFINDING_NETWORK_DISTANCE = 800 * 200;

	/// The height above a point from which we cast downward to find the floor.
	const float FLOOR_TRACE_HEIGHT = 500.0f;

	/// How far down we trace to find a floor below a candidate point.
	const float FLOOR_TRACE_DEPTH = 1000.0f;

	// Rejected nodes from the last flood fill, grouped by rejection reason.
	extern std::vector<FVector> rejectedNodesNoFloor;
	extern std::vector<FVector> rejectedNodesSteepSlope;
	extern std::vector<FVector> rejectedNodesPathBlocked;

	void FloodFillPathfindingNetwork();
	void ComputeReachSpecs();
}