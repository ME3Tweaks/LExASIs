#include "CoverClient/Utilities/DebugTools.hpp"
#include "CoverClient/PathfindingGeneration.hpp"
#include "CoverClient/CoverGeneration.hpp"
#include "CoverClient/Utilities/PathUtilities.hpp"	
#include "CoverClient/ScoutHarness.hpp"
#include "Common/Objects.hpp"

#include <cmath>
#include <cstdio>
#include <cwchar>
#include <vector>

namespace CoverClient
{

	void DrawDebugLine(const FVector& start, const FVector& end, const FLinearColor& color, const float thickness, const bool persistent)
	{
		ULineBatchComponent* lineBatcher = persistent ? (*GWorld)->PersistentLineBatcher : (*GWorld)->LineBatcher;
		lineBatcher->FPrimitiveDrawInterfaceVfTable->DrawLine(reinterpret_cast<BYTE*>(lineBatcher) + offsetof(ULineBatchComponent, FPrimitiveDrawInterfaceVfTable), start, end, color, 1, thickness);
	}

	void DrawDebugCube(AActor* actor, FVector location, float size, BYTE r, BYTE g, BYTE b, float distanceClip)
	{
		if (distanceClip > 0 && actor != nullptr) {
			// Compute squared distance from the actor to the target location and
			// skip drawing if it's farther than distanceClip. Use squared
			// comparison to avoid the cost of a sqrt.
			FVector delta = actor->LOCATION - location;
			float distSq = delta.X * delta.X + delta.Y * delta.Y + delta.Z * delta.Z;
			float clipSq = distanceClip * distanceClip;
			if (distSq > clipSq) {
				return;
			}
		}

		FVector extent{ size,size,size };
		actor->DrawDebugBox(location, extent, r, g, b, false);
	}

	void DrawGeneratedPathfinding() {
		if (allNetworkNodes.size() == 0) {
			return;
		}

		auto engine = ::Common::FindFirstObject<USFXEngine, true>();
		if (!engine) {
			return;
		}

		auto wi = static_cast<ABioWorldInfo*>(engine->GetCurrentWorldInfo());
		auto playerController = wi->GetALocalPlayerController();
		if (playerController == nullptr) {
			return;
		}

		auto player = playerController->Pawn;
		if (player == nullptr) {
			return; // Do nothing.
		}

		for (std::size_t i = 0; i < allNetworkNodes.size(); i++) {
			DrawDebugCube(player, allNetworkNodes[i]->position, 5.0f, 0, 255, 0, NODE_VISUALIZE_CLIP_DISTANCE);
		}

		for (std::size_t i = 0; i < rejectedNodesNoFloor.size(); i++) {
			DrawDebugCube(player, rejectedNodesNoFloor[i], 5.0f, 255, 0, 0, NODE_VISUALIZE_CLIP_DISTANCE);      // Red: no floor below
		}

		for (std::size_t i = 0; i < rejectedNodesSteepSlope.size(); i++) {
			DrawDebugCube(player, rejectedNodesSteepSlope[i], 5.0f, 255, 255, 0, NODE_VISUALIZE_CLIP_DISTANCE); // Yellow: slope too steep
		}

		for (std::size_t i = 0; i < rejectedNodesPathBlocked.size(); i++) {
			DrawDebugCube(player, rejectedNodesPathBlocked[i], 5.0f, 0, 0, 255, NODE_VISUALIZE_CLIP_DISTANCE);  // Blue: path blocked
		}
	}

	ASFXPawn_Player* GetPlayer() {
		auto engine = ::Common::FindFirstObject<USFXEngine, true>();
		if (!engine) {
			LEASI_INFO("No engine found");
			return nullptr;
		}

		auto wi = static_cast<ABioWorldInfo*>(engine->GetCurrentWorldInfo());
		auto playerController = wi->GetALocalPlayerController();
		if (playerController == nullptr) {
			LEASI_WARN("No Player.");
			return nullptr;
		}

		return (ASFXPawn_Player*)playerController->Pawn;
	}

