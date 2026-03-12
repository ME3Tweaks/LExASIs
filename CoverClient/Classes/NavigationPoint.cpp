#include "Common/Objects.hpp"
#include "CoverClient/Client.hpp"
#include "CoverClient/Hooks.hpp"
#include "CoverClient/Utilities/PathUtilities.hpp"
#include "CoverClient/Classes/PathfindingNode.hpp"
#include "CoverClient/Helpers.hpp"
#include "CoverClient/PipeHandler.hpp"
#include "CoverClient/Classes/NavigationPoint.hpp"

namespace CoverClient {

	const char* NavigationPoint::GetNavName() const {
		return "NavigationPoint";
	}

	void NavigationPoint::WriteNodeToHost() const {
		auto pipeHandle = CreateHostPipe();

		WriteStringA(GetNavName());

		DWORD numBytesWritten = 0;
		WriteFile(pipeHandle, &position.X, 4, &numBytesWritten, NULL);
		WriteFile(pipeHandle, &position.Y, 4, &numBytesWritten, NULL);
		WriteFile(pipeHandle, &position.Z, 4, &numBytesWritten, NULL);

		WriteFile(pipeHandle, &rotation.Pitch, 4, &numBytesWritten, NULL);
		WriteFile(pipeHandle, &rotation.Yaw, 4, &numBytesWritten, NULL);
		WriteFile(pipeHandle, &rotation.Roll, 4, &numBytesWritten, NULL);

		// Write out approximate line of fire
		WriteFile(pipeHandle, &approximateLineOfFire, 4, &numBytesWritten, NULL);

		// Write out collision cylinder size as two ints
		if (collisionCylinderSize.X == 0 || collisionCylinderSize.Z == 0) {
			LEASI_ERROR("Node has invalid collision cylinder size: ({}, {})", collisionCylinderSize.X, collisionCylinderSize.Z);
		}
		WriteFile(pipeHandle, &collisionCylinderSize.X, 4, &numBytesWritten, NULL);
		WriteFile(pipeHandle, &collisionCylinderSize.Z, 4, &numBytesWritten, NULL);

		// Write out reachspecs: for each reachable entry write target index (int), radius (float), height (float)
		DWORD reachableCount = static_cast<DWORD>(reachableIndices.size());
		WriteFile(pipeHandle, &reachableCount, 4, &numBytesWritten, NULL);
		for (const auto& rs : reachableIndices) {
			WriteFile(pipeHandle, &rs.index, 4, &numBytesWritten, NULL);
			WriteFile(pipeHandle, &rs.radius, 4, &numBytesWritten, NULL);
			WriteFile(pipeHandle, &rs.height, 4, &numBytesWritten, NULL);
		}
	}

	void NavigationPoint::ComputeApproximateLineOfFire()
	{
		// --- THE MAIN CALCULATION ---
		auto actor = static_cast<AActor*>(AActor::StaticClass()->ClassDefaultObject);

		// Define our constants based on the original code
		const int NUM_SECTORS = 10;
		const int BITS_PER_SECTOR = 3;
		const float MAX_TRACE_DIST = 4000.0f;

		int PackedData = 0;

		// Divide 360 degrees into 10 slices (36 degrees each)
		float AngleStep = 360.0f / NUM_SECTORS;

		for (int i = 0; i < NUM_SECTORS; ++i) {
			// 1. Calculate the direction vector for this sector
			float CurrentAngle = i * AngleStep;
			float Radian = CurrentAngle * (3.14159f / 180.0f);

			FVector Dir;
			Dir.X = std::cos(Radian);
			Dir.Y = std::sin(Radian);
			Dir.Z = 0.0f; // Assuming 2D horizontal sweep

			// 2. Perform the raycast using the game's Trace API to see how much open space we have.
			// Build start/end positions from the Nav position and the direction vector.
			// Todo: Improve this by firing multiple raycasts per sector in a small cone, 
			// then averaging the results. This would reduce noise and better capture thin cover.
			FVector rayStart = { position.X, position.Y, position.Z };
			FVector rayEnd = { rayStart.X + Dir.X * MAX_TRACE_DIST, rayStart.Y + Dir.Y * MAX_TRACE_DIST, rayStart.Z + Dir.Z * MAX_TRACE_DIST };
			FVector rayHitLocation{};
			FVector rayHitNormal{};
			FVector zeroExtent = { 0.0f, 0.0f, 0.0f };
			auto rayHitActor = actor->Trace(rayEnd, rayStart, 0, zeroExtent, TRUE, &rayHitLocation, &rayHitNormal, nullptr);

			float HitDist;
			if (rayHitActor != nullptr) {
				float dx = rayHitLocation.X - rayStart.X;
				float dy = rayHitLocation.Y - rayStart.Y;
				float dz = rayHitLocation.Z - rayStart.Z;
				HitDist = std::sqrt((dx * dx) + (dy * dy) + (dz * dz));
			}
			else {
				HitDist = MAX_TRACE_DIST;
			}

			// 3. Convert that distance to our 0-7 value
			int SectorValue = QuantizeDistance(HitDist);

			// 4. Pack the bits into the integer
			// Shift the 3-bit value to the correct slot and OR it in
			PackedData |= (SectorValue << (i * BITS_PER_SECTOR));
		}

		approximateLineOfFire = PackedData;
	}

	/// <summary>
	/// List of collision cylinder sizes from largest to smallest
	/// </summary>
	const std::vector<FVector> CollisionCylinderSizes = {
		FVector{140.0f, 140.0f, 195.0f}, // BOSS
		FVector{115.0f, 115.0f, 145.0f}, // BRUTE
		FVector{105.0f, 105.0f, 145.0f}, // MINIBOSS
		FVector{50.0f,  50.0f,  125.0f}, // BANSHEE
		FVector{34.0f,  34.0f,  90.0f}  // MOOK
	};

	// Maps a physical distance to the 0-7 integer scale for approximate line of fire.
	int QuantizeDistance(float Dist) {
		if (Dist >= 4000.0f) return 7;
		if (Dist >= 3000.0f) return 6;
		if (Dist >= 2400.0f) return 5;
		if (Dist >= 1800.0f) return 4;
		if (Dist >= 1250.0f) return 3;
		if (Dist >= 800.0f)  return 2;
		if (Dist >= 400.0f)  return 1;
		return 0;
	}


	void NavigationPoint::SizeNode()
	{
		for (size_t i = 0; i < CollisionCylinderSizes.size(); i++) {
			const auto& size = CollisionCylinderSizes[i];
			if (DoesSizeFitAtLocation(this, static_cast<int>(size.X), static_cast<int>(size.Z))) {
				// This size fits
				collisionCylinderSize = size;
				return;
			}
		}

		// If none of the sizes fit, we can either leave it as is or set a special value. Here we leave it as is.
		LEASI_ERROR("No valid pathfinding node size!");
	}
}