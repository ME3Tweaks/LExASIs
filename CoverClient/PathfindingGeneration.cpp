#include <unordered_map>
#include <unordered_set>
#include <queue>
#include "Common/Objects.hpp"
#include "CoverClient/PathfindingGeneration.hpp"
#include "CoverClient/Utilities/PathUtilities.hpp"
#include "CoverClient/Classes/PathfindingNode.hpp"

namespace CoverClient {

#pragma region Pathfinding Network Generation

    struct VectorHash {
        size_t operator()(const std::tuple<int, int, int>& key) const {
            auto h1 = std::hash<int>{}(std::get<0>(key));
            auto h2 = std::hash<int>{}(std::get<1>(key));
            auto h3 = std::hash<int>{}(std::get<2>(key));
            return h1 ^ (h2 * 2654435761u) ^ (h3 * 40503u);
        }
    };

    /// Snaps a world-space coordinate to the grid defined by NODE_SEPARATION_DISTANCE and returns a hashable key.
    /// Only use for non-cover nodes! Cover nodes do not use the grid.
    static std::tuple<int, int, int> SnapToGrid(const FVector& v) {
        int gx = static_cast<int>(std::round(v.X / NODE_SEPARATION_DISTANCE));
        int gy = static_cast<int>(std::round(v.Y / NODE_SEPARATION_DISTANCE));
        int gz = static_cast<int>(std::round(v.Z / NODE_SEPARATION_DISTANCE));
        return { gx, gy, gz };
    }

    // Storage for rejected nodes, grouped by rejection reason
    std::vector<FVector> rejectedNodesNoFloor;
    std::vector<FVector> rejectedNodesSteepSlope;
    std::vector<FVector> rejectedNodesPathBlocked;

    void FloodFillPathfindingNetwork() {
        allNetworkNodes.clear();
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
        allNetworkNodes.push_back(std::unique_ptr<PathNode>(new PathNode(seedLocation, FRotator{}))); // Don't care about the rotation

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
                allNetworkNodes.emplace_back(std::unique_ptr<PathNode>(new PathNode(nodePos, FRotator{}))); // Don't care about the rotation
            }
        }

        // Print all discovered nodes
        LEASI_INFO("FloodFill: Found {} pathfinding nodes", allNetworkNodes.size());
        for (size_t i = 0; i < allNetworkNodes.size(); i++) {
            LEASI_INFO("  Node {}: ({}, {}, {})", i, allNetworkNodes[i]->position.X, allNetworkNodes[i]->position.Y, allNetworkNodes[i]->position.Z);
        }
    }

    void ComputeReachSpecs() {
        for (auto& node : allNetworkNodes) {
            node->reachableIndices.clear();
        }
        if (allNetworkNodes.empty()) {
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
        gridToIndex.reserve(allNetworkNodes.size());
        for (size_t i = 0; i < allNetworkNodes.size(); i++) {
            auto key = SnapToGrid(allNetworkNodes[i]->position);
            gridToIndex[{ std::get<0>(key), std::get<1>(key) }] = i;
        }

        // All 8 grid-neighbor offsets.
        const int neighborOffsets[8][2] = {
            {  1,  0 }, { -1,  0 }, {  0,  1 }, {  0, -1 },
            {  1,  1 }, {  1, -1 }, { -1,  1 }, { -1, -1 },
        };

        int reachspecCount = 0;
        for (size_t i = 0; i < allNetworkNodes.size(); i++) {
            allNetworkNodes[i]->ComputeApproximateLineOfFire(); // Compute the line of fire here as well
            allNetworkNodes[i]->SizeNode();
            auto keyTuple = SnapToGrid(allNetworkNodes[i]->position);
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
                    auto hitActor = player->Trace(allNetworkNodes[j]->position, allNetworkNodes[i]->position, 0, cylinderSize, TRUE, &hitLocation, &hitNormal, nullptr);
                    if (hitActor == nullptr) {
                        // Record reachability with the extent used (radius in X, height in Z)
                        ReachSpec a;
                        a.index = static_cast<int>(j);
                        a.radius = cylinderSize.X;
                        a.height = cylinderSize.Z;
                        ReachSpec b;
                        b.index = static_cast<int>(i);
                        b.radius = cylinderSize.X;
                        b.height = cylinderSize.Z;
                        allNetworkNodes[i]->reachableIndices.push_back(a);
                        allNetworkNodes[j]->reachableIndices.push_back(b);
                        reachspecCount++;
                        break;
                    }
                }
            }
        }

        LEASI_INFO("ComputeReachSpecs: Found {} reachspecs across {} nodes", reachspecCount, allNetworkNodes.size());
    }
#pragma endregion Pathfinding Network Generation

}