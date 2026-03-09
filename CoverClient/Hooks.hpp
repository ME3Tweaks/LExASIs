#pragma once

#include <memory>
#include <LESDK/Headers.hpp>
#include <LESDK/Init.hpp>
#include "Common/Base.hpp"
#include "CoverClient/Client.hpp"

namespace CoverClient
{
    extern int* GIsSeekFreePCConsole; // Used to trick game into outputting HLSL  @ 0x7ff74c06f724; // RVA 17DF724

    using tGetTlkString = FString* (void* gTalkTable, int id, FString* outStr, int, int);
    extern tGetTlkString* GetTlkString; // @ 7ff74a927400 LE3
    extern tGetTlkString* GetTlkString_orig; // @ 7ff74a927400 LE3

    // For directing to the shader compiler levels.
    using t_UObject_ProcessInternal = void(UObject* Context, FFrame* Stack, void* Result);
    extern t_UObject_ProcessInternal* UObject_ProcessInternal_orig;
    void UObject_ProcessInternal_hook(UObject* Context, FFrame* Stack, void* Result);

    // For overriding the loading screen a shader uses
    using tPlayLoadingMovie = void(USFXLoadMovieManager* param_1, FString* levelName, int param3);
	extern tPlayLoadingMovie* PlayLoadingMovie_orig;
    void PlayLoadingMovie_hook(USFXLoadMovieManager* param_1, FString* levelName, int param3);

    // For making sure game doesn't pause
    using tReturnBool = BOOL();
	extern tReturnBool* IsWindowFocused_orig;
	BOOL IsWindowFocused_hook();

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

    FString* GetTlkString_hook(void* gTalkTable, int id, FString* outStr, int something, int somethingElse);
    // void ShowLoadMessage_hook(void* unknown, int param_2, UINT stringId, int someBool);
}
