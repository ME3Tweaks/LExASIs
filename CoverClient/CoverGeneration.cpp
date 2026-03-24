#include "CoverClient/CoverGeneration.hpp"
#include "CoverClient/Utilities/PathUtilities.hpp"
#include "CoverClient/Helpers.hpp" // Added to access GetAxes
#include "LESDK/Headers.hpp"
#include "Common/Objects.hpp"
#include <memory>

namespace CoverClient
{
	std::vector<CoverLink*> coverLinks;
	std::unordered_map<CoverSlotMarker*, std::vector<ComputedFireLink>> computedFireLinks;
	std::unordered_map<CoverSlotMarker*, std::vector<DangerNavEntry>> computedDangerNavs;
	std::unordered_map<CoverSlotMarker*, std::vector<DangerCoverEntry>> computedExposedCover;

	// Firing offsets in local space (X=forward into cover, Y=right from pawn perspective, Z=up)
	constexpr FVector StandingLeanOffset = { 0.0f, 78.0f, 69.0f };
	constexpr FVector CrouchLeanOffset = { 0.0f, 70.0f, 19.0f };
	constexpr FVector PopupOffset = { 0.0f,  0.0f, 70.0f };

	/// <summary>
	/// Gets fire location from in-game ACoverLink
	/// </summary>
	/// <param name="csm"></param>
	/// <param name="action"></param>
	/// <returns></returns>
	FVector ComputeFirePosition(ACoverLink* link, const int slotIdx, unsigned char action) {
		// These appear to get offsets.
		FRotator rot = link->GetSlotRotation(slotIdx, 0);
		FVector pos = link->GetSlotLocation(slotIdx, 0);

		// Need to get the actual location value!!


		const float yawRad = UnrealRotationUnitsToRadians(rot.Yaw);

		// Are these right?
		const float rightX = -std::sin(yawRad);
		const float rightY = std::cos(yawRad);

		const FCoverSlot& owningSlot = link->Slots.GetData()[slotIdx];

		if (action == 0) { // CA_Default: basic eye height, no lateral offset (matches GetSlotViewPoint default)
			pos.Z += (owningSlot.CoverType == 1) ? StandingLeanOffset.Z : CrouchLeanOffset.Z;
			return pos;
		}

		if (owningSlot.CoverType == 1) { // CT_Standing
			float ySign = (action == 3) ? -1.0f : 1.0f; // CA_LeanLeft inverts Y
			pos.X += ySign * StandingLeanOffset.Y * rightX;
			pos.Y += ySign * StandingLeanOffset.Y * rightY;
			pos.Z += StandingLeanOffset.Z;
		}
		else { // CT_MidLevel
			if (action == 5) { // CA_PopUp
				pos.Z += PopupOffset.Z;
			}
			else {
				float ySign = (action == 3) ? -1.0f : 1.0f;
				pos.X += ySign * CrouchLeanOffset.Y * rightX;
				pos.Y += ySign * CrouchLeanOffset.Y * rightY;
				pos.Z += CrouchLeanOffset.Z;
			}
		}
		return pos;
	}

	FVector ComputeFirePosition(const CoverSlotMarker* csm, unsigned char action) {
		const float yawRad = UnrealRotationUnitsToRadians(csm->rotation.Yaw);
		// Pawn right vector (pawn faces outward, opposite of slot yaw)
		//const float rightX = std::sin(yawRad);
		//const float rightY = -std::cos(yawRad);
		const float rightX = -std::sin(yawRad);
		const float rightY = std::cos(yawRad);

		FVector pos = csm->position;

		if (action == 0) { // CA_Default: basic eye height, no lateral offset (matches GetSlotViewPoint default)
			pos.Z += (csm->slot->CoverType == 1) ? StandingLeanOffset.Z : CrouchLeanOffset.Z;
			return pos;
		}

		if (csm->slot->CoverType == 1) { // CT_Standing
			float ySign = (action == 3) ? -1.0f : 1.0f; // CA_LeanLeft inverts Y
			pos.X += ySign * StandingLeanOffset.Y * rightX;
			pos.Y += ySign * StandingLeanOffset.Y * rightY;
			pos.Z += StandingLeanOffset.Z;
		}
		else { // CT_MidLevel
			if (action == 5) { // CA_PopUp
				pos.Z += PopupOffset.Z;
			}
			else {
				float ySign = (action == 3) ? -1.0f : 1.0f;
				pos.X += ySign * CrouchLeanOffset.Y * rightX;
				pos.Y += ySign * CrouchLeanOffset.Y * rightY;
				pos.Z += CrouchLeanOffset.Z;
			}
		}
		return pos;
	}

	namespace {
		constexpr float BACK_DIST = 36.0f;
		constexpr float STEP_SIZE = 16.0f; // Scan distance
		constexpr float MAX_SCAN_DIST = 2000.0f; // Max length of the spline
		constexpr float NODE_SPACING = 256.0f; // Space between nodes, BioWare uses 256.
		constexpr float NORMAL_DOT_THRESHOLD = 0.9f;  // Normal tolerance to allow slight changes
		constexpr float TRACE_EXTRA = 20.0f; // Extra to ensure trace hits
		constexpr float COVERSLOT_FLOOR_OFFSET = 59.5; // Units above the floor where nodes are placed. Measured in-game to get this value
		constexpr float FLOOR_TRACE_DEPTH = 256.0f; // How far downward to trace when searching for the floor
		constexpr float MAX_HEIGHT_CHANGE = 64.0f; // Max floor height delta between steps before treating as a cliff edge
		constexpr float LEAN_TRACE_DIST = 64.0f; // Length of the 45-degree lean peek trace at cover edges
		constexpr float CLIMB_NODE_OFFSET = 25.0f; // Z offset applied to the climb hit location
		constexpr float CLIMB_TRACE_ANGLE = 0.7f; // Radians above horizontal for the climb trace start
		constexpr float CLIMB_TRACE_DIST = 50.0f; // Distance for the climb trace start and downward check
		constexpr float MANTLE_MAX_DEPTH = 48.0f; // Maximum cover depth (past the near face) to allow mantling
		constexpr float FIRELINK_RADIUS = 3000.0f; // Max distance between slots to consider fire links
		constexpr float UCONST_COVERLINK_DangerDist = 1536.0f;

