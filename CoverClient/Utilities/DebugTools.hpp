#pragma once
#include "LESDK/Headers.hpp"

namespace CoverClient {
	// The distance to clip debug visuals at, in Unreal units. This is used to avoid drawing debug visuals too far from the player, which can cause performance issues.
	const float NODE_VISUALIZE_CLIP_DISTANCE = 2500.f;

	void DrawDebugLine(const FVector& start, const FVector& end, const FLinearColor& color, const float thickness, const bool persistent);
	void DrawDebugCube(AActor* actor, FVector location, float size, BYTE r, BYTE g, BYTE b, float distanceClip);
	void DrawGeneratedPathfinding();
	void DrawDebugTraces();
	void DrawGameFireLinksForCurrentSlot();
	void DrawClientFireLinksForCurrentSlot();
	void DrawClientDangerNavsForCurrentSlot();
	void DrawClientExposedCoverForCurrentSlot();
	void DrawInteractionPositions();
}