
#include "Common/Base.hpp"
#include "Common/DefaultLogger.hpp"
#include "MilanLauncher/Entry.hpp"
#include "MilanLauncher/SharedVersion.h"
#include <filesystem>
#include <optional>

SPI_PLUGINSIDE_SUPPORT(SDK_TARGET_NAME_W ASI_NAME_NO_SPACE_W, DEVELOPER_W, L"" VERSION_STRING_W, SPI_GAME_SDK_TARGET, SPI_VERSION_V4);
SPI_PLUGINSIDE_POSTLOAD;
SPI_PLUGINSIDE_ASYNCATTACH;

#ifdef NDEBUG
#define SHOW_CONSOLE false
#else
#define SHOW_CONSOLE true
#endif


// Variables 
// ==================

tLogScriptError* LogScriptError_orig = nullptr;
tLogScriptError* Log1_orig = nullptr;
tSingleIntBool* IsResumeCheck_orig = nullptr;
tCreateString* CreateString_orig = nullptr;
tInvokeActionScript* InternalInvokeActionScript = nullptr;
// If game launch should boot LMP
bool BootingLMP = false;
static std::optional<::LESDK::Initializer> gInterfacePtr;

SPI_IMPLEMENT_ATTACH
{
	gInterfacePtr.emplace(InterfacePtr, SDK_TARGET_NAME_A ASI_NAME_NO_SPACE_A);
	::LESDK::Initializer& Init = *gInterfacePtr;
	Common::SetupDefaultLogger(SDK_TARGET_NAME_A, ASI_NAME_NO_SPACE_A, SHOW_CONSOLE, Init);
	::MilanLauncher::InitializeHooks(Init);
	return true;
}

