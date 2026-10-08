#pragma once

#include <memory>
#include <LESDK/Headers.hpp>
#include <LESDK/Init.hpp>
#include "Common/Base.hpp"

namespace ControllerKeyboard
{
	// For invoking the OnKeyboardEntryComplete delegate from our hook.
	using t_onKeyboardEntryComplete = void(USFXGUIHelper_ConsoleKeyboard* keyboardHelper, int inputOK, FString* textEntered);
	extern t_onKeyboardEntryComplete* OnKeyboardEntryComplete;

	// For hooking the DisplayKeyboard method.
	using t_displayKeyboard = void(
		USFXGUIHelper_ConsoleKeyboard* keyboardHelper,
		int srTitle,
		int srDescription,
		Keyboard_Options keyboardType,
		int maxLength,
		const FString& defaultValue
	);

	extern t_displayKeyboard* displayKeyboard_orig;

	void displayKeyboard_hook(
		USFXGUIHelper_ConsoleKeyboard* self,
		int srTitle,
		int srDescription,
		Keyboard_Options keyboardType,
		int maxLength,
		const FString& defaultValue
	);

	// FTickableObject::Tick on the helper's tickable sub-object (at +0x60). The game only ticks it while
	// bKeyboardVisible is set (IsTickable), and always on the game thread, so keyboard results are delivered here.
	using t_keyboardTick = void(void* tickable, float deltaTime);
	extern t_keyboardTick* keyboardTick_orig;
	void keyboardTick_hook(void* tickable, float deltaTime);

	// For allowing SWF to detect if this ASI is installed
	using t_asGetIniBool_GetValueImpl = UINT(void* param1, void* param2, FString* param3, FString* param4, FString* param5);
	extern t_asGetIniBool_GetValueImpl* asGetIniBool_GetValueImpl_orig;
	UINT asGetIniBool_hook(void* param1, void* param2, FString* filename, FString* section, FString* key);
}
