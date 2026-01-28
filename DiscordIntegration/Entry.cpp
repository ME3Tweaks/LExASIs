
#include "Common/Base.hpp"
#include "Common/DefaultLogger.hpp"

#include "DiscordIntegration/Entry.hpp"
#include "DiscordIntegration/Hooks.hpp"
#include "DiscordIntegration/SharedVersion.h"


SPI_PLUGINSIDE_SUPPORT(SDK_TARGET_NAME_W ASI_NAME_NO_SPACE_W, DEVELOPER_W, L"" VERSION_STRING_W, SPI_GAME_SDK_TARGET, SPI_VERSION_ANY);
SPI_PLUGINSIDE_POSTLOAD;
SPI_PLUGINSIDE_ASYNCATTACH;


SPI_IMPLEMENT_ATTACH
{
	::DiscordIntegration::HookManager = new ::LESDK::Initializer{ InterfacePtr, SDK_TARGET_NAME_A ASI_NAME_NO_SPACE_A };
	::Common::SetupDefaultLogger(SDK_TARGET_NAME_A, ASI_NAME_NO_SPACE_A);

	::DiscordIntegration::InitializeGlobals();
	::DiscordIntegration::InitializeHooks();

	return true;
}

SPI_IMPLEMENT_DETACH
{
	LEASI_UNUSED(InterfacePtr);

	delete ::DiscordIntegration::HookManager;
	::DiscordIntegration::HookManager = nullptr;
	/*
	// Flush and release file logger
	if (::DiscordIntegration::FileLogger)
	{
		::DiscordIntegration::FileLogger->flush();
		::DiscordIntegration::FileLogger.reset();
		spdlog::drop(ASI_NAME_NO_SPACE_A);*/
		//}

	::LESDK::TerminateConsole();
	return true;
}


namespace DiscordIntegration
{
	::LESDK::Initializer* HookManager = nullptr;

	void InitializeGlobals()
	{
		Common::InitializeRequiredGlobals(*::DiscordIntegration::HookManager);

		// Load GWorld
		GWorld = ::DiscordIntegration::HookManager->ResolveTyped<UWorld*>(BUILTIN_GWORLD_RIP);
		CHECK_RESOLVED(GWorld);

		LEASI_INFO("globals initialized");
	}
}
