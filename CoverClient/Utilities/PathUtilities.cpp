#pragma once

#include "LESDK/Headers.hpp"
#include "Common/Objects.hpp"
#include "CoverClient/Utilities/PathUtilities.hpp"
#include "CoverClient/CoverGeneration.hpp"
#include "CoverClient/Classes/CoverLink.hpp"
#include "CoverClient/Classes/CoverSlotMarker.hpp"
#include "CoverClient/Classes/NavigationPoint.hpp"
#include <memory>
#include <numbers>

namespace CoverClient {
	std::vector<std::unique_ptr<NavigationPoint>> allNetworkNodes;

	bool DoesSizeFitAtLocation(NavigationPoint* Nav, int Radius, int Height) {
		auto actor = static_cast<AActor*>(AActor::StaticClass()->ClassDefaultObject);

		FVector HitLocation;
		FVector HitNormal;
		FTraceHitInfo HitInfo;

		// Extent is a vector representing the "half-sizes" of your box.
		// X and Y are the Radius, Z is the Height.
		FVector Extent{ (float)Radius / 2, (float)Radius / 2, (float)Height / 2 };

		// Trace from the location to the exact same location.
		// The 'true' bool tells it to trace against actors, not just the world.
		auto HitActor = actor->Trace(Nav->position, Nav->position, FALSE, Extent, 0, &HitLocation, &HitNormal, &HitInfo);

		// If HitActor is None, nothing was hit, meaning the space is clear.
		return HitActor == nullptr;
	}

	// Convert a direction vector to a rotator (Pitch, Yaw, Roll).
	// Pitch = rotation around Y (look up/down), Yaw = rotation around Z (turn), Roll = 0.
	FRotator FVectorToFRotator(const FVector& v)
	{
		// Use atan2 for robust angle calculation. Pitch is based on Z vs XY length.
		const float RAD_TO_DEG = 180.0f / 3.14159265358979323846f;
		const float DEG_TO_UNREAL = 65536.0f / 360.0f; // Unreal rotation units per degree

		float x = (float)v.X;
		float y = (float)v.Y;
		float z = (float)v.Z;

		// Compute angles in degrees in range (-180,180]
		float yawDeg = std::atan2(y, x) * RAD_TO_DEG;
		float pitchDeg = std::atan2(z, std::sqrt(x * x + y * y)) * RAD_TO_DEG;

		// Normalize to [0,360)
		auto normalize360 = [](float deg) {
			float d = std::fmod(deg, 360.0f);
			if (d < 0.0f) d += 360.0f;
			return d;
			};

		yawDeg = normalize360(yawDeg);
		pitchDeg = normalize360(pitchDeg);

		// Convert to Unreal rotation units (0..65535)
		auto toUnreal = [&](float deg) -> int {
			// Round to nearest integer and wrap to 16-bit range
			int val = static_cast<int>(std::round(deg * DEG_TO_UNREAL)) & 0xFFFF;
			return val;
			};

		return FRotator(toUnreal(pitchDeg), toUnreal(yawDeg), 0);
	}

	// Crazy math to convert World to Local offset
	// Thank you Gemini
	// 1. Calculate the rotation axes(Forward, Right, Up) from your Euler rotation.
	// UE3 uses degrees and a specific order: Yaw, Pitch, Roll.
	void GetAxes(float Pitch, float Yaw, float Roll, FVector& X, FVector& Y, FVector& Z) {
		float fP = (float)(Pitch * (std::numbers::pi / 180.0f));
		float fY = (float)(Yaw * (std::numbers::pi / 180.0f));
		float fR = (float)(Roll * (std::numbers::pi / 180.0f));

		float SP = sin(fP), CP = cos(fP);
		float SY = sin(fY), CY = cos(fY);
		float SR = sin(fR), CR = cos(fR);

		X = FVector(CP * CY, CP * SY, SP);
		Y = FVector(SR * SP * CY - CR * SY, SR * SP * SY + CR * CY, -SR * CP);
		Z = FVector(-(CR * SP * CY + SR * SY), CY * SR - CR * SP * SY, CR * CP);
	}

	float UnrRotToDeg(int UnrealRotation) {
		// We use 65536.0f (2^16) because that represents the full circle wrap-around
		return (float)UnrealRotation * (360.0f / 65536.0f);
	}

