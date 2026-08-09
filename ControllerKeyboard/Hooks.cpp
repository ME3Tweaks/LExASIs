#include "Hooks.hpp"

#include <XGameRuntimeInit.h>
#include <XGameUI.h>
#include <new>
#include <mutex>
#include <string>
#include <vector>
#include <Windows.h>
#include "Common/Objects.hpp"

namespace
{
	XGameUiTextEntryInputScope MapInputScope(Keyboard_Options keyboardType)
	{
		switch (keyboardType)
		{
		case KEYBOARD_EMAIL:
			return XGameUiTextEntryInputScope::EmailSmtpAddress;
		case KEYBOARD_PASSWORD:
			return XGameUiTextEntryInputScope::Password;
		case KEYBOARD_CODE:
			return XGameUiTextEntryInputScope::Number;
		case KEYBOARD_STANDARD:
		default:
			return XGameUiTextEntryInputScope::Default;
		}
	}

	bool EnsureGdkInitialized()
	{
		static std::once_flag initOnce;
		static HRESULT initResult = E_FAIL;

		std::call_once(initOnce, []() {
			initResult = XGameRuntimeInitialize();
		});

		return SUCCEEDED(initResult);
	}

	std::string FStringToUtf8(const FString& value)
	{
		const wchar_t* wide = value.Chars();
		if (wide == nullptr || wide[0] == L'\0')
		{
			return {};
		}

		const int wideLength = static_cast<int>(value.Length());
		const int utf8Length = WideCharToMultiByte(CP_UTF8, 0, wide, wideLength, nullptr, 0, nullptr, nullptr);
		if (utf8Length <= 0)
		{
			return {};
		}

		std::string utf8;
		utf8.resize(static_cast<size_t>(utf8Length));
		WideCharToMultiByte(CP_UTF8, 0, wide, wideLength, utf8.data(), utf8Length, nullptr, nullptr);
		return utf8;
	}

	FString Utf8ToFString(const char* utf8)
	{
		FString value{};
		if (utf8 != nullptr && utf8[0] != '\0')
		{
			value.AppendUtf8(utf8);
		}

		return value;
	}

	FString ResolveLocalizedString(int stringRef, const FString& fallback)
	{
		if (stringRef > 0)
		{
			auto* sfxGame = Common::FindFirstObject<ASFXGame>();
			if (sfxGame != nullptr)
			{
				auto localized = sfxGame->GetSimpleString(stringRef, 0);
				if (localized.Any())
				{
					return localized;
				}
			}
		}

		return fallback;
	}

	struct KeyboardAsyncContext
	{
		USFXGUIHelper_ConsoleKeyboard* keyboardHelper = nullptr;
		XAsyncBlock async{};
	};
}

namespace ControllerKeyboard
{
	// ! Global variables
	// ========================================

	// ! DisplayKeyboard Hook
	// ========================================
	// We completely replace this logic because the original function just throws an error and completes input immediately;
	// obviously we want an actual keyboard to appear.
	t_displayKeyboard* displayKeyboard_orig = nullptr;

	void displayKeyboard_hook(
		USFXGUIHelper_ConsoleKeyboard* self,
		int srTitle,
		int srDescription,
		Keyboard_Options keyboardType,
		int maxLength,
		const FString& defaultValue
	)
	{
		if (self == nullptr || OnKeyboardEntryComplete == nullptr)
		{
			return;
		}

		self->bKeyboardVisible = true;

		if (!EnsureGdkInitialized())
		{
			self->bKeyboardVisible = false;
			FString result{};
			OnKeyboardEntryComplete(self, 0, &result);
			return;
		}

		const FString localizedTitle = ResolveLocalizedString(srTitle, self->sTitle);
		const FString localizedDescription = ResolveLocalizedString(srDescription, self->sDescription);

		const std::string titleUtf8 = FStringToUtf8(localizedTitle);
		const std::string descriptionUtf8 = FStringToUtf8(localizedDescription);
		const std::string defaultUtf8 = FStringToUtf8(defaultValue);

		const char* title = titleUtf8.empty() ? nullptr : titleUtf8.c_str();
		const char* description = descriptionUtf8.empty() ? nullptr : descriptionUtf8.c_str();
		const char* defaultText = defaultUtf8.empty() ? nullptr : defaultUtf8.c_str();

		const uint32_t maxTextLength = maxLength > 0 ? static_cast<uint32_t>(maxLength) : 1024U;

		auto* asyncContext = new (std::nothrow) KeyboardAsyncContext{};
		if (asyncContext == nullptr)
		{
			self->bKeyboardVisible = false;
			FString result{};
			OnKeyboardEntryComplete(self, 0, &result);
			return;
		}

		asyncContext->keyboardHelper = self;
		asyncContext->async.context = asyncContext;
		asyncContext->async.queue = nullptr;
		asyncContext->async.callback = [](XAsyncBlock* async)
		{
			auto* context = static_cast<KeyboardAsyncContext*>(async->context);
			FString result{};
			int inputAccepted = 0;

			if (SUCCEEDED(XAsyncGetStatus(async, false)))
			{
				uint32_t resultBufferSize = 0;
				if (SUCCEEDED(XGameUiShowTextEntryResultSize(async, &resultBufferSize)) && resultBufferSize > 0)
				{
					std::vector<char> resultBuffer(resultBufferSize, '\0');
					uint32_t resultBufferUsed = 0;
					if (SUCCEEDED(XGameUiShowTextEntryResult(async, resultBufferSize, resultBuffer.data(), &resultBufferUsed)))
					{
						result = Utf8ToFString(resultBuffer.data());
						inputAccepted = 1;
					}
				}
			}

			if (context->keyboardHelper != nullptr)
			{
				context->keyboardHelper->bKeyboardVisible = false;
				if (OnKeyboardEntryComplete != nullptr)
				{
					OnKeyboardEntryComplete(context->keyboardHelper, inputAccepted, &result);
				}
			}

			delete context;
		};

		const HRESULT openResult = XGameUiShowTextEntryAsync(
			&asyncContext->async,
			title,
			description,
			defaultText,
			MapInputScope(keyboardType),
			maxTextLength
		);

		if (FAILED(openResult))
		{
			delete asyncContext;
			self->bKeyboardVisible = false;
			FString result{};
			OnKeyboardEntryComplete(self, 0, &result);
		}
	}

	// + OnKeyboardEntryComplete
	// ========================================
	t_onKeyboardEntryComplete* OnKeyboardEntryComplete = nullptr;
}
