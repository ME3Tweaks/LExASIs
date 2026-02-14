#pragma once

#include <memory>
#include <LESDK/Headers.hpp>
#include <LESDK/Init.hpp>
#include "Common/Base.hpp"

namespace AceSlammer
{
    // ! UObject::ProcessEvent hook for logging UnrealScript function calls.
    // ========================================

    using t_UObject_ProcessEvent = void(UObject* Context, UFunction* Function, void* Parms, void* Result);
    extern t_UObject_ProcessEvent* UObject_ProcessEvent_orig;
    void UObject_ProcessEvent_hook(UObject* Context, UFunction* Function, void* Parms, void* Result);
}
