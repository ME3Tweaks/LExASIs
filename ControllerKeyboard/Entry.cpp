
#include "Common/Base.hpp"
#include "Common/DefaultLogger.hpp"
#include "ControllerKeyboard/Entry.hpp"
#include "ControllerKeyboard/Hooks.hpp"
#include "ControllerKeyboard/SharedVersion.h"

SPI_PLUGINSIDE_SUPPORT(SDK_TARGET_NAME_W ASI_NAME_NO_SPACE_W, DEVELOPER_W, L"" VERSION_STRING_W, SPI_GAME_SDK_TARGET, SPI_VERSION_V4);
SPI_PLUGINSIDE_POSTLOAD;
SPI_PLUGINSIDE_ASYNCATTACH;


SPI_IMPLEMENT_ATTACH
{
	::LESDK::Initializer Init{ InterfacePtr, SDK_TARGET_NAME_A ASI_NAME_NO_SPACE_A };
#ifdef _DEBUG	
::Common::SetupDefaultLogger(SDK_TARGET_NAME_A, ASI_NAME_NO_SPACE_A, false, Init);
#endif	
	::ControllerKeyboard::InitializeGlobals(Init);
	::ControllerKeyboard::InitializeHooks(Init);

	return true;
}

SPI_IMPLEMENT_DETACH
{
	LEASI_UNUSED(InterfacePtr);
	::Common::ShutdownLogger();
	return true;
}


namespace ControllerKeyboard
{
	void InitializeGlobals(::LESDK::Initializer& Init)
	{
		Common::InitializeRequiredGlobals(Init);
	}

	void InitializeHooks(::LESDK::Initializer& Init)
	{
		// Method to invoke when keyboard entry is complete. This is used to call the original delegate from our hook.
		OnKeyboardEntryComplete = Init.ResolveTyped<t_onKeyboardEntryComplete>(BUILTIN_CONSOLEKEYBOARD_ONENTRYCOMPLETE_RVA);
		CHECK_RESOLVED(OnKeyboardEntryComplete);

		// For making it show the keyboard. InstallHook returns the trampoline; the resolved target itself now jumps to our hook.
		auto const displayKeyboard_target = Init.ResolveTyped<t_displayKeyboard>(BUILTIN_CONSOLEKEYBOARD_DISPLAYKEYBOARD_RVA);
		CHECK_RESOLVED(displayKeyboard_target);
		displayKeyboard_orig = (t_displayKeyboard*)Init.InstallHook("SFXGUIHelper_ConsoleKeyboard::DisplayKeyboard", displayKeyboard_target, displayKeyboard_hook);
		CHECK_RESOLVED(displayKeyboard_orig);

		// USFXGUIHelper_ConsoleKeyboard's FTickableObject::Tick (vtable slot 1). Keyboard results are
		// delivered from here so they reach UnrealScript on the game thread.
		auto const keyboardTick_target = Init.ResolveTyped<t_keyboardTick>(BUILTIN_CONSOLEKEYBOARD_TICK_RVA);
		CHECK_RESOLVED(keyboardTick_target);
		keyboardTick_orig = (t_keyboardTick*)Init.InstallHook("SFXGUIHelper_ConsoleKeyboard::Tick", keyboardTick_target, keyboardTick_hook);
		CHECK_RESOLVED(keyboardTick_orig);

		// For allowing SWF to detect if this ASI is installed
		auto const asGetIniBool_target = Init.ResolveTyped<t_asGetIniBool_GetValueImpl>(BUILTIN_ASGETINIBOOL_GETVALUEIMPL_RVA); // nice
		CHECK_RESOLVED(asGetIniBool_target);
		asGetIniBool_GetValueImpl_orig = (t_asGetIniBool_GetValueImpl*)Init.InstallHook("ASGetIniBool::GetValueImpl", asGetIniBool_target, asGetIniBool_hook);
		CHECK_RESOLVED(asGetIniBool_GetValueImpl_orig);
	}
}
