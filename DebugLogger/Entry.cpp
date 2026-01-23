
#include "Common/Base.hpp"
#include "Common/DefaultLogger.hpp"
#include "DebugLogger/Entry.hpp"
#include "DebugLogger/Hooks.hpp"
#include "DebugLogger/SharedVersion.h"


SPI_PLUGINSIDE_SUPPORT(SDK_TARGET_NAME_W ASI_NAME_NO_SPACE_W, DEVELOPER_W, L"" VERSION_STRING_W, SPI_GAME_SDK_TARGET, SPI_VERSION_ANY);
SPI_PLUGINSIDE_POSTLOAD;
SPI_PLUGINSIDE_ASYNCATTACH;


SPI_IMPLEMENT_ATTACH
{
	::LESDK::Initializer Init{ InterfacePtr, SDK_TARGET_NAME_A ASI_NAME_NO_SPACE_A };
	Common::SetupDefaultLogger(SDK_TARGET_NAME_A, ASI_NAME_NO_SPACE_A);
	::DebugLogger::InitializeGlobals(Init);
	::DebugLogger::InitializeHooks(Init);

	return true;
}

SPI_IMPLEMENT_DETACH
{
	LEASI_UNUSED(InterfacePtr);
	::LESDK::TerminateConsole();
	return true;
}


namespace DebugLogger
{
	void InitializeGlobals(::LESDK::Initializer& Init)
	{
		Common::InitializeRequiredGlobals(Init);

		LEASI_TRACE("Globals initialized");
	}

	void InitializeHooks(::LESDK::Initializer& Init)
	{
		InstallSharedHooks(Init);
		InstallGameSpecificHooks(Init);
	}
}
