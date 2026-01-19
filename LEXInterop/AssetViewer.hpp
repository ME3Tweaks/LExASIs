#pragma once

#include <string>
#include <cstring>
#include <LESDK/Headers.hpp>
#include "Utilities.hpp"
#include "Common/Base.hpp"
#include "Common/Objects.hpp"

namespace LEXInterop
{
    // Asset viewer functionality - spawning assets and window focus control
    class AssetViewer
    {
    public:
        // Handle pipe commands for asset viewer
        static bool HandleCommand(std::string command)
        {
            // All AssetViewer commands start with ASSETV_
            if (!StartsWith("ASSETV_", command))
                return false;

            if (StartsWith("ASSETV_ALLOW_WINDOW_PAUSE", command))
            {
                SetAllowWindowPausing(true);
                return true;
            }

            if (StartsWith("ASSETV_DISALLOW_WINDOW_PAUSE", command))
            {
                SetAllowWindowPausing(false);
                return true;
            }

            if (StartsWith("ASSETV_SPAWN_ASSET ", command))
            {
                // Parse: ASSETV_SPAWN_ASSET <actorPath>
                auto subCommand = GetCommandParam(command);
                const auto actorFullPath = GetCommandParam(command);

                auto widePath = s2ws(actorFullPath);
                auto actorObj = UObject::FindObject<AActor>(widePath.c_str());
                if (actorObj)
                {
                    SpawnActor(actorObj->Class, actorObj);
                }
                return true;
            }


            if (StartsWith("ASSETV_CHANGE_PAWN ", command))
            {
                // Test code.
                auto subCommand = GetCommandParam(command);
                const auto actorFullPath = GetCommandParam(command);

                // Copy the string to the buffer
                //strcpy(SpawnableArchetype, actorFullPath.c_str());

                // CauseEvent: ChangeActor
                //auto player = reinterpret_cast<ABioPlayerController*>(FindObjectOfType(ABioPlayerController::StaticClass()));
                //if (player)
                //{
                //	FName foundName;
                //	StaticVariables::CreateName(L"ChangeActor", 0, & foundName);
                //	player->CauseEvent(foundName);
                //}
                //return true;

                //ActorPackageFile = s2ws(GetCommandParam(command));
                //ActorMemoryPath = s2ws(GetCommandParam(command));

                return true;
            }

            return false;
        }

        // Process game events for asset viewer.
        // Returns true to allow other handlers.
        static bool ProcessEvent(UObject* Context, UFunction* Function, void* Parms, void* Result)
        {
            LEASI_UNUSED_4(Context, Function, Parms, Result);

            //if (!strcmp(Context->Class->GetName(), "LEXSeqAct_SpawnPawn") && !strcmp(Function->GetName(), "Activated"))
            {
                // Make sure the variables are set
                //SetSequenceString(static_cast<USequenceOp*>(Context), L"Type", LEAssetViewer::SpawnableArchetype);

                //const auto spGame = static_cast<AGameInfo*>(FindObjectOfType(AGameInfo::StaticClass()));
                //spGame->ServerOptions = FString(SpawnableArchetype);
                //auto t = "";
                //FVector v = FVector();
                //v.X = -4013;
                //v.Y = -6046;
                //v.Z = -26559;
                //FRotator r = FRotator();

                //auto pf = FString(ActorPackageFile.data());
                //spGame->PreloadPackage(pf); // Load into memory

                //// How do we know when this is done...?
                ////std::this_thread::sleep_for(std::chrono::seconds(1));

                //auto mp = FString(ActorMemoryPath.data());
                //auto pawn = spGame->SpawnPawn(mp, v, r, false);
                //FName tag;
                //StaticVariables::CreateName(L"AnimatedActor", 0, &tag);
                //pawn->Tag = tag;
            }

            //if (!strcmp(Context->Class->GetName(), "LEXSeqAct_SpawnPawn") && !strcmp(Function->GetName(), "HookedSpawn"))
            //{
            //	// Make sure the variables are set
            //	SetSequenceString(static_cast<USequenceOp*>(Context), L"Type", LEAssetViewer::SpawnableArchetype);
            //}

            return true;
        }

    private:
        static inline AActor* s_CurrentSpawnedActor = nullptr;
        static inline bool s_AllowPausing = false;

        // Window focus hook - prevents game from pausing when window loses focus
        using t_IsWindowFocused = bool();
        static inline t_IsWindowFocused* IsWindowFocused_orig = nullptr;

        static bool IsWindowFocused_hook()
        {
            if (s_AllowPausing)
            {
                return IsWindowFocused_orig();
            }
            // Always report window as focused
            return true;
        }

        static void SetAllowWindowPausing(bool allow)
        {
            s_AllowPausing = allow;
            // Note: Hook installation would be done during initialization
        }

        static bool SpawnActor(UClass* actorClass, AActor* actorTemplate)
        {
            auto player = Common::FindFirstObject<ABioPlayerController>();
            if (player)
            {
                if (s_CurrentSpawnedActor)
                {
                    s_CurrentSpawnedActor->Destroy();
                    s_CurrentSpawnedActor = nullptr;
                }

                SFXName spawnName(L"SpawnedActor", 0);
                FVector spawnPos{};
                FRotator spawnRot{};

#if defined(SDK_TARGET_LE1) || defined(SDK_TARGET_LE2)
                s_CurrentSpawnedActor = player->Spawn(actorClass, player, spawnName, spawnPos, spawnRot, actorTemplate, nullptr, false, false);
#elif defined(SDK_TARGET_LE3)
                s_CurrentSpawnedActor = player->Spawn(actorClass, player, spawnName, spawnPos, spawnRot, actorTemplate, false, false);
#endif
                return true;
            }
            return false;
        }
    };
}