#include <spdlog/details/windows_include.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "Common/Base.hpp"
#include "StreamingLevelsHUD/Entry.hpp"
#include "StreamingLevelsHUD/Hooks.hpp"

SPI_PLUGINSIDE_SUPPORT(SDK_TARGET_NAME_W L"StreamingLevelsHUD", L"ME3Tweaks", L"0.1.0", SPI_GAME_SDK_TARGET, SPI_VERSION_ANY);
SPI_PLUGINSIDE_POSTLOAD;
SPI_PLUGINSIDE_ASYNCATTACH;


SPI_IMPLEMENT_ATTACH
{
	::LESDK::Initializer Init{ InterfacePtr, SDK_TARGET_NAME_A "StreamingLevelsHUD" };

	::StreamingLevelsHUD::InitializeGlobals(Init);
	::StreamingLevelsHUD::InitializeHooks(Init);

	return true;
}

SPI_IMPLEMENT_DETACH
{
	LEASI_UNUSED(InterfacePtr);
#ifdef _DEBUG
	::LESDK::TerminateConsole();
#endif
	return true;
}


namespace StreamingLevelsHUD
{

#define CHECK_RESOLVED(variable)                                                    \
    do {                                                                            \
        LEASI_VERIFYA(variable != nullptr, "failed to resolve " #variable, "");     \
        LEASI_TRACE("resolved " #variable " => {}", (void*)variable);               \
    } while (false)

	void InitializeGlobals(::LESDK::Initializer& Init)
	{
		GMalloc = Init.ResolveTyped<FMallocLike*>(BUILTIN_GMALLOC_RIP);
		CHECK_RESOLVED(GMalloc);

		UObject::GObjObjects = Init.ResolveTyped<TArray<UObject*>>(BUILTIN_GOBOBJECTS_RIP);
		CHECK_RESOLVED(UObject::GObjObjects);
		SFXName::GBioNamePools = Init.ResolveTyped<SFXNameEntry const*>(BUILTIN_SFXNAMEPOOLS_RIP);
		CHECK_RESOLVED(SFXName::GBioNamePools);
		SFXName::GInitMethod = Init.ResolveTyped<SFXName::tInitMethod>(BUILTIN_SFXNAMEINIT_PHOOK);
		CHECK_RESOLVED(SFXName::GInitMethod);

		LEASI_INFO("globals initialized");
	}

	void InitializeHooks(::LESDK::Initializer& Init)
	{
		// UObject hooks.
		// ----------------------------------------
		auto const UObject_ProcessEvent_target = Init.ResolveTyped<t_UObject_ProcessEvent>(BUILTIN_PROCESSEVENT_PHOOK);
		CHECK_RESOLVED(UObject_ProcessEvent_target);
		UObject_ProcessEvent_orig = (t_UObject_ProcessEvent*)Init.InstallHook("UObject::ProcessEvent", UObject_ProcessEvent_target, UObject_ProcessEvent_hook);
		CHECK_RESOLVED(UObject_ProcessEvent_orig);

		LEASI_INFO("hooks initialized");
	}
}