	void DrawDebugTraces() {
		static const FLinearColor TraceLineColorGood{ 0.0f, 1.0f, 0.0f, 1.0f };
		static const FLinearColor TraceLineColorBad{ 1.0f, 0.0f, 0.0f, 1.0f };
		auto engine = ::Common::FindFirstObject<USFXEngine, true>();
		if (!engine) {
			LEASI_INFO("FloodFill: No engine found");
			return;
		}

		auto wi = static_cast<ABioWorldInfo*>(engine->GetCurrentWorldInfo());
		auto playerController = wi->GetALocalPlayerController();
		if (playerController == nullptr) {
			LEASI_WARN("No Player.");
			return;
		}

		auto player = playerController->Pawn;
		if (player == nullptr) {
			return; // Do nothing.
		}

		FVector seedLocation = player->LOCATION;

		// 8 cardinal directions on the XY plane
		const FVector directions[8] = {
			{  NODE_SEPARATION_DISTANCE,  0.0f,                     0.0f },  // E
			{ -NODE_SEPARATION_DISTANCE,  0.0f,                     0.0f },  // W
			{  0.0f,                      NODE_SEPARATION_DISTANCE, 0.0f },  // N
			{  0.0f,                     -NODE_SEPARATION_DISTANCE, 0.0f },  // S
			{  NODE_SEPARATION_DISTANCE,  NODE_SEPARATION_DISTANCE, 0.0f },  // NE
			{  NODE_SEPARATION_DISTANCE, -NODE_SEPARATION_DISTANCE, 0.0f },  // SE
			{ -NODE_SEPARATION_DISTANCE,  NODE_SEPARATION_DISTANCE, 0.0f },  // NW
			{ -NODE_SEPARATION_DISTANCE, -NODE_SEPARATION_DISTANCE, 0.0f },  // SW
		};


		for (int i = 0; i < 8; i++) {

			bool isBlocked = false;
			bool isCliff = false;

			FVector candidateEnd = { seedLocation.X + directions[i].X, seedLocation.Y + directions[i].Y, seedLocation.Z + directions[i].Z };

			// Perform trace to see where real end is at.
			FVector hitLocation{};
			FVector hitNormal{};
			auto hitActor = player->Trace(candidateEnd, seedLocation, 0, player->GetCollisionExtent(), TRUE, &hitLocation, &hitNormal, nullptr);

			if (hitActor != nullptr) {
				isBlocked = true;
				// Something blocked the path; mark as visited so we don't retry
				candidateEnd = hitLocation;
			}

			// We also want to do a downwards check if we are not already blocked
			if (!isBlocked) {
				// Compute the horizontal distance from the seed to the candidate. We'll use
				// this as the vertical trace length so we can properly test a 45-degree slope.
				float hx = candidateEnd.X - seedLocation.X;
				float hy = candidateEnd.Y - seedLocation.Y;
				float horizontalDistance = std::sqrt((hx * hx) + (hy * hy));

				// Downward trace that has the same length as the horizontal trace.
				FVector floorTraceStart = { candidateEnd.X, candidateEnd.Y, candidateEnd.Z };
				FVector floorTraceEnd = { candidateEnd.X, candidateEnd.Y, candidateEnd.Z - horizontalDistance };
				FVector floorHitLocation{};
				FVector floorHitNormal{};
				FVector zeroExtent = { 0.0f, 0.0f, 0.0f };
				auto floorHit = player->Trace(floorTraceEnd, floorTraceStart, 0, zeroExtent, TRUE, &floorHitLocation, &floorHitNormal, nullptr);

				if (floorHit == nullptr) {
					// No floor below; 
					isCliff = true;
				}

				if (!isCliff) {
					// Reject large drops/rises -- floor must be within one collision extent in Z (handles ramps, rejects cliffs)
					auto zDiff = std::abs(floorHitLocation.Z - candidateEnd.Z);
					// Compare against the distance between seedLocation and candidateEnd, which means it should be no more than a 45 degree slope.

					if (zDiff > horizontalDistance) {
						isCliff = true;
					}
				}
				DrawDebugLine(floorTraceStart, floorTraceEnd, isCliff ? TraceLineColorBad : TraceLineColorGood, 5.0f, false);
			}

			DrawDebugLine(seedLocation, candidateEnd, (isBlocked || isCliff) ? TraceLineColorBad : TraceLineColorGood, 5.0f, false);
			// Seems that you have to draw a 0-width line or nothing draws.
			DrawDebugLine(seedLocation, candidateEnd, TraceLineColorGood, 0, false);
		}
	}

	void DrawCover() {
		static const FLinearColor coverLinkColor{ 255, 0, 255 };
		auto actor = static_cast<AActor*>(AActor::StaticClass()->ClassDefaultObject);
		for (const auto cl : coverLinks) {
			// Storage for all nodes on the pathfinding network. Includes CoverLink and CoverSlotMarker nodes as well as normal nodes
			actor->DrawDebugPoint(cl->position, 16.0f, coverLinkColor, true);
			for (const auto& slot : cl->slots) {
				actor->DrawDebugStar(slot->position, 12.0f, 255, 0, 0, true);
				actor->DrawDebugLine(cl->position, slot->position, 128, 128, 64, true); // Required or nothing will actually draw
			}
		}
	}

