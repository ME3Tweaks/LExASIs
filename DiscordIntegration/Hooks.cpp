#include "Hooks.hpp"
#include "Entry.hpp"
#include "Discord.hpp"

namespace DiscordIntegration
{
	void InitializeHooks()
	{
		// UObject::ProcessEvent hook for UnrealScript Activated() logging
		// ----------------------------------------
		
		auto Init = *::DiscordIntegration::HookManager;
		auto const UObject_ProcessEvent_target = Init.ResolveTyped<t_UObject_ProcessEvent>(BUILTIN_PROCESSEVENT_PHOOK);
		CHECK_RESOLVED(UObject_ProcessEvent_target);
		UObject_ProcessEvent_orig = (t_UObject_ProcessEvent*)Init.InstallHook("UObject::ProcessEvent", UObject_ProcessEvent_target, UObject_ProcessEvent_hook);
		CHECK_RESOLVED(UObject_ProcessEvent_orig);

		LEASI_INFO("hooks initialized");
	}

	tBioRequestExit* BioRequestExit_orig = nullptr;
	void BioRequestExit_hook(UBOOL bForce, INT ExitCode, INT Unused)
	{
		ShutdownClient();
		BioRequestExit_orig(bForce, ExitCode, Unused);
	}


	// ! Global variables
	// ========================================
	
	bool* GIsRequestingExit = nullptr;
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

		// Only do stuff if Discord SDK is ready

		if (discordSDKReady) {
			if (GIsRequestingExit && *GIsRequestingExit) {
				ShutdownClient();
			}
			else {
				if (delta >= 8000) {
					lastUpdate = now;
					updateStatus();
				}
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
