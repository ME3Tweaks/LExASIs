#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <cstring>
#include <cwchar>
#include "Common/Objects.hpp"
#include "CoverClient/Client.hpp"
#include "CoverClient/Hooks.hpp"
#include "CoverClient/Utilities/PathUtilities.hpp"
#include "CoverClient/Classes/PathfindingNode.hpp"
#include "CoverClient/Helpers.hpp"
#include "CoverClient/PipeHandler.hpp"

namespace CoverClient
{
	std::wstring currentAction;

	void UpdateLoadScreenMessage() {
		auto interaction = Common::FindFirstObject<USFXGUIInteraction>();
		if (interaction)
		{
			auto saveLoadWidget = interaction->eventGetSaveLoadWidget();
			if (saveLoadWidget)
			{
				saveLoadWidget->ShowLoadingMessage(FALSE, 170447);
			}
		}
	}

	void UpdateCurrentAction(std::wstring_view newAction) {
		currentAction = newAction;
		UpdateLoadScreenMessage();
	}

	/// <summary>
	/// Writes sized data to a pipe by first writing the data size, then the data itself.
	/// </summary>
	/// <param name="pipeHandle">The handle to the pipe to write to.</param>
	/// <param name="size">The size in bytes of the data to write.</param>
	/// <param name="data">A pointer to the data buffer to write.</param>
	void WriteSizedData(HANDLE pipeHandle, DWORD size, const void* data) {
		DWORD numBytesWritten = 0;
		WriteFile(pipeHandle, &size, 4, &numBytesWritten, NULL);
		WriteFile(pipeHandle, data, size, &numBytesWritten, NULL);
	}

	/// <summary>
	/// Writes a command to CoverHost telling it to switch reading modes.
	/// </summary>
	/// <param name="pipeHandle"></param>
	/// <param name="command"></param>
	void WriteCommand(HANDLE pipeHandle, CoverHostCommand command) {
		DWORD numBytesWritten = 0;
		BYTE cmd_sc = static_cast<BYTE>(command);
		WriteFile(pipeHandle, &cmd_sc, 1, &numBytesWritten, NULL);
	}

	// Writes a narrow (ASCII/UTF-8) string in UnrealString format:
	//  - 4 byte length (DWORD) of the string in bytes, not including the null terminator
	//  - raw string bytes (no null terminator)
	void WriteStringA(HANDLE pipeHandle, const char* data) {
		if (data == nullptr) {
			DWORD zero = 0;
			DWORD num = 0;
			WriteFile(pipeHandle, &zero, 4, &num, NULL);
			return;
		}

		DWORD numBytesWritten = 0;
		DWORD len = static_cast<DWORD>(std::strlen(data));
		// Prefix with 4-byte length (not including null)
		WriteFile(pipeHandle, &len, 4, &numBytesWritten, NULL);
		if (len > 0) {
			WriteFile(pipeHandle, data, len, &numBytesWritten, NULL);
		}
	}

	// Writes a wide (UTF-16) string in UnrealString format:
	//  - 4 byte signed value equal to -(length_in_chars * 2)
	//  - UTF-16LE string bytes (length_in_chars * 2), not including the null terminator
	void WriteStringW(HANDLE pipeHandle, const wchar_t* data) {
		if (data == nullptr) {
			int32_t zero = 0;
			DWORD num = 0;
			WriteFile(pipeHandle, &zero, 4, &num, NULL);
			return;
		}

		DWORD numBytesWritten = 0;
		size_t charCount = std::wcslen(data);
		// Unreal expects a negative 4-byte value: -2 * number_of_characters
		int32_t prefix = -static_cast<int32_t>(charCount * sizeof(wchar_t));
		WriteFile(pipeHandle, &prefix, 4, &numBytesWritten, NULL);
		if (charCount > 0) {
			DWORD byteCount = static_cast<DWORD>(charCount * sizeof(wchar_t));
			WriteFile(pipeHandle, data, byteCount, &numBytesWritten, NULL);
		}
	}

	/// The distance in Unreal units that we should separate pathfinding nodes when flood filling the pathfinding network.
	const float NODE_SEPARATION_DISTANCE = 280;
	const float MAX_PATHFINDING_NETWORK_DISTANCE = 800 * 200;
	const float NODE_VISUALIZE_CLIP_DISTANCE = 2500.f;

	/// The height above a point from which we cast downward to find the floor.
	const float FLOOR_TRACE_HEIGHT = 500.0f;

	/// How far down we trace to find a floor below a candidate point.
	const float FLOOR_TRACE_DEPTH = 1000.0f;

