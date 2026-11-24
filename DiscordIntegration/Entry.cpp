
#include "Common/Base.hpp"
#include "Common/DefaultLogger.hpp"

#include "DiscordIntegration/Entry.hpp"
#include "DiscordIntegration/Hooks.hpp"
#include "DiscordIntegration/SharedVersion.h"

#include <delayimp.h>


SPI_PLUGINSIDE_SUPPORT(SDK_TARGET_NAME_W ASI_NAME_NO_SPACE_W, DEVELOPER_W, L"" VERSION_STRING_W, SPI_GAME_SDK_TARGET, SPI_VERSION_V4);
SPI_PLUGINSIDE_POSTLOAD;
SPI_PLUGINSIDE_ASYNCATTACH;


namespace
{
	// The Discord SDK DLL is delay-loaded (see CMakeLists.txt). A missing DLL, or one from another SDK
	// version, would otherwise only fail at the first call into it, which kills the game. Binding every
	// import up front turns that into a check. No C++ objects here: SEH cannot unwind them.
	constexpr char k_discordSdkDll[] = "discord_partner_sdk.dll";

	int FilterDelayLoadFailure(DWORD const code)
	{
		return code == VcppException(ERROR_SEVERITY_ERROR, ERROR_MOD_NOT_FOUND)
			|| code == VcppException(ERROR_SEVERITY_ERROR, ERROR_PROC_NOT_FOUND)
			? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH;
	}

	// ERROR_SUCCESS, ERROR_MOD_NOT_FOUND (no DLL), or ERROR_PROC_NOT_FOUND (a DLL from another SDK version).
	DWORD BindDiscordSdk()
	{
		__try
		{
			return SUCCEEDED(__HrLoadAllImportsForDll(k_discordSdkDll)) ? ERROR_SUCCESS : ERROR_MOD_NOT_FOUND;
		}
		__except (FilterDelayLoadFailure(GetExceptionCode()))
		{
			return GetExceptionCode() & 0xFFFF;
		}
	}
}


SPI_IMPLEMENT_ATTACH
{
	::DiscordIntegration::HookManager = new ::LESDK::Initializer{ InterfacePtr, SDK_TARGET_NAME_A ASI_NAME_NO_SPACE_A };
	::Common::SetupDefaultLogger(SDK_TARGET_NAME_A, ASI_NAME_NO_SPACE_A, false, *::DiscordIntegration::HookManager);

	// Nothing calls into the SDK unless the hooks are installed, so a failed check leaves this ASI inert.
	DWORD const sdkError = BindDiscordSdk();
	if (sdkError == ERROR_PROC_NOT_FOUND)
	{
		LEASI_ERROR("{} is from a different Discord SDK version than this ASI was built against; Discord integration is disabled", k_discordSdkDll);
		return true;
	}
	if (sdkError != ERROR_SUCCESS)
	{
		LEASI_ERROR("{} was not found next to the game executable; Discord integration is disabled", k_discordSdkDll);
		return true;
	}

	::DiscordIntegration::InitializeGlobals();
	::DiscordIntegration::InitializeHooks();

	return true;
}

SPI_IMPLEMENT_DETACH
{
	LEASI_UNUSED(InterfacePtr);

	delete ::DiscordIntegration::HookManager;
	::DiscordIntegration::HookManager = nullptr;
	::Common::ShutdownLogger();
	return true;
}


namespace DiscordIntegration
{
	::LESDK::Initializer* HookManager = nullptr;

	void InitializeGlobals()
	{
		Common::InitializeRequiredGlobals(*::DiscordIntegration::HookManager);

		// Load GWorld
		GWorld = ::DiscordIntegration::HookManager->ResolveTyped<UWorld*>(BUILTIN_GWORLD_RVA);
		CHECK_RESOLVED(GWorld);

		GIsRequestingExit = ::DiscordIntegration::HookManager->ResolveTyped<UBOOL>(BUILTIN_GISREQUESTINGEXIT_RVA);
		CHECK_RESOLVED(GIsRequestingExit);

		LEASI_INFO("globals initialized");
	}
}
