#pragma once

#include <LESDK/Headers.hpp>
#include <cstring>
#include "Common/Base.hpp"

namespace LEXInterop
{
    // Shared data cached from game state for use by other features.
    // Updated automatically during ProcessEvent on PlayerTick.
    class SharedData
    {
    public:
        // The player camera POV
        static inline FTPOV CachedPlayerPOV{};

        // The player location
        static inline FVector CachedPlayerPosition{};

        // Process game events to update cached data.
        // Always returns true to allow other handlers to process.
        static bool ProcessEvent(UObject* Context, UFunction* Function, void* Parms, void* Result)
        {
            LEASI_UNUSED_2(Parms, Result);

            if (Context->IsA<ABioPlayerController>() &&
                Function->GetName().Equals(L"PlayerTick"))
            {
                auto playerController = static_cast<ABioPlayerController*>(Context);
                if (playerController->PlayerCamera != nullptr)
                {
                    CachedPlayerPOV = playerController->PlayerCamera->CameraCache.POV;
                    if (playerController->Pawn)
                    {
                        CachedPlayerPosition = playerController->Pawn->LOCATION;
                    }
                }
            }

            return true; // Always allow other handlers
        }
    };
}
