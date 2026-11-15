// Standalone harness for ControllerKeyboard's Windows keyboard path (XGameUiShowTextEntryAsync), run outside the game.
//
// It calls the text entry API the same way the ASI does (default process queue, same arguments) from a plain window,
// then logs every foreground window change and every XInput button change while the text entry is open. If the pad
// types here but not in-game, the game is in the way; the switches below make this window act more like the game
// to narrow down how:
//
//   --xinput      poll XInputGetState every frame, like the game does (also logs button changes)
//   --gameinput   also read the pad through GameInput, which the system keyboard uses, and log button changes
//                 (each line names the device the reading came from, vendor:product)
//   --inject      once the text entry has the foreground, press D-pad right, right, down, A, A, left, A through
//                 Windows' own gamepad injection (InputInjector). If the keyboard reacts to these but not to the
//                 real pad, the real pad's path into the Gaming UI is the problem; if it ignores both, the Gaming
//                 UI ignores gamepad input altogether.
//   --wgi         also read pads through Windows.Gaming.Input, the API XAML apps such as the Gaming UI use for
//                 gamepad navigation; logs pad count changes and button changes per pad
//   --noentry     don't open the text entry automatically; press F5 when ready
//   --refocus     take the foreground back whenever another window gets it (SetForegroundWindow)
//   --fullscreen  borderless window covering the primary monitor
//   --topmost     WS_EX_TOPMOST window
//
// F5 in the test window opens the text entry again, Esc quits. Run it from a console to see the log.

#include <Windows.h>
#include <Xinput.h>
#include <GameInput.h>
#include <XGameRuntimeInit.h>
#include <XGameUI.h>
#include <XAsync.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cwchar>
#include <string>
#include <thread>
#include <vector>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Gaming.Input.h>
#include <winrt/Windows.UI.Input.Preview.Injection.h>

namespace
{
	struct Options
	{
		bool pollXInput = false;
		bool pollGameInput = false;
		bool refocus = false;
		bool fullscreen = false;
		bool topmost = false;
		bool inject = false;
		bool pollWgi = false;
		bool noEntry = false;
	};

	Options g_options{};
	HWND g_window = nullptr;
	std::atomic<bool> g_entryOpen = false;
	const auto g_start = std::chrono::steady_clock::now();

	void Log(const wchar_t* format, ...)
	{
		const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - g_start).count();
		wprintf(L"[%7.3f] ", static_cast<double>(elapsed) / 1000.0);

		va_list args;
		va_start(args, format);
		vwprintf(format, args);
		va_end(args);

