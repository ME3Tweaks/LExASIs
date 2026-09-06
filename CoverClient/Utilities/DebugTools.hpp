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

	/// Set by cc.spawnlink and toggled by cc.drawlinks. Lives here rather than in Hooks.cpp
	/// with the other toggles because CmdSpawnLink turns it on, and Hooks.cpp is downstream
	/// of both.
	/// Hands the overlay the HUD Canvas for the frame being drawn, and takes it back when the
	/// frame ends. Only valid inside PostRender.
	void SetHudCanvas(UCanvas* canvas);

	extern bool DrawSpawnedLinks;
	/// Write the placing branch above each slot in the world. Detail radius only, off by
	/// default -- text costs far more than a box.
	///
	/// Drawn on the Canvas ourselves rather than through Engine.Actor.DrawDebugString. That
	/// function does not draw: it files the string on the PlayerController's DebugTextList,
	/// which is only painted by AHUD::DrawDebugTextList -- neither BioHUD nor SFXHUDMP calls
	/// it, and it does not appear in the executable at all.
	extern bool DrawSlotOriginLabels;

	/// How far from the player spawned links are drawn, in world units. Set by
	/// `cc.drawlinks <radius>`.
	///
	/// This has its own value rather than sharing NODE_VISUALIZE_CLIP_DISTANCE because the two
	/// overlays have very different densities: a path node is one cube, a cover slot is a
	/// pillar, a facing stub, lean stubs, a pop-up star, a chain segment, a mantle line and a
	/// marker cube -- around thirty batched lines. cc.sweep can make hundreds of slots in one
	/// command, and at the shared 2500u clip that is tens of thousands of lines every
	/// PostRender.
	extern float SpawnedLinkDrawDistance;

	/// Draws every ACoverLink cc.spawnlink created this session: a pillar per slot coloured
	/// and sized by cover type, the facing each slot took, the order the placement walk laid
	/// them down in, and the mantle links between them.
	void DrawSpawnedCoverLinks();
}