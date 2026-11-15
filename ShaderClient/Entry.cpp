
#include "Common/Base.hpp"
#include "Common/DefaultLogger.hpp"
#include "ShaderClient/Entry.hpp"
#include "ShaderClient/Hooks.hpp"
#include "ShaderClient/Helpers.hpp"
#include "ShaderClient/SharedVersion.h"
#include "Client.hpp"
#include <atomic>
#include <cwchar>
#include <cstring>
#include <iterator>

SPI_PLUGINSIDE_SUPPORT(SDK_TARGET_NAME_W ASI_NAME_NO_SPACE_W, DEVELOPER_W, L"" VERSION_STRING_W, SPI_GAME_SDK_TARGET, SPI_VERSION_V4);
SPI_PLUGINSIDE_POSTLOAD;
SPI_PLUGINSIDE_ASYNCATTACH;


SPI_IMPLEMENT_ATTACH
{
	::LESDK::Initializer Init{ InterfacePtr, SDK_TARGET_NAME_A ASI_NAME_NO_SPACE_A };
	::ShaderClient::DisableMinimumResolutionGuard(Init);
	Common::SetupDefaultLogger(SDK_TARGET_NAME_A, ASI_NAME_NO_SPACE_A, true, Init);
	::ShaderClient::InitializeGlobals(Init);
	::ShaderClient::InitializeHooks(Init);
	LEASI_INFO("Welcome to Milan Shader Client");
	return true;
}

SPI_IMPLEMENT_DETACH
{
	LEASI_UNUSED(InterfacePtr);
	::ShaderClient::StopResolutionDialogWatchdog();
	::Common::ShutdownLogger();
	return true;
}


namespace ShaderClient
{
	namespace
	{
		struct ResolutionDialogInfo
		{
			HWND OkButton = nullptr;
			int VisibleButtonCount = 0;
			bool HasMessageText = false;
		};

		std::atomic_bool ResolutionDialogWatchdogStop = false;
		HANDLE ResolutionDialogWatchdogHandle = nullptr;

		using tCompilerSetDisplay = void(void*, UINT);
		using tCompilerEnumerateDisplays = void(void*);
		using tCompilerOutputCapability = UINT(void*);
		tCompilerOutputCapability* CompilerOutputCapabilityOriginal = nullptr;
		tCompilerSetDisplay* CompilerSetDisplayOriginal = nullptr;
		tCompilerEnumerateDisplays* CompilerEnumerateDisplays = nullptr;
		int* CompilerDisplayTop = nullptr;
		int* CompilerDisplayLeft = nullptr;
		int* CompilerDisplayHeight = nullptr;
		int* CompilerDisplayWidth = nullptr;
		RECT* CompilerDisplayRect = nullptr;

		void CompilerSetDisplay(void* rhi, UINT displayIndex)
		{
			auto bytes = static_cast<BYTE*>(rhi);
			if (*reinterpret_cast<int*>(bytes + 0x114) == 0)
			{
				CompilerEnumerateDisplays(rhi);
				*reinterpret_cast<int*>(bytes + 0x114) = 1;
			}
			const int displayCount = *reinterpret_cast<int*>(bytes + 0xd0) - *reinterpret_cast<int*>(bytes + 0xfc);
			if (displayCount != 0)
			{
				CompilerSetDisplayOriginal(rhi, displayIndex);
				return;
			}
			*reinterpret_cast<UINT*>(bytes + 0x110) = 0;
			*CompilerDisplayTop = 0;
			*CompilerDisplayLeft = 0;
			*CompilerDisplayHeight = 720;
			*CompilerDisplayWidth = 1280;
			*CompilerDisplayRect = RECT{0, 0, 1280, 720};
			LEASI_WARN("Compile-only display fallback: DXGI returned no outputs; using 1280x720 window bounds");
		}

		UINT CompilerOutputCapability(void* output)
		{
			if (output != nullptr)
			{
				return CompilerOutputCapabilityOriginal(output);
			}
			// Match the native non-HDR result when no DXGI output is available.
			LEASI_WARN("Compile-only display fallback: no DXGI output; using SDR capability");
			return 2;
		}

		void InstallCompilerDisplayFallback(::LESDK::Initializer& Init)
		{
#if defined(SDK_TARGET_LE3) || defined(LEASI_GAME_LMP)
			// LE3 11.3.2 RHISetCurrentDisplay dereferences Displays.Find(0) even when the map is empty.
			// Only compiler launches use fallback bounds; populated native display maps keep their path.
			constexpr BYTE DisplayEntry[] = {0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x20,0x83,0xb9,0x14,0x01,0x00,0x00,0x00,0x8b,0xfa,0x48,0x8b,0xd9};
			constexpr BYTE EnumerateEntry[] = {0x48,0x8b,0xc4,0x55,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8d,0xa8,0x38,0xfb,0xff,0xff};
			constexpr BYTE CapabilityEntry[] = {0x40,0x53,0x48,0x81,0xec,0xe0,0x00,0x00,0x00,0x48,0xc7,0x44,0x24,0x28,0xfe,0xff,0xff,0xff};
			auto capabilityTarget = Init.ResolveTyped<tCompilerOutputCapability>(LEASI_RVA(0x9a78b0));
			auto target = Init.ResolveTyped<tCompilerSetDisplay>(LEASI_RVA(0x9add10));
			CompilerEnumerateDisplays = Init.ResolveTyped<tCompilerEnumerateDisplays>(LEASI_RVA(0x9a9ad0));
			if (target == nullptr || CompilerEnumerateDisplays == nullptr || capabilityTarget == nullptr
				|| std::memcmp(reinterpret_cast<const void*>(capabilityTarget), CapabilityEntry, sizeof(CapabilityEntry)) != 0
				|| std::memcmp(reinterpret_cast<const void*>(target), DisplayEntry, sizeof(DisplayEntry)) != 0
				|| std::memcmp(reinterpret_cast<const void*>(CompilerEnumerateDisplays), EnumerateEntry, sizeof(EnumerateEntry)) != 0)
			{
				LEASI_ERROR("Compiler display fallback disabled: LE3 11.3.2 entry validation failed");
				return;
			}
			CompilerDisplayTop = Init.ResolveTyped<int>(LEASI_RVA(0x1cb9338));
			CompilerDisplayLeft = Init.ResolveTyped<int>(LEASI_RVA(0x1cb9334));
			CompilerDisplayHeight = Init.ResolveTyped<int>(LEASI_RVA(0x1cb9330));
			CompilerDisplayWidth = Init.ResolveTyped<int>(LEASI_RVA(0x1cb930c));
			CompilerDisplayRect = Init.ResolveTyped<RECT>(LEASI_RVA(0x1cb9340));
			CHECK_RESOLVED(CompilerDisplayTop);
			CHECK_RESOLVED(CompilerDisplayLeft);
			CHECK_RESOLVED(CompilerDisplayHeight);
			CHECK_RESOLVED(CompilerDisplayWidth);
			CHECK_RESOLVED(CompilerDisplayRect);
			CompilerSetDisplayOriginal = reinterpret_cast<tCompilerSetDisplay*>(Init.InstallHook("CompilerSetDisplay", target, CompilerSetDisplay));
			CHECK_RESOLVED(CompilerSetDisplayOriginal);
			CompilerOutputCapabilityOriginal = reinterpret_cast<tCompilerOutputCapability*>(Init.InstallHook("CompilerOutputCapability", capabilityTarget, CompilerOutputCapability));
			CHECK_RESOLVED(CompilerOutputCapabilityOriginal);
#else
			LEASI_UNUSED(Init);
#endif
		}

		BOOL CALLBACK InspectResolutionDialogChild(HWND child, LPARAM parameter)
		{
			auto* const info = reinterpret_cast<ResolutionDialogInfo*>(parameter);
			wchar_t className[32]{};
			GetClassNameW(child, className, static_cast<int>(std::size(className)));

			if (std::wcscmp(className, L"BUTTON") == 0 && IsWindowVisible(child))
			{
				++info->VisibleButtonCount;
				if (GetDlgCtrlID(child) == IDOK && IsWindowEnabled(child))
				{
					info->OkButton = child;
				}
			}
			else if (std::wcscmp(className, L"STATIC") == 0)
			{
				wchar_t text[1024]{};
				if (GetWindowTextW(child, text, static_cast<int>(std::size(text))) > 0)
				{
					info->HasMessageText = true;
				}
			}

			return TRUE;
		}

		bool IsResolutionDialog(HWND window, HWND* okButton)
		{
			if (!IsWindowVisible(window))
			{
				return false;
			}

			DWORD processId = 0;
			if (GetWindowThreadProcessId(window, &processId) == 0 || processId != GetCurrentProcessId())
			{
				return false;
			}

			wchar_t className[32]{};
			if (GetClassNameW(window, className, static_cast<int>(std::size(className))) == 0 ||
				std::wcscmp(className, L"#32770") != 0)
			{
				return false;
			}

			wchar_t title[128]{};
			if (GetWindowTextW(window, title, static_cast<int>(std::size(title))) != 0)
			{
				return false;
			}

			const HWND owner = GetWindow(window, GW_OWNER);
			if (owner != nullptr)
			{
				DWORD ownerProcessId = 0;
				if (GetWindowThreadProcessId(owner, &ownerProcessId) == 0 || ownerProcessId != processId)
				{
					return false;
				}
			}

			ResolutionDialogInfo info;
			EnumChildWindows(window, InspectResolutionDialogChild, reinterpret_cast<LPARAM>(&info));
			if (info.OkButton == nullptr || info.VisibleButtonCount != 1 || !info.HasMessageText)
			{
				return false;
			}

			*okButton = info.OkButton;
			return true;
		}

		BOOL CALLBACK FindResolutionDialog(HWND window, LPARAM parameter)
		{
			auto* const result = reinterpret_cast<HWND*>(parameter);
			HWND okButton = nullptr;
			if (IsResolutionDialog(window, &okButton))
			{
				*result = window;
				return FALSE;
			}
			return TRUE;
		}

		DWORD WINAPI ResolutionDialogWatchdogThread(LPVOID)
		{
			constexpr DWORD PollIntervalMs = 25;
			constexpr DWORD WatchdogDurationMs = 10000;
			for (DWORD elapsed = 0; elapsed < WatchdogDurationMs && !ResolutionDialogWatchdogStop.load(); elapsed += PollIntervalMs)
			{
				HWND dialog = nullptr;
				EnumWindows(FindResolutionDialog, reinterpret_cast<LPARAM>(&dialog));
				if (dialog != nullptr)
				{
					const HWND okButton = GetDlgItem(dialog, IDOK);
					if (okButton != nullptr && PostMessageW(dialog, WM_COMMAND, MAKEWPARAM(IDOK, BN_CLICKED), reinterpret_cast<LPARAM>(okButton)))
					{
						OutputDebugStringA("ShaderClient: dismissed the startup resolution message box.\n");
						return 0;
					}
				}
				Sleep(PollIntervalMs);
			}
			return 0;
		}

		void StartResolutionDialogWatchdog()
		{
			ResolutionDialogWatchdogStop.store(false);
			ResolutionDialogWatchdogHandle = CreateThread(nullptr, 0, ResolutionDialogWatchdogThread, nullptr, 0, nullptr);
		}
	}

	void StopResolutionDialogWatchdog()
	{
		ResolutionDialogWatchdogStop.store(true);
		if (ResolutionDialogWatchdogHandle != nullptr)
		{
			WaitForSingleObject(ResolutionDialogWatchdogHandle, 1000);
			CloseHandle(ResolutionDialogWatchdogHandle);
			ResolutionDialogWatchdogHandle = nullptr;
		}
	}

	void DisableMinimumResolutionGuard(::LESDK::Initializer& Init)
	{
#if defined(SDK_TARGET_LE3) || defined(LEASI_GAME_LMP)
		// LE3 11.3.2 FEngineLoop::PreInit checks GetSystemMetrics(0/1) against
		// 640x480 at RVA 0x9bb69c. Its failing path resolves Launch.error_resolutiontoolow
		// (TLK id 0x544d7), shows a modal MessageBoxW, then calls appRequestExit. Patch
		// the branch before PreInit runs, and patch both failure calls as a late-attach fallback.
		auto const patchBytes = [&Init](size_t rva, const BYTE* expected, const BYTE* replacement, size_t size)
		{
			auto const address = static_cast<BYTE*>(Init.Resolve(::LESDK::Address::FromOffset(rva)));
			if (address == nullptr)
			{
				return false;
			}

			MEMORY_BASIC_INFORMATION memoryInfo{};
			if (VirtualQuery(address, &memoryInfo, sizeof(memoryInfo)) == 0 || memoryInfo.State != MEM_COMMIT)
			{
				return false;
			}
			const auto addressValue = reinterpret_cast<uintptr_t>(address);
			const auto regionStart = reinterpret_cast<uintptr_t>(memoryInfo.BaseAddress);
			const auto regionEnd = regionStart + memoryInfo.RegionSize;
			if (addressValue < regionStart || addressValue > regionEnd || size > regionEnd - addressValue)
			{
				return false;
			}
			constexpr DWORD ReadableProtection = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY |
				PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
			if ((memoryInfo.Protect & ReadableProtection) == 0)
			{
				return false;
			}
			if (std::memcmp(address, replacement, size) == 0)
			{
				return true;
			}
			if (std::memcmp(address, expected, size) != 0)
			{
				return false;
			}

			DWORD oldProtection = 0;
			if (!VirtualProtect(address, size, PAGE_EXECUTE_READWRITE, &oldProtection))
			{
				return false;
			}
			std::memcpy(address, replacement, size);
			VirtualProtect(address, size, oldProtection, &oldProtection);
			FlushInstructionCache(GetCurrentProcess(), address, size);
			return true;
		};

		constexpr BYTE ResolutionGuardExpected[] = { 0x7c, 0x0f }; // JL 0x...b6ad
		constexpr BYTE ResolutionGuardPatch[] = { 0xeb, 0x27 }; // JMP 0x...b6c5
		constexpr BYTE ResolutionMessageCall[] = { 0xe8, 0x97, 0x13, 0x79, 0xff };
		constexpr BYTE ResolutionExitCall[] = { 0xe8, 0x8b, 0x32, 0x79, 0xff };
		constexpr BYTE NopCall[] = { 0x90, 0x90, 0x90, 0x90, 0x90 };

		const bool guardPatched = patchBytes(0x9bb69c, ResolutionGuardExpected, ResolutionGuardPatch, sizeof(ResolutionGuardPatch));
		const bool messagePatched = patchBytes(0x9bb6b4, ResolutionMessageCall, NopCall, sizeof(NopCall));
		const bool exitPatched = patchBytes(0x9bb6c0, ResolutionExitCall, NopCall, sizeof(NopCall));
		if (!guardPatched || !messagePatched || !exitPatched)
		{
			OutputDebugStringA("ShaderClient: one or more minimum-resolution patches did not match; inspect the LE3 executable build.\n");
		}
		StartResolutionDialogWatchdog();
#else
		LEASI_UNUSED(Init);
#endif
	}

	void InitializeGlobals(::LESDK::Initializer& Init)
	{
		Common::InitializeRequiredGlobals(Init);

		// Read by the static-mesh FLODInfo hook to recognise the DefaultMaterial substitution.
		GEngine = Init.ResolveTyped<UEngine*>(BUILTIN_GENGINE_RVA);
		CHECK_RESOLVED(GEngine);
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

		auto cli = std::wstring(GetCommandLineW());
		if (cli.find(L"-NoShaderCompile") != std::wstring::npos) {
			LEASI_INFO(L"Disabling shader compiling");
			EnableShaderCompile = false;
		}
		if (cli.find(L"-ShaderCompile") != std::wstring::npos) {
			BootShaderCompilerLevel = true;
			InstallCompilerDisplayFallback(Init);
		}
		if (cli.find(L"-SendDebugPackages") != std::wstring::npos) {
			SendDebugPackages = true;
		}
		if (cli.find(L"-CaptureOnly") != std::wstring::npos) {
			LEASI_INFO(L"Capture-only mode: shaders will be recorded but not compiled");
			CaptureOnly = true;
			SendDebugPackages = true;
		}

		// Auto-exit. The level's Kismet sequence exits on a scripted trigger rather than on the
		// compiler being finished, which can leave the game mid-write and fail the package with
		// "pipe did not drain" host-side. -AutoExitIdleSeconds=N tunes it; 0 turns it off and leaves
		// the Kismet sequence as the only way out.
		if (cli.find(L"-NoExitOnLoadScreen") != std::wstring::npos) {
			LEASI_INFO(L"Auto-exit on load screen disabled; the game will exit by its own returntolauncher path");
			ExitWhenLoadScreenDrops = false;
		}
		if (auto const idleAt = cli.find(L"-AutoExitIdleSeconds="); idleAt != std::wstring::npos) {
			auto const valueAt = idleAt + std::wcslen(L"-AutoExitIdleSeconds=");
			auto const endAt = cli.find(L' ', valueAt);
			auto const value = cli.substr(valueAt, endAt == std::wstring::npos ? std::wstring::npos : endAt - valueAt);
			try {
				CompileIdleExitSeconds = std::stoi(value);
				LEASI_INFO(L"Auto-exit idle window set to {}s", CompileIdleExitSeconds);
			}
			catch (...) {
				LEASI_WARN(L"Could not parse -AutoExitIdleSeconds value '{}'; keeping {}s", value.c_str(), CompileIdleExitSeconds);
			}
		}
		// Backstop only. The load-screen hook above is the normal way out; this covers the case where
		// the load screen never comes down (a compile that wedges), so a run cannot hang forever
		// waiting for a process that is never going to exit. The window is deliberately long so it
		// cannot fire during a legitimately slow single material.
		if (BootShaderCompilerLevel) {
			StartCompileIdleWatchdog();
		}

		if (cli.find(L"-AllowLegacyOpaqueSceneDepthTest") != std::wstring::npos) {
			LEASI_WARN(L"Enabling legacy opaque SceneDepth compatibility mode");
			AllowLegacyOpaqueSceneDepthTest = true;
		}
		// Repoints both traces below at a different material family. Read before they are enabled so
		// their log lines name the family they will actually match. The value runs to the next space,
		// which is enough for a material path and keeps this consistent with the other switches.
		if (auto const filterAt = cli.find(L"-TraceMaterialFilter="); filterAt != std::wstring::npos) {
			auto const valueAt = filterAt + std::wcslen(L"-TraceMaterialFilter=");
			auto const endAt = cli.find(L' ', valueAt);
			auto filter = cli.substr(valueAt, endAt == std::wstring::npos ? std::wstring::npos : endAt - valueAt);
			if (!filter.empty()) {
				MaterialTraceFilter = std::move(filter);
			}
		}
		// Reads "--switch=value" or "--switch value". Both forms are accepted because the host
		// generates these but they are also typed by hand when reproducing a single package run.
		// A value that starts with '-' is treated as absent: that means the switch was passed with
		// nothing after it and we have run into the next switch.
		auto const readSwitchValue = [&cli](const wchar_t* name) -> std::wstring {
			auto const at = cli.find(name);
			if (at == std::wstring::npos) {
				return {};
			}

			auto pos = at + std::wcslen(name);
			while (pos < cli.size() && (cli[pos] == L'=' || cli[pos] == L' ')) {
				++pos;
			}

			auto const end = cli.find(L' ', pos);
			auto value = cli.substr(pos, end == std::wstring::npos ? std::wstring::npos : end - pos);
			return value.starts_with(L'-') ? std::wstring{} : value;
		};

		auto const readSwitchCount = [&readSwitchValue](const wchar_t* name, int& target) {
			auto const value = readSwitchValue(name);
			if (value.empty()) {
				return;
			}
			try {
				target = std::stoi(value);
			}
			catch (...) {
				// A malformed count just leaves the counter off the loading screen. This is cosmetic
				// progress text; it must never be a reason the compile does not start.
				LEASI_WARN(L"Ignoring malformed count for {}: '{}'", name, value);
			}
		};

		// Progress context for the compiler level's loading screen. The host runs one game process
		// per package, so without these the window gives no clue how far through the run it is.
		readSwitchCount(L"--numdone", ShaderProgressDone);
		readSwitchCount(L"--numtotal", ShaderProgressTotal);
		CurrentPackageName = readSwitchValue(L"--currentpackage");
		RebuildShaderProgressMessage();

		if (cli.find(L"-TraceRvrMaterialResolution") != std::wstring::npos) {
			LEASI_INFO(L"Enabling opt-in transient material-resource trace for materials matching '{}'", MaterialTraceFilter);
			TraceRvrMaterialResolution = true;
		}
		if (cli.find(L"-TraceMaterialTranslation") != std::wstring::npos) {
			LEASI_INFO(L"Enabling opt-in material-translation trace for materials matching '{}'", MaterialTraceFilter);
			TraceMaterialTranslation = true;
		}

		// INIT SHADER MAP - used to trigger hlsl compile when shader fails to load
		HOOK_PATTERN_POSTHOOK(InitShaderMap, tInitShaderMap, "54 41 55 41 56 41 57 48 81 ec a0 00 00 00 48 c7 44 24 40 fe ff ff ff 49 89 5b 10 49 89 6b 18 49 89 73 20 45 8b e1 45 8b f0 48 8b ea");
		
		if (EnableShaderCompile) {
			// Validated against the currently supported LE3 executable in Ghidra:
			// FHLSLMaterialTranslator::Translate at RVA 0x2C8BE0. The hook runs after the
			// native translator generated its HLSL, letting the test filter only its known
			// compatibility diagnostic without changing package or material state.
			HOOK_RVA(FHLSLTranslator_Translate, tHLSLTranslate, LEASI_RVA(0x2C8BE0));

			// FMaterialCompile - used to inspect result of shader map compile and actually generate shader cache
			HOOK_PATTERN_POSTHOOK(FMaterialCompile, tFMaterialCompile, "41 54 41 55 41 56 41 57 48 8d ac 24 38 f8 ff ff 48 81 ec c8 08 00 00 48 c7 45 f8 fe ff ff ff");

			HOOK_PATTERN_POSTHOOK(FMaterialShaderMap_Compile, tCompileShader, "50 48 c7 44 24 30 fe ff ff ff 48 89 5c 24 60 48 89 74 24 68 48 8b fa 48 8b 02 48 8d 54 24 38 48 8b cf ff 90 b8 01 00 00 90");

			auto traceEntryMatches = [&Init](size_t rva, const BYTE* expected, size_t expectedLength) {
				auto const actual = static_cast<const BYTE*>(Init.Resolve(::LESDK::Address::FromOffset(rva)));
				return actual != nullptr && std::memcmp(actual, expected, expectedLength) == 0;
			};

			if (TraceRvrMaterialResolution) {
				// ShaderHost's transient compile reaches this child UMaterial method directly; it
				// does not call the Rvr wrapper's GetMaterial/AllocateResource virtuals. Validate
				// the supported LE3 11.3.2 entry before installing the opt-in diagnostic hook.
				constexpr BYTE UMaterialSelectResourceEntry[] = {
					0x48, 0x89, 0x5C, 0x24, 0x18, 0x56, 0x57, 0x41, 0x54, 0x41, 0x56, 0x41, 0x57,
					0x48, 0x83, 0xEC, 0x50, 0x4C, 0x63, 0xB4, 0x24, 0xA0, 0x00, 0x00, 0x00,
					0x48, 0x8B, 0xD9, 0x48, 0x8B, 0x89, 0xA0
				};
				if (!traceEntryMatches(0x289870, UMaterialSelectResourceEntry, sizeof(UMaterialSelectResourceEntry))) {
					LEASI_ERROR(L"Transient material trace disabled: LE3 11.3.2 entry-byte validation failed");
					WriteRvrMaterialResolutionTraceStatus(L"disabled because an LE3 11.3.2 entry-byte validation failed");
					TraceRvrMaterialResolution = false;
				}
				else {
					HOOK_RVA(UMaterial_SelectResource, tUMaterialSelectResource, LEASI_RVA(0x289870));
					WriteRvrMaterialResolutionTraceStatus(L"UMaterial::VirtFunc_102 installed after LE3 11.3.2 entry-byte validation");
				}
			}

			if (TraceMaterialTranslation) {
				// Both entries are validated before installing either diagnostic hook. They expose the
				// normal-expression compression choice and its final UnpackTexture arguments, and must
				// never run on an unknown executable build.
				constexpr BYTE UnpackTextureEntry[] = {
					0x40, 0x53, 0x55, 0x56, 0x57, 0x41, 0x56, 0x41, 0x57, 0x48, 0x83, 0xEC, 0x78,
					0x48, 0x8B, 0x05, 0x44, 0xB4, 0x52, 0x01, 0x48, 0x33, 0xC4,
					0x48, 0x89, 0x44, 0x24, 0x50, 0x49, 0x8B, 0xE8, 0x44
				};
				constexpr BYTE NormalTextureSampleCompileEntry[] = {
					0x48, 0x89, 0x5C, 0x24, 0x10, 0x48, 0x89, 0x74, 0x24, 0x18, 0x57, 0x48, 0x83, 0xEC, 0x30,
					0x48, 0x8B, 0x19, 0x48, 0x8B, 0xF2, 0x48, 0x8B, 0x91, 0x80, 0x00, 0x00, 0x00, 0x48, 0x8B, 0xF9, 0x48
				};
				if (!traceEntryMatches(0x2A0070, UnpackTextureEntry, sizeof(UnpackTextureEntry)) ||
					!traceEntryMatches(0x288F70, NormalTextureSampleCompileEntry, sizeof(NormalTextureSampleCompileEntry))) {
					LEASI_ERROR(L"Material-translation trace disabled: LE3 11.3.2 entry-byte validation failed");
					WriteMaterialTranslationTraceStatus(L"disabled because a material-translation LE3 11.3.2 entry-byte validation failed");
					TraceMaterialTranslation = false;
				}
				else {
					HOOK_RVA(UnpackTexture, tUnpackTexture, LEASI_RVA(0x2A0070));
					HOOK_RVA(NormalTextureSampleCompile, tNormalTextureSampleCompile, LEASI_RVA(0x288F70));
					WriteMaterialTranslationTraceStatus(L"UnpackTexture and NormalTextureSample::Compile installed after LE3 11.3.2 entry-byte validation");
				}
			}

			//INIT_FIND_PATTERN_POSTHOOK(InitShaderMap, /*"4c 8b dc 57 41*/ "54 41 55 41 56 41 57 48 81 ec a0 00 00 00 48 c7 44 24 40 fe ff ff ff 49 89 5b 10 49 89 6b 18 49 89 73 20 45 8b e1 45 8b f0 48 8b ea")
			//INIT_HOOK_PATTERN(InitShaderMap);

			// FMaterialCompile - used to inspect result of shader map compile and actually generate shader cache
			//INIT_FIND_PATTERN_POSTHOOK(FMaterialCompile, /*"40 55 53 56 57*/ "41 54 41 55 41 56 41 57 48 8d ac 24 38 f8 ff ff 48 81 ec c8 08 00 00 48 c7 45 f8 fe ff ff ff")
			//INIT_HOOK_PATTERN(FMaterialCompile);

			//FMaterialShaderMap_Compile = Init.ResolveTyped<tCompileShader>(LEASI_RVA(0x2a7a30));
			//INIT_FIND_PATTERN_POSTHOOK(FMaterialShaderMap_Compile, /*"40 57 48 83 ec*/ "50 48 c7 44 24 30 fe ff ff ff 48 89 5c 24 60 48 89 74 24 68 48 8b fa 48 8b 02 48 8d 54 24 38 48 8b cf ff 90 b8 01 00 00 90");
			//INIT_HOOK_PATTERN(FMaterialShaderMap_Compile);

			// Hooked this way to allow hook sharing
			HOOK_PATTERN_POSTHOOK(GetTlkString, tGetTlkString,/*"89 54 24 10 4c*/ "8b dc 57 41 54 41 55 41 56 41 57 48 83 ec 60 49 c7 43 a8 fe ff ff ff");

			// HOOK_RVA(ShowLoadMessage, tShowLoadMessage, LEASI_RVA(0xc23830))

			HOOK_RVA(PlayLoadingMovie, tPlayLoadingMovie, LEASI_RVA(0xbd9a70));
			// SFXLoadMovieManager's stop-loading-movie implementation. Image base 0x7ff74a890000,
			// function at 0x7ff74b471b40. The script glue (StopLoadingMovieNative, 0xb6db40) only
			// unpacks a parameter and calls this, so this catches native callers too.
			HOOK_RVA(StopLoadingMovie, tStopLoadingMovie, LEASI_RVA(0xbe1b40));

			if (BootShaderCompilerLevel) {
				HOOK_RVA(IsWindowFocused, tReturnBool, LEASI_RVA(0xad40e0));
			}

		}
	
		HOOK_RVA(FShaderMap_IsComplete, tShaderMapIsComplete, LEASI_RVA(0x2ae400));
		// Verified in LE3 11.3.2 Ghidra: ATerrain::GetCachedMaterial @ 0x7ff74b097c70.
		// This is the authoritative terrain-mask selection/fallback boundary.
		HOOK_RVA(ATerrain_GetCachedMaterial, tTerrainGetCachedMaterial, LEASI_RVA(0x807c70));
		// Verified in LE3 11.3.2 Ghidra: UMaterial vtable slot 98 is at RVA 0x284ff0.
		// The hook reports only the calls made by FTerrainSceneProxy's foliage path.
		HOOK_RVA(UMaterial_CheckMaterialUsage, tUMaterialCheckMaterialUsage, LEASI_RVA(0x284ff0));

		// Verified in LE3 11.3.2 Ghidra: FStaticMeshSceneProxy::FLODInfo::FLODInfo @ 0x7ff74b06d5d0.
		// This is the silent static-lighting usage fallback to DefaultMaterial. The hook reads native
		// UStaticMesh render-data offsets, so it is only installed when the entry bytes match.
		{
			constexpr BYTE StaticMeshLODInfoCtorEntry[] = {
				0x48, 0x8B, 0xC4, 0x48, 0x89, 0x48, 0x08, 0x56, 0x57, 0x41, 0x54, 0x41, 0x56, 0x41, 0x57,
				0x48, 0x83, 0xEC, 0x40, 0x48, 0xC7, 0x40, 0xB8, 0xFE, 0xFF, 0xFF, 0xFF, 0x48, 0x89, 0x58, 0x18
			};
			auto const actual = static_cast<const BYTE*>(Init.Resolve(::LESDK::Address::FromOffset(0x7DD5D0)));
			if (actual == nullptr || std::memcmp(actual, StaticMeshLODInfoCtorEntry, sizeof(StaticMeshLODInfoCtorEntry)) != 0) {
				LEASI_ERROR(L"Static-mesh DefaultMaterial fallback logging disabled: LE3 11.3.2 entry-byte validation failed");
			}
			else {
				HOOK_RVA(StaticMeshLODInfoCtor, tStaticMeshLODInfoCtor, LEASI_RVA(0x7DD5D0));
			}
		}

		// Used to redirect us to the shader compiler levels
		HOOK_RVA(UObject_ProcessInternal, t_UObject_ProcessInternal, BUILTIN_PROCESSINTERNAL_RVA);
#if defined(NOTDEFINED)

		// uncomment to make _setSomething_hook break. The game crashes in here after a few iterations
		//INIT_FIND_PATTERN_POSTHOOK(_setSomething, /*"48 89 5c 24 10*/ "48 89 6c 24 18 57 48 83 ec 20 48 8b d9 48 8b fa 8b 49 34 bd 01 00 00 00 85 c9 7e 1d 4c 63 4b 30 44 89 0a 48 8b 13 4f 8d 04 89 42 8b 04 82 89 43 30 8d 41 ff 89 43 34 eb 53 44 8b 43 0c 48 89 74 24 30 8b 73 08 8d 56 01 89 53 08 41 3b d0 7e 24 41 b9 14 00 00 00 48 8b cb e8 fd ad db ff");
		//INIT_HOOK_PATTERN(_setSomething);

		//INIT_FIND_PATTERN_POSTHOOK(FindFirstBit, /*"4c 8b 59 08 45 */ "33 d2 4c 63 01 41 8b d2 49 8b 43 10 48 85 c0 49 0f 44 d3 48 0b d0 48 8d 05 96 29 ff 00 4c 0f 44 d0 4c 0b d2 43 8b 14 82 23 51 10 75 32");
		//INIT_HOOK_PATTERN(FindFirstBit);


		// HLSL Generator - used to capture hlsl to file
		//INIT_FIND_PATTERN_POSTHOOK(FHLSLTranslator_Translate, /*"48 8b c4 55 41*/ "54 41 55 41 56 41 57 48 8d a8 08 ff ff ff 48 81 ec d0 01 00 00 48 c7 45 a8 fe ff ff ff 48 89 58 10 48 89 70 18 48 89 78 20 48 8b 05 b3 28 50 01 48 33 c4 48 89 85 c0 00 00 00 48 8b f1 c7 41 08 01 00 00 00")
		//INIT_HOOK_PATTERN(FHLSLTranslator_Translate);

#endif

		// VTables
		StringArchiveProxyVTableStart = Init.ResolveTyped<void>(LEASI_RVA(0x10dfbf0));
		MemoryArchiveWriterVTableStart = Init.ResolveTyped<void>(LEASI_RVA(0x107fac8));
		TerrainMaterialResourceVTable = Init.ResolveTyped<void>(LEASI_RVA(0x127aa28));

		// Functions
		FMaterial_GetFriendlyName = Init.ResolveTyped<tFGetMaterialFriendlyName>(LEASI_RVA(0x2BDEC0));
		FHLSL_GetMaterialCode = Init.ResolveTyped<tGetMaterialCode>(LEASI_RVA(0x2BE1F0));
		GetShaderTypeList = Init.ResolveTyped<tGetShaderTypeList>(LEASI_RVA(0x387540));
		GetVertexFactoryTypeList = Init.ResolveTyped<tGetVertexFactoryTypeList>(LEASI_RVA(0x8d4830));
		SerializeUShaderCache = Init.ResolveTyped<tSerializeUShaderCache>(LEASI_RVA(0x376840));
		SerializeMaterialResource = Init.ResolveTyped<tSerializeMaterialResource>(LEASI_RVA(0x2c56a0));
		SerializeTerrainMaterialResource = Init.ResolveTyped<tSerializeTerrainMaterialResource>(LEASI_RVA(0x7feac0));
		// SerializeFUniformExpressionSet = Init.ResolveTyped<tSerializeFUniformExpressionSet>(LEASI_RVA(0x2C66B0));
		static_construct_object = Init.ResolveTyped<tStaticConstructObject>(BUILTIN_STATICCONSTRUCTOBJECT_RVA);
		FStaticParameterSet::StaticParameterSetEqualRef = Init.ResolveTyped<FStaticParameterSet::tStaticParameterSetEqual>(LEASI_RVA(0x2A46A0));
		CreateArchiveProxy = Init.ResolveTyped<tCreateArchiveProxy>(LEASI_RVA(0x235EE0));
		// FString::StrHash = Init.ResolveTyped<FString::tappStrHash>(LEASI_RVA(0x9F710));

		// for debugging purposes
		// ByteOrderSerialize = Init.ResolveTyped<tByteOrderSerialize>(LEASI_RVA(0x84930));

		// Globals
		GFileManager = Init.ResolveTyped<FFileManager*>(LEASI_RVA(0x17cd5d0)); 
		GObjTransientPackage = Init.ResolveTyped<UPackage*>(LEASI_RVA(0x1887DA0));
		GIsSeekFreePCConsole = Init.ResolveTyped<int>(LEASI_RVA(0x17DF724));
		GError = Init.ResolveTyped<FArchive*>(LEASI_RVA(0x17CD570));
		GCookingTarget = Init.ResolveTyped<ECookingTarget>(LEASI_RVA(0x17cd580));

		LoadCompiler();
	}
}
