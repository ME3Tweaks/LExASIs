#pragma once

#include "Common/Base.hpp"
#include <LESDK/Headers.hpp>
#include <LESDK/Init.hpp>
#include <LESDK/Common/Core.hpp>

namespace DiscordIntegration
{
    // If the application is about to exit
	// ========================================
    extern UBOOL* GIsRequestingExit;

#if defined(SDK_TARGET_LE1)
	extern void** GTlkTable;
#endif

    void InitializeHooks();

	// ! FEngineLoop::Exit hook to shut down the Discord client
	// ========================================
#if defined(SDK_TARGET_LE1)
	#define DISCORD_FENGINELOOP_EXIT_RVA LEASI_RVA(0x9b7980)
#elif defined(SDK_TARGET_LE2)
	#define DISCORD_FENGINELOOP_EXIT_RVA LEASI_RVA(0x99db30)
#elif defined(SDK_TARGET_LE3)
	#define DISCORD_FENGINELOOP_EXIT_RVA LEASI_RVA(0x9b8e20)
#endif

	using tFEngineLoop_Exit = void(void* EngineLoop);
	extern tFEngineLoop_Exit* FEngineLoop_Exit_orig;
	void FEngineLoop_Exit_hook(void* EngineLoop);

    // ! UObject::ProcessEvent hook for pumping to Discord
    // ========================================

    using t_UObject_ProcessEvent = void(UObject* Context, UFunction* Function, void* Parms, void* Result);
    extern t_UObject_ProcessEvent* UObject_ProcessEvent_orig;
    void UObject_ProcessEvent_hook(UObject* Context, UFunction* Function, void* Parms, void* Result);

}
