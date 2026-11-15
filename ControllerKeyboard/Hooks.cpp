#include "Hooks.hpp"

#if defined(CONTROLLERKEYBOARD_HAS_GDK)
#include <XGameRuntimeInit.h>
#include <XGameUI.h>
#endif

#if defined(CONTROLLERKEYBOARD_HAS_STEAMWORKS)
// Headers only: nothing links steam_api64.lib. The game never ships steam_api64.dll (and it may not be a Steam copy at
// all), so the Steam keyboard talks to the running Steam client's own steamclient64.dll instead. See SteamKeyboard below.
#pragma warning(push)
#pragma warning(disable: 4828)
#include <steam/steam_api.h>
#pragma warning(pop)
#endif

#include <cstdint>
#include <cwchar>
#include <iterator>
#include <mutex>
#include <new>
#include <string>
#include <vector>
#include <Windows.h>
#include "Common/Objects.hpp"

namespace
{
	// ! Shared helpers
	// ========================================

	std::string FStringToUtf8(const FString& value)
	{
		const wchar_t* wide = value.Chars();
		const int wideLength = static_cast<int>(value.Length());
		if (wideLength <= 0)
		{
			return {};
		}

		const int utf8Length = WideCharToMultiByte(CP_UTF8, 0, wide, wideLength, nullptr, 0, nullptr, nullptr);
		if (utf8Length <= 0)
		{
			return {};
		}

		std::string utf8(static_cast<size_t>(utf8Length), '\0');
		WideCharToMultiByte(CP_UTF8, 0, wide, wideLength, utf8.data(), utf8Length, nullptr, nullptr);
		return utf8;
	}

	std::wstring Utf8ToWide(const char* utf8)
	{
		if (utf8 == nullptr || utf8[0] == '\0')
		{
			return {};
		}

		const int wideLength = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
		if (wideLength <= 1)
		{
			return {};
		}

		std::wstring wide(static_cast<size_t>(wideLength), L'\0');
		MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wide.data(), wideLength);
		wide.resize(static_cast<size_t>(wideLength - 1)); // drop the terminator MultiByteToWideChar wrote
		return wide;
	}

	FString ResolveLocalizedString(int stringRef, const FString& fallback)
	{
		if (stringRef > 0)
		{
			auto* sfxGame = Common::FindFirstObject<ASFXGame>();
			if (sfxGame != nullptr)
			{
				auto localized = sfxGame->GetSimpleString(stringRef, 0);
				if (localized.Length() > 0)
				{
					return localized;
				}
			}
		}

		return fallback;
	}

	std::string BuildPrompt(const FString& title, const FString& description)
	{
		const std::string titleUtf8 = FStringToUtf8(title);
		const std::string descriptionUtf8 = FStringToUtf8(description);

		if (titleUtf8.empty())
		{
			return descriptionUtf8;
		}

		if (descriptionUtf8.empty())
		{
			return titleUtf8;
		}

		return titleUtf8 + "\n" + descriptionUtf8;
	}

	// The console Tick (USFXGUIHelper_ConsoleKeyboard vtable slot 1) strips SFXGUIInputHandler.BannedChars from the
	// entered text before completing; the PC build compiled that path out, so do it here.
	std::wstring StripBannedChars(std::wstring text)
	{
		UClass* const inputHandlerClass = USFXGUIInputHandler::StaticClass();
		auto* const defaults = inputHandlerClass != nullptr
			? static_cast<USFXGUIInputHandler*>(inputHandlerClass->ClassDefaultObject)
			: nullptr;
		if (defaults == nullptr || defaults->BannedChars.Length() == 0)
		{
			return text;
		}

		const wchar_t* const banned = defaults->BannedChars.Chars();
		std::erase_if(text, [banned](wchar_t const c) { return std::wcschr(banned, c) != nullptr; });
		return text;
	}


	// ! Pending request
	// ========================================
	// Only touched on the game thread: DisplayKeyboard starts a request, and both backends complete it from the
	// helper's Tick (Steam callbacks are polled there, GDK completions are dispatched there).

	struct PendingKeyboard
	{
		USFXGUIHelper_ConsoleKeyboard* helper = nullptr;
		uint32_t id = 0;
		bool complete = false;
		bool accepted = false;
		std::wstring text{};
	};

	PendingKeyboard g_pending{};
	uint32_t g_nextRequestId = 1;

	uint32_t BeginRequest(USFXGUIHelper_ConsoleKeyboard* helper)
	{
		g_pending = PendingKeyboard{ helper, g_nextRequestId++ };
		return g_pending.id;
	}

	void CompleteRequest(uint32_t requestId, bool accepted, std::wstring text)
	{
		if (requestId != g_pending.id || g_pending.complete)
		{
			return; // A newer request replaced this one.
		}

		g_pending.complete = true;
		g_pending.accepted = accepted;
		g_pending.text = accepted ? std::move(text) : std::wstring{};
	}


