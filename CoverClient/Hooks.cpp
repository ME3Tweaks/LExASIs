
#include "CoverClient/Hooks.hpp"
#include "CoverClient/Helpers.hpp"
#include "CoverClient/Client.hpp"
#include "CoverClient/PathfindingGeneration.hpp"
#include "CoverClient/CoverGeneration.hpp"
#include "CoverClient/Classes/CoverLink.hpp"
#include "CoverClient/Utilities/DebugTools.hpp"
#include "Common/Objects.hpp"
#include "LESDK/Headers.hpp"
#include <map>

namespace CoverClient
{
	tGetTlkString* GetTlkString_orig = nullptr;
	FString* GetTlkString_hook(void* gTalkTable, int id, FString* outStr, int something, int somethingElse) {
		// This causes breakpoint in debug mode,
		// and seems to crash game if no debugger,
		// is it cause our wchar_t* is not GMallocd?
		/*if (id == 347396) { // Window Title
			*outStr = L"Milan Shader Compiler";
			return outStr;
		}*/
		//if (id == 1) {
		//	*outStr = L"Shader compile in progress...";
		//	return outStr;
		//}
		//else if (id == 170447) { // Loading widget Loading
		//	if (currentAction.empty()) {
		//		return GetTlkString_orig(gTalkTable, id, outStr, something, somethingElse);
		//	}

		//	FString message = currentAction.c_str();
		//	*outStr = message;
		//	return outStr;
		//}
		//else {
			return GetTlkString_orig(gTalkTable, id, outStr, something, somethingElse);
		//}
	}

	void RunConsoleCommand(const FString& command) {
		auto playerController = ::Common::FindFirstObject<APlayerController>();
		if (playerController) {
			playerController->ConsoleCommand(command, 0);
		}
	}

	bool BootShaderCompilerLevel = false;
	t_UObject_ProcessInternal* UObject_ProcessInternal_orig = nullptr;
	void UObject_ProcessInternal_hook(UObject* const Context, FFrame* const Stack, void* const Result)
	{
		static bool hasRunSplashCheck = false;

		if (!hasRunSplashCheck) {
			FString const FunctionName = Stack->Node->GetFullPath();
			if (FunctionName.Contains(L"BioSeqAct_ShouldShowSplashSequence")) {
				LEASI_INFO(L"ProcessInternal: {}", *FunctionName);
				hasRunSplashCheck = true;

				auto cli = std::wstring(GetCommandLineW());
				if (BootShaderCompilerLevel) {
					LEASI_INFO(L"Loading shader compiler level");
					RunConsoleCommand(L"at shadercompiler");
				}
			}
		}
		UObject_ProcessInternal_orig(Context, Stack, Result);
	}

	t_UGameEngine_Exec* UGameEngine_Exec_orig = nullptr;
	DWORD UGameEngine_Exec_hook(UGameEngine* const Context, WCHAR const* const Command, void* const Archive)
	{
		if (std::wstring_view const CommandView{ Command };
			CommandView.starts_with(L"cc.") || CommandView.starts_with(L"CC."))
		{
			FString const CommandCopy{ Command };
			if (CommandCopy.Contains(L"cc.buildpathfinding"))
			{
				LEASI_INFO(L"Beginning pathfinding network build");
				FloodFillPathfindingNetwork();
			} else if (CommandCopy.Contains(L"cc.buildcover"))
			{
				LEASI_INFO(L"Beginning cover generation from the camera POV");
				GenerateCoverFromCamera();
			}
			else if (CommandCopy.Contains(L"cc.sendtohost"))
			{
				LEASI_INFO(L"Computing ReachSpecs...");
				ComputeReachSpecs();
				LEASI_INFO(L"Sending pathfinding data to CoverHost");
				SendNodesToCoverHost();
			}
		}

		return UGameEngine_Exec_orig(Context, Command, Archive);
	}

	t_UObject_ProcessEvent* UObject_ProcessEvent_orig = nullptr;
	void UObject_ProcessEvent_hook(UObject* Context, UFunction* Function, void* Parms, void* Result)
	{
		auto funcFullName = Function->GetFullName();

		if (funcFullName.Equals(L"Function SFXGame.BioHUD.PostRender"))
		{
			//DrawDebugTraces();
			DrawGeneratedPathfinding();
		}

		UObject_ProcessEvent_orig(Context, Function, Parms, Result);
	}
}