		wprintf(L"\n");
		fflush(stdout);
	}

	std::wstring DescribeWindow(HWND window)
	{
		if (window == nullptr)
		{
			return L"(none)";
		}

		wchar_t title[256]{};
		wchar_t windowClass[128]{};
		GetWindowTextW(window, title, static_cast<int>(std::size(title)));
		GetClassNameW(window, windowClass, static_cast<int>(std::size(windowClass)));

		DWORD processId = 0;
		GetWindowThreadProcessId(window, &processId);

		wchar_t image[MAX_PATH]{};
		if (HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId); process != nullptr)
		{
			DWORD imageSize = static_cast<DWORD>(std::size(image));
			QueryFullProcessImageNameW(process, 0, image, &imageSize);
			CloseHandle(process);
		}

		const wchar_t* imageName = std::wcsrchr(image, L'\\');
		wchar_t buffer[700]{};
		swprintf_s(buffer, L"%p \"%s\" class=%s pid=%lu (%s)%s", static_cast<void*>(window), title, windowClass, processId,
			imageName != nullptr ? imageName + 1 : image, processId == GetCurrentProcessId() ? L" [this process]" : L"");
		return buffer;
	}

	void LogForegroundChanges()
	{
		static HWND lastForeground = reinterpret_cast<HWND>(-1);
		HWND const foreground = GetForegroundWindow();
		if (foreground != lastForeground)
		{
			lastForeground = foreground;
			Log(L"foreground: %s", DescribeWindow(foreground).c_str());
		}
	}

	void PollXInput()
	{
		static WORD lastButtons[XUSER_MAX_COUNT]{};
		static bool lastConnected[XUSER_MAX_COUNT]{};

		for (DWORD user = 0; user < XUSER_MAX_COUNT; ++user)
		{
			XINPUT_STATE state{};
			const bool connected = XInputGetState(user, &state) == ERROR_SUCCESS;
			if (connected != lastConnected[user])
			{
				lastConnected[user] = connected;
				Log(L"xinput %lu: %s", user, connected ? L"connected" : L"disconnected");
			}

			if (connected && state.Gamepad.wButtons != lastButtons[user])
			{
				lastButtons[user] = state.Gamepad.wButtons;
				Log(L"xinput %lu: buttons=0x%04X%s%s", user, state.Gamepad.wButtons, g_entryOpen ? L" (text entry open)" : L"",
					GetForegroundWindow() == g_window ? L" [test window focused]" : L" [other window focused]");
			}
		}
	}

	IGameInput* g_gameInput = nullptr;

	void PollGameInput()
	{
		static HRESULT lastResult = S_FALSE;
		static GameInputGamepadButtons lastButtons = GameInputGamepadNone;

		IGameInputReading* reading = nullptr;
		const HRESULT hr = g_gameInput->GetCurrentReading(GameInputKindGamepad, nullptr, &reading);
		if (hr != lastResult)
		{
			lastResult = hr;
			Log(L"gameinput: GetCurrentReading = 0x%08X%s", static_cast<unsigned>(hr),
				GetForegroundWindow() == g_window ? L" [test window focused]" : L" [other window focused]");
		}

		if (SUCCEEDED(hr) && reading != nullptr)
		{
			GameInputGamepadState state{};
			if (reading->GetGamepadState(&state) && state.buttons != lastButtons)
			{
				lastButtons = state.buttons;

				IGameInputDevice* device = nullptr;
				reading->GetDevice(&device);
				GameInputDeviceInfo const* const info = device != nullptr ? device->GetDeviceInfo() : nullptr;

				Log(L"gameinput: buttons=0x%08X device=%04X:%04X%s%s", static_cast<unsigned>(state.buttons),
					info != nullptr ? info->vendorId : 0, info != nullptr ? info->productId : 0,
					g_entryOpen ? L" (text entry open)" : L"",
					GetForegroundWindow() == g_window ? L" [test window focused]" : L" [other window focused]");

				if (device != nullptr)
				{
					device->Release();
				}
			}

			reading->Release();
		}
	}

	void PollWindowsGamingInput()
	{
		using winrt::Windows::Gaming::Input::Gamepad;

		static uint32_t lastCount = UINT32_MAX;
		static std::vector<uint32_t> lastButtons{};

		auto const pads = Gamepad::Gamepads();
		const uint32_t count = pads.Size();
		if (count != lastCount)
		{
			lastCount = count;
			lastButtons.assign(count, 0);
			Log(L"wgi: %u gamepad(s)%s", count, GetForegroundWindow() == g_window ? L" [test window focused]" : L" [other window focused]");
		}

		for (uint32_t i = 0; i < count; ++i)
		{
			const uint32_t buttons = static_cast<uint32_t>(pads.GetAt(i).GetCurrentReading().Buttons);
			if (buttons != lastButtons[i])
			{
				lastButtons[i] = buttons;
				Log(L"wgi: pad %u buttons=0x%08X%s%s", i, buttons, g_entryOpen ? L" (text entry open)" : L"",
					GetForegroundWindow() == g_window ? L" [test window focused]" : L" [other window focused]");
			}
		}
	}

	bool IsGamingUiForeground()
	{
		wchar_t title[64]{};
		GetWindowTextW(GetForegroundWindow(), title, static_cast<int>(std::size(title)));
		return wcscmp(title, L"Gaming UI") == 0;
	}

	// Feeds a fixed sequence of presses through InputInjector, which goes into the same system gamepad path a real
	// pad does. Runs on its own thread so the message loop keeps logging.
	void RunInjection()
	{
		using namespace winrt::Windows::UI::Input::Preview::Injection;
		using winrt::Windows::Gaming::Input::GamepadButtons;

		winrt::init_apartment(winrt::apartment_type::multi_threaded);

		try
		{
			InputInjector injector = InputInjector::TryCreate();
			if (!injector)
			{
				Log(L"inject: InputInjector::TryCreate returned null; injection isn't allowed for this process");
				return;
			}

			injector.InitializeGamepadInjection();
			Log(L"inject: gamepad injection initialized; waiting for the injected pad to appear");
			Sleep(1500);

			struct Step
			{
				GamepadButtons buttons;
				const wchar_t* name;
			};

			const Step steps[] = {
				{ GamepadButtons::DPadRight, L"D-pad right" },
				{ GamepadButtons::DPadRight, L"D-pad right" },
				{ GamepadButtons::DPadDown, L"D-pad down" },
				{ GamepadButtons::A, L"A" },
				{ GamepadButtons::A, L"A" },
				{ GamepadButtons::DPadLeft, L"D-pad left" },
				{ GamepadButtons::A, L"A" },
			};

			Log(L"inject: WATCH THE KEYBOARD - presses start in 3 seconds");
			Sleep(3000);

			for (const Step& step : steps)
			{
				// Only press while the Gaming UI is in front, so a stray click doesn't send these somewhere else.
				if (!IsGamingUiForeground())
				{
					Log(L"inject: paused; click the Gaming UI window to continue");
					while (!IsGamingUiForeground())
					{
						Sleep(100);
					}
					Log(L"inject: Gaming UI is in front again; resuming in 1 second");
					Sleep(1000);
				}

				InjectedInputGamepadInfo pressed{};
				pressed.Buttons(step.buttons);
				injector.InjectGamepadInput(pressed);
				Log(L"inject: %s down", step.name);
				Sleep(150);

				injector.InjectGamepadInput(InjectedInputGamepadInfo{});
				Sleep(1000);
			}

			injector.UninitializeGamepadInjection();
			Log(L"inject: done");
		}
		catch (winrt::hresult_error const& error)
		{
			Log(L"inject: failed 0x%08X %s", static_cast<unsigned>(error.code().value), error.message().c_str());
		}
	}

	struct AsyncContext
	{
		XAsyncBlock async{};
	};

	void CALLBACK OnTextEntryComplete(XAsyncBlock* async)
	{
		auto* const context = static_cast<AsyncContext*>(async->context);

		const HRESULT status = XAsyncGetStatus(async, false);
		std::string text{};
		if (SUCCEEDED(status))
		{
			uint32_t resultSize = 0;
			if (SUCCEEDED(XGameUiShowTextEntryResultSize(async, &resultSize)))
			{
				std::vector<char> buffer(static_cast<size_t>(resultSize) + 1, '\0');
				uint32_t resultUsed = 0;
				if (SUCCEEDED(XGameUiShowTextEntryResult(async, resultSize, buffer.data(), &resultUsed)))
				{
					text = buffer.data();
				}
			}
		}

		Log(L"text entry finished: status=0x%08X thread=%lu text(utf8)=\"%S\"", static_cast<unsigned>(status), GetCurrentThreadId(), text.c_str());
		g_entryOpen = false;
		delete context;
	}

	void OpenTextEntry()
	{
		if (g_entryOpen)
		{
			Log(L"text entry already open");
			return;
		}

		auto* const context = new AsyncContext{};
		context->async.queue = nullptr; // default process queue, same as the ASI
		context->async.context = context;
		context->async.callback = OnTextEntryComplete;

		g_entryOpen = true;
		const HRESULT hr = XGameUiShowTextEntryAsync(&context->async, "ControllerKeyboard test", "Type with the controller",
			"default", XGameUiTextEntryInputScope::Default, 64);
		Log(L"XGameUiShowTextEntryAsync = 0x%08X (thread %lu)", static_cast<unsigned>(hr), GetCurrentThreadId());

		if (FAILED(hr))
		{
			g_entryOpen = false;
			delete context;
		}
	}

	LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
	{
		switch (message)
		{
		case WM_KEYDOWN:
			if (wParam == VK_F5)
			{
				OpenTextEntry();
				return 0;
			}
			if (wParam == VK_ESCAPE)
			{
				DestroyWindow(window);
				return 0;
			}
			break;
		case WM_ACTIVATE:
			Log(L"WM_ACTIVATE %s", LOWORD(wParam) == WA_INACTIVE ? L"inactive" : L"active");
			break;
		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;
		}

		return DefWindowProcW(window, message, wParam, lParam);
	}

	void ParseOptions(int argc, wchar_t** argv)
	{
		for (int i = 1; i < argc; ++i)
		{
			if (wcscmp(argv[i], L"--xinput") == 0) g_options.pollXInput = true;
			else if (wcscmp(argv[i], L"--gameinput") == 0) g_options.pollGameInput = true;
			else if (wcscmp(argv[i], L"--refocus") == 0) g_options.refocus = true;
			else if (wcscmp(argv[i], L"--fullscreen") == 0) g_options.fullscreen = true;
			else if (wcscmp(argv[i], L"--topmost") == 0) g_options.topmost = true;
			else if (wcscmp(argv[i], L"--inject") == 0) g_options.inject = true;
			else if (wcscmp(argv[i], L"--wgi") == 0) g_options.pollWgi = true;
			else if (wcscmp(argv[i], L"--noentry") == 0) g_options.noEntry = true;
			else Log(L"unknown option: %s", argv[i]);
		}
	}
}