SPI_IMPLEMENT_DETACH
{
	LEASI_UNUSED(InterfacePtr);
	Common::ShutdownLogger();
	return true;
}
#define MILAN_HOOK(MethodName, MethodSig, rva)  \
	do { auto const target = Init.ResolveTyped<MethodSig>(rva); \
		CHECK_RESOLVED(target); \
		MethodName##_orig = (MethodSig*)Init.InstallHook(#MethodName, target, MethodName##_hook); \
		CHECK_RESOLVED(MethodName##_orig); \
	} while(0)



namespace MilanLauncher
{
	void InitializeHooks(::LESDK::Initializer& Init)
	{
		MILAN_HOOK(LogScriptError, tLogScriptError, LogScriptError_RVA);
		MILAN_HOOK(Log1, tLog1, Log1_RVA);

		// Used to intercept at the last possible moment the game that is going to be
		// set up for launch. We can inspect the game num to tell if it is MP (Game = 5)
		MILAN_HOOK(IsResumeCheck, tSingleIntBool, IsResumeCheck_RVA);

		// Used to intercept the creation of the game path and remove the -RESUME
		// flag caused by returning true in the previous hook
		MILAN_HOOK(CreateString, tCreateString, CreateString_RVA);

		// Used to inform actionscipt that LMP is available.
		InternalInvokeActionScript = Init.ResolveTyped<tInvokeActionScript>(InternalInvokeActionScript_RVA);

		// No longer necessary
		LEASI_INFO("MilanLauncher plugin initialized");
	}


	void* ComputeAddress(void* base, long long offset) {
		long long newBase = (long long)base;
		return (void*)(offset + newBase);
	}

	// Holds the launcher app class pointer
	void** GLauncherApp;

	// Fetches GLauncherApp static variable, populating it if it hasn't been already
	void** GetGLauncherApp() {
		if (GLauncherApp == nullptr) {
			// Address of the launcher app pointer
			GLauncherApp = (void**)gInterfacePtr->Resolve(LauncherApp_RVA);
		}
		return GLauncherApp;
	}


	// Invokes an actionscript method with no parameters
	void InvokeActionScriptMethod(std::string fullMethodName) {
		// Decomp shows signature seems to be like this:
		// InvokeActionScriptMethod ? (uVar5, "theScreen.enableCards", 0, 0, VT_Undefined);
		// Interface, MethodName, [Always 0], GFxValue, ConversionEnum);

		// Decomp sets up param 1 like this
		// uVar1 = **(undefined8 **)(param_1 + 0x1b8);
		auto pInterfaceObj = (void***)ComputeAddress(*GetGLauncherApp(), 0x1b8);
		auto result = InternalInvokeActionScript(**pInterfaceObj, fullMethodName.c_str(), 0, nullptr, 0);
		LEASI_INFO("Invocation of '{}' result: {}", fullMethodName, result);
	}

	void SetGameNum(int num) {
		int* gameNumPtr = (int*)ComputeAddress(*GetGLauncherApp(), 0x80);
		*gameNumPtr = num;
	}

	std::string FormatLauncherLogStr(char* baseStr, char* param_2, const char* param_3, char* param_4) {
		char buf[1024];
		sprintf(buf, baseStr, param_2, param_3, param_4);
		std::string bufStr(buf);
		while (!bufStr.empty() && bufStr.back() == '\n')
			bufStr.pop_back();
		return bufStr;
	}

	void Log1_hook(char* baseStr, char* param_2, const char* param_3, char* param_4) {
		LEASI_INFO("Launcher: " + FormatLauncherLogStr(baseStr, param_2, param_3, param_4));
	}

	void LogScriptError_hook(char* baseStr, char* param_2, const char* param_3, char* param_4) {
		LEASI_WARN("Launcher: " + FormatLauncherLogStr(baseStr, param_2, param_3, param_4));
	}
	// If we should override the next string creation
	bool HookNextString = false;
	bool IsResumeCheck_hook(int gameNum) {
		// 1,2,3 are games.
		// 4 is Resume.
		// [NEW] 5 is LMP.

		// We hook here as this occurs right before the game executable path check
		if (gameNum == 5) {
			HookNextString = true;
			BootingLMP = true;
			SetGameNum(3);
			gameNum = 3; // We are going to act like we're booting ME3
			return true; // We return true to make code path assign the value so we don't abort the game launch. It will set IsResume, but we can ignore that later
		}

		// Original logic.
		return gameNum == 4;
	}

	// Check if LMP is installed and invokes the actionscript method to set up the UI if it is.
	void CheckMPInstalled() {
		auto mpExePath = "../LMP/Binaries/Win64/MassEffectMultiplayer.exe";
		if (std::filesystem::exists(mpExePath))
		{
			InvokeActionScriptMethod("theScreen.MpInstalled");
		}
	}

	// This hook is used to detect MP when the PressAnyKey text is shown on screen
	// Which is the overlay for the main launcher UI
	// This doesn't cover return to launcher as it seems to reinitialize the UI.
	void* CreateString_hook(void* storage, char* data, size_t dataLen) {
		if (HookNextString && BootingLMP) {
			// We are booting LMP, so we need to change the executable being built
			HookNextString = false; // Reset the flag

			// Unset IsResume flag
			auto launcher = *GetGLauncherApp();
			*((bool*)ComputeAddress(launcher, 0x710)) = 0; // IsResume flag location.

			char* newString = "../LMP/Binaries/Win64/MassEffectMultiplayer.exe";
			LEASI_INFO("Redirecting game launch to {}", newString);
			return CreateString_orig(storage, newString, 47); // 47 chars long.
		}


		// BackgroundMusic string is used to lookup the audio config,
		// We use this to know when the launcher shows up
		if (dataLen == 0xF && strcmp(data, "BackgroundMusic") == 0) {
			// Check if parameter is "PressAnyKey"
			// This parameter is called on initial engagement with the UI
			//if (dataLen == 0xb && strcmp(data, "PressAnyKey") == 0) {
			LEASI_INFO("BackgroundMusic created.");	
			CheckMPInstalled();
		}

		return CreateString_orig(storage, data, dataLen);
	}
}
