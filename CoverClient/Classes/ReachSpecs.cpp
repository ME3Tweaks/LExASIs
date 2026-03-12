#include "CoverClient/Classes/ReachSpecs.hpp"

namespace CoverClient {
    ReachSpec::ReachSpec(int idx, float r, float h)
        : index(idx), radius(r), height(h) {}

    SlotToSlotReachSpec::SlotToSlotReachSpec(int idx, float r, float h)
        : ReachSpec(idx, r, h) {}

	const char* ReachSpec::ClassName() const { return "ReachSpec"; }
	const char* SlotToSlotReachSpec::ClassName() const { return "SlotToSlotReachSpec"; }
}