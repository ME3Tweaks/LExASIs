#pragma once

#include <memory>
#include <LESDK/Headers.hpp>
#include <LESDK/Init.hpp>
#include "Common/Base.hpp"

namespace Bio2DAPrinter
{
	// If 2DAs can be printed
	extern bool CanPrint2DAs;

    // Prints all 2DAs in memory to the log
    void Print2DAs();

    // ! UObject::ProcessEvent hook for intercepting BioHUD.PostRender
    // ========================================
    using t_UObject_ProcessEvent = void(UObject* Context, UFunction* Function, void* Parms, void* Result);
    extern t_UObject_ProcessEvent* UObject_ProcessEvent_orig;
    void UObject_ProcessEvent_hook(UObject* Context, UFunction* Function, void* Parms, void* Result);
}
