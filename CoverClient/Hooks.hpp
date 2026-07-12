#pragma once

#include <memory>
#include <LESDK/Headers.hpp>
#include <LESDK/Init.hpp>
#include "Common/Base.hpp"
#include "CoverClient/Client.hpp"

namespace CoverClient
{
    extern int* GIsSeekFreePCConsole; // Used to trick game into outputting HLSL  @ 0x7ff74c06f724; // RVA 17DF724
    extern bool DrawGameFireLinksForSlot; // Turns drawing the current slot's firelinks on or off. (game)
    extern bool DrawClientFireLinksForSlot; // Turns drawing the current slot's firelinks on or off (coverclient)
	extern bool DrawClientExposedCoverForSlot; // Turns drawing the current slot's exposed cover on or off (coverclient)
	extern bool DrawClientDangerNavsForSlot; // Turns drawing the current slot's danger navs on or off (coverclient)

    using tGetTlkString = FString* (void* gTalkTable, int id, FString* outStr, int, int);
    extern tGetTlkString* GetTlkString_orig; // @ 7ff74a927400 LE3
    FString* GetTlkString_hook(void* gTalkTable, int id, FString* outStr, int something, int somethingElse);

    // For directing to a specific level
    using t_UObject_ProcessInternal = void(UObject* Context, FFrame* Stack, void* Result);
    extern t_UObject_ProcessInternal* UObject_ProcessInternal_orig;
    void UObject_ProcessInternal_hook(UObject* Context, FFrame* Stack, void* Result);

	// ! UGameEngine::Exec For console command handling
    // ========================================
    using t_UGameEngine_Exec = DWORD(UGameEngine* Context, WCHAR const* Command, void* Archive);
    extern t_UGameEngine_Exec* UGameEngine_Exec_orig;
    DWORD UGameEngine_Exec_hook(UGameEngine* Context, WCHAR const* Command, void* Archive);

    // ! UObject::ProcessEvent hook for intercepting BioHUD.PostRender
    // ========================================
    using t_UObject_ProcessEvent = void(UObject* Context, UFunction* Function, void* Parms, void* Result);
    extern t_UObject_ProcessEvent* UObject_ProcessEvent_orig;
    void UObject_ProcessEvent_hook(UObject* Context, UFunction* Function, void* Parms, void* Result);
}