	void DrawGameFireLinksForCurrentSlot() {
		auto player = GetPlayer();
		if (player == nullptr) {
			return; // Do nothing.
		}

		if (!player->IsInCover()) {
			return; // Do nothing.
		}

		if (!player->CurrentLink) {
			return; // Do nothing.
		}

		static const FLinearColor& FireLinkColor{ 255, 0, 0 }; // Game computed

		// This appears to be making a copy
		const auto& slot = player->CurrentLink->Slots.GetData()[player->CurrentSlotIdx];

		// Draw the firelinks from the slot marker to the target using the offsets
		FCoverInfo targetCoverInfo;
		for (UINT i = 0; i < slot.FireLinks.Count(); i++) {
			const auto& fireLink = slot.FireLinks.GetData()[i];
			if (player->CurrentLink->GetFireLinkTargetCoverInfo(player->CurrentSlotIdx, i, 0, &targetCoverInfo)) {
				for (UINT j = 0; j < fireLink.Interactions.Count(); j++) {
					unsigned char srcType;
					unsigned char srcAction;
					unsigned char dstType;
					unsigned char dstAction;
					player->CurrentLink->UnPackFireLinkInteractionInfo(fireLink.Interactions.GetData()[j], &srcType, &srcAction, &dstType, &dstAction);

					FVector srcViewPoint = player->CurrentLink->GetSlotViewPoint(player->CurrentSlotIdx, srcType, srcAction);
					FVector dstViewPoint = targetCoverInfo.Link->GetSlotViewPoint(targetCoverInfo.SlotIdx, dstType, dstAction);
					DrawDebugLine(srcViewPoint, dstViewPoint, FireLinkColor, 2.0f, false);
				}
			}
		}
		if (slot.FireLinks.Count() > 0) {
			// Seems that you have to draw a 0-width line or nothing draws.
			DrawDebugLine(player->location, player->location, FireLinkColor, 0, false);
		}
	}

	void DrawClientFireLinksForCurrentSlot() {
		auto player = GetPlayer();
		if (player == nullptr) {
			return; // Do nothing.
		}

		if (!player->IsInCover()) {
			return; // Do nothing.
		}

		if (!player->CurrentLink) {
			return; // Do nothing.
		}

		static const FLinearColor ComputedFireLinkColor{ 255, 0, 0 };

		// Cache: only recompute when the player moves to a different cover slot
		static ACoverLink* cachedLink = nullptr;
		static int cachedSlotIdx = -1;
		static std::vector<ComputedFireLink> cachedLinks;

		if (player->CurrentLink != cachedLink || player->CurrentSlotIdx != cachedSlotIdx) {
			cachedLink = nullptr;
			cachedSlotIdx = -1;
			cachedLinks.clear();

			// Find the CoverSlotMarker in our data that matches the player's current game slot by position
			const auto& gameSlot = player->CurrentLink->Slots.GetData()[player->CurrentSlotIdx];
			if (!gameSlot.SlotMarker) {
				return;
			}
			const FVector& slotPos = gameSlot.SlotMarker->LOCATION;

			CoverLink* srcLink = nullptr;
			CoverSlotMarker* srcCsm = nullptr;
			for (auto* cl : coverLinks) {
				for (auto* csm : cl->slots) {
					FVector diff = {
						csm->position.X - slotPos.X,
						csm->position.Y - slotPos.Y,
						csm->position.Z - slotPos.Z
					};
					if (diff.X * diff.X + diff.Y * diff.Y + diff.Z * diff.Z < 16.0f) {
						srcLink = cl;
						srcCsm = csm;
						break;
					}
				}
				if (srcCsm) break;
			}

			if (!srcCsm) {
				return; // No matching slot in our cover data
			}

			cachedLinks = ComputeFireLinksForSlot(srcLink, srcCsm, static_cast<AActor*>(player));
			cachedLink = player->CurrentLink;
			cachedSlotIdx = player->CurrentSlotIdx;
		}

		if (cachedLinks.empty()) {
			return;
		}

		for (const auto& fl : cachedLinks) {
			for (const auto& interaction : fl.interactions) {
				DrawDebugLine(interaction.srcFirePos, interaction.dstFirePos, ComputedFireLinkColor, 2.0f, false);
			}
		}
		// Seems that you have to draw a 0-width line or nothing draws.
		DrawDebugLine(player->location, player->location, ComputedFireLinkColor, 0, false);
	}

	void DrawClientExposedCoverForCurrentSlot() {
		auto player = GetPlayer();
		if (player == nullptr) {
			return; // Do nothing.
		}

		if (!player->IsInCover()) {
			return; // Do nothing.
		}

		if (!player->CurrentLink) {
			return; // Do nothing.
		}

		static const FLinearColor DangerNavColor{ 255, 255, 0 };

		// Cache: only redo the slot lookup when the player moves to a different cover slot
		static ACoverLink* cachedLink = nullptr;
		static int cachedSlotIdx = -1;
		static CoverSlotMarker* cachedCsm = nullptr;

		if (player->CurrentLink != cachedLink || player->CurrentSlotIdx != cachedSlotIdx) {
			cachedLink = nullptr;
			cachedSlotIdx = -1;
			cachedCsm = nullptr;

			// Find the CoverSlotMarker in our data that matches the player's current game slot by position
			const auto& gameSlot = player->CurrentLink->Slots.GetData()[player->CurrentSlotIdx];
			if (!gameSlot.SlotMarker) {
				return;
			}
			const FVector& slotPos = gameSlot.SlotMarker->LOCATION;

			for (auto* cl : coverLinks) {
				for (auto* csm : cl->slots) {
					FVector diff = {
						csm->position.X - slotPos.X,
						csm->position.Y - slotPos.Y,
						csm->position.Z - slotPos.Z
					};
					if (diff.X * diff.X + diff.Y * diff.Y + diff.Z * diff.Z < 16.0f) {
						cachedCsm = csm;
						break;
					}
				}
				if (cachedCsm) break;
			}

			if (!cachedCsm) {
				return; // No matching slot in our cover data
			}

			cachedLink = player->CurrentLink;
			cachedSlotIdx = player->CurrentSlotIdx;
		}

		if (!cachedCsm) {
			return;
		}

		auto it = computedExposedCover.find(cachedCsm);
		if (it == computedExposedCover.end() || it->second.empty()) {
			return;
		}

		for (const auto& entry : it->second) {
			DrawDebugLine(cachedCsm->position, entry.srcSlot->position, DangerNavColor, 2.0f, false);
		}
		// Seems that you have to draw a 0-width line or nothing draws.
		DrawDebugLine(player->location, player->location, DangerNavColor, 0, false);
	}

	void DrawClientDangerNavsForCurrentSlot() {
		auto player = GetPlayer();
		if (player == nullptr) {
			return; // Do nothing.
		}

		if (!player->IsInCover()) {
			return; // Do nothing.
		}

		if (!player->CurrentLink) {
			return; // Do nothing.
		}

		static const FLinearColor ExposedCoverColor{ 0, 64, 255 };

		// Cache: only redo the slot lookup when the player moves to a different cover slot
		static ACoverLink* cachedLink = nullptr;
		static int cachedSlotIdx = -1;
		static CoverSlotMarker* cachedCsm = nullptr;

		if (player->CurrentLink != cachedLink || player->CurrentSlotIdx != cachedSlotIdx) {
			cachedLink = nullptr;
			cachedSlotIdx = -1;
			cachedCsm = nullptr;

			// Find the CoverSlotMarker in our data that matches the player's current game slot by position
			const auto& gameSlot = player->CurrentLink->Slots.GetData()[player->CurrentSlotIdx];
			if (!gameSlot.SlotMarker) {
				return;
			}
			const FVector& slotPos = gameSlot.SlotMarker->LOCATION;

			for (auto* cl : coverLinks) {
				for (auto* csm : cl->slots) {
					FVector diff = {
						csm->position.X - slotPos.X,
						csm->position.Y - slotPos.Y,
						csm->position.Z - slotPos.Z
					};
					if (diff.X * diff.X + diff.Y * diff.Y + diff.Z * diff.Z < 16.0f) {
						cachedCsm = csm;
						break;
					}
				}
				if (cachedCsm) break;
			}

			if (!cachedCsm) {
				return; // No matching slot in our cover data
			}

			cachedLink = player->CurrentLink;
			cachedSlotIdx = player->CurrentSlotIdx;
		}

		if (!cachedCsm) {
			return;
		}

		auto it = computedDangerNavs.find(cachedCsm);
		if (it == computedDangerNavs.end() || it->second.empty()) {
			return;
		}

		for (const auto& entry : it->second) {
			DrawDebugLine(cachedCsm->position, entry.node->position, ExposedCoverColor, 2.0f, false);
		}
		// Seems that you have to draw a 0-width line or nothing draws.
		DrawDebugLine(player->location, player->location, ExposedCoverColor, 0, false);
	}

	void DrawInteractionPositions() {
		auto player = GetPlayer();
		if (player == nullptr) {
			return;
		}

		if (!player->IsInCover()) {
			return;
		}

		if (!player->CurrentLink) {
			return;
		}

		// Look up matching CoverSlotMarker
		const auto& gameSlot = player->CurrentLink->Slots.GetData()[player->CurrentSlotIdx];
		if (!gameSlot.SlotMarker) {
			return;
		}

		auto actor = static_cast<AActor*>(AActor::StaticClass()->ClassDefaultObject);

		// CA_LeanLeft=3, CA_LeanRight=4, CA_PopUp=5
		if (gameSlot.bLeanLeft) {
			FVector leanLeftPos = ComputeFirePosition(player->CurrentLink, player->CurrentSlotIdx, 3);
			actor->DrawDebugStar(leanLeftPos, 12, 255, 0, 0, false);
		}

		if (gameSlot.bLeanRight) {
			FVector leanRightPos = ComputeFirePosition(player->CurrentLink, player->CurrentSlotIdx, 4);
			actor->DrawDebugStar(leanRightPos, 12, 0, 255, 0, false);
		}

		if (gameSlot.bCanPopUp) {
			FVector popUpPos = ComputeFirePosition(player->CurrentLink, player->CurrentSlotIdx, 5);
			actor->DrawDebugStar(popUpPos, 12, 0, 0, 255, false);
		}

		// Seems that you have to draw a 0-width line or nothing draws.
		DrawDebugLine(player->location, player->location, FLinearColor{ 255, 128, 0, 1 }, 0, false);
	}

	// ! Spawned cover link overlay -- cc.drawlinks
	// ========================================
	//
	// A generated CoverLink is invisible: it has no mesh, and in-game AddCoverSlot never
	// creates the CoverSlotMarker actors the editor drew its cover sprites from. So there was
	// no way to see what placement produced, no way to judge whether it feels right, and no
	// way to notice you had seeded the same wall twice.

	bool DrawSpawnedLinks = true;
	bool DrawSlotOriginLabels = false;
	float SpawnedLinkDrawDistance = 1024.0f;

	namespace
	{
		/// Cover-type constants, matching ECoverType. CT_None means AutoAdjustSlot could not
		/// decide what the slot was standing against -- worth seeing, so it gets its own colour
		/// rather than being skipped.
		constexpr unsigned char k_ctNone = 0;
		constexpr unsigned char k_ctStanding = 1;
		constexpr unsigned char k_ctMidLevel = 2;

		/// The HUD's Canvas for the frame currently being drawn, or null outside PostRender.
		///
		/// Text is the one thing the debug drawing helpers cannot do. DrawDebugBox and friends
		/// queue into the engine's line batcher, which the renderer draws on its own;
		/// DrawDebugString instead files the string on the PlayerController's DebugTextList,
		/// and that list is only painted by AHUD::DrawDebugTextList -- which neither BioHUD nor
		/// SFXHUDMP calls, and which does not appear anywhere in the executable. So the strings
		/// were reaching the engine correctly and simply had nobody to paint them.
		///
		/// Drawing them ourselves needs the Canvas, and the Canvas only exists during
		/// PostRender, which is why it is handed in per frame rather than cached.
		UCanvas* g_HudCanvas = nullptr;

		/// A screen rectangle a label has already claimed this frame.
		struct LabelRect
		{
			float X0, Y0, X1, Y1;
		};

		/// Every label drawn so far this frame.
		///
		/// Coincident slots are common -- a degenerate link is two slots in the same place by
		/// definition, and 39 links in the last sweep were exactly that -- so their labels land
		/// on the same pixels and render as unreadable overstrike. Staggering by a formula over
		/// the link and slot indices does not fix it: any such formula is periodic, and two
		/// slots that happen to land on the same period still overstrike. Instead each label is
		/// measured and pushed down past whatever is already occupying that space, which is
		/// exact however many labels pile up and whatever their indices are.
		///
		/// Screen space rather than world space, because a world-space offset collapses again
		/// as soon as the camera looks along the axis it was offset on.
		std::vector<LabelRect> g_LabelRects;

		/// How far one push moves a label down, in pixels. Slightly more than the text height
		/// so consecutive rows are separated by a visible gap rather than touching.
		constexpr float k_labelRowHeight = 13.0f;
		/// A ceiling on pushes, so a pile of labels cannot walk one of them down the screen
		/// far enough to be misread as belonging to something else.
		constexpr int k_maxLabelPushes = 16;

		/// Draws text at a world position, or does nothing if it is off screen or behind us.
		void DrawWorldLabel(const FVector& at, const wchar_t* text,
			unsigned char r, unsigned char g, unsigned char b)
		{
			if (g_HudCanvas == nullptr || text == nullptr) { return; }

			// Project returns screen X/Y with the depth in Z. Behind the camera comes back
			// with a non-positive Z and screen coordinates that are mirrored nonsense, so the
			// depth test has to come first.
			const FVector screen = g_HudCanvas->Project(at);
			if (screen.Z <= 0.0f) { return; }
			if (screen.X < 0.0f || screen.Y < 0.0f
				|| screen.X > static_cast<float>(g_HudCanvas->SizeX)
				|| screen.Y > static_cast<float>(g_HudCanvas->SizeY))
			{
				return;
			}

			// Ask the Canvas for the real extent rather than guessing from the character
			// count: the labels are proportional text, so "L3 s0 RightEdge" and
			// "L37 s11 TransitionMidToStand" differ in width by more than their length ratio.
			const FString measured{ text };
			float width = 0.0f;
			float height = 0.0f;
			g_HudCanvas->StrLen(measured, &width, &height);
			if (width <= 0.0f) { width = 8.0f * static_cast<float>(std::wcslen(text)); }
			if (height <= 0.0f) { height = k_labelRowHeight; }

			LabelRect rect{ screen.X, screen.Y, screen.X + width, screen.Y + height };
			for (int push = 0; push < k_maxLabelPushes; push++)
			{
				bool bClear = true;
				for (const LabelRect& taken : g_LabelRects)
				{
					if (rect.X0 < taken.X1 && taken.X0 < rect.X1
						&& rect.Y0 < taken.Y1 && taken.Y0 < rect.Y1)
					{
						bClear = false;
						break;
					}
				}
				if (bClear) { break; }
				rect.Y0 += k_labelRowHeight;
				rect.Y1 += k_labelRowHeight;
			}
			g_LabelRects.push_back(rect);

			g_HudCanvas->CurX = rect.X0;
			g_HudCanvas->CurY = rect.Y0;
			g_HudCanvas->DrawColor = FColor{ b, g, r, 255 };
			g_HudCanvas->DrawTextScaled(measured, 1.0f, 1.0f);
		}

		FVector AddScaled(const FVector& base, const FVector& dir, float scale)
		{
			return { base.X + dir.X * scale, base.Y + dir.Y * scale, base.Z + dir.Z * scale };
		}

		/// FRotationMatrix(rot).GetAxes(), so a slot's facing can be drawn.
		void SlotAxes(const FRotator& rot, FVector& axisX, FVector& axisY, FVector& axisZ)
		{
			const float SP = std::sin(UnrealRotationUnitsToRadians(rot.Pitch));
			const float CP = std::cos(UnrealRotationUnitsToRadians(rot.Pitch));
			const float SY = std::sin(UnrealRotationUnitsToRadians(rot.Yaw));
			const float CY = std::cos(UnrealRotationUnitsToRadians(rot.Yaw));
			const float SR = std::sin(UnrealRotationUnitsToRadians(rot.Roll));
			const float CR = std::cos(UnrealRotationUnitsToRadians(rot.Roll));
			axisX = { CP * CY,                    CP * SY,                   SP };
			axisY = { SR * SP * CY - CR * SY,     SR * SP * SY + CR * CY,   -SR * CP };
			axisZ = { -(CR * SP * CY + SR * SY),  CY * SR - CR * SP * SY,    CR * CP };
		}
	}

	void SetHudCanvas(UCanvas* canvas)
	{
		g_HudCanvas = canvas;
		// The claimed-rectangle list is per frame. Cleared on the way in as well as the way
		// out so a PostRender that throws part way through cannot leave stale rectangles
		// blocking the next frame's labels.
		g_LabelRects.clear();
	}

