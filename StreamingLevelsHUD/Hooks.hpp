#pragma once

#include <LESDK/Headers.hpp>
#include <LESDK/Init.hpp>
#include "Common/Base.hpp"
#include <map>
#include <set>
#include <filesystem>

#ifdef SDK_TARGET_LE1
#define WINDOWNAME "Mass Effect"
#endif
#ifdef SDK_TARGET_LE2
#define WINDOWNAME "Mass Effect 2"
#endif
#ifdef SDK_TARGET_LE3
#define WINDOWNAME "Mass Effect 3"
#endif

namespace StreamingLevelsHUD
{
    // ! UObject hooks.
    // ========================================

    using t_UObject_ProcessEvent = void(UObject* Context, UFunction* Function, void* Parms, void* Result);
    extern t_UObject_ProcessEvent* UObject_ProcessEvent_orig;
    void UObject_ProcessEvent_hook(UObject* Context, UFunction* Function, void* Parms, void* Result);

    // Methods
    void SetTextScale();
}
