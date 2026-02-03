#include "Entry.hpp"
#include "Hooks.hpp"

#include <SPI.h>
#include "Common/Base.hpp"

#include <spdlog/spdlog.h>
#include "LinkerPrinter/SharedVersion.h"

// ! SPI metadata
// ========================================

SPI_PLUGINSIDE_SUPPORT(SDK_TARGET_NAME_W ASI_NAME_NO_SPACE_W, DEVELOPER_W, L"" VERSION_STRING_W, SPI_GAME_SDK_TARGET, SPI_VERSION_ANY);
SPI_PLUGINSIDE_POSTLOAD;
SPI_PLUGINSIDE_ASYNCATTACH;

namespace LinkerPrinter
{
	// ! Global variables
	// ========================================

	std::unique_ptr<Common::ME3TweaksLogger> FileLogger = nullptr;
	std::map<FString, FString> NodePathToFileNameMap;
	bool CanPrint = true;

	// ! Initialization
	// ========================================

	void InitializeGlobals(::LESDK::Initializer& Init)
	{
		Common::InitializeRequiredGlobals(Init);
	}

	void InitializeHooks(::LESDK::Initializer& Init)
	{
		// Install SetLinker hook
		auto  const setLinkerTarget = Init.ResolveTyped<t_SetLinker>(BUILTIN_SETLINKER_RVA);
		SetLinker_orig = (t_SetLinker*)Init.InstallHook("UObject::SetLinker", setLinkerTarget, SetLinker_hook);

		// Install ProcessEvent hook
		auto const processEventTarget = Init.ResolveTyped<t_UObject_ProcessEvent>(BUILTIN_PROCESSEVENT_RVA);
		UObject_ProcessEvent_orig = (t_UObject_ProcessEvent*)Init.InstallHook("UObject::ProcessEvent", processEventTarget, UObject_ProcessEvent_hook);
	}
}

// ! SPI implementation
// ========================================

SPI_IMPLEMENT_ATTACH
{

	::LESDK::Initializer Init{ InterfacePtr, ASI_NAME_NO_SPACE_A };

#ifdef _DEBUG
	::LESDK::InitializeConsole();
#endif
	auto outputType = Common::ME3TweaksLogger::LogOutput(
		Common::ME3TweaksLogger::LogOutput::OutputToFile 
#ifdef _DEBUG
		| Common::ME3TweaksLogger::LogOutput::OutputToConsole
#endif
	);
	try
	{
		::LinkerPrinter::FileLogger = std::make_unique<Common::ME3TweaksLogger>(ASI_NAME_NO_SPACE_A, outputType, ASI_NAME_NO_SPACE_A ".log");
	}
	catch (const spdlog::spdlog_ex& ex)
	{
		LEASI_ERROR("Failed to create " ASI_NAME_NO_SPACE_A ".log: {}", ex.what());
		return false;
	}

	// Initialize globals and hooks
	::LinkerPrinter::InitializeGlobals(Init);
	::LinkerPrinter::InitializeHooks(Init);

	// Print user message
	::LinkerPrinter::FileLogger->info(L"Press CTRL + O to dump all objects that have loaded and their source.");

	return true;
}

SPI_IMPLEMENT_DETACH
{
	LEASI_UNUSED(InterfacePtr);

	// Flush and cleanup logger
	if (::LinkerPrinter::FileLogger)
	{
		try {
			spdlog::shutdown();
		} catch (int errorCode) {
			LEASI_UNUSED(errorCode);
			// We can't do anything. Just swallow it
		}
	}

	::LESDK::TerminateConsole();

	return true;
}