	struct VectorHash {
		size_t operator()(const std::tuple<int, int, int>& key) const {
			auto h1 = std::hash<int>{}(std::get<0>(key));
			auto h2 = std::hash<int>{}(std::get<1>(key));
			auto h3 = std::hash<int>{}(std::get<2>(key));
			return h1 ^ (h2 * 2654435761u) ^ (h3 * 40503u);
		}
	};

	/// Snaps a world-space coordinate to the grid defined by NODE_SEPARATION_DISTANCE and returns a hashable key.
	static std::tuple<int, int, int> SnapToGrid(const FVector& v) {
		int gx = static_cast<int>(std::round(v.X / NODE_SEPARATION_DISTANCE));
		int gy = static_cast<int>(std::round(v.Y / NODE_SEPARATION_DISTANCE));
		int gz = static_cast<int>(std::round(v.Z / NODE_SEPARATION_DISTANCE));
		return { gx, gy, gz };
	}

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
		if (nodes.size() == 0) {
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

		for (std::size_t i = 0; i < nodes.size(); i++) {
			DrawDebugCube(player, nodes[i].position, 5.0f, 0, 255, 0, NODE_VISUALIZE_CLIP_DISTANCE);
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
		static FLinearColor TraceLineColorGood{ 0.0f, 1.0f, 0.0f, 1.0f };
		static FLinearColor TraceLineColorBad{ 1.0f, 0.0f, 0.0f, 1.0f };
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

	// Storage for all confirmed node positions
	std::vector<PathfindingNode> nodes;

	// Storage for rejected nodes, grouped by rejection reason
	std::vector<FVector> rejectedNodesNoFloor;
	std::vector<FVector> rejectedNodesSteepSlope;
	std::vector<FVector> rejectedNodesPathBlocked;

	void FloodFillPathfindingNetwork() {
		nodes.clear();
		rejectedNodesNoFloor.clear();
		rejectedNodesSteepSlope.clear();
		rejectedNodesPathBlocked.clear();
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
		auto traceExtent = player->GetCollisionExtent();
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

		// Visited set keyed on snapped grid coordinates
		std::unordered_set<std::tuple<int, int, int>, VectorHash> visited;

		// BFS queue stores world-space positions of confirmed walkable nodes
		std::queue<FVector> frontier;

		// Seed the flood fill with the player's location
		auto seedKey = SnapToGrid(seedLocation);
		visited.insert(seedKey);
		frontier.push(seedLocation);
		nodes.push_back({ seedLocation });

		while (!frontier.empty()) {
			auto size = frontier.size();
			LEASI_TRACE(L"Frontier size: {}", size);
			FVector current = frontier.front();
			frontier.pop();

			// Stop expanding if we're too far from the seed
			float dx = current.X - seedLocation.X;
			float dy = current.Y - seedLocation.Y;
			auto distFromSeed = (dx * dx + dy * dy);
			if (distFromSeed > MAX_PATHFINDING_NETWORK_DISTANCE * MAX_PATHFINDING_NETWORK_DISTANCE) {
				LEASI_ERROR(L"Abort due to too far: {}", distFromSeed);
				continue;
			}

			for (int i = 0; i < 8; i++) {
				FVector candidateEnd = { current.X + directions[i].X, current.Y + directions[i].Y, current.Z + directions[i].Z };

				auto gridKey = SnapToGrid(candidateEnd);
				if (visited.count(gridKey)) {
					// LEASI_WARN(L"HasVisited");
					continue;
				}

				// Horizontal step distance; also the floor trace window to cover the full +-45-degree slope range
				float hx = candidateEnd.X - current.X;
				float hy = candidateEnd.Y - current.Y;
				float horizontalDistance = std::sqrt((hx * hx) + (hy * hy));

				// Floor trace: start above current (catches upslopes) and end below (catches downslopes).
				// The window of +-horizontalDistance from current.Z covers any floor reachable within a 45-degree slope.
				FVector floorTraceStart = { candidateEnd.X, candidateEnd.Y, current.Z + horizontalDistance };
				FVector floorTraceEnd = { candidateEnd.X, candidateEnd.Y, current.Z - horizontalDistance };
				FVector floorHitLocation{};
				FVector floorHitNormal{};
				FVector zeroExtent = { 0.0f, 0.0f, 0.0f };
				FTraceHitInfo traceHitInfo{};
				auto floorHit = player->Trace(floorTraceEnd, floorTraceStart, 0, zeroExtent, FALSE, &floorHitLocation, &floorHitNormal, &traceHitInfo);

				if (floorHit == nullptr || floorHitNormal.Z <= 0.0f) {
					// The wide-range trace failed: either no geometry at all, or the start point
					// was inside a ceiling slab and we exited through its bottom face (downward
					// normal).  Retry with a downward-only trace from current.Z.  That start
					// height is guaranteed to be in open air (current is a confirmed walkable
					// node), so it stays below any ceiling that blocked the first attempt and
					// still finds the floor for same-level or slightly-lower candidates.
					FVector retryStart = { candidateEnd.X, candidateEnd.Y, current.Z };
					FVector retryEnd = { candidateEnd.X, candidateEnd.Y, current.Z - horizontalDistance };
					FVector retryHitLocation{};
					FVector retryHitNormal{};
					FTraceHitInfo retryHitInfo{};
					auto retryHit = player->Trace(retryEnd, retryStart, 0, zeroExtent, FALSE, &retryHitLocation, &retryHitNormal, &retryHitInfo);

					if (retryHit == nullptr || retryHitNormal.Z <= 0.0f) {
						visited.insert(gridKey);
						LEASI_WARN(L"NoFloor");
						rejectedNodesNoFloor.push_back(candidateEnd);
						continue;
					}

					floorHit = retryHit;
					floorHitLocation = retryHitLocation;
					floorHitNormal = retryHitNormal;
				}

				// Reject slopes steeper than 45 degrees by comparing floor heights directly.
				// current.Z - traceExtent.Z reconstructs the floor surface height at the current node.
				//float currentFloorZ = current.Z - traceExtent.Z;
				auto zDiff = std::abs(floorHitLocation.Z - candidateEnd.Z);
				if (zDiff > (horizontalDistance + 1)) {
					// +1 allows for exact values
					// Do not mark gridKey -- the cell may be reachable from a neighbor at a more compatible height
					LEASI_WARN(L"Too big of a change: {} > {}", zDiff, (horizontalDistance + 1));
					rejectedNodesSteepSlope.push_back(candidateEnd);
					continue;
				}

				// Compute the actual traversal-height position at the candidate.
				// Using the slope-adjusted Z prevents the horizontal sweep from clipping through rising geometry.
				FVector actualCandidatePos = { candidateEnd.X, candidateEnd.Y, floorHitLocation.Z + traceExtent.Z };

				// Horizontal trace to the slope-adjusted candidate to check for blocking geometry (walls, obstacles)
				FVector hitLocation{};
				FVector hitNormal{};
				auto hitActor = player->Trace(actualCandidatePos, current, 0, traceExtent, TRUE, &hitLocation, &hitNormal, nullptr);

				if (hitActor != nullptr) {
					// Do not mark gridKey -- a wall between this pair of nodes does not mean the cell is unreachable from another direction
					LEASI_WARN(L"PathBlocked");
					rejectedNodesPathBlocked.push_back(actualCandidatePos);
					continue;
				}

				FVector nodePos = actualCandidatePos;
				auto nodeKey = SnapToGrid(nodePos);
				if (visited.count(nodeKey)) {
					visited.insert(gridKey);
					LEASI_WARN(L"Already visited this node");
					continue;
				}

				LEASI_TRACE(L">> Adding node.");
				visited.insert(gridKey);
				visited.insert(nodeKey);
				frontier.push(nodePos);
				nodes.push_back({ nodePos });
			}
		}

		// Print all discovered nodes
		LEASI_INFO("FloodFill: Found {} pathfinding nodes", nodes.size());
		for (size_t i = 0; i < nodes.size(); i++) {
			LEASI_INFO("  Node {}: ({}, {}, {})", i, nodes[i].position.X, nodes[i].position.Y, nodes[i].position.Z);
		}
	}

	void ComputeReachSpecs() {
		for (auto& node : nodes) {
			node.reachableIndices.clear();
		}
		if (nodes.empty()) {
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
			return;
		}
		auto traceExtent = player->GetCollisionExtent();

		// Build an XY-keyed spatial lookup: (grid_x, grid_y) -> node index.
		// Keyed only on XY because floor-adjusted Z varies across neighbors.
		struct XYHash {
			size_t operator()(const std::pair<int, int>& key) const {
				return std::hash<int>{}(key.first) ^ (std::hash<int>{}(key.second) * 2654435761u);
			}
		};
		std::unordered_map<std::pair<int, int>, size_t, XYHash> gridToIndex;
		gridToIndex.reserve(nodes.size());
		for (size_t i = 0; i < nodes.size(); i++) {
			auto key = SnapToGrid(nodes[i].position);
			gridToIndex[{ std::get<0>(key), std::get<1>(key) }] = i;
		}

		// All 8 grid-neighbor offsets.
		const int neighborOffsets[8][2] = {
			{  1,  0 }, { -1,  0 }, {  0,  1 }, {  0, -1 },
			{  1,  1 }, {  1, -1 }, { -1,  1 }, { -1, -1 },
		};

		int reachspecCount = 0;
		for (size_t i = 0; i < nodes.size(); i++) {
			ComputeApproximateLineOfFire(&nodes[i], player); // Compute the line of fire here as well
			SizeNode(&nodes[i], player);
			auto keyTuple = SnapToGrid(nodes[i].position);
			int gx = std::get<0>(keyTuple);
			int gy = std::get<1>(keyTuple);

			for (const auto& offset : neighborOffsets) {
				auto it = gridToIndex.find({ gx + offset[0], gy + offset[1] });
				if (it == gridToIndex.end()) {
					continue;
				}
				size_t j = it->second;
				if (j <= i) {
					continue; // Already evaluated this pair
				}

				FVector hitLocation{};
				FVector hitNormal{};

                // Find the largest collision cylinder we can that allows us to path to the target
                for (size_t csIdx = 0; csIdx < CollisionCylinderSizes.size(); csIdx++) {
                    const auto& cylinderSize = CollisionCylinderSizes[csIdx];
                    auto hitActor = player->Trace(nodes[j].position, nodes[i].position, 0, cylinderSize, TRUE, &hitLocation, &hitNormal, nullptr);
                    if (hitActor == nullptr) {
                        // Record reachability with the extent used (radius in X, height in Z)
                        PathfindingNode::ReachSpec a{ static_cast<int>(j), cylinderSize.X, cylinderSize.Z };
                        PathfindingNode::ReachSpec b{ static_cast<int>(i), cylinderSize.X, cylinderSize.Z };
                        nodes[i].reachableIndices.push_back(a);
                        nodes[j].reachableIndices.push_back(b);
                        reachspecCount++;
                        break;
                    }
                }
			}
		}

		LEASI_INFO("ComputeReachSpecs: Found {} reachspecs across {} nodes", reachspecCount, nodes.size());

		// Now we need to send the data to the host.
		SendDataToCoverHost();
	}

	void SendDataToCoverHost() {
		auto pipeHandle = CreateHostPipe();

		// PersistentLevel -> TheWorld -> ContainingPackage
		auto worldName = (*GWorld)->PersistentLevel->Outer->Outer->Name.GetName();
		LEASI_INFO("WorldName: {}", worldName);
		WriteCommand(pipeHandle, CoverHostCommand::LevelName);
		WriteStringA(pipeHandle, worldName);

		// Write out the node data
		DWORD numBytesWritten = 0;
		// Send a command byte first so the host knows what to expect
		WriteCommand(pipeHandle, CoverHostCommand::PathfindingNodeList);
		// Then send the number of nodes
		DWORD nodeCount = static_cast<DWORD>(nodes.size());
		WriteFile(pipeHandle, &nodeCount, 4, &numBytesWritten, NULL);

		// Then send each node's data: position (3 floats) followed by the count of reachable nodes and their indices
		for (const auto& node : nodes) {
			WriteFile(pipeHandle, &node.position.X, 4, &numBytesWritten, NULL);
			WriteFile(pipeHandle, &node.position.Y, 4, &numBytesWritten, NULL);
			WriteFile(pipeHandle, &node.position.Z, 4, &numBytesWritten, NULL);

			// Write out approximate line of fire
			WriteFile(pipeHandle, &node.approximateLineOfFire, 4, &numBytesWritten, NULL);

			// Write out collision cylinder size as two ints
			if (node.collisionCylinderSize.X == 0 || node.collisionCylinderSize.Z == 0) {
				LEASI_ERROR("Node has invalid collision cylinder size: ({}, {})", node.collisionCylinderSize.X, node.collisionCylinderSize.Z);
			}
			WriteFile(pipeHandle, &node.collisionCylinderSize.X, 4, &numBytesWritten, NULL);
			WriteFile(pipeHandle, &node.collisionCylinderSize.Z, 4, &numBytesWritten, NULL);

            // Write out reachspecs: for each reachable entry write target index (int), radius (float), height (float)
            DWORD reachableCount = static_cast<DWORD>(node.reachableIndices.size());
            WriteFile(pipeHandle, &reachableCount, 4, &numBytesWritten, NULL);
            for (const auto& rs : node.reachableIndices) {
                WriteFile(pipeHandle, &rs.index, 4, &numBytesWritten, NULL);
                WriteFile(pipeHandle, &rs.radius, 4, &numBytesWritten, NULL);
                WriteFile(pipeHandle, &rs.height, 4, &numBytesWritten, NULL);
            }
		}
	}
}