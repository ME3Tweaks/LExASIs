#include "Common/Base.hpp"
#include "Common/DefaultLogger.hpp"
#include "ConfigDumper/Entry.hpp"
#include "ConfigDumper/Hooks.hpp"
#include "ConfigDumper/ConfigDump.hpp"
#include "ConfigDumper/SharedVersion.h"

// ! SPI metadata
// ========================================

SPI_PLUGINSIDE_SUPPORT(SDK_TARGET_NAME_W ASI_NAME_NO_SPACE_W, DEVELOPER_W, L"" VERSION_STRING_W, SPI_GAME_SDK_TARGET, SPI_VERSION_V4);
SPI_PLUGINSIDE_POSTLOAD;
SPI_PLUGINSIDE_ASYNCATTACH;

SPI_IMPLEMENT_ATTACH
{
	::LESDK::Initializer Init{ InterfacePtr, SDK_TARGET_NAME_A ASI_NAME_NO_SPACE_A };
	Common::SetupDefaultLogger(SDK_TARGET_NAME_A, ASI_NAME_NO_SPACE_A, true, Init);

	::ConfigDumper::InitializeGlobals(Init);
	::ConfigDumper::InitializeHooks(Init);

	LEASI_INFO("ConfigDumper ready -- after DLC mount completes, run DUMPRUNTIMECONFIG for the "
		"engine's raw per-layer config, or DUMPRESOLVEDCONFIG for the actual merged values");

	return true;
}

SPI_IMPLEMENT_DETACH
{
	LEASI_UNUSED(InterfacePtr);
	::Common::ShutdownLogger();
	return true;
}


namespace ConfigDumper
{
	void InitializeGlobals(::LESDK::Initializer& Init)
	{
		Common::InitializeRequiredGlobals(Init);
		InitializeConfigDumpGlobals(Init);
		InitializeResolvedConfigGlobals(Init);

		LEASI_TRACE("Globals initialized");
	}

	void InitializeHooks(::LESDK::Initializer& Init)
	{
		// UEngine::Exec hook for the DUMPRUNTIMECONFIG console command.
		// ----------------------------------------
		auto const UEngine_Exec_target = Init.ResolveTyped<t_UEngine_Exec>(BUILTIN_UENGINE_EXEC_RVA);
		CHECK_RESOLVED(UEngine_Exec_target);
		UEngine_Exec_orig = (t_UEngine_Exec*)Init.InstallHook("UEngine::Exec", UEngine_Exec_target, UEngine_Exec_hook);
		CHECK_RESOLVED(UEngine_Exec_orig);

		LEASI_TRACE("Hooks initialized");
	}
}
