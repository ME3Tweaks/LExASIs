
#include "Common/Base.hpp"
#include "Common/DefaultLogger.hpp"
#include "CoverClient/Entry.hpp"
#include "CoverClient/Hooks.hpp"
#include "CoverClient/Helpers.hpp"
#include "CoverClient/SharedVersion.h"

SPI_PLUGINSIDE_SUPPORT(SDK_TARGET_NAME_W ASI_NAME_NO_SPACE_W, DEVELOPER_W, L"" VERSION_STRING_W, SPI_GAME_SDK_TARGET, SPI_VERSION_ANY);
SPI_PLUGINSIDE_POSTLOAD;
SPI_PLUGINSIDE_ASYNCATTACH;


SPI_IMPLEMENT_ATTACH
{
	::LESDK::Initializer Init{ InterfacePtr, SDK_TARGET_NAME_A ASI_NAME_NO_SPACE_A };
	Common::SetupDefaultLogger(SDK_TARGET_NAME_A, ASI_NAME_NO_SPACE_A, true);
	::CoverClient::InitializeGlobals(Init);
	::CoverClient::InitializeHooks(Init);
	LEASI_INFO("Welcome to Milan Cover Client");
	return true;
}

SPI_IMPLEMENT_DETACH
{
	LEASI_UNUSED(InterfacePtr);
	return true;
}


namespace CoverClient
{
	void InitializeGlobals(::LESDK::Initializer& Init)
	{
		Common::InitializeRequiredGlobals(Init);
	}

#define HOOK_PATTERN_POSTHOOK(NAME, SIG, POSTHOOK) \
		do { \
	auto const NAME##_target = Init.ResolveTyped<SIG>(::LESDK::Address::FromPostHook(POSTHOOK)); \
	CHECK_RESOLVED(NAME##_target); \
	NAME##_orig = (SIG*)Init.InstallHook(#NAME, NAME##_target, NAME##_hook); \
	CHECK_RESOLVED(NAME##_orig); \
		} while (0);

#define HOOK_RVA(NAME, SIG, RVA) \
		do { \
	auto const NAME##_target = Init.ResolveTyped<SIG>(RVA); \
	CHECK_RESOLVED(NAME##_target); \
	NAME##_orig = (SIG*)Init.InstallHook(#NAME, NAME##_target, NAME##_hook); \
	CHECK_RESOLVED(NAME##_orig); \
		} while (0);

	void InitializeHooks(::LESDK::Initializer& Init)
	{
		// All of these addresses are for LE3 currently.

		/*auto cli = std::wstring(GetCommandLineW());
		if (cli.find(L"-NoShaderCompile") != std::wstring::npos) {
			LEASI_INFO(L"Disabling shader compiling");
			EnableShaderCompile = false;
		}*/

		// Hooked this way to allow hook sharing
		HOOK_PATTERN_POSTHOOK(GetTlkString, tGetTlkString,/*"89 54 24 10 4c*/ "8b dc 57 41 54 41 55 41 56 41 57 48 83 ec 60 49 c7 43 a8 fe ff ff ff");

		// Used to redirect us to the shader compiler levels
		HOOK_RVA(UObject_ProcessInternal, t_UObject_ProcessInternal, BUILTIN_PROCESSINTERNAL_RVA);

		// Functions
		static_construct_object = Init.ResolveTyped<tStaticConstructObject>(BUILTIN_STATICCONSTRUCTOBJECT_RVA);
	
		// For console commands
		auto const UGameEngine_Exec_target = Init.ResolveTyped<t_UGameEngine_Exec>(BUILTIN_UGAMEENGINE_EXEC_RVA);
		CHECK_RESOLVED(UGameEngine_Exec_target);
		UGameEngine_Exec_orig = (t_UGameEngine_Exec*)Init.InstallHook("UGameEngine::Exec", UGameEngine_Exec_target, UGameEngine_Exec_hook);
		CHECK_RESOLVED(UGameEngine_Exec_orig);


		// Needed for drawing lines.
		GWorld = Init.ResolveTyped<UWorld*>(BUILTIN_GWORLD_RVA);
		CHECK_RESOLVED(GWorld);

		// For debug drawing
		auto const UObject_ProcessEvent_target = Init.ResolveTyped<t_UObject_ProcessEvent>(BUILTIN_PROCESSEVENT_RVA);
		CHECK_RESOLVED(UObject_ProcessEvent_target);
		UObject_ProcessEvent_orig = (t_UObject_ProcessEvent*)Init.InstallHook("UObject::ProcessEvent", UObject_ProcessEvent_target, UObject_ProcessEvent_hook);
		CHECK_RESOLVED(UObject_ProcessEvent_orig);
	}
}
