#pragma once

#include <LESDK/Headers.hpp>
#include <LESDK/Init.hpp>

namespace ImageScanner
{
    enum class CaptureCommands : unsigned char {
        START_CAPTURE = 1,
        CAPTURE_COUNT = 2,
        CAPTURE = 3,
        CAPTURE_END = 4
    };


    // If capture is in progress.
	extern bool bIsCaptureInProgress;

    // How many captures to take in a scan
    extern int numCaptures;

    // The current capture number
    extern int currentCaptureNum;

    // How far apart each capture should be
    extern int captureSpacing;

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

#if defined(SDK_TARGET_LE1)
    #define APP_MAKE_BITMAP_RVA    ::LESDK::Address::FromOffset(0x154fc0)
#elif defined(SDK_TARGET_LE2)
    #define APP_MAKE_BITMAP_RVA    ::LESDK::Address::FromOffset(0xfd810)
#elif defined(SDK_TARGET_LE3)
    #define APP_MAKE_BITMAP_RVA    ::LESDK::Address::FromOffset(0x119ed0)
#endif

    // ! AppCreateBitmap
    // ========================================
    using t_AppCreateBitmap = void(wchar_t* pattern, int width, int height, FColor* data, void* fileManager);    
    extern t_AppCreateBitmap* appCreateBitmap_orig;
    void appCreateBitmap_hook(wchar_t* pattern, int width, int height, FColor* data, void* fileManager);
}
