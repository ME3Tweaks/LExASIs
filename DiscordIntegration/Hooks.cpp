#include "Hooks.hpp"
#include "Entry.hpp"
#include "Discord.hpp"
#include "LmpPresence.hpp"

namespace DiscordIntegration
{
	void InitializeHooks()
	{
		// UObject::ProcessEvent hook for UnrealScript Activated() logging
		// ----------------------------------------
		
		auto Init = *::DiscordIntegration::HookManager;
		auto const UObject_ProcessEvent_target = Init.ResolveTyped<t_UObject_ProcessEvent>(BUILTIN_PROCESSEVENT_RVA);
		CHECK_RESOLVED(UObject_ProcessEvent_target);
		UObject_ProcessEvent_orig = (t_UObject_ProcessEvent*)Init.InstallHook("UObject::ProcessEvent", UObject_ProcessEvent_target, UObject_ProcessEvent_hook);
		CHECK_RESOLVED(UObject_ProcessEvent_orig);

		auto const FEngineLoop_Exit_target = Init.ResolveTyped<tFEngineLoop_Exit>(DISCORD_FENGINELOOP_EXIT_RVA);
		CHECK_RESOLVED(FEngineLoop_Exit_target);
		FEngineLoop_Exit_orig = (tFEngineLoop_Exit*)Init.InstallHook("FEngineLoop::Exit", FEngineLoop_Exit_target, FEngineLoop_Exit_hook);
		CHECK_RESOLVED(FEngineLoop_Exit_orig);

		LEASI_INFO("hooks initialized");
	}

	tFEngineLoop_Exit* FEngineLoop_Exit_orig = nullptr;
	void FEngineLoop_Exit_hook(void* EngineLoop)
	{
		ShutdownClient();
		FEngineLoop_Exit_orig(EngineLoop);
	}


	// ! Global variables
	// ========================================

	UBOOL* GIsRequestingExit = nullptr;
	void** GTlkTable = nullptr; // Is this a double pointer?

	// ! UObject::ProcessEvent hook
	// ========================================

	t_UObject_ProcessEvent* UObject_ProcessEvent_orig = nullptr;
	void UObject_ProcessEvent_hook(UObject* Context, UFunction* Function, void* Parms, void* Result)
	{
		// std::cout << string_format("[U] %s->%s()\n", Context->GetFullPath(), Function->GetFullName(false)).c_str();

	   // Setup Discord on first viewport draw
		if (!firstEventFired) {
			firstEventFired = true;
			firstEventTime = std::chrono::steady_clock::now();
		}

		std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
		auto delta = std::chrono::duration_cast<std::chrono::milliseconds> (now - lastUpdate).count();

		if (!startedDiscordInit) {
			auto startDelta = std::chrono::duration_cast<std::chrono::milliseconds> (now - firstEventTime).count();
			if (startDelta > 10000) {
				startedDiscordInit = true;
				setupDiscord();
			}
		}

		if (GIsRequestingExit && *GIsRequestingExit) {
			// Covers a client that is still connecting or authorizing
			ShutdownClient();
		}
		else if (discordSDKReady) {
			// Only do stuff if Discord SDK is ready
#ifdef LEASI_GAME_LMP
			// Lobby changes go out sooner, within Discord's limit of 5 updates per 20 seconds.
			Lmp::Tick();
			bool const lobbyChanged = Lmp::HasUnsentChange() && delta >= 4000;
#else
			bool const lobbyChanged = false;
#endif
			if (delta >= 8000 || lobbyChanged) {
				lastUpdate = now;
				updateStatus();
			}
		}

		// We must run callbacks, or SDK won't be able to mark
		// itself as ready
		if (delta >= 500) {
			discordpp::RunCallbacks();
		}

		// Run the original event
		UObject_ProcessEvent_orig(Context, Function, Parms, Result);
	}
}
