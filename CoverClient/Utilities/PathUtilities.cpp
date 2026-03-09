
#include "CoverClient/Utilities/PathUtilities.hpp"

namespace CoverClient {
	bool DoesSizeFitAtLocation(PathfindingNode* Nav, AActor* player, int Radius, int Height) {
		FVector HitLocation;
		FVector HitNormal;
		FTraceHitInfo HitInfo;

		// Extent is a vector representing the "half-sizes" of your box.
		// X and Y are the Radius, Z is the Height.
		FVector Extent{ (float)Radius / 2, (float)Radius / 2, (float)Height / 2 };

		// Trace from the location to the exact same location.
		// The 'true' bool tells it to trace against actors, not just the world.
		auto HitActor = player->Trace(Nav->position, Nav->position, FALSE, Extent, 0, &HitLocation, &HitNormal, &HitInfo);

		// If HitActor is None, nothing was hit, meaning the space is clear.
		return HitActor == nullptr;
	}
}