#if defined(CONTROLLERKEYBOARD_HAS_STEAMWORKS)
	// ! Steam keyboard (Big Picture / Steam Deck)
	// ========================================
	// This does what SteamAPI_Init does, minus steam_api64.dll: load the running client's steamclient64.dll (the
	// registry names it), create a pipe, connect to the logged-in user and get ISteamUtils. Callbacks are polled
	// with Steam_BGetCallback, the same export steam_api's manual dispatch uses.
	//
	// Only tried when Steam's overlay is in the process. ShowGamepadTextInput draws in the overlay, so without it
	// there is nothing to show, and its presence means Steam launched this process.
	namespace SteamKeyboard
	{
		using t_CreateInterface = void*(const char* version, int* returnCode);
		// Older clients took a third HSteamCall* out-parameter. Passing one is harmless on x64 if it is ignored.
		using t_BGetCallback = bool(HSteamPipe pipe, CallbackMsg_t* message, int32* call);
		using t_FreeLastCallback = bool(HSteamPipe pipe);

		// Mass Effect Legendary Edition. The same ID BioWare ships in ME3\Binaries\Win64\steam_appid.txt.
		constexpr wchar_t k_legendaryEditionAppId[] = L"1328670";

		ISteamClient* g_client = nullptr;
		ISteamUtils* g_utils = nullptr;
		HSteamPipe g_pipe = 0;
		HSteamUser g_user = 0;
		t_BGetCallback* g_getCallback = nullptr;
		t_FreeLastCallback* g_freeLastCallback = nullptr;
		bool g_connectAttempted = false;
		uint32_t g_requestId = 0;

		bool IsEnvironmentFlagSet(const wchar_t* name)
		{
			wchar_t value[8]{};
			const DWORD length = GetEnvironmentVariableW(name, value, static_cast<DWORD>(std::size(value)));
			return length > 0 && length < std::size(value) && value[0] == L'1';
		}

		// ShowGamepadTextInput only works in Big Picture or on Steam Deck, and needs the overlay in this process.
		// Steam sets SteamTenfoot=1 on processes it launches from Big Picture and SteamDeck=1 on Deck. Outside those,
		// don't connect at all: registering this process with Steam (ConnectToGlobalUser) when its keyboard can't
		// show anyway is the prime suspect for controller input not reaching the Windows text entry window.
		bool CanShowKeyboard()
		{
			return GetModuleHandleW(L"gameoverlayrenderer64.dll") != nullptr
				&& (IsEnvironmentFlagSet(L"SteamTenfoot") || IsEnvironmentFlagSet(L"SteamDeck"));
		}

		HMODULE LoadSteamClient()
		{
			if (HMODULE const loaded = GetModuleHandleW(L"steamclient64.dll"); loaded != nullptr)
			{
				return loaded;
			}

			wchar_t path[MAX_PATH]{};
			DWORD pathSize = sizeof(path);
			if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\Valve\\Steam\\ActiveProcess", L"SteamClientDll64",
				RRF_RT_REG_SZ, nullptr, path, &pathSize) != ERROR_SUCCESS || path[0] == L'\0')
			{
				return nullptr;
			}

			// Altered search path so its own dependencies (tier0_s64.dll etc.) load from the Steam folder.
			return LoadLibraryExW(path, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
		}

		void Disconnect()
		{
			if (g_client != nullptr && g_pipe != 0)
			{
				if (g_user != 0)
				{
					g_client->ReleaseUser(g_pipe, g_user);
				}
				g_client->BReleaseSteamPipe(g_pipe);
			}

			g_client = nullptr;
			g_utils = nullptr;
			g_pipe = 0;
			g_user = 0;
		}

		bool Connect()
		{
			if (g_utils != nullptr)
			{
				return true;
			}

			if (g_connectAttempted || !CanShowKeyboard())
			{
				return false;
			}

			g_connectAttempted = true;

			// steamclient reads the app ID from the environment, like SteamAPI_Init after it reads steam_appid.txt.
			// Steam sets it on processes it launches; LMP or a launcher in between may not pass it on.
			if (GetEnvironmentVariableW(L"SteamAppId", nullptr, 0) == 0)
			{
				SetEnvironmentVariableW(L"SteamAppId", k_legendaryEditionAppId);
			}

			HMODULE const steamClient = LoadSteamClient();
			if (steamClient == nullptr)
			{
				LEASI_WARN("Steam overlay is loaded but steamclient64.dll could not be loaded; using the Windows keyboard");
				return false;
			}

			auto* const createInterface = reinterpret_cast<t_CreateInterface*>(GetProcAddress(steamClient, "CreateInterface"));
			g_getCallback = reinterpret_cast<t_BGetCallback*>(GetProcAddress(steamClient, "Steam_BGetCallback"));
			g_freeLastCallback = reinterpret_cast<t_FreeLastCallback*>(GetProcAddress(steamClient, "Steam_FreeLastCallback"));
			if (createInterface == nullptr || g_getCallback == nullptr || g_freeLastCallback == nullptr)
			{
				LEASI_WARN("steamclient64.dll is missing expected exports; using the Windows keyboard");
				return false;
			}

			g_client = static_cast<ISteamClient*>(createInterface(STEAMCLIENT_INTERFACE_VERSION, nullptr));
			if (g_client == nullptr)
			{
				LEASI_WARN("Steam client does not provide {}; using the Windows keyboard", STEAMCLIENT_INTERFACE_VERSION);
				return false;
			}

			g_pipe = g_client->CreateSteamPipe();
			if (g_pipe != 0)
			{
				g_user = g_client->ConnectToGlobalUser(g_pipe);
			}
			if (g_user != 0)
			{
				g_utils = g_client->GetISteamUtils(g_pipe, STEAMUTILS_INTERFACE_VERSION);
			}

			if (g_utils == nullptr)
			{
				LEASI_WARN("Could not connect to the Steam user; using the Windows keyboard");
				Disconnect();
				return false;
			}

			LEASI_INFO("Connected to Steam for the gamepad keyboard");
			return true;
		}

		bool Show(uint32_t requestId, Keyboard_Options keyboardType, uint32_t maxTextLength, const std::string& prompt, const std::string& defaultText)
		{
			if (!Connect())
			{
				return false;
			}

			// Returns false unless the overlay can show it (Big Picture, Steam Deck).
			const bool shown = g_utils->ShowGamepadTextInput(
				keyboardType == KEYBOARD_PASSWORD ? k_EGamepadTextInputModePassword : k_EGamepadTextInputModeNormal,
				k_EGamepadTextInputLineModeSingleLine,
				prompt.c_str(),
				maxTextLength,
				defaultText.c_str());

			if (shown)
			{
				g_requestId = requestId;
			}

			return shown;
		}

		void Pump()
		{
			if (g_utils == nullptr)
			{
				return;
			}

			bool dismissed = false;
			bool submitted = false;

			CallbackMsg_t message{};
			int32 call = 0;
			while (g_getCallback(g_pipe, &message, &call))
			{
				if (message.m_iCallback == GamepadTextInputDismissed_t::k_iCallback
					&& message.m_cubParam >= static_cast<int>(sizeof(GamepadTextInputDismissed_t)))
				{
					dismissed = true;
					submitted = reinterpret_cast<GamepadTextInputDismissed_t const*>(message.m_pubParam)->m_bSubmitted;
				}

				g_freeLastCallback(g_pipe);
			}

			if (!dismissed)
			{
				return;
			}

			std::wstring text{};
			if (submitted)
			{
				// Length includes the terminator.
				const uint32 length = g_utils->GetEnteredGamepadTextLength();
				std::vector<char> buffer(static_cast<size_t>(length) + 1, '\0');
				if (length > 0 && g_utils->GetEnteredGamepadTextInput(buffer.data(), static_cast<uint32>(buffer.size())))
				{
					text = Utf8ToWide(buffer.data());
				}
			}

			CompleteRequest(g_requestId, submitted, std::move(text));
		}
	}
#endif


#if defined(CONTROLLERKEYBOARD_HAS_GDK)
	// ! Windows keyboard (GDK text entry)
	// ========================================
	// XGameUiShowTextEntryAsync opens a separate text entry window; Windows adds its touch keyboard only when the
	// user is set up for one (tablet mode etc.). This uses the default process queue like the original GDK-only
	// version did, so the completion callback runs on a thread-pool thread. It only queues the result; Pump hands
	// it over from the helper's Tick, on the game thread.
	namespace GdkKeyboard
	{
		bool g_initAttempted = false;
		bool g_ready = false;

		struct AsyncContext
		{
			XAsyncBlock async{};
			uint32_t requestId = 0;
		};

		struct Result
		{
			uint32_t requestId = 0;
			bool accepted = false;
			std::wstring text{};
		};

		std::mutex g_resultsMutex{};
		std::vector<Result> g_results{};

		bool EnsureInitialized()
		{
			if (!g_initAttempted)
			{
				g_initAttempted = true;

				const HRESULT hr = XGameRuntimeInitialize();
				g_ready = SUCCEEDED(hr);
				if (!g_ready)
				{
					LEASI_WARN("Gaming Runtime unavailable (0x{:08X}); the Windows keyboard is disabled", static_cast<uint32_t>(hr));
				}
			}

			return g_ready;
		}

		void CALLBACK OnTextEntryComplete(XAsyncBlock* async)
		{
			auto* const context = static_cast<AsyncContext*>(async->context);

			Result result{ context->requestId };
			if (SUCCEEDED(XAsyncGetStatus(async, false)))
			{
				uint32_t resultSize = 0;
				if (SUCCEEDED(XGameUiShowTextEntryResultSize(async, &resultSize)))
				{
					std::vector<char> buffer(static_cast<size_t>(resultSize) + 1, '\0');
					uint32_t resultUsed = 0;
					if (SUCCEEDED(XGameUiShowTextEntryResult(async, resultSize, buffer.data(), &resultUsed)))
					{
						result.text = Utf8ToWide(buffer.data());
						result.accepted = true;
					}
				}
			}

			delete context;

			std::lock_guard lock(g_resultsMutex);
			g_results.push_back(std::move(result));
		}

		bool Show(uint32_t requestId, Keyboard_Options keyboardType, uint32_t maxTextLength, const std::string& title, const std::string& description, const std::string& defaultText)
		{
			if (!EnsureInitialized())
			{
				return false;
			}

			XGameUiTextEntryInputScope inputScope = XGameUiTextEntryInputScope::Default;
			switch (keyboardType)
			{
			case KEYBOARD_EMAIL:
				inputScope = XGameUiTextEntryInputScope::EmailSmtpAddress;
				break;
			case KEYBOARD_PASSWORD:
				inputScope = XGameUiTextEntryInputScope::Password;
				break;
			case KEYBOARD_CODE:
				inputScope = XGameUiTextEntryInputScope::Number;
				break;
			default:
				break;
			}

			auto* const context = new (std::nothrow) AsyncContext{};
			if (context == nullptr)
			{
				return false;
			}

			context->requestId = requestId;
			context->async.queue = nullptr; // default process queue
			context->async.context = context;
			context->async.callback = OnTextEntryComplete;

			const HRESULT hr = XGameUiShowTextEntryAsync(
				&context->async,
				title.empty() ? nullptr : title.c_str(),
				description.empty() ? nullptr : description.c_str(),
				defaultText.empty() ? nullptr : defaultText.c_str(),
				inputScope,
				maxTextLength);

			if (FAILED(hr))
			{
				LEASI_WARN("XGameUiShowTextEntryAsync failed (0x{:08X})", static_cast<uint32_t>(hr));
				delete context;
				return false;
			}

			return true;
		}

		void Pump()
		{
			std::vector<Result> results{};
			{
				std::lock_guard lock(g_resultsMutex);
				results.swap(g_results);
			}

			for (Result& result : results)
			{
				CompleteRequest(result.requestId, result.accepted, std::move(result.text));
			}
		}
	}
