#include "CoverClient/Classes/ReachSpecs.hpp"
#include "CoverSlotMarker.hpp"

namespace CoverClient {
    ReachSpec::ReachSpec(int idx, float r, float h)
        : index(idx), radius(r), height(h) {}

    SlotToSlotReachSpec::SlotToSlotReachSpec(int idx, float r, float h)
        : ReachSpec(idx, r, h) {}

	const char* ReachSpec::ClassName() const { return "ReachSpec"; }
	const char* SlotToSlotReachSpec::ClassName() const { return "SlotToSlotReachSpec"; }

    ReachSpec ReachSpec::GetReachSpecType(const NavigationPoint* fromNode, const NavigationPoint* toNode) {
        if (dynamic_cast<const CoverSlotMarker*>(fromNode) && dynamic_cast<const CoverSlotMarker*>(toNode)) {
            // Both nodes are cover slots, so this is a SlotToSlotReachSpec
            return SlotToSlotReachSpec(0, 0.0f, 0.0f); // Placeholder values for index, radius, and height
        }
        // Default to a generic ReachSpec for other node types
        return ReachSpec(0, 0.0f, 0.0f); // Placeholder values for index, radius, and height
	}
}