
#include "Common/Base.hpp"
#include "AceSlammer/Entry.hpp"
#include "AceSlammer/Hooks.hpp"
#include "AceSlammer/SharedVersion.h"

SPI_PLUGINSIDE_SUPPORT(SDK_TARGET_NAME_W ASI_NAME_NO_SPACE_W, DEVELOPER_W, L"" VERSION_STRING_W, SPI_GAME_SDK_TARGET, SPI_VERSION_ANY);
SPI_PLUGINSIDE_POSTLOAD;
SPI_PLUGINSIDE_ASYNCATTACH;


SPI_IMPLEMENT_ATTACH
{
	::LESDK::Initializer Init{ InterfacePtr, SDK_TARGET_NAME_A ASI_NAME_NO_SPACE_A };

	::AceSlammer::InitializeGlobals(Init);
	::AceSlammer::InitializeHooks(Init);

	return true;
}

SPI_IMPLEMENT_DETACH
{
	LEASI_UNUSED(InterfacePtr);
	return true;
}


namespace AceSlammer
{
	void InitializeGlobals(::LESDK::Initializer& Init)
	{
		Common::InitializeRequiredGlobals(Init);
	}

	void InitializeHooks(::LESDK::Initializer& Init)
	{
		// UObject::ProcessEvent hook for UnrealScript function logging
		// ----------------------------------------
		auto const UObject_ProcessEvent_target = Init.ResolveTyped<t_UObject_ProcessEvent>(BUILTIN_PROCESSEVENT_RVA);
		CHECK_RESOLVED(UObject_ProcessEvent_target);
		UObject_ProcessEvent_orig = (t_UObject_ProcessEvent*)Init.InstallHook("UObject::ProcessEvent", UObject_ProcessEvent_target, UObject_ProcessEvent_hook);
		CHECK_RESOLVED(UObject_ProcessEvent_orig);
	}
}