#endif

	bool IsAnyKeyboardAvailable()
	{
#if defined(CONTROLLERKEYBOARD_HAS_STEAMWORKS)
		if (SteamKeyboard::CanShowKeyboard())
		{
			return true;
		}
#endif
#if defined(CONTROLLERKEYBOARD_HAS_GDK)
		if (GdkKeyboard::EnsureInitialized())
		{
			return true;
		}
#endif
		return false;
	}
}

namespace ControllerKeyboard
{
	// ! ASGetIniBool::GetValueImpl Hook
	// ========================================
	// We hook this so we can ask for values in an ini
	// and if the ASI doesn't hook the method and return a value
	// it will return the default value and the SWF can change
	// behavior.

	t_asGetIniBool_GetValueImpl* asGetIniBool_GetValueImpl_orig = nullptr;
	UINT asGetIniBool_hook(void* param1, void* param2, FString* filename, FString* section, FString* key) {
		// LEASI_INFO(L"asGetIniBool_hook called with filename: {}, section: {}, key: {}", filename->Chars(), section->Chars(), key->Chars());
		if (section->Equals(L"Engine.ASIMods") && key->Equals(L"ControllerKeyboard")) // We don't check the filename, we don't really care
		{
			uintptr_t* pRetVal = *(uintptr_t**)param2; // dereference Params* to get pRetVal (GFxValue*)
			*(bool*)(pRetVal + 2) = IsAnyKeyboardAvailable(); // the actual bool value
			*(uint32_t*)(pRetVal + 1) = 2;       // GFxValue type tag = Bool
			return true;
		}
		return asGetIniBool_GetValueImpl_orig(param1, param2, filename, section, key);
	}

