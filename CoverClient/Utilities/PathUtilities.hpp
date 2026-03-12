#pragma once

#include "CoverClient/Classes/NavigationPoint.hpp"
#include <LESDK/Headers.hpp>
#include <memory>

namespace CoverClient {
	// Storage for all nodes on the pathfinding network. Stores pointers so derived types are preserved
	extern std::vector<std::unique_ptr<NavigationPoint>> allNetworkNodes;

	bool DoesSizeFitAtLocation(NavigationPoint* Nav, int Radius, int Height);
	FRotator FVectorToFRotator(const FVector& v);
	FVector CalculateLocationOffset(FVector MyLoc, FRotator MyRot, FVector TargetLoc);
}