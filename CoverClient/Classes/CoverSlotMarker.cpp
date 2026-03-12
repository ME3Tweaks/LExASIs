#include "CoverClient/Classes/CoverSlotMarker.hpp"

namespace CoverClient
{

	void CoverSlotMarker::WriteNodeToHost() const {
		NavigationPoint::WriteNodeToHost();

		// Write custom data here.
	}

	const char* CoverSlotMarker::GetNavName() const {
		return "CoverSlotMarker";
	}

	void CoverSlotMarker::InitializeSlot() {
		// The default slot struct has a bunch of values that we need to set to the actual 'invalid' state.
		slot.MantleTarget.SlotIdx = -1;
		slot.bEnabled = true;
		slot.CoverType = 2; // CT_MidLevel
		slot.Actions.Add(5); //popup

		// These are also from the struct defaults; no idea if we should use them, tune as needed
		slot.bCanMantle = true;
		slot.bCanCoverSlip_Left = true;
		slot.bCanCoverSlip_Right = true;
		slot.bAllowPopup = true;
		slot.bAllowMantle = true;
		slot.bAllowSwatTurn = true;
		slot.bAllowCoverTurn = true;
	}
}