#pragma once 

#include "Common/Base.hpp"
#include <LESDK/Headers.hpp>
#include <vector>
#include "CoverClient/Classes/ReachSpecs.hpp"

namespace CoverClient {
	// Collision cylinder sizes stored as FVector(radius, height, 0)
	// Sorted by radius (desc) then height (desc)
	extern const std::vector<FVector> CollisionCylinderSizes;

	/// <summary>
	/// Describes a single pathfinding node in the level. This is the base for all other node types.
	/// </summary>
	class NavigationPoint {
	public:
		FVector position;
		FRotator rotation;
		INT approximateLineOfFire; // LE3 only.
		FVector collisionCylinderSize; // radius is X and Y, height is Z
		std::vector<ReachSpec> reachableIndices; // todo: Make this a vector of unique pointers so we don't object slice when we have derived classes with additional data.
		int networkIndex = -1; // index into allNetworkNodes.
	
		NavigationPoint(FVector position, FRotator rotation) {
			this->position = position;
			this->rotation = rotation;

			// Defaults.
			this->approximateLineOfFire = 0;
			this->collisionCylinderSize = FVector(0.0f, 0.0f, 0.0f);
		}

		void ComputeApproximateLineOfFire();
		void SizeNode();
		virtual void WriteNodeToHost() const;
		virtual const char* GetNavName() const;
	};

	int QuantizeDistance(float Dist);
}