		// Some cover constnts
		constexpr float MaxFireLinkDist = 3000.0f;
		constexpr float COVERLINK_ExposureDot = 0.4f;
		constexpr float COVERLINK_EdgeCheckDot = 0.25f;
		constexpr float COVERLINK_EdgeExposureDot = 0.85f;
		constexpr float MINFIRELINKANGLE = 0.45f;           // Minimum dot for src slot facing toward dest (from GetFireActions)
		constexpr float COVERLINK_MinFireAngleDist = 128.0f; // Minimum projected distance for a valid fire link (from GetFireActions)

		/// <summary>
		/// Decomposes an FRotator (in Unreal rotation units) into its three world-space axes.
		/// Matches the UE3 FRotationMatrix::GetAxes convention.
		/// axisX = forward, axisY = right, axisZ = up.
		/// </summary>
		void GetSlotAxes(const FRotator& rot, FVector& axisX, FVector& axisY, FVector& axisZ) {
			const float pitchRad = UnrealRotationUnitsToRadians(rot.Pitch);
			const float yawRad = UnrealRotationUnitsToRadians(rot.Yaw);
			const float rollRad = UnrealRotationUnitsToRadians(rot.Roll);
			const float SP = std::sin(pitchRad), CP = std::cos(pitchRad);
			const float SY = std::sin(yawRad), CY = std::cos(yawRad);
			const float SR = std::sin(rollRad), CR = std::cos(rollRad);
			axisX = { CP * CY,                    CP * SY,                   SP };
			axisY = { SR * SP * CY - CR * SY,     SR * SP * SY + CR * CY,   -SR * CP };
			axisZ = { -(CR * SP * CY + SR * SY),  CY * SR - CR * SP * SY,    CR * CP };
		}

		bool FindFloorZ(AActor* pawn, const FVector& pos, float& outFloorZ) {
			FVector zeroExtent{};
			FVector floorEnd = { pos.X, pos.Y, pos.Z - FLOOR_TRACE_DEPTH };
			FVector hitLoc{}, hitNormal{};
			auto hit = pawn->Trace(floorEnd, pos, 0, zeroExtent, TRUE, &hitLoc, &hitNormal, nullptr);
			if (!hit) {
				return false;
			}
			outFloorZ = hitLoc.Z;
			return true;
		}

		/// <summary>
		/// Fires a trace at 3/4 player height toward the surface. Returns true if the surface is present at
		/// standing height, indicating this is a standing cover slot rather than mid-level.
		/// </summary>
		bool CheckStandingCover(AActor* pawn, const FVector& nodePos, float floorZ, float pawnHalfHeight, const FVector& hitNormal, float traceDepth) {
			FVector zeroExtent{};
			FVector upperStart = { nodePos.X, nodePos.Y, floorZ + pawnHalfHeight * 1.5f };
			FVector upperEnd = {
				upperStart.X - hitNormal.X * traceDepth,
				upperStart.Y - hitNormal.Y * traceDepth,
				upperStart.Z - hitNormal.Z * traceDepth
			};
			FVector hitLoc{}, hitNorm{};
			return pawn->Trace(upperEnd, upperStart, 0, zeroExtent, TRUE, &hitLoc, &hitNorm, nullptr) != nullptr;
		}

		/// <summary>
		/// Fires a 45-degree outward trace from a cover edge node to test whether the player can peek
		/// around the corner. Returns true if nothing was hit (open space for leaning).
		/// </summary>
		bool CheckLeanTrace(AActor* pawn, const FVector& nodePos, float leanDirX, float leanDirY) {
			FVector zeroExtent{};
			FVector traceEnd = {
				nodePos.X + leanDirX * LEAN_TRACE_DIST,
				nodePos.Y + leanDirY * LEAN_TRACE_DIST,
				nodePos.Z
			};
			FVector hitLoc{}, hitNorm{};
			return pawn->Trace(traceEnd, nodePos, 0, zeroExtent, TRUE, &hitLoc, &hitNorm, nullptr) == nullptr;
		}

		/// <summary>
		/// Steps along sideDir * dirSign from scanOrigin, tracing back towards the surface at each
		/// position. Stops when the trace misses (no geometry), the returned normal diverges from
		/// hitNormal beyond NORMAL_DOT_THRESHOLD, or a cliff edge is detected. Returns the last valid distance.
		/// outObjectEdge is set to true when the scan stopped because the surface ended (object edge),
		/// rather than due to a lateral wall or cliff.
		/// </summary>
		float ScanEdge(AActor* pawn, const FVector& scanOrigin, const FVector& hitNormal, const FVector& sideDir, float dirSign, bool& outObjectEdge) {
			outObjectEdge = false;
			const float traceDepth = BACK_DIST + TRACE_EXTRA;
			FVector zeroExtent{};

			// Establish a baseline floor height for cliff detection
			float prevFloorZ = scanOrigin.Z;
			{
				float foundFloorZ;
				if (FindFloorZ(pawn, scanOrigin, foundFloorZ)) {
					prevFloorZ = foundFloorZ;
				}
			}

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
				// Floor trace to detect cliff edges
				float floorZ;
				if (!FindFloorZ(pawn, testPos, floorZ)) {
					break; // Cliff edge: no floor below
				}
				if (std::abs(floorZ - prevFloorZ) > MAX_HEIGHT_CHANGE) {
					break; // Cliff edge: too much height change
				}
				prevFloorZ = floorZ;

				FVector traceEnd = {
					testPos.X - hitNormal.X * traceDepth,
					testPos.Y - hitNormal.Y * traceDepth,
					testPos.Z - hitNormal.Z * traceDepth
				};
				FVector traceHitLoc{};
				FVector traceHitNormal{};
				auto hit = pawn->Trace(traceEnd, testPos, 0, zeroExtent, TRUE, &traceHitLoc, &traceHitNormal, nullptr);
				if (hit == nullptr) {
					outObjectEdge = true;
					break;
				}
				float dot = traceHitNormal.X * hitNormal.X + traceHitNormal.Y * hitNormal.Y + traceHitNormal.Z * hitNormal.Z;
				if (dot < NORMAL_DOT_THRESHOLD) {
					outObjectEdge = true;
					break;
				}
			}
			return std::max(0.0f, dist - STEP_SIZE);
		}