	// Computes the location offset from a given location with a rotation.
	FVector CalculateLocationOffset(FVector MyLoc, FRotator MyRot, FVector TargetLoc) {
		// Difference in world space (TargetLoc - MyLoc)
		FVector RelativePos = TargetLoc - MyLoc;

		// Get the orientation axes of "MyRot"
		FVector AxisX, AxisY, AxisZ;
		GetAxes(UnrRotToDeg(MyRot.Pitch), UnrRotToDeg(MyRot.Yaw), UnrRotToDeg(MyRot.Roll), AxisX, AxisY, AxisZ);

		// InverseTransform (Rotate the relative vector into local space)
		// This is equivalent to: LocalVector = WorldVector * Transpose(RotationMatrix)
		// Each row is RelativePos dotted with one axis. The Z terms were AxisX.X and AxisY.Y
		// rather than AxisX.Z / AxisY.Z, which silently corrupted every serialised
		// LocationOffset the moment a slot sat at a different height from its link -- which
		// is always, because AutoAdjustSlot ground-snaps slots and the link does not move.
		return FVector(
			(RelativePos.X * AxisX.X) + (RelativePos.Y * AxisX.Y) + (RelativePos.Z * AxisX.Z),
			(RelativePos.X * AxisY.X) + (RelativePos.Y * AxisY.Y) + (RelativePos.Z * AxisY.Z),
			(RelativePos.X * AxisZ.X) + (RelativePos.Y * AxisZ.Y) + (RelativePos.Z * AxisZ.Z)
		);
	}

	// This is a reimplementation of WorldInfo->AllNavigationPoints. That uses an unreal iterator,
	// which sucks to use in our SDK
	std::vector<ANavigationPoint*> GetAllNavigationPoints(UClass* baseClass) {
		std::vector<ANavigationPoint*> navPoints;
		auto engine = ::Common::FindFirstObject<USFXEngine, true>();
		if (!engine) {
			return navPoints;
		}
		auto wi = static_cast<ABioWorldInfo*>(engine->GetCurrentWorldInfo());

		// Reimplementation
		if (wi->NavigationPointList != nullptr)
		{
			ANavigationPoint* nav = wi->NavigationPointList;

			// Decomp shows it checks for navigation point as baseclass and runs different branch if it isn't
			if (baseClass && baseClass != ANavigationPoint::StaticClass())
			{
				// get the next NavigationPoint in the iteration
				while (nav != nullptr)
				{
					if (nav->IsA(baseClass))
					{
						navPoints.push_back(nav);
					}
					nav = nav->nextNavigationPoint;
					if (nav == nullptr)
					{
						// No more nav points.
						break;
					}
				}
			}
			else
			{
				// Decomp shows there is version that doesn't do a IsA() cast check
				// get the next NavigationPoint in the iteration
				while (nav != nullptr)
				{
					navPoints.push_back(nav);
					nav = nav->nextNavigationPoint;
					if (nav == nullptr)
					{
						// No more nav points.
						break;
					}
				}
			}
		}
		return navPoints;
	}

	void LoadExistingLevelNodes() {

		// Clear existing data
		allNetworkNodes.clear();
		coverLinks.clear();

		auto navList = GetAllNavigationPoints(nullptr);

		for (auto navPoint : navList) {
			if (navPoint->IsA(ACoverLink::StaticClass())) {
				auto sdkLink = static_cast<ACoverLink*>(navPoint);

				auto cl = std::make_unique<CoverLink>(sdkLink->location, sdkLink->Rotation);

				// Create CoverSlotMarkers for each slot in the link
				for (int slotIdx = 0; slotIdx < (int)sdkLink->Slots.Count(); slotIdx++) {
					FCoverSlot& sdkSlot = sdkLink->Slots(slotIdx);

					FVector slotPos = sdkLink->GetSlotLocation(slotIdx, FALSE);
					FRotator slotRot = sdkLink->GetSlotRotation(slotIdx, FALSE);

					// InitializeSlot sets defaults; we overwrite with the actual level data below
					allNetworkNodes.push_back(std::make_unique<CoverSlotMarker>(slotPos, slotRot));
					const auto& csm = static_cast<CoverSlotMarker*>(allNetworkNodes.back().get());
					csm->networkIndex = (int)allNetworkNodes.size() - 1;
					csm->slot = &sdkSlot;

					cl->AddCoverSlot(csm);
				}

				// CoverLink is added after its slots, matching the generation pattern
				allNetworkNodes.push_back(std::move(cl));
				coverLinks.push_back(static_cast<CoverLink*>(allNetworkNodes.back().get()));
			}
			else if (navPoint->IsA(ACoverSlotMarker::StaticClass())) {
				// Skip standalone CoverSlotMarkers; they are handled via their parent CoverLink
				continue;
			}
			else {
				// Plain navigation point
				allNetworkNodes.push_back(std::make_unique<NavigationPoint>(navPoint->location, navPoint->Rotation));
			}
		}

		LEASI_INFO("Loaded {} existing network nodes ({} cover links) from level.", allNetworkNodes.size(), coverLinks.size());

		DrawCover();
	}
}