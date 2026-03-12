#include "CoverClient/CoverGeneration.hpp"
#include "CoverClient/Utilities/PathUtilities.hpp"
#include "LESDK/Headers.hpp"
#include "Common/Objects.hpp"
#include "PathfindingGeneration.hpp"
#include <memory>

namespace CoverClient
{
    std::vector<CoverLink*> coverLinks;

	namespace {
		constexpr float BACK_DIST = 75.0f; // How far back from the normal we look
		constexpr float STEP_SIZE = 16.0f; // Scan distance
		constexpr float MAX_SCAN_DIST = 2000.0f; // Max length of the spline
		constexpr float NODE_SPACING = 256.0f; // Space between nodes, BioWare uses 256.
		constexpr float NORMAL_DOT_THRESHOLD = 0.9f;  // Normal tolerance to allow slight changes
		constexpr float TRACE_EXTRA = 20.0f; // Extra to ensure trace hits
		constexpr float NODE_FLOOR_OFFSET = 0.0f; // Units above the floor where nodes are placed
		constexpr float FLOOR_TRACE_DEPTH = 256.0f; // How far downward to trace when searching for the floor
		constexpr float MAX_HEIGHT_CHANGE = 64.0f; // Max floor height delta between steps before treating as a cliff edge

		/// <summary>
		/// Steps along sideDir * dirSign from scanOrigin, tracing back towards the surface at each
		/// position. Stops when the trace misses (no geometry) or the returned normal diverges from
		/// hitNormal beyond NORMAL_DOT_THRESHOLD. Returns the last valid distance.
		/// </summary>
		float ScanEdge(AActor* pawn, const FVector& scanOrigin, const FVector& hitNormal, const FVector& sideDir, float dirSign) {
			const float traceDepth = BACK_DIST + TRACE_EXTRA;
			FVector zeroExtent{};
			float dist = 0.0f;
			for (; dist <= MAX_SCAN_DIST; dist += STEP_SIZE) {
				FVector testPos = {
					scanOrigin.X + sideDir.X * dirSign * dist,
					scanOrigin.Y + sideDir.Y * dirSign * dist,
					scanOrigin.Z
				};
				// Check for lateral obstruction (e.g. a wall or counter crossing the cover surface)
				if (dist > 0.0f) {
					FVector prevPos = {
						scanOrigin.X + sideDir.X * dirSign * (dist - STEP_SIZE),
						scanOrigin.Y + sideDir.Y * dirSign * (dist - STEP_SIZE),
						scanOrigin.Z
					};
					FVector lateralHitLoc{};
					FVector lateralHitNormal{};
					auto lateralHit = pawn->Trace(testPos, prevPos, 0, zeroExtent, TRUE, &lateralHitLoc, &lateralHitNormal, nullptr);
					if (lateralHit != nullptr) {
						break;
					}
				}
				FVector traceEnd = {
					testPos.X - hitNormal.X * traceDepth,
					testPos.Y - hitNormal.Y * traceDepth,
					testPos.Z - hitNormal.Z * traceDepth
				};
				FVector traceHitLoc{};
				FVector traceHitNormal{};
				auto hit = pawn->Trace(traceEnd, testPos, 0, zeroExtent, TRUE, &traceHitLoc, &traceHitNormal, nullptr);
				if (hit == nullptr) {
					break;
				}
				float dot = traceHitNormal.X * hitNormal.X + traceHitNormal.Y * hitNormal.Y + traceHitNormal.Z * hitNormal.Z;
				if (dot < NORMAL_DOT_THRESHOLD) {
					break;
				}
			}
			return std::max(0.0f, dist - STEP_SIZE);
		}
	}

	void GenerateCoverFromCamera() {
		// Get a trace from the camera
		auto engine = ::Common::FindFirstObject<USFXEngine, true>();
		if (!engine) {
			return;
		}

		auto wi = static_cast<ABioWorldInfo*>(engine->GetCurrentWorldInfo());
		auto playerController = wi->GetALocalPlayerController();
		if (playerController == nullptr) {
			return;
		}
		if (playerController->Pawn == nullptr) {
			return; // Need a pawn for some funcs
		}

		AActor* hitActor = nullptr;
		FVector hitLocation{};
		FVector hitNormal{};
		auto playerCamera = static_cast<ASFXPlayerCamera*>(playerController->PlayerCamera);
		if (!playerCamera->GetTrace(&hitActor, &hitLocation, &hitNormal)) {
			LEASI_WARN(L"No trace hit.");
			return;
		}

		LEASI_INFO("Trace hit: {},{},{}", hitLocation.X, hitLocation.Y, hitLocation.Z);

		// Compute a horizontal side vector perpendicular to the surface normal:
		//   cross(hitNormal, worldUp) = (hitNormal.Y, -hitNormal.X, 0)
		FVector sideDir = { hitNormal.Y, -hitNormal.X, 0.0f };
		float sideLen = std::sqrt(sideDir.X * sideDir.X + sideDir.Y * sideDir.Y);
		if (sideLen < 0.001f) {
			LEASI_WARN(L"Cover surface normal is nearly vertical, skipping.");
			return;
		}
		sideDir.X /= sideLen;
		sideDir.Y /= sideLen;

		// Back away from the surface along the normal to get the scan origin
		FVector scanOrigin = {
			hitLocation.X + hitNormal.X * BACK_DIST,
			hitLocation.Y + hitNormal.Y * BACK_DIST,
			hitLocation.Z + hitNormal.Z * BACK_DIST
		};

		auto* pawn = playerController->Pawn;

		auto cl = std::make_unique<CoverLink>(playerCamera->CameraCache.POV.location, playerCamera->CameraCache.POV.Rotation);

		// Orient sideDir to point right from the camera's perspective so nodes are always left-to-right
		float camYaw = UnrealRotationUnitsToRadians(playerCamera->CameraCache.POV.Rotation.Yaw);
		FVector camRight = { -std::sin(camYaw), std::cos(camYaw), 0.0f };
		if (sideDir.X * camRight.X + sideDir.Y * camRight.Y < 0.0f) {
			sideDir.X = -sideDir.X;
			sideDir.Y = -sideDir.Y;
		}

		// Scan left to find the leftmost valid starting position
		float leftDist = ScanEdge(pawn, scanOrigin, hitNormal, sideDir, -1.0f);
		LEASI_INFO("Cover left edge: {} units from scan origin", leftDist);

		FRotator facing = FVectorToFRotator(FVector{ -hitNormal.X, -hitNormal.Y, -hitNormal.Z });
		FVector leftEdge = {
			scanOrigin.X - sideDir.X * leftDist,
			scanOrigin.Y - sideDir.Y * leftDist,
			scanOrigin.Z
		};

		// Walk right from the left edge, generating nodes as we go
		const float traceDepth = BACK_DIST + TRACE_EXTRA;
		FVector zeroExtent{};
		FVector prevPos = leftEdge;
		float lastValidDist = 0.0f;
		float lastPlacedNodeDist = 0.0f;
		float nextNodeAt = NODE_SPACING;

		allNetworkNodes.push_back(std::make_unique<CoverSlotMarker>(leftEdge, facing));
		auto csm = static_cast<CoverSlotMarker*>(allNetworkNodes.back().get());
		csm->networkIndex = (int) allNetworkNodes.size() - 1; // Assign the index into the nodes list so host can look it up
		cl->AddCoverSlot(csm);

		for (float dist = STEP_SIZE; dist <= MAX_SCAN_DIST; dist += STEP_SIZE) {
			FVector testPos = {
				leftEdge.X + sideDir.X * dist,
				leftEdge.Y + sideDir.Y * dist,
				leftEdge.Z
			};

			// Lateral obstruction check (e.g. a wall or counter crossing the cover surface)
			FVector lateralHitLoc{};
			FVector lateralHitNormal{};
			auto lateralHit = pawn->Trace(testPos, prevPos, 0, zeroExtent, TRUE, &lateralHitLoc, &lateralHitNormal, nullptr);
			if (lateralHit != nullptr) {
				break;
			}

			// Surface presence and normal check
			FVector traceEnd = {
				testPos.X - hitNormal.X * traceDepth,
				testPos.Y - hitNormal.Y * traceDepth,
				testPos.Z - hitNormal.Z * traceDepth
			};
			FVector traceHitLoc{};
			FVector traceHitNormal{};
			auto hit = pawn->Trace(traceEnd, testPos, 0, zeroExtent, TRUE, &traceHitLoc, &traceHitNormal, nullptr);
			if (hit == nullptr) {
				break;
			}
			float dot = traceHitNormal.X * hitNormal.X + traceHitNormal.Y * hitNormal.Y + traceHitNormal.Z * hitNormal.Z;
			if (dot < NORMAL_DOT_THRESHOLD) {
				break;
			}

			lastValidDist = dist;
			prevPos = testPos;

			// Place a node at each NODE_SPACING interval as we go
			if (dist >= nextNodeAt) {
				FVector nodePos = {
					leftEdge.X + sideDir.X * nextNodeAt,
					leftEdge.Y + sideDir.Y * nextNodeAt,
					leftEdge.Z
				};
				allNetworkNodes.push_back(std::make_unique<CoverSlotMarker>(nodePos, facing));
				csm = static_cast<CoverSlotMarker*>(allNetworkNodes.back().get());
				csm->networkIndex = (int)allNetworkNodes.size() - 1; // Assign the index into the nodes list so host can look it up
				cl->AddCoverSlot(csm);

				lastPlacedNodeDist = nextNodeAt;
				nextNodeAt += NODE_SPACING;
			}
		}

		// Add the rightmost node at the final valid position if the walk did not land on it
		if (lastValidDist > lastPlacedNodeDist + 1.0f) {
			FVector rightEdge = {
				leftEdge.X + sideDir.X * lastValidDist,
				leftEdge.Y + sideDir.Y * lastValidDist,
				leftEdge.Z
			};

			allNetworkNodes.push_back(std::make_unique<CoverSlotMarker>(rightEdge, facing));
			csm = static_cast<CoverSlotMarker*>(allNetworkNodes.back().get());
			csm->networkIndex = (int)allNetworkNodes.size() - 1; // Assign the index into the nodes list so host can look it up
			cl->AddCoverSlot(csm);
		}

		LEASI_INFO("Generated {} cover slot(s) for new CoverLink.", cl->slots.size());

		// Slot ownership is already in allNetworkNodes; cl->slots holds non-owning raw pointers.
		// Transfer ownership of cl into the network list and keep a non-owning pointer for drawing.
		// This is done AFTER all the cover slots have been added.
		allNetworkNodes.push_back(std::move(cl));
		coverLinks.push_back(static_cast<CoverLink*>(allNetworkNodes.back().get()));

		DrawCover();
	}
}