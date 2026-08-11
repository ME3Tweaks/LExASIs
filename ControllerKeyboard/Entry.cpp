
#include "Common/Base.hpp"
#include "Common/DefaultLogger.hpp"
#include "ControllerKeyboard/Entry.hpp"
#include "ControllerKeyboard/Hooks.hpp"
#include "ControllerKeyboard/SharedVersion.h"

SPI_PLUGINSIDE_SUPPORT(SDK_TARGET_NAME_W ASI_NAME_NO_SPACE_W, DEVELOPER_W, L"" VERSION_STRING_W, SPI_GAME_SDK_TARGET, SPI_VERSION_ANY);
SPI_PLUGINSIDE_POSTLOAD;
SPI_PLUGINSIDE_ASYNCATTACH;


SPI_IMPLEMENT_ATTACH
{
	::LESDK::Initializer Init{ InterfacePtr, SDK_TARGET_NAME_A ASI_NAME_NO_SPACE_A };
#ifdef DEBUG	
::Common::SetupDefaultLogger(SDK_TARGET_NAME_A, ASI_NAME_NO_SPACE_A);
#endif	
	::ControllerKeyboard::InitializeGlobals(Init);
	::ControllerKeyboard::InitializeHooks(Init);

	return true;
}

SPI_IMPLEMENT_DETACH
{
	LEASI_UNUSED(InterfacePtr);
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
		// LE3 RVA:
		OnKeyboardEntryComplete = Init.ResolveTyped<t_onKeyboardEntryComplete>(LEASI_RVA(0xadf040));

		// For making it show the keyboard
		displayKeyboard_orig = Init.ResolveTyped<t_displayKeyboard>(LEASI_RVA(0xacf7b0));
		CHECK_RESOLVED(displayKeyboard_orig);
		Init.InstallHook("SFXGUIHelper_ConsoleKeyboard::DisplayKeyboard", displayKeyboard_orig, displayKeyboard_hook);
		CHECK_RESOLVED(displayKeyboard_orig);

		// For allowing SWF to detect if this ASI is installed
		asGetIniBool_GetValueImpl_orig = Init.ResolveTyped<t_asGetIniBool_GetValueImpl>(LEASI_RVA(0xbabe70)); // nice
		CHECK_RESOLVED(asGetIniBool_GetValueImpl_orig);
		Init.InstallHook("ASGetIniBool::GetValueImpl", asGetIniBool_GetValueImpl_orig, asGetIniBool_hook);
		CHECK_RESOLVED(asGetIniBool_GetValueImpl_orig);
	}
}