		/// <summary>
		/// Checks whether a midlevel cover slot can be climbed across its full coverage area.
		/// The coverage area spans from halfway to the left neighbor to halfway to the right
		/// neighbor (or only in the available direction at edges). Fires a downward trace from
		/// a start point offset CLIMB_TRACE_DIST units at CLIMB_TRACE_ANGLE radians above
		/// horizontal toward the cover surface. Returns true only if all sample traces hit
		/// geometry. outClimbLocation receives the center sample hit location raised by
		/// CLIMB_NODE_OFFSET.
		/// </summary>
		bool CheckClimbSlot(AActor* pawn, const CoverLink* link, int slotIndex, FVector& outClimbLocation) {
			const CoverSlotMarker* csm = link->slots[slotIndex];

			// Only midlevel cover can be climbed
			if (csm->slot->CoverType == 1) { // CT_Standing
				return false;
			}

			// Derive the direction toward the cover surface from the slot's facing yaw
			const float yawRad = UnrealRotationUnitsToRadians(csm->rotation.Yaw);
			const float towardX = -std::cos(yawRad);
			const float towardY = -std::sin(yawRad);

			// Horizontal and vertical components of the trace start offset
			const float horzComp = std::cos(CLIMB_TRACE_ANGLE) * CLIMB_TRACE_DIST;
			const float vertComp = std::sin(CLIMB_TRACE_ANGLE) * CLIMB_TRACE_DIST;

			// Determine the coverage range boundaries
			const bool hasLeft = slotIndex > 0;
			const bool hasRight = slotIndex < static_cast<int>(link->slots.size()) - 1;

			FVector samples[3];
			int numSamples = 0;

			if (hasLeft) {
				const CoverSlotMarker* leftNeighbor = link->slots[slotIndex - 1];
				samples[numSamples++] = {
					(csm->position.X + leftNeighbor->position.X) * 0.5f,
					(csm->position.Y + leftNeighbor->position.Y) * 0.5f,
					csm->position.Z
				};
			}
			samples[numSamples++] = csm->position; // Center
			if (hasRight) {
				const CoverSlotMarker* rightNeighbor = link->slots[slotIndex + 1];
				samples[numSamples++] = {
					(csm->position.X + rightNeighbor->position.X) * 0.5f,
					(csm->position.Y + rightNeighbor->position.Y) * 0.5f,
					csm->position.Z
				};
			}

			FVector zeroExtent{};
			FVector centerHitLoc{};
			const int centerIdx = hasLeft ? 1 : 0;

			for (int si = 0; si < numSamples; si++) {
				const FVector& sample = samples[si];
				FVector traceStart = {
					sample.X + towardX * horzComp,
					sample.Y + towardY * horzComp,
					sample.Z + vertComp
				};
				FVector traceEnd = {
					traceStart.X,
					traceStart.Y,
					traceStart.Z - CLIMB_TRACE_DIST
				};
				FVector hitLoc{}, hitNorm{};
				if (pawn->Trace(traceEnd, traceStart, 0, zeroExtent, TRUE, &hitLoc, &hitNorm, nullptr) == nullptr) {
					return false;
				}
				if (si == centerIdx) {
					centerHitLoc = hitLoc;
				}
			}

			outClimbLocation = { centerHitLoc.X, centerHitLoc.Y, centerHitLoc.Z + CLIMB_NODE_OFFSET };
			return true;
		}

