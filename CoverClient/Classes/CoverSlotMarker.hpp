#pragma once

#include "LESDK/Headers.hpp"
#include "CoverClient/Classes/NavigationPoint.hpp"
#include <string>
#include <vector>

namespace CoverClient
{
    /// <summary>
    /// A single detected cover position along a surface
    /// </summary>
    class CoverSlotMarker : public NavigationPoint {
    public:
        int networkIndex = -1; // If we see -1 it means it's not properly set.

        CoverSlotMarker(FVector position, FRotator rotation) : NavigationPoint(position, rotation) {
            InitializeSlot();
        }
        void WriteNodeToHost() const override;
        const char* GetNavName() const override;
		void InitializeSlot();

		// Slot information is stored here, but is contextually relevant to the CoverLink that owns this slot, so it is written out as part of that.
		FCoverSlot* slot = nullptr;
		std::unique_ptr<FCoverSlot> ownedSlot; // Used for generated slots that aren't pointed into engine memory

		// Arrays        
        /*
        std::vector<std::string> Actions;
        std::vector<FFireLink>      FireL]inks;
        std::vector<int>         ExposedCoverPackedProperties;
        std::vector<int>         DangerCoverPackedProperties;
        std::vector<FCoverReference>      SlipTarget;
        std::vector<FSlotMoveRef>      SlipRefs;
        std::vector<FCoverReference>      OverlapClaims;

        // Structs / objects
        FCoverReference   MantleTarget;
        FVector  LocationOffset = { 0, 0, 0 };
        FRotator RotationOffset = { 0, 0, 0 };
        int      SlotOwner      = 0;  // always 0; only set at runtime
        int      SlotMarker     = 0;

        // Ints / float
        int   TurnTargetPackedProperties      = -1;
        int   CoverTurnTargetPackedProperties  = -1;
        int   ExtraCost                        = 0;
        float LeanTraceDist                    = 0;

        // Booleans
        bool bLeanLeft               = false;
        bool bLeanRight              = false;
        bool bForceCanPopUp          = true;
        bool bCanPopUp               = true;
        bool bCanMantle              = false;
        bool bCanClimbUp             = false;
        bool bForceCanCoverSlip_Left  = false;
        bool bForceCanCoverSlip_Right = false;
        bool bCanCoverSlip_Left       = false;
        bool bCanCoverSlip_Right      = false;
        bool bCanSwatTurn_Left        = false;
        bool bCanSwatTurn_Right       = false;
        bool bCanCoverTurn_Left       = false;
        bool bCanCoverTurn_Right      = false;

        bool bEnabled             = true;
        bool bAllowPopup          = true;
        bool bAllowMantle         = false;
        bool bAllowCoverSlip      = true;
        bool bAllowClimbUp        = false;
        bool bAllowSwatTurn       = true;
        bool bAllowCoverTurn      = true;
        bool bForceNoGroundAdjust  = true;
        bool bPlayerOnly          = false;
        bool bUnsafeCover         = false;
        bool bFailedToFindSurface  = false;

        // Enums (stored as string value name)
        std::string ForceCoverType      = "CT_MidLevel";
        std::string CoverType           = "CT_MidLevel";
        std::string LocationDescription = "CoverDesc_None";
        */
    };
}