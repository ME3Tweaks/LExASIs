#pragma once

#include "Common/Base.hpp"
#include <LESDK/Headers.hpp>
#include <LESDK/Init.hpp>
#include <LESDK/Common/Core.hpp>

namespace DiscordIntegration
{
    // If the application is about to exit
	// ========================================
    extern bool* GIsRequestingExit;

#if defined(SDK_TARGET_LE1)
	extern void** GTlkTable;
#endif

    void InitializeHooks();

	// ! BioRequestExit hook to shutdown Discord client
	// ========================================

    using tBioRequestExit = void(UBOOL bForce, INT ExitCode, INT Unused);
    extern tBioRequestExit* BioRequestExit_orig;
    void BioRequestExit_hook(UBOOL bForce, INT ExitCode, INT Unused);

    // ! UObject::ProcessEvent hook for pumping to Discord
    // ========================================

    using t_UObject_ProcessEvent = void(UObject* Context, UFunction* Function, void* Parms, void* Result);
    extern t_UObject_ProcessEvent* UObject_ProcessEvent_orig;
    void UObject_ProcessEvent_hook(UObject* Context, UFunction* Function, void* Parms, void* Result);

}
