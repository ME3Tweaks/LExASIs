#pragma once

#include "CoverClient/Classes/NavigationPoint.hpp"
#include "CoverClient/Classes/CoverSlotMarker.hpp"
#include <LESDK/Headers.hpp>
#include <vector>

namespace CoverClient {
	/// <summary>
	/// Describes a coverlink node in the level
	/// </summary>
    class CoverLink : public NavigationPoint {
    public:
        // Non-owning pointers to the cover slots. Ownership is held by the global
        // network list (allNetworkNodes). This allows the CoverLink to reference
        // its slots for rendering without owning them.
        std::vector<CoverSlotMarker*> slots;

        CoverLink(FVector position, FRotator rotation) : NavigationPoint(position, rotation) {}

        void WriteNodeToHost() const override;
        const char* GetNavName() const override;
        // Adds a non-owning pointer to a slot (caller retains/assigns ownership).
        void AddCoverSlot(CoverSlotMarker* marker);
    };
}