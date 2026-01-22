
#include "Common/Base.hpp"
#include "Common/DefaultLogger.hpp"
#include "2DAPrinter/Entry.hpp"
#include "2DAPrinter/Hooks.hpp"
#include "2DAPrinter/SharedVersion.h"


SPI_PLUGINSIDE_SUPPORT(SDK_TARGET_NAME_W ASI_NAME_NO_SPACE_W, DEVELOPER_W, L"" VERSION_STRING_W, SPI_GAME_SDK_TARGET, SPI_VERSION_ANY);
SPI_PLUGINSIDE_POSTLOAD;
SPI_PLUGINSIDE_ASYNCATTACH;


SPI_IMPLEMENT_ATTACH
{
	::LESDK::Initializer Init{ InterfacePtr, SDK_TARGET_NAME_A ASI_NAME_NO_SPACE_A };
	Common::SetupDefaultLogger(SDK_TARGET_NAME_A, ASI_NAME_NO_SPACE_A);
	::Bio2DAPrinter::InitializeGlobals(Init);
	::Bio2DAPrinter::InitializeHooks(Init);

	/*
	auto outputType = Common::ME3TweaksLogger::LogOutput(
		Common::ME3TweaksLogger::LogOutput::OutputToFile |
		Common::ME3TweaksLogger::LogOutput::OutputToConsole
	);
	try
	{
		::Bio2DAPrinter::FileLogger = std::make_unique<Common::ME3TweaksLogger>(loggerName, outputType, "SeqActLog.log");
	}
	catch (const spdlog::spdlog_ex& ex)
	{
		LEASI_ERROR("Failed to create SeqActLog.log: {}", ex.what());
		return false;
	}
	*/

	return true;
}

SPI_IMPLEMENT_DETACH
{
	LEASI_UNUSED(InterfacePtr);

	// Flush and release file logger
	//if (::Bio2DAPrinter::FileLogger)
	//{
	//	::Bio2DAPrinter::FileLogger->flush();
	//	::Bio2DAPrinter::FileLogger.reset();
	//	spdlog::drop(loggerName);
	//}

	::LESDK::TerminateConsole();
	return true;
}


namespace Bio2DAPrinter
{
	void InitializeGlobals(::LESDK::Initializer& Init)
	{
		Common::InitializeRequiredGlobals(Init);
		LEASI_TRACE("Globals initialized");
	}

	void InitializeHooks(::LESDK::Initializer& Init)
	{
		// UObject::ProcessEvent hook for 2DAPrinter keyboard events
		// ----------------------------------------
		auto const UObject_ProcessEvent_target = Init.ResolveTyped<t_UObject_ProcessEvent>(BUILTIN_PROCESSEVENT_PHOOK);
		CHECK_RESOLVED(UObject_ProcessEvent_target);
		UObject_ProcessEvent_orig = (t_UObject_ProcessEvent*)Init.InstallHook("UObject::ProcessEvent", UObject_ProcessEvent_target, UObject_ProcessEvent_hook);
		CHECK_RESOLVED(UObject_ProcessEvent_orig);

		LEASI_INFO("Hooks initialized");
	}
}