		/// <summary>
		/// Checks whether a midlevel cover slot can be mantled (vaulted over). Calls CheckClimbSlot
		/// to validate the midlevel guard and the climb point across the slot's coverage area, then
		/// additionally verifies:
		///   - The cover is shallow enough to vault: the far-side floor (sampled at BACK_DIST +
		///     MANTLE_MAX_DEPTH from the slot) is at approximately our current ground Z. If the
		///     cover extends that far, FindFloorZ returns the cover top Z instead, which is
		///     elevated enough to fail the height tolerance.
		///   - The airspace above the cover on our side is clear: a horizontal trace at the climb
		///     location height from our position to the far side must not be blocked.
		/// outClimbLocation receives the same value as CheckClimbSlot on success.
		/// </summary>
		bool CheckMantleSlot(AActor* pawn, const CoverLink* link, int slotIndex, FVector& outClimbLocation) {
			// Validates midlevel guard and the climb point across the full slot coverage range
			FVector climbLoc{};
			if (!CheckClimbSlot(pawn, link, slotIndex, climbLoc)) {
				return false;
			}

			const CoverSlotMarker* csm = link->slots[slotIndex];
			const float yawRad = UnrealRotationUnitsToRadians(csm->rotation.Yaw);
			const float towardX = -std::cos(yawRad);
			const float towardY = -std::sin(yawRad);
			const float farSideReach = BACK_DIST + MANTLE_MAX_DEPTH;

			FVector zeroExtent{};

			// Far-side floor check: the landing zone must be at the same ground height as our side.
			// If the cover extends beyond MANTLE_MAX_DEPTH, FindFloorZ returns the cover top Z
			// instead of the real floor, which is elevated enough to fail the height tolerance.
			const float currentFloorZ = csm->position.Z - COVERSLOT_FLOOR_OFFSET;
			FVector farSidePos = {
				csm->position.X + towardX * farSideReach,
				csm->position.Y + towardY * farSideReach,
				climbLoc.Z
			};
			float farFloorZ;
			if (!FindFloorZ(pawn, farSidePos, farFloorZ)) {
				return false; // No ground on the far side
			}
			if (std::abs(farFloorZ - currentFloorZ) > MAX_HEIGHT_CHANGE) {
				return false; // Far-side ground is at a different level (cover top, ledge, or drop)
			}

			// Clearance trace: from our position at climb height, trace horizontally to the far side.
			// Catches any ceiling, overhang, or obstruction that would block the vault.
			FVector clearStart = {
				csm->position.X,
				csm->position.Y,
				climbLoc.Z
			};
			FVector clearEnd = {
				csm->position.X + towardX * farSideReach,
				csm->position.Y + towardY * farSideReach,
				climbLoc.Z
			};
			FVector clearHitLoc{}, clearHitNorm{};
			if (pawn->Trace(clearEnd, clearStart, 0, zeroExtent, TRUE, &clearHitLoc, &clearHitNorm, nullptr) != nullptr) {
				return false; // Obstruction at vault height - cannot mantle
			}

			outClimbLocation = climbLoc;
			return true;
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
		const float pawnHalfHeight = pawn->GetCollisionExtent().Z;


		FVector ecl = { -120993.922f, 32262.0f, 2760.0f };
		float eclZFloor = 0;
		FindFloorZ(pawn, ecl, eclZFloor);
		auto offset = std::abs(ecl.Z - eclZFloor);
		LEASI_INFO("Offset: {}", offset);

		auto cl = std::make_unique<CoverLink>(playerCamera->CameraCache.POV.location, playerCamera->CameraCache.POV.Rotation);
		// Set location and rotation of the cover link to floor snap and remove the pitch.
		cl->rotation.Pitch = 0;

		// This should probably check if there is floor below the camera at all and abort if not, but for now just clamp to the camera height if no floor is found.
		FindFloorZ(pawn, cl->position, cl->position.Z);
		cl->position.Z += pawn->GetCollisionExtent().Z;


		// Orient sideDir to point right from the camera's perspective so nodes are always left-to-right
		float camYaw = UnrealRotationUnitsToRadians(playerCamera->CameraCache.POV.Rotation.Yaw);
		FVector camRight = { -std::sin(camYaw), std::cos(camYaw), 0.0f };
		if (sideDir.X * camRight.X + sideDir.Y * camRight.Y < 0.0f) {
			sideDir.X = -sideDir.X;
			sideDir.Y = -sideDir.Y;
		}

		// Scan left to find the leftmost valid starting position
		bool leftEdgeIsObjectEnd = false;
		float leftDist = ScanEdge(pawn, scanOrigin, hitNormal, sideDir, -1.0f, leftEdgeIsObjectEnd);
		LEASI_INFO("Cover left edge: {} units from scan origin", leftDist);

		FRotator facing = FVectorToFRotator(FVector{ -hitNormal.X, -hitNormal.Y, -hitNormal.Z });
		FVector leftEdge = {
			scanOrigin.X - sideDir.X * leftDist,
			scanOrigin.Y - sideDir.Y * leftDist,
			scanOrigin.Z
		};

		// Find the floor below the left edge and snap its Z to the node height
		float leftFloorZ;
		if (!FindFloorZ(pawn, leftEdge, leftFloorZ)) {
			LEASI_WARN(L"No floor found at left edge, aborting cover generation.");
			return;
		}
		leftEdge.Z = leftFloorZ + COVERSLOT_FLOOR_OFFSET;

		// Walk right from the left edge, generating nodes as we go
		const float traceDepth = BACK_DIST + TRACE_EXTRA;
		FVector zeroExtent{};
		FVector prevPos = leftEdge;
		float prevFloorZ = leftFloorZ;
		float lastValidDist = 0.0f;
		float lastPlacedNodeDist = 0.0f;
		float nextNodeAt = NODE_SPACING;
		bool rightEdgeIsObjectEnd = false;

		allNetworkNodes.push_back(std::make_unique<CoverSlotMarker>(leftEdge, facing));
		auto csm = static_cast<CoverSlotMarker*>(allNetworkNodes.back().get());
		csm->networkIndex = (int)allNetworkNodes.size() - 1; // Assign the index into the nodes list so host can look it up
		cl->AddCoverSlot(csm);
		bool prevStepIsStanding = CheckStandingCover(pawn, leftEdge, leftFloorZ, pawnHalfHeight, hitNormal, traceDepth);
		if (prevStepIsStanding) {
			csm->slot->CoverType = 1; // CT_Standing
		}
		else {
			csm->slot->bCanPopUp = true;
		}
		if (leftEdgeIsObjectEnd) {
			const float invSqrt2 = 1.0f / std::sqrt(2.0f);
			const float leanDirX = (-sideDir.X - hitNormal.X) * invSqrt2;
			const float leanDirY = (-sideDir.Y - hitNormal.Y) * invSqrt2;
			if (CheckLeanTrace(pawn, leftEdge, leanDirX, leanDirY)) {
				csm->slot->Actions.Add(3); // CA_LeanLeft
				csm->slot->bLeanLeft = true;
			}
		}

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

			// Floor trace to detect cliff edges and find the node height at this position
			float floorZ;
			if (!FindFloorZ(pawn, testPos, floorZ)) {
				break; // Cliff edge: no floor below
			}
			if (std::abs(floorZ - prevFloorZ) > MAX_HEIGHT_CHANGE) {
				break; // Cliff edge: too much height change
			}
			prevFloorZ = floorZ;
			testPos.Z = floorZ + COVERSLOT_FLOOR_OFFSET;

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
				rightEdgeIsObjectEnd = true;
				break;
			}
			float dot = traceHitNormal.X * hitNormal.X + traceHitNormal.Y * hitNormal.Y + traceHitNormal.Z * hitNormal.Z;
			if (dot < NORMAL_DOT_THRESHOLD) {
				rightEdgeIsObjectEnd = true;
				break;
			}

			lastValidDist = dist;
			prevPos = testPos;

