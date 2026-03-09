#pragma once
#include <LESDK/Headers.hpp>
#include "CoverClient/Classes/PathfindingNode.hpp"

#define PRINT_DEBUG_INFO false

namespace CoverClient
{
	enum class CoverHostCommand : BYTE
	{
		PathfindingNodeList = 1,
		LevelName = 2,
	};
	extern std::wstring currentAction;

	// List of found pathfinding nodes.
	extern std::vector<PathfindingNode> nodes;

	// Rejected nodes from the last flood fill, grouped by rejection reason.
	extern std::vector<FVector> rejectedNodesNoFloor;
	extern std::vector<FVector> rejectedNodesSteepSlope;
	extern std::vector<FVector> rejectedNodesPathBlocked;
	void UpdateCurrentAction(std::wstring_view newAction);
	void FloodFillPathfindingNetwork();
	void ComputeReachSpecs();
	void SendDataToCoverHost();
	void DrawDebugTraces();
	void DrawGeneratedPathfinding();
	void DrawDebugCube(AActor* actor, FVector location, float size, BYTE r, BYTE g, BYTE b, float distanceClip = 0.f);
}