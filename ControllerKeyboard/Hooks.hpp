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
}
