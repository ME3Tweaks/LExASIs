#pragma once

#include <LESDK/Headers.hpp>
#include <LESDK/Init.hpp>

namespace PNGScreenshots
{
    extern bool fpngInitialized;

    // The initial screenshot index we should check against. As we take screenshots we know the previous images will always exist
    // and as such we can just increment this on auto-generated screenshots to match the game's incremental system
    extern int cachedScreenshotIndex;

    // Hopefully user never gets here but this is so it doesn't make an infinite loop
    extern int maxScreenshotIndex;

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
