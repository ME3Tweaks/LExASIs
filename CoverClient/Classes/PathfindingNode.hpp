#pragma once

#include <LESDK/Headers.hpp>
#include <vector>

namespace CoverClient {

	/// <summary>
	/// Describes a single pathfinding node in the level
	/// </summary>
	struct PathfindingNode {
		FVector position;
		INT approximateLineOfFire;
		FVector collisionCylinderSize; // radius is X and Y, height is Z

		// Reachability entry includes target node index and the cylinder extent
		// (radius stored in X, height stored in Z)
		struct ReachSpec {
			int index;
			float radius;
			float height;
		};

		std::vector<ReachSpec> reachableIndices;
	};

	// Collision cylinder sizes stored as FVector(radius, height, 0)
	// Sorted by radius (desc) then height (desc)
	extern const std::vector<FVector> CollisionCylinderSizes;

	void ComputeApproximateLineOfFire(PathfindingNode* Nav, AActor* player);
	void SizeNode(PathfindingNode* Nav, AActor* player);
}