			// Check cover height at this step; place a transition node if it changed since the last step
			bool currentStepIsStanding = CheckStandingCover(pawn, testPos, floorZ, pawnHalfHeight, hitNormal, traceDepth);
			if (currentStepIsStanding != prevStepIsStanding) {
				allNetworkNodes.push_back(std::make_unique<CoverSlotMarker>(testPos, facing));
				csm = static_cast<CoverSlotMarker*>(allNetworkNodes.back().get());
				csm->networkIndex = (int)allNetworkNodes.size() - 1; // Assign the index into the nodes list so host can look it up
				cl->AddCoverSlot(csm);
				if (currentStepIsStanding) {
					csm->slot->CoverType = 1; // CT_Standing
				}
				else {
					csm->slot->bCanPopUp = true;
				}
				lastPlacedNodeDist = dist;
				nextNodeAt = dist + NODE_SPACING;
			}
			else if (dist >= nextNodeAt) {
				// Place a node at each NODE_SPACING interval as we go
				FVector nodePos = {
					leftEdge.X + sideDir.X * nextNodeAt,
					leftEdge.Y + sideDir.Y * nextNodeAt,
					leftEdge.Z
				};
				float nodePosFloorZ;
				if (FindFloorZ(pawn, nodePos, nodePosFloorZ)) {
					nodePos.Z = nodePosFloorZ + COVERSLOT_FLOOR_OFFSET;
				}
				allNetworkNodes.push_back(std::make_unique<CoverSlotMarker>(nodePos, facing));
				csm = static_cast<CoverSlotMarker*>(allNetworkNodes.back().get());
				csm->networkIndex = (int)allNetworkNodes.size() - 1; // Assign the index into the nodes list so host can look it up
				cl->AddCoverSlot(csm);
				if (CheckStandingCover(pawn, nodePos, nodePos.Z - COVERSLOT_FLOOR_OFFSET, pawnHalfHeight, hitNormal, traceDepth)) {
					csm->slot->CoverType = 1; // CT_Standing
				}
				else {
					csm->slot->bCanPopUp = true;
				}
				lastPlacedNodeDist = nextNodeAt;
				nextNodeAt += NODE_SPACING;
			}
			prevStepIsStanding = currentStepIsStanding;
		}

		// Add the rightmost node at the final valid position if the walk did not land on it
		if (lastValidDist > lastPlacedNodeDist + 1.0f) {
			FVector rightEdge = {
				leftEdge.X + sideDir.X * lastValidDist,
				leftEdge.Y + sideDir.Y * lastValidDist,
				leftEdge.Z
			};
			float rightEdgeFloorZ;
			if (FindFloorZ(pawn, rightEdge, rightEdgeFloorZ)) {
				rightEdge.Z = rightEdgeFloorZ + COVERSLOT_FLOOR_OFFSET;
			}

			allNetworkNodes.push_back(std::make_unique<CoverSlotMarker>(rightEdge, facing));
			csm = static_cast<CoverSlotMarker*>(allNetworkNodes.back().get());
			csm->networkIndex = (int)allNetworkNodes.size() - 1; // Assign the index into the nodes list so host can look it up
			cl->AddCoverSlot(csm);
			if (CheckStandingCover(pawn, rightEdge, rightEdge.Z - COVERSLOT_FLOOR_OFFSET, pawnHalfHeight, hitNormal, traceDepth)) {
				csm->slot->CoverType = 1; // CT_Standing
			}
			else {
				csm->slot->bCanPopUp = true;
			}
		}
		if (rightEdgeIsObjectEnd) {
			const float invSqrt2 = 1.0f / std::sqrt(2.0f);
			const float leanDirX = (sideDir.X - hitNormal.X) * invSqrt2;
			const float leanDirY = (sideDir.Y - hitNormal.Y) * invSqrt2;
			if (CheckLeanTrace(pawn, csm->position, leanDirX, leanDirY)) {
				csm->slot->Actions.Add(4); // CA_LeanRight
				csm->slot->bLeanRight = true;
			}
		}

		// Go through slots and mark reachable indices for each adjacent nodes


		LEASI_INFO("Generated {} cover slot(s) for new CoverLink.", cl->slots.size());

		// Slot ownership is already in allNetworkNodes; cl->slots holds non-owning raw pointers.
		// Transfer ownership of cl into the network list and keep a non-owning pointer for drawing.
		// This is done AFTER all the cover slots have been added.
		allNetworkNodes.push_back(std::move(cl));
		coverLinks.push_back(static_cast<CoverLink*>(allNetworkNodes.back().get()));

		DrawCover();
	}

	std::vector<ComputedFireLink> ComputeFireLinksForSlot(CoverLink* srcLink, CoverSlotMarker* srcSlot, AActor* pawn) {
		constexpr float FIRELINK_RADIUS_SQ = MaxFireLinkDist * MaxFireLinkDist;
		constexpr float MIN_FIRELINK_DIST_SQ = 128 * 128;

		auto mySlotMarkerName = srcSlot->slot->SlotMarker->GetFullPath();


		const float yawRad = UnrealRotationUnitsToRadians(srcSlot->rotation.Yaw);
		const FVector outwardNormal = { std::cos(yawRad), std::sin(yawRad), 0.0f };

		std::vector<unsigned char> srcActions;
		if (srcSlot->slot->bLeanLeft) srcActions.push_back(3);   // CA_LeanLeft
		if (srcSlot->slot->bLeanRight) srcActions.push_back(4);  // CA_LeanRight
		if (srcSlot->slot->CoverType == 2 && srcSlot->slot->bCanPopUp) srcActions.push_back(5); // CA_PopUp

		if (srcActions.empty()) return {};

		std::vector<ComputedFireLink> results;
		FVector zeroExtent{};

		for (auto* dstLink : coverLinks) {
			if (dstLink == srcLink) continue;

			for (int dsi = 0; dsi < static_cast<int>(dstLink->slots.size()); dsi++) {

				auto slotMarkerName = srcSlot->slot->SlotMarker->GetFullPath();

				if (mySlotMarkerName.Equals(L"BioD_CerMir_250Pools.TheWorld.PersistentLevel.CoverSlotMarker_4032") && slotMarkerName.Equals(L"BioD_CerMir_250Pools.TheWorld.PersistentLevel.CoverSlotMarker_3995")) {
					LEASI_TRACE("Testing...");
				}

				auto* dstCsm = dstLink->slots[dsi];

				FVector diff = {
					dstCsm->position.X - srcSlot->position.X,
					dstCsm->position.Y - srcSlot->position.Y,
					dstCsm->position.Z - srcSlot->position.Z
				};

				// Is slot too far away?
				float distSq = diff.X * diff.X + diff.Y * diff.Y + diff.Z * diff.Z;
				if (distSq > FIRELINK_RADIUS_SQ) 
					continue;

				// Is slot too close?
				if (distSq < MIN_FIRELINK_DIST_SQ) {
					continue;
				}



				// Is the firing position in front of us? We only allow a specific cone for firing
				float dot = diff.X * outwardNormal.X + diff.Y * outwardNormal.Y;
				if (dot <= MINFIRELINKANGLE)
					continue;

				std::vector<unsigned char> dstActions{ 0 }; // CA_Default is always available for the dest slot (matches DestInfo in GetFireLinkInfo)
				if (dstCsm->slot->bLeanLeft) dstActions.push_back(3);
				if (dstCsm->slot->bLeanRight) dstActions.push_back(4);
				if (dstCsm->slot->CoverType == 2 && dstCsm->slot->bCanPopUp) dstActions.push_back(5);

				if (dstActions.empty()) 
					continue;

				ComputedFireLink fireLink;
				fireLink.targetLink = dstLink;
				fireLink.targetSlotIndex = dsi;

				// Enumerate all source actions to all dest actions.
				for (unsigned char sa : srcActions) {
					FVector srcFirePos = ComputeFirePosition(srcSlot, sa);
					for (unsigned char da : dstActions) {
						FVector dstFirePos = ComputeFirePosition(dstCsm, da);

						FVector hitLoc{}, hitNorm{};
						FCheckResult result{};
						auto noHit = SingleLineCheck(*GWorld, &result, pawn, &dstFirePos, &srcFirePos, TRACE_World | TRACE_StopAtAnyHit | TRACE_ComplexCollision, &zeroExtent, nullptr);

						if (noHit == true) {
							FireLinkInteraction interaction;
							interaction.srcCoverType = srcSlot->slot->CoverType;
							interaction.srcAction = sa;
							interaction.dstCoverType = dstCsm->slot->CoverType;
							interaction.dstAction = da;
							interaction.srcFirePos = srcFirePos;
							interaction.dstFirePos = dstFirePos;
							fireLink.interactions.push_back(interaction);
						}
					}
				}

				if (!fireLink.interactions.empty()) {
					results.push_back(std::move(fireLink));
				}
			}
		}

		return results;
	}

	void ComputeFireLinks() {
		auto engine = ::Common::FindFirstObject<USFXEngine, true>();
		if (!engine) {
			LEASI_WARN(L"ComputeFireLinks: no engine found.");
			return;
		}
		auto wi = static_cast<ABioWorldInfo*>(engine->GetCurrentWorldInfo());
		auto playerController = wi->GetALocalPlayerController();
		if (!playerController || !playerController->Pawn) {
			LEASI_WARN(L"ComputeFireLinks: no pawn available.");
			return;
		}
		auto* pawn = playerController->Pawn;

		computedFireLinks.clear();

		// Collect all eligible source slots
		struct SlotEntry { CoverLink* link; CoverSlotMarker* csm; };
		std::vector<SlotEntry> eligibleSlots;
		for (auto* cl : coverLinks) {
			for (auto* csm : cl->slots) {
				if (csm->slot->bLeanLeft || csm->slot->bLeanRight ||
					(csm->slot->CoverType == 2 && csm->slot->bCanPopUp)) {
					eligibleSlots.push_back({ cl, csm });
				}
			}
		}

		if (eligibleSlots.empty()) {
			LEASI_INFO("ComputeFireLinks: no eligible slots found.");
			return;
		}

		LEASI_INFO("ComputeFireLinks: computing for {} eligible slots.", eligibleSlots.size());

		for (size_t i = 0; i < eligibleSlots.size(); i++) {
			auto& entry = eligibleSlots[i];
			auto links = ComputeFireLinksForSlot(entry.link, entry.csm, pawn);
			if (!links.empty()) {
				computedFireLinks[entry.csm] = std::move(links);
			}
			if ((i + 1) % 10 == 0 || i + 1 == eligibleSlots.size()) {
				LEASI_INFO("ComputeFireLinks progress: {}/{}", i + 1, eligibleSlots.size());
			}
		}

		// Report results
		int totalFireLinks = 0;
		int totalInteractions = 0;
		for (const auto& [csm, links] : computedFireLinks) {
			totalFireLinks += static_cast<int>(links.size());
			for (const auto& fl : links) {
				totalInteractions += static_cast<int>(fl.interactions.size());
			}
		}
		LEASI_INFO("ComputeFireLinks complete: {} fire links with {} interactions across {} slots.",
			totalFireLinks, totalInteractions, computedFireLinks.size());
	}

	std::vector<DangerCoverEntry> ComputeExposedCoverForSlot(CoverSlotMarker* csm, AActor* pawn) {
		std::vector<DangerCoverEntry> results;

		auto mySlotMarkerName = csm->slot->SlotMarker->GetFullPath();

		// Get the destination slot's axes (X = into-wall in UE3 convention, matching GetExposedInfo)
		FVector axisX, axisY, axisZ;
		GetSlotAxes(csm->rotation, axisX, axisY, axisZ);

		// Invert the vectors to point OUT from the wall instead of INTO it
		//axisX.X = -axisX.X; axisX.Y = -axisX.Y; axisX.Z = -axisX.Z;
		//axisY.X = -axisY.X; axisY.Y = -axisY.Y; axisY.Z = -axisY.Z;

		// Hoist csm's action list: CA_Default is always present for the dest slot (matches DestInfo
		// in GetFireActions which explicitly adds CA_Default). Slot-specific actions are appended.
		// csmHasActions mirrors the DestActions.Num() > 0 gate that enables interaction validation.
		const bool csmHasActions = csm->slot->bLeanLeft || csm->slot->bLeanRight || (csm->slot->CoverType == 2 && csm->slot->bCanPopUp);
		std::vector<unsigned char> csmActions = { 0 }; // CA_Default always included
		if (csm->slot->bLeanLeft) csmActions.push_back(3);
		if (csm->slot->bLeanRight) csmActions.push_back(4);
		if (csm->slot->CoverType == 2 && csm->slot->bCanPopUp) csmActions.push_back(5);

		// For all coverlinks in the world.
		for (auto* srcLink : coverLinks) {
			// For every slot in the coverlink, excluding the destination slot itself.
			for (auto* srcSlot : srcLink->slots) {
				if (srcSlot == csm)
					continue;

				auto slotMarkerName = srcSlot->slot->SlotMarker->GetFullPath();

				if (mySlotMarkerName.Equals(L"BioD_CerMir_125Land.TheWorld.PersistentLevel.CoverSlotMarker_3041") && slotMarkerName.Equals(L"BioD_CerMir_125Land.TheWorld.PersistentLevel.CoverSlotMarker_3125")) {
					LEASI_TRACE("Testing...");
				}
				// csm = our cover slot, srcSlot = potential threat slot we're checking against. We want to determine if srcSlot is a threat to csm and if so how much.
				// Direction from dest (csm) to src (the threatening slot), then normalized
				FVector vectToChk = {
					srcSlot->position.X - csm->position.X,
					srcSlot->position.Y - csm->position.Y,
					srcSlot->position.Z - csm->position.Z
				};

				float distSq = vectToChk.X * vectToChk.X + vectToChk.Y * vectToChk.Y + vectToChk.Z * vectToChk.Z;
				if (distSq > MaxFireLinkDist * MaxFireLinkDist)
					continue;

				float dist = std::sqrt(distSq);
				float invDist = (dist > 0.001f) ? 1.0f / dist : 1.0f;
				vectToChk.X *= invDist;
				vectToChk.Y *= invDist;
				vectToChk.Z *= invDist;

				// Fire angle validity check (from GetFireActions):
				// The source slot must be facing toward csm (FireAngleDot >= MINFIRELINKANGLE) and the
				// projected distance in that direction must meet the minimum (FireAngleDist >= 128).
				// vectToChk is the normalized direction from csm to srcSlot, so src-to-csm is its negation.
				{
					FVector srcAxisX, srcAxisY, srcAxisZ;
					GetSlotAxes(srcSlot->rotation, srcAxisX, srcAxisY, srcAxisZ);
					float fireAngleDot = -(srcAxisX.X * vectToChk.X + srcAxisX.Y * vectToChk.Y + srcAxisX.Z * vectToChk.Z);
					float fireAngleDist = fireAngleDot * dist;
					if (fireAngleDot < MINFIRELINKANGLE || fireAngleDist < COVERLINK_MinFireAngleDist)
						continue;
				}

				// Interaction LOS check (from GetFireActions ValidInteractionList):
				// When csm has non-default actions, at least one (srcAction × csmAction) pair must
				// have an unobstructed line of sight (CanFireLinkHit). If srcSlot has no fire
				// actions the interaction list stays empty and we skip, matching GetFireActions behaviour.
				if (csmHasActions) {
					std::vector<unsigned char> srcActions;
					if (srcSlot->slot->bLeanLeft) srcActions.push_back(3);
					if (srcSlot->slot->bLeanRight) srcActions.push_back(4);
					if (srcSlot->slot->CoverType == 2 && srcSlot->slot->bCanPopUp) srcActions.push_back(5);

					if (srcActions.empty())
						continue;

					FVector zeroExtent{};
					bool anyValidInteraction = false;
					for (unsigned char sa : srcActions) {
						if (anyValidInteraction) break;
						FVector srcFirePos = ComputeFirePosition(srcSlot, sa);
						for (unsigned char da : csmActions) {
							FVector dstFirePos = ComputeFirePosition(csm, da);
							FVector hitLoc{}, hitNorm{}, extent{};
							FCheckResult hitResult{};
							if (SingleLineCheck(*GWorld, &hitResult, pawn, &dstFirePos, &srcFirePos, TRACE_World | TRACE_StopAtAnyHit | TRACE_ComplexCollision, &extent, nullptr) == true) {
								anyValidInteraction = true;
								break;
							}
						}
					}
					if (!anyValidInteraction)
						continue;
				}

				// Determine the exposure angle threshold.
			// If the threat is far to the side of an edge slot, tighten the valid exposure angle.
				float testDot = COVERLINK_ExposureDot;
				float yDot = axisY.X * vectToChk.X + axisY.Y * vectToChk.Y + axisY.Z * vectToChk.Z;
				if ((csm->slot->bLeanLeft && yDot < -COVERLINK_EdgeCheckDot) ||
					(csm->slot->bLeanRight && yDot > COVERLINK_EdgeCheckDot)) {
					testDot = COVERLINK_EdgeExposureDot;
				}

				float xDot = axisX.X * vectToChk.X + axisX.Y * vectToChk.Y + axisX.Z * vectToChk.Z;
				if (xDot <= testDot) {
					float exposedScale;
					if (xDot > 0.0f) {
						// Threat is still partially in front: scale exposure danger
						exposedScale = 1.0f - (xDot / testDot);
					}
					else {
						// Threat is to the side or behind: great flank, max exposure
						exposedScale = 1.0f;
					}

					// Scale exposure down by distance once past half the max range
					float halfMax = MaxFireLinkDist / 2.0f;
					if (dist > halfMax) {
						exposedScale *= 1.0f - ((dist - halfMax) / halfMax);
					}

					results.push_back({ srcSlot, exposedScale });
				}
			}
		}

		return results;
	}

	void ComputeExposedCover() {
		auto engine = ::Common::FindFirstObject<USFXEngine, true>();
		if (!engine) {
			LEASI_WARN(L"ComputeExposedCover: no engine found.");
			return;
		}
		auto wi = static_cast<ABioWorldInfo*>(engine->GetCurrentWorldInfo());
		auto playerController = wi->GetALocalPlayerController();
		if (!playerController || !playerController->Pawn) {
			LEASI_WARN(L"ComputeExposedCover: no pawn available.");
			return;
		}
		auto* pawn = playerController->Pawn;

		computedExposedCover.clear();

		// Iterate over all cover links and their slots
		int totalSlotsEvaluated = 0;
		for (auto* cl : coverLinks) {
			for (auto* csm : cl->slots) {
				auto exposedCovers = ComputeExposedCoverForSlot(csm, pawn);
				if (!exposedCovers.empty()) {
					computedExposedCover[csm] = std::move(exposedCovers);
				}
				totalSlotsEvaluated++;
			}
		}

		int totalExposures = 0;
		for (const auto& [csm, entries] : computedExposedCover) {
			totalExposures += static_cast<int>(entries.size());
		}
		LEASI_INFO("ComputeExposedCover complete: {} exposure entries across {} slots (out of {} evaluated).",
			totalExposures, computedExposedCover.size(), totalSlotsEvaluated);
	}

	/// <summary>
	/// Computes the Danger Navs for the slot, in front of where the cover points.
	/// </summary>
	/// <param name="csm"></param>
	/// <param name="pawn"></param>
	/// <returns></returns>
	std::vector<DangerNavEntry> ComputeDangerNavsForSlot(CoverSlotMarker* csm, AActor* pawn) {
		std::vector<DangerNavEntry> results;
		FVector zeroExtent{};

		// Compare against all network nodes.
		for (auto& nodePtr : allNetworkNodes) {
			auto* nav = nodePtr.get();
			if (!nav || nav == csm)
				continue;

			FVector slotToNav = {
				nav->position.X - csm->position.X,
				nav->position.Y - csm->position.Y,
				nav->position.Z - csm->position.Z
			};

			float dist = std::sqrt(slotToNav.X * slotToNav.X + slotToNav.Y * slotToNav.Y + slotToNav.Z * slotToNav.Z);
			if (dist > UCONST_COVERLINK_DangerDist)
				continue;

			// Get the slot's forward vector (axisX)
			FVector axisX, axisY, axisZ;
			GetSlotAxes(csm->rotation, axisX, axisY, axisZ);

			// Invert the vectors to point OUT from the wall instead of INTO it
			//axisX.X = -axisX.X; axisX.Y = -axisX.Y; axisX.Z = -axisX.Z;
			//axisY.X = -axisY.X; axisY.Y = -axisY.Y; axisY.Z = -axisY.Z;

			// Normalized dot product check (DotP >= 0 means in front of cover)
			float dotP = (slotToNav.X * axisX.X + slotToNav.Y * axisX.Y + slotToNav.Z * axisX.Z) / (dist > 0.001f ? dist : 1.0f);

			if (dotP >= 0.0f) {
				for (unsigned char action : csm->slot->Actions) {
					FVector viewPt = ComputeFirePosition(csm, action);
					// Trace to Nav->Location + FVector(0,0,20)
					FVector targetPos = { nav->position.X, nav->position.Y, nav->position.Z + 20.0f };
					FVector hitLoc{}, hitNorm{}, extent{};
					FCheckResult hitResult{};
					auto noHit = SingleLineCheck(*GWorld, &hitResult, pawn, &targetPos, &viewPt, TRACE_World | TRACE_StopAtAnyHit | TRACE_ComplexCollision, &extent, nullptr);
					if (noHit == true) {
						float scaleByDist = dist / UCONST_COVERLINK_DangerDist;
						results.push_back({ nav, 1.0f - scaleByDist });
						break;
					}
				}
			}
		}
		return results;
	}


	/// <summary>
	/// For every cover slot, determines which other navigation points can threaten from the front and records the
	/// exposure scale (0..1).
	/// Results are stored in computedDangerNavs, keyed by the destination (threatened) slot.
	/// </summary>
	void ComputeDangerNavs() {
		auto engine = ::Common::FindFirstObject<USFXEngine, true>();
		if (!engine) {
			LEASI_WARN(L"ComputeDangerNavs: no engine found.");
			return;
		}
		auto wi = static_cast<ABioWorldInfo*>(engine->GetCurrentWorldInfo());
		auto playerController = wi->GetALocalPlayerController();
		if (!playerController || !playerController->Pawn) {
			LEASI_WARN(L"ComputeDangerNavs: no pawn available.");
			return;
		}
		auto* pawn = playerController->Pawn;

		computedDangerNavs.clear();

		// Build a flat list of all slots across all cover links
		// This is the list of things that need danger navs built for them.
		struct SlotEntry { CoverSlotMarker* csm; };
		std::vector<SlotEntry> allSlots;
		for (auto* cl : coverLinks) {
			for (auto* csm : cl->slots) {
				allSlots.push_back({ csm });
			}
		}

		if (allSlots.empty()) {
			LEASI_INFO("ComputeDangerNavs: no slots found.");
			return;
		}

		LEASI_INFO("ComputeDangerNavs: evaluating {} slots.", allSlots.size());

		// For all slots in the level...
		for (auto& srcSlot : allSlots) {
			auto dangers = ComputeDangerNavsForSlot(srcSlot.csm, pawn);
			if (!dangers.empty()) {
				computedDangerNavs[srcSlot.csm] = std::move(dangers);
			}
		}

		int totalDangers = 0;
		for (const auto& [csm, entries] : computedDangerNavs) {
			totalDangers += static_cast<int>(entries.size());
		}
		LEASI_INFO("ComputeDangerCovers: {} danger entries across {} threatened slots.", totalDangers, computedDangerNavs.size());
	}
}