
#include "Common/Base.hpp"
#include "SeqActLogEnabler/Entry.hpp"
#include "SeqActLogEnabler/Hooks.hpp"
#include "SeqActLogEnabler/SharedVersion.h"

constexpr auto loggerName = "SeqActLogEnabler";

SPI_PLUGINSIDE_SUPPORT(SDK_TARGET_NAME_W ASI_NAME_NO_SPACE_W, DEVELOPER_W, L"" VERSION_STRING_W, SPI_GAME_SDK_TARGET, SPI_VERSION_ANY);
SPI_PLUGINSIDE_POSTLOAD;
SPI_PLUGINSIDE_ASYNCATTACH;


SPI_IMPLEMENT_ATTACH
{
	::LESDK::Initializer Init{ InterfacePtr, SDK_TARGET_NAME_A ASI_NAME_NO_SPACE_A };
	// Initialize console and file logger
#ifdef _DEBUG
	::LESDK::InitializeConsole();
#endif

	::SeqActLogEnabler::InitializeGlobals(Init);
	::SeqActLogEnabler::InitializeHooks(Init);

	auto outputType = Common::ME3TweaksLogger::LogOutput(
		Common::ME3TweaksLogger::LogOutput::OutputToFile 
#ifdef _DEBUG
		| Common::ME3TweaksLogger::LogOutput::OutputToConsole
#endif
	);
	try
	{
		::SeqActLogEnabler::FileLogger = std::make_unique<Common::ME3TweaksLogger>(loggerName, outputType, ASI_NAME_NO_SPACE_A ".log");
	}
	catch (const spdlog::spdlog_ex& ex)
	{
		LEASI_ERROR("Failed to create SeqActLog.log: {}", ex.what());
		// return false;
	}

	// Initialize screen logger
	::SeqActLogEnabler::OnScreenLogger = std::make_unique<::SeqActLogEnabler::ScreenLogger>(L"SeqAct_Log Messages");

	return true;
}

SPI_IMPLEMENT_DETACH
{
	LEASI_UNUSED(InterfacePtr);

	// Release screen logger
	//::SeqActLogEnabler::OnScreenLogger.reset();

	// Flush and release file logger
	try {
		if (::SeqActLogEnabler::FileLogger)
		{
			::SeqActLogEnabler::FileLogger->flush();
			::SeqActLogEnabler::FileLogger.reset();
			spdlog::drop(loggerName);
		}
	}
	catch (int code) {
		// Do nothing
		LEASI_UNUSED(code);
	}

	::LESDK::TerminateConsole();
	return true;
}


namespace SeqActLogEnabler
{
	void InitializeGlobals(::LESDK::Initializer& Init)
	{
		Common::InitializeRequiredGlobals(Init);

		LEASI_INFO("globals initialized");
	}

	void InitializeHooks(::LESDK::Initializer& Init)
	{
		// UObject::ProcessEvent hook for SeqAct_Log and BioHUD.PostRender
		// ----------------------------------------
		auto const UObject_ProcessEvent_target = Init.ResolveTyped<t_UObject_ProcessEvent>(BUILTIN_PROCESSEVENT_RVA);
		CHECK_RESOLVED(UObject_ProcessEvent_target);
		UObject_ProcessEvent_orig = (t_UObject_ProcessEvent*)Init.InstallHook("UObject::ProcessEvent", UObject_ProcessEvent_target, UObject_ProcessEvent_hook);
		CHECK_RESOLVED(UObject_ProcessEvent_orig);

		LEASI_INFO("hooks initialized");
	}
}
