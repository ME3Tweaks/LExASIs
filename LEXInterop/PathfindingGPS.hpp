#pragma once

#include <sstream>
#include <cmath>
#include <LESDK/Headers.hpp>
#include "Utilities.hpp"
#include "Common/Base.hpp"

namespace LEXInterop
{
    // Sends player GPS data (position and rotation) to LEX in real-time
    class PathfindingGPS
    {
    public:
        // Whether the GPS is currently active
        static inline bool IsActive = false;

        // Handle pipe commands for GPS activation
        static bool HandleCommand(char* command)
        {
            if (IsCmd(&command, "ACTIVATE_PLAYERGPS"))
            {
                LEASI_INFO("Activating player GPS");
                IsActive = true;
                return true;
            }
            if (IsCmd(&command, "DEACTIVATE_PLAYERGPS"))
            {
                LEASI_INFO("Deactivating player GPS");
                IsActive = false;
                return true;
            }
            return false;
        }

        // Process game events to send GPS data.
        // Returns true to allow other handlers to process.
        static bool ProcessEvent(UObject* Context, UFunction* Function, void* Parms, void* Result)
        {
            LEASI_UNUSED_2(Parms, Result);

            if (!IsActive) return true;

            if (Context->IsA<ABioPlayerController>() && Function->GetName().Equals(L"PlayerTick"))
            {
                auto playerController = static_cast<ABioPlayerController*>(Context);
                if (playerController->Pawn)
                {
                    const auto& loc = playerController->Pawn->LOCATION;
                    const auto& rot = playerController->Pawn->Rotation;

                    // Only send if position or rotation changed
                    if (!ApproximatelyEqual(loc.X, s_LastX) ||
                        !ApproximatelyEqual(loc.Y, s_LastY) ||
                        !ApproximatelyEqual(loc.Z, s_LastZ) ||
                        rot.Pitch != s_LastPitch ||
                        rot.Yaw != s_LastYaw ||
                        rot.Roll != s_LastRoll)
                    {
                        // Send position
                        std::wstringstream ss;
                        ss << L"PATHFINDING_GPS PLAYERLOC=" << loc.X << L"," << loc.Y << L"," << loc.Z << L",";
                        ss << L" PLAYERROT=" << rot.Pitch << L"," << rot.Yaw << L"," << rot.Roll;
                        SendStringToLEX(ss.str());

                        // Update cached values
                        s_LastX = loc.X;
                        s_LastY = loc.Y;
                        s_LastZ = loc.Z;
                        s_LastPitch = rot.Pitch;
                        s_LastYaw = rot.Yaw;
                        s_LastRoll = rot.Roll;
                    }
                }
            }

            return true; // Allow other handlers
        }

    private:
        static inline float s_LastX = 0.0f;
        static inline float s_LastY = 0.0f;
        static inline float s_LastZ = 0.0f;
        static inline int s_LastPitch = 0;
        static inline int s_LastYaw = 0;
        static inline int s_LastRoll = 0;

        static bool ApproximatelyEqual(float a, float b, float epsilon = 0.001f)
        {
            return std::fabs(a - b) <= ((std::fabs(a) < std::fabs(b) ? std::fabs(b) : std::fabs(a)) * epsilon);
        }
    };
}