	void DrawSpawnedCoverLinks()
	{
		const auto& links = GetSpawnedLinks();
		if (links.empty())
		{
			return;
		}

		auto* const player = GetPlayer();
		if (player == nullptr)
		{
			return;
		}

		auto* const cls = ACoverLink::StaticClass();
		auto* const defaults = (cls != nullptr)
			? static_cast<ACoverLink*>(cls->ClassDefaultObject) : nullptr;
		if (defaults == nullptr)
		{
			return;
		}

		// Draw the pillar at the height the slot actually offers, so standing and mid-level
		// cover are told apart by silhouette and not just by colour.
		const float standHeight = defaults->StandHeight;
		const float midHeight = defaults->MidHeight;
		const float alignDist = defaults->AlignDist;

		auto* const drawer = static_cast<AActor*>(AActor::StaticClass()->ClassDefaultObject);
		const FVector playerLoc = player->LOCATION;
		const float clipSq = SpawnedLinkDrawDistance * SpawnedLinkDrawDistance;

		// Everything except the pillar and the walk chain is detail that only reads at close
		// range anyway, and it is most of the line count -- the marker cube alone is twelve
		// lines. Past half the draw distance, drop to the silhouette.
		const float detailSq = clipSq * 0.25f;

		// A hard ceiling on line volume whatever the radius is set to. cc.sweep can make
		// hundreds of links in a single command and this runs every PostRender, so without a
		// ceiling one sweep in a large room costs the frame rate and the only way out is to
		// already know which command turns the overlay off.
		constexpr int k_maxSlotsDrawn = 600;
		int slotsDrawn = 0;
		bool bBudgetHit = false;

		int linkIndex = -1;
		for (auto* const link : links)
		{
			linkIndex++;
			if (link == nullptr || bBudgetHit)
			{
				continue;
			}

			const int slotCount = static_cast<int>(link->Slots.Count());
			FVector previousSlotLoc{};
			bool bHavePrevious = false;

			for (int i = 0; i < slotCount; i++)
			{
				const FCoverSlot& slot = link->Slots(i);
				const FVector slotLoc = link->GetSlotLocation(i, FALSE);

				const FVector delta = {
					slotLoc.X - playerLoc.X, slotLoc.Y - playerLoc.Y, slotLoc.Z - playerLoc.Z };
				const float distSq =
					delta.X * delta.X + delta.Y * delta.Y + delta.Z * delta.Z;
				const bool bInRange = distSq <= clipSq;
				const bool bDetail = distSq <= detailSq;

				// The chain still has to advance even for a slot we skip, or the link between
				// the two slots either side of a clipped one would be drawn wrong.
				const FVector thisSlotLoc = slotLoc;
				if (!bInRange)
				{
					previousSlotLoc = thisSlotLoc;
					bHavePrevious = true;
					continue;
				}

				if (slotsDrawn >= k_maxSlotsDrawn)
				{
					bBudgetHit = true;
					break;
				}
				slotsDrawn++;

				float height = midHeight;
				unsigned char r = 255, g = 255, b = 0;   // CT_None: yellow.
				if (!slot.bEnabled)
				{
					// Disabled slots would serialise as dead cover, so make them loud.
					r = 255; g = 0; b = 0;
					height = midHeight;
				}
				else if (slot.CoverType == k_ctStanding)
				{
					r = 0; g = 255; b = 64;
					height = standHeight;
				}
				else if (slot.CoverType == k_ctMidLevel)
				{
					r = 0; g = 176; b = 255;
					height = midHeight;
				}
				else if (slot.CoverType == k_ctNone)
				{
					height = midHeight;
				}

				// Pillar from the floor to the height this slot claims to cover.
				const FVector pillarCentre = { slotLoc.X, slotLoc.Y, slotLoc.Z + height * 0.5f };
				drawer->DrawDebugBox(pillarCentre, FVector{ 10.0f, 10.0f, height * 0.5f },
					r, g, b, FALSE);

				// ! Which branch put this slot here, written above the slot.
				//
				// The provenance tally answers "which branch is misbehaving" across a whole
				// sweep; this answers "which branch put THAT one there" while looking straight
				// at it, which is the question a screenshot actually raises.
				//
				// Detail radius only, and off by default. Text costs far more than a box and
				// the overlay is already the reason cc.drawlinks needed a radius.
				if (DrawSlotOriginLabels && bDetail)
				{
					const auto* const placement = GetSlotPlacement(link, i);
					// The link index leads, because the question these labels exist to answer
					// is usually asked where two links MEET -- an inside corner one of them
					// should have turned. Without it, two chains ending nose to nose are
					// indistinguishable from one chain that continued. It is the index into
					// the spawn list, which cc.explainlink prints too, so a label can be
					// carried straight to the log.
					// Both numbering schemes, because the game and this overlay disagree:
					// 'profile cover self' names cover "CoverLink_12", while the spawn list
					// calls the same link L13. cc.explainlink takes either form, so whichever
					// number is in front of you can be typed straight in.
					wchar_t label[96]{};
					_snwprintf_s(label, _TRUNCATE, L"C%d L%d s%d %hs%hs",
						LinkObjectNumber(link), linkIndex, i,
						(placement != nullptr) ? SlotOriginName(placement->origin) : "?",
						(slot.bEnabled == 0) ? " DEAD" : "");
					DrawWorldLabel(FVector{ slotLoc.X, slotLoc.Y, slotLoc.Z + height + 16.0f },
						label, r, g, b);
				}

				if (bDetail)
				{
					FVector axisX{}, axisY{}, axisZ{};
					SlotAxes(link->GetSlotRotation(i, FALSE), axisX, axisY, axisZ);

					// Which way the slot faces, i.e. into its wall. A slot whose arrow does not
					// point at geometry was placed against nothing.
					const FVector arrowBase = { slotLoc.X, slotLoc.Y, slotLoc.Z + 12.0f };
					drawer->DrawDebugLine(arrowBase,
						AddScaled(arrowBase, axisX, alignDist + 12.0f), 255, 160, 0, FALSE);

					// Lean and pop-up, drawn at the height each is actually taken from -- these
					// are what decide whether cover feels usable.
					if (slot.bLeanLeft)
					{
						const FVector at = { slotLoc.X, slotLoc.Y, slotLoc.Z + height * 0.75f };
						drawer->DrawDebugLine(at, AddScaled(at, axisY, -alignDist),
							255, 0, 255, FALSE);
					}
					if (slot.bLeanRight)
					{
						const FVector at = { slotLoc.X, slotLoc.Y, slotLoc.Z + height * 0.75f };
						drawer->DrawDebugLine(at, AddScaled(at, axisY, alignDist),
							255, 0, 255, FALSE);
					}
					if (slot.bCanPopUp)
					{
						const FVector at = { slotLoc.X, slotLoc.Y, slotLoc.Z + height };
						drawer->DrawDebugStar(at, 10.0f, 255, 255, 255, FALSE);
					}
				}

				// The order the placement walk laid slots down in. A chain that doubles back or
				// jumps is a walk that went wrong, which slot flags alone will not show.
				if (bHavePrevious)
				{
					const FVector a = { previousSlotLoc.X, previousSlotLoc.Y,
						previousSlotLoc.Z + 6.0f };
					const FVector c = { slotLoc.X, slotLoc.Y, slotLoc.Z + 6.0f };
					drawer->DrawDebugLine(a, c, 200, 200, 200, FALSE);
				}
				previousSlotLoc = thisSlotLoc;
				bHavePrevious = true;

				// Mantle targets, drawn from the top of this slot to the top of the landing
				// slot -- the vault network, and the whole point of the exercise.
				if (auto* const target = bDetail
					? static_cast<ACoverLink*>(slot.MantleTarget.Actor) : nullptr)
				{
					const int targetIdx = slot.MantleTarget.SlotIdx;
					if (targetIdx >= 0 && targetIdx < static_cast<int>(target->Slots.Count()))
					{
						const FVector targetLoc = target->GetSlotLocation(targetIdx, FALSE);
						const FVector from = { slotLoc.X, slotLoc.Y, slotLoc.Z + height };
						const FVector to = { targetLoc.X, targetLoc.Y, targetLoc.Z + midHeight };
						drawer->DrawDebugLine(from, to, 255, 0, 128, FALSE);
					}
				}

				// A slot with no CoverSlotMarker is not enterable cover, and that is invisible
				// otherwise -- the slot looks identical either way. Mark the ones that have a
				// marker, at the marker's own location, so a marker that ended up somewhere
				// other than its slot shows up too.
				if (auto* const marker = bDetail ? slot.SlotMarker : nullptr)
				{
					const FVector markerLoc = marker->LOCATION;
					drawer->DrawDebugBox(markerLoc, FVector{ 5.0f, 5.0f, 5.0f },
						255, 255, 255, FALSE);
				}

				// Mark the two ends of the placement walk. DrawDebugString would be the obvious
				// way to label every slot, but it pushes onto the HUD's debug text list rather
				// than the one-frame line batcher, and this runs every PostRender -- so the
				// ends get a mast each instead, which is what the open question actually needs:
				// whether the walk stopped at a real edge or gave up mid-surface.
				if (bDetail && (i == 0 || i == slotCount - 1))
				{
					const unsigned char mast = (i == 0) ? 255 : 128;
					const FVector base = { slotLoc.X, slotLoc.Y, slotLoc.Z + height };
					drawer->DrawDebugLine(base,
						FVector{ base.X, base.Y, base.Z + 48.0f }, mast, mast, mast, FALSE);
				}
			}
		}

		// Once, not every frame: this is a per-PostRender path and a log line here would be
		// its own performance problem.
		static bool bWarnedBudget = false;
		if (bBudgetHit && !bWarnedBudget)
		{
			bWarnedBudget = true;
			LEASI_WARN("Spawned-link overlay hit its {}-slot draw budget, so some links are "
				"not being drawn. Narrow it with 'cc.drawlinks <radius>' (currently {:.0f}u) "
				"or turn it off with 'cc.drawlinks 0'.",
				k_maxSlotsDrawn, SpawnedLinkDrawDistance);
		}

		// DrawInteractionPositions found that nothing draws unless a zero-width line is issued
		// through the line batcher; the same applies here.
		DrawDebugLine(playerLoc, playerLoc, FLinearColor{ 255, 128, 0, 1 }, 0, false);
	}
}