	// ! DisplayKeyboard Hook
	// ========================================
	// The PC build of this native immediately completes with EntryOK=0 ("Keyboard Interface Not Available On PC").
	// We show the Steam keyboard if we can, else the Windows one, and only fall back to that stub if neither shows.
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
			displayKeyboard_orig(self, srTitle, srDescription, keyboardType, maxLength, defaultValue);
			return;
		}

		const FString localizedTitle = ResolveLocalizedString(srTitle, self->sTitle);
		const FString localizedDescription = ResolveLocalizedString(srDescription, self->sDescription);
		const uint32_t maxTextLength = maxLength > 0 ? static_cast<uint32_t>(maxLength) : 1024U;
		const std::string defaultUtf8 = FStringToUtf8(defaultValue);

		const uint32_t requestId = BeginRequest(self);
		bool shown = false;

#if defined(CONTROLLERKEYBOARD_HAS_STEAMWORKS)
		shown = SteamKeyboard::Show(requestId, keyboardType, maxTextLength, BuildPrompt(localizedTitle, localizedDescription), defaultUtf8);
#endif
#if defined(CONTROLLERKEYBOARD_HAS_GDK)
		if (!shown)
		{
			shown = GdkKeyboard::Show(requestId, keyboardType, maxTextLength,
				FStringToUtf8(localizedTitle), FStringToUtf8(localizedDescription), defaultUtf8);
		}
#endif

		if (!shown)
		{
			BeginRequest(nullptr); // invalidate
			displayKeyboard_orig(self, srTitle, srDescription, keyboardType, maxLength, defaultValue);
			return;
		}

		// Makes the game tick the helper (IsTickable returns this bit), which is where the result is delivered.
		self->bKeyboardVisible = true;
	}

	// ! FTickableObject::Tick Hook
	// ========================================
	t_keyboardTick* keyboardTick_orig = nullptr;

	void keyboardTick_hook(void* tickable, float deltaTime)
	{
		keyboardTick_orig(tickable, deltaTime);

#if defined(CONTROLLERKEYBOARD_HAS_STEAMWORKS)
		SteamKeyboard::Pump();
#endif
#if defined(CONTROLLERKEYBOARD_HAS_GDK)
		GdkKeyboard::Pump();
#endif

		// The FTickableObject sub-object (VfTable_FTickableObject) sits at +0x60 in USFXGUIHelper_ConsoleKeyboard.
		auto* const self = reinterpret_cast<USFXGUIHelper_ConsoleKeyboard*>(static_cast<char*>(tickable) - 0x60);
		if (g_pending.helper != self || !g_pending.complete)
		{
			return;
		}

		// Clear first: the delegate may open another keyboard.
		const bool accepted = g_pending.accepted;
		const std::wstring text = StripBannedChars(std::move(g_pending.text));
		g_pending = PendingKeyboard{};

		FString entered{ text.c_str() };
		self->bKeyboardVisible = false;
		OnKeyboardEntryComplete(self, accepted ? 1 : 0, &entered);
	}

	// + OnKeyboardEntryComplete
	// ========================================
	t_onKeyboardEntryComplete* OnKeyboardEntryComplete = nullptr;
}
