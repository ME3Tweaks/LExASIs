#include "CoverClient/Utilities/DebugTools.hpp"
#include "CoverClient/PathfindingGeneration.hpp"
#include "CoverClient/CoverGeneration.hpp"
#include "CoverClient/Utilities/PathUtilities.hpp"	
#include "Common/Objects.hpp"

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
				actor->DrawDebugLine(cl->position, slot->position, 128, 128, 64, true);
			}
		}
	}
}