int wmain(int argc, wchar_t** argv)
{
	ParseOptions(argc, argv);
	Log(L"options: xinput=%d gameinput=%d wgi=%d refocus=%d fullscreen=%d topmost=%d inject=%d noentry=%d", g_options.pollXInput,
		g_options.pollGameInput, g_options.pollWgi, g_options.refocus, g_options.fullscreen, g_options.topmost, g_options.inject,
		g_options.noEntry);

	// Windows.Gaming.Input needs the WinRT apartment on the thread that polls it.
	winrt::init_apartment(winrt::apartment_type::single_threaded);

	const HRESULT init = XGameRuntimeInitialize();
	Log(L"XGameRuntimeInitialize = 0x%08X", static_cast<unsigned>(init));
	if (FAILED(init))
	{
		return 1;
	}

	if (g_options.pollGameInput)
	{
		const HRESULT created = GameInputCreate(&g_gameInput);
		Log(L"GameInputCreate = 0x%08X", static_cast<unsigned>(created));
		if (FAILED(created))
		{
			g_gameInput = nullptr;
		}
	}

	WNDCLASSW windowClass{};
	windowClass.lpfnWndProc = WindowProc;
	windowClass.hInstance = GetModuleHandleW(nullptr);
	windowClass.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512)); // IDC_ARROW (that macro is narrow without UNICODE)
	windowClass.hbrBackground = static_cast<HBRUSH>(GetStockObject(DKGRAY_BRUSH));
	windowClass.lpszClassName = L"ControllerKeyboardTextEntryTest";
	RegisterClassW(&windowClass);

	DWORD style = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
	int x = 100, y = 100, width = 800, height = 450;
	if (g_options.fullscreen)
	{
		style = WS_POPUP | WS_VISIBLE;
		x = 0;
		y = 0;
		width = GetSystemMetrics(SM_CXSCREEN);
		height = GetSystemMetrics(SM_CYSCREEN);
	}

	g_window = CreateWindowExW(g_options.topmost ? WS_EX_TOPMOST : 0, windowClass.lpszClassName,
		L"ControllerKeyboard text entry test (F5 = open, Esc = quit)", style, x, y, width, height,
		nullptr, nullptr, windowClass.hInstance, nullptr);
	SetForegroundWindow(g_window);

	bool opened = false;
	bool injected = false;
	MSG message{};
	while (true)
	{
		while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
		{
			if (message.message == WM_QUIT)
			{
				XGameRuntimeUninitialize();
				return 0;
			}

			TranslateMessage(&message);
			DispatchMessageW(&message);
		}

		// Give the window a moment to become foreground first, like a game that is already running.
		if (!opened && !g_options.noEntry && std::chrono::steady_clock::now() - g_start > std::chrono::seconds(1))
		{
			opened = true;
			OpenTextEntry();
		}

		// Inject once something else (the Gaming UI) has the foreground, so the presses are aimed at it.
		if (g_options.inject && !injected && g_entryOpen && GetForegroundWindow() != g_window
			&& std::chrono::steady_clock::now() - g_start > std::chrono::seconds(3))
		{
			injected = true;
			std::thread(RunInjection).detach();
		}

		LogForegroundChanges();

		if (g_options.pollXInput)
		{
			PollXInput();
		}

		if (g_gameInput != nullptr)
		{
			PollGameInput();
		}

		if (g_options.pollWgi)
		{
			PollWindowsGamingInput();
		}

		if (g_options.refocus && GetForegroundWindow() != g_window)
		{
			SetForegroundWindow(g_window);
		}

		Sleep(16);
	}
}
