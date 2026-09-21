#include "Entry.hpp"
#include "Hooks.hpp"

#include <SPI.h>
#include "Common/Base.hpp"
#include "Common/DefaultLogger.hpp"
#include "LinkerPatcher/SharedVersion.h"

// ! SPI metadata
// ========================================

SPI_PLUGINSIDE_SUPPORT(SDK_TARGET_NAME_W ASI_NAME_NO_SPACE_W, DEVELOPER_W, L"" VERSION_STRING_W, SPI_GAME_SDK_TARGET, SPI_VERSION_V4);
SPI_PLUGINSIDE_POSTLOAD;
SPI_PLUGINSIDE_ASYNCATTACH;

SPI_IMPLEMENT_ATTACH
{
	::LESDK::Initializer Init{ InterfacePtr, SDK_TARGET_NAME_A ASI_NAME_NO_SPACE_A };
	Common::SetupDefaultLogger(SDK_TARGET_NAME_A, ASI_NAME_NO_SPACE_A, true, Init);

	::LinkerPatcher::InitializeGlobals(Init);
	::LinkerPatcher::InitializeHooks(Init);

	return true;
}

SPI_IMPLEMENT_DETACH
{
	LEASI_UNUSED(InterfacePtr);
	::Common::ShutdownLogger();
	return true;
}


namespace LinkerPatcher
{
	void InitializeGlobals(::LESDK::Initializer& Init)
	{
		Common::InitializeRequiredGlobals(Init);
		InitializeConfigGlobals(Init);

		LEASI_TRACE("Globals initialized");
	}

	void InitializeHooks(::LESDK::Initializer& Init)
	{
		// Config loading is NOT done here -- GConfig is still null at SPI_IMPLEMENT_ATTACH time (the
		// config system isn't initialized yet), confirmed by ForceLoadLinkerPatchesConfig() logging
		// "GConfig is null" when attempted from here. Instead, InstallLoadPackageHook's hook calls
		// EnsureConfigLoaded() lazily, once package loading actually starts (matching how DebugLogger
		// already hooks the same LoadPackage function).
		// LoadPackage is installed LAST on purpose: it is the hook that triggers EnsureConfigLoaded(),
		// after which redirects can fire and start loading patch packages out of their non-TOC folder.
		// Installing it last guarantees FindPackageFile is already live to resolve that folder.
		InstallFindPackageFileHook(Init);
		InstallCreateExportHook(Init);
		InstallLoadPackageHook(Init);

		LEASI_TRACE("Hooks initialized");
	}
}
