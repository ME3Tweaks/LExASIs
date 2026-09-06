#include "CoverClient/Hooks.hpp"
#include "CoverClient/Helpers.hpp"
#include "CoverClient/Client.hpp"
#include "CoverClient/EditorPipe.hpp"
#include "CoverClient/PathfindingGeneration.hpp"
#include "CoverClient/CoverGeneration.hpp"
#include "CoverClient/CoverExport.hpp"
#include "CoverClient/ScoutHarness.hpp"
#include "CoverClient/Classes/CoverLink.hpp"
#include "CoverClient/Utilities/DebugTools.hpp"
#include "CoverClient/Utilities/PathUtilities.hpp"
#include "Common/Objects.hpp"
#include "LESDK/Headers.hpp"
#include <map>
#include <string>
#include <cwchar>

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

	bool BootCoverTestLevel = true;
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
				if (cli.find(L" -bootcovertest") != std::wstring::npos) {
					if (BootCoverTestLevel) {
						LEASI_INFO(L"Loading CoverTest level");
						RunConsoleCommand(L"at BioP_CoverTest");
					}
				}
			}
		}
		UObject_ProcessInternal_orig(Context, Stack, Result);
	}


	bool DrawGameFireLinksForSlot = false;
	bool DrawClientFireLinksForSlot = false;
	bool DrawClientExposedCoverForSlot = false;
	bool DrawClientDangerNavsForSlot = false;
	bool DrawCoverInteractionLocations = false;
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
			}
			else if (CommandCopy.Contains(L"cc.buildcover"))
			{
				LEASI_INFO(L"Beginning cover generation from the camera POV");
				GenerateCoverFromCamera();
			}
			else if (CommandCopy.Contains(L"cc.loadleveldata"))
			{
				LEASI_INFO(L"Loading level data for generation");
				LoadExistingLevelNodes();
			}
			else if (CommandCopy.Contains(L"cc.sendtohost"))
			{
				LEASI_INFO(L"Computing FireLinks...");
				ComputeFireLinks();
				LEASI_INFO(L"Computing DangerNavs...");
				//ComputeDangerNavs();
				LEASI_INFO(L"Computing ExposedCovers...");
				//ComputeExposedCover();
				LEASI_INFO(L"Computing ReachSpecs...");
				ComputeReachSpecs();
				LEASI_INFO(L"Sending pathfinding data to CoverHost");
				SendNodesToCoverHost();
			}

			// Cover data compute
			else if (CommandCopy.Contains(L"cc.computefirelinks")) {
				ComputeFireLinks();
			}
			else if (CommandCopy.Contains(L"cc.computedangernavs")) {
				ComputeDangerNavs();
			}
			else if (CommandCopy.Contains(L"cc.computeexposedcover")) {
				ComputeExposedCover();
			}


			// Scout harness spike -- see Notes/02-le3-native-cover-functions.md
			else if (CommandCopy.Contains(L"cc.testscout")) {
				CmdTestScout();
			}
			else if (CommandCopy.Contains(L"cc.testautoadjust")) {
				CmdTestAutoAdjust();
			}
			else if (CommandCopy.Contains(L"cc.testbuildslotinfo")) {
				CmdTestBuildSlotInfo();
			}
			else if (CommandCopy.Contains(L"cc.spawnlink")) {
				CmdSpawnLink();
			}
			else if (CommandCopy.Contains(L"cc.sweep")) {
				// cc.sweep [radius] [maxSamples] [maxLinks]. Missing or 0 means "work it out",
				// which is also what an unparseable argument gives -- wcstof leaves the cursor
				// where it found it and returns zero.
				float Radius = 0.0f;
				int MaxSamples = 0;
				int MaxLinks = 0;
				if (auto const Space = CommandView.find(L' '); Space != std::wstring_view::npos)
				{
					std::wstring Arguments{ CommandView.substr(Space + 1) };
					wchar_t* Cursor = Arguments.data();
					Radius = std::wcstof(Cursor, &Cursor);
					MaxSamples = static_cast<int>(std::wcstof(Cursor, &Cursor));
					MaxLinks = static_cast<int>(std::wcstof(Cursor, &Cursor));
				}
				CmdSweep(Radius, MaxSamples, MaxLinks);
			}
			else if (CommandCopy.Contains(L"cc.importlinks")) {
				CmdImportLinks();
			}
			else if (CommandCopy.Contains(L"cc.despawnenemyspawnpoints")) {
				CmdDespawnEnemySpawnPoints();
			}
			else if (CommandCopy.Contains(L"cc.enemyspawnpoint")) {
				// cc.enemyspawnpoint [count] -- how many of the nearest cover slots to put a
				// spawn point on. Missing or unparseable means one.
				int Count = 1;
				if (auto const Space = CommandView.find(L' '); Space != std::wstring_view::npos)
				{
					std::wstring const Argument{ CommandView.substr(Space + 1) };
					int const Parsed =
						static_cast<int>(std::wcstof(Argument.c_str(), nullptr));
					if (Parsed > 0) { Count = Parsed; }
				}
				CmdSpawnEnemySpawnPoints(Count);
			}
			else if (CommandCopy.Contains(L"cc.despawnlinks")) {
				CmdDespawnLinks();
			}
			else if (CommandCopy.Contains(L"cc.buildlinks")) {
				CmdBuildLinks();
			}
			else if (CommandCopy.Contains(L"cc.buildslips")) {
				CmdBuildSlips();
			}
			else if (CommandCopy.Contains(L"cc.clearslips")) {
				CmdClearSlips();
			}
			else if (CommandCopy.Contains(L"cc.slips")) {
				// cc.slips [radius]  -- every edge slot near you and what it connects to.
				float Radius = 512.0f;
				if (auto const Space = CommandView.find(L' '); Space != std::wstring_view::npos)
				{
					std::wstring const Argument{ CommandView.substr(Space + 1) };
					wchar_t* Cursor = nullptr;
					double const Parsed = std::wcstod(Argument.c_str(), &Cursor);
					if (Cursor != Argument.c_str()) { Radius = static_cast<float>(Parsed); }
				}
				CmdSlips(Radius);
			}
			else if (CommandCopy.Contains(L"cc.explainlink")) {
				// cc.explainlink         pick by aim
				// cc.explainlink C12     CoverLink_12, the name the game's own
				//                        'profile cover self' prints
				// cc.explainlink L13     the spawn-list index
				// cc.explainlink 13      same as L13
				//
				// Both forms are on the in-world label ("C12 L13 s4 ..."), so whichever
				// number is to hand can be typed without converting it.
				int Index = -1;
				int ObjectNumber = -1;
				if (auto const Space = CommandView.find(L' '); Space != std::wstring_view::npos)
				{
					std::wstring Argument{ CommandView.substr(Space + 1) };
					while (!Argument.empty() && Argument.front() == L' ')
					{
						Argument.erase(Argument.begin());
					}

					bool bObjectNumber = false;
					if (!Argument.empty() && (Argument.front() == L'C' || Argument.front() == L'c'))
					{
						bObjectNumber = true;
						Argument.erase(Argument.begin());
					}
					else if (!Argument.empty()
						&& (Argument.front() == L'L' || Argument.front() == L'l'))
					{
						Argument.erase(Argument.begin());
					}

					wchar_t* Cursor = nullptr;
					long const Parsed = std::wcstol(Argument.c_str(), &Cursor, 10);
					if (Cursor != Argument.c_str())
					{
						if (bObjectNumber) { ObjectNumber = static_cast<int>(Parsed); }
						else { Index = static_cast<int>(Parsed); }
					}
				}
				CmdExplainLink(Index, ObjectNumber);
			}
			else if (CommandCopy.Contains(L"cc.drawslotorigins")) {
				DrawSlotOriginLabels = !DrawSlotOriginLabels;
				LEASI_INFO(L"Slot origin labels {}.",
					DrawSlotOriginLabels ? L"ON (within the detail radius)" : L"OFF");
			}
			else if (CommandCopy.Contains(L"cc.covercheck")) {
				CmdCoverCheck();
			}

			// Debug drawing tools
			else if (CommandCopy.Contains(L"cc.drawgamefirelinks")) {
				DrawGameFireLinksForSlot = !DrawGameFireLinksForSlot;
			}
			else if (CommandCopy.Contains(L"cc.drawclientfirelinks")) {
				DrawClientFireLinksForSlot = !DrawClientFireLinksForSlot;
			}
			else if (CommandCopy.Contains(L"cc.drawclientexposedcover")) {
				DrawClientExposedCoverForSlot = !DrawClientExposedCoverForSlot;
			}
			else if (CommandCopy.Contains(L"cc.drawclientdangernavs")) {
				DrawClientDangerNavsForSlot = !DrawClientDangerNavsForSlot;
			}
			else if (CommandCopy.Contains(L"cc.drawinteractionpositions")) {
				DrawCoverInteractionLocations = !DrawCoverInteractionLocations;
			}
			else if (CommandCopy.Contains(L"cc.drawlinks")) {
				// cc.drawlinks         toggle
				// cc.drawlinks <n>     draw within n units (0 turns it off)
				if (auto const Space = CommandView.find(L' '); Space != std::wstring_view::npos)
				{
					std::wstring const Argument{ CommandView.substr(Space + 1) };
					float const Radius = std::wcstof(Argument.c_str(), nullptr);
					DrawSpawnedLinks = Radius > 0.0f;
					if (DrawSpawnedLinks) { SpawnedLinkDrawDistance = Radius; }
				}
				else
				{
					DrawSpawnedLinks = !DrawSpawnedLinks;
				}
				if (DrawSpawnedLinks)
				{
					LEASI_INFO(L"Spawned cover link overlay ON, drawing within {:.0f}u "
						L"(full detail within {:.0f}u).",
						SpawnedLinkDrawDistance, SpawnedLinkDrawDistance * 0.5f);
				}
				else
				{
					LEASI_INFO(L"Spawned cover link overlay OFF");
				}
			}

			// Single command versions
			else if (CommandCopy.Contains(L"cc.dangernavs")) {
				DrawClientDangerNavsForSlot = true;
				LoadExistingLevelNodes();
				ComputeDangerNavs();
			}
			else if (CommandCopy.Contains(L"cc.exposedcover")) {
				DrawClientExposedCoverForSlot = true;
				LoadExistingLevelNodes();
				ComputeExposedCover();
			}
			else if (CommandCopy.Contains(L"cc.firelinks")) {
				DrawClientFireLinksForSlot = true;
				LoadExistingLevelNodes();
				ComputeFireLinks();
			}
		}

		return UGameEngine_Exec_orig(Context, Command, Archive);
	}

	t_UObject_ProcessEvent* UObject_ProcessEvent_orig = nullptr;
	void UObject_ProcessEvent_hook(UObject* Context, UFunction* Function, void* Parms, void* Result)
	{
		// Any HUD's PostRender, not one named class.
		//
		// LE3 draws through SFXGame.BioHUD and LMP through SFXHUDMP; they are mutually
		// exclusive -- only one exists in a given game's packages -- so matching a literal
		// class path binds this ASI to one of the two. AHUD is what they share, and Canvas
		// lives there, which is all the overlay needs.
		//
		// Name first, cast second: ProcessEvent is on the hot path for every script call in
		// the game, so the cheap test goes first and the class walk only runs on a match.
		if (Function->GetName().Equals(L"PostRender"))
		{
			auto* const hud = Context->Cast<AHUD>();
			if (hud == nullptr)
			{
				UObject_ProcessEvent_orig(Context, Function, Parms, Result);
				return;
			}

			// Only the LOCAL PLAYER'S HUD, or the overlay draws once per HUD actor.
			//
			// Broadening the match from a named class to any AHUD also picked up every other
			// HUD in the world -- sub-HUDs hang off HudOwner, and each gets its own PostRender
			// with its own Canvas and its own HudCanvasScale. Boxes drawn twice land on top of
			// each other and look normal; TEXT drawn twice at slightly different scales comes
			// out doubled and unreadable, which is exactly what it did. It is also double the
			// work for nothing.
			if (hud->PlayerOwner == nullptr || hud->HudOwner != nullptr)
			{
				UObject_ProcessEvent_orig(Context, Function, Parms, Result);
				return;
			}

			// The Canvas is only valid for the duration of this call, so it is handed over
			// per frame and taken back below rather than cached.
			SetHudCanvas(hud->Canvas);
			if (auto* engine = ::Common::FindFirstObject<USFXEngine, true>())
			{
				auto* wi = static_cast<ABioWorldInfo*>(engine->GetCurrentWorldInfo());
				if (auto* pc = wi->GetALocalPlayerController())
				{
					if (pc->Pawn)
					{
						UpdateEditorPlayerData(pc->Pawn->LOCATION, pc->Pawn->Rotation);
					}
					if (pc->PlayerCamera)
					{
						const auto& pov = pc->PlayerCamera->CameraCache.POV;
						UpdateEditorCameraData(pov.location, pov.Rotation, pov.FOV);
					}
				}
			}

			//DrawDebugTraces();
			DrawGeneratedPathfinding();

			if (DrawGameFireLinksForSlot) {
				DrawGameFireLinksForCurrentSlot();
			}
			if (DrawClientFireLinksForSlot) {
				DrawClientFireLinksForCurrentSlot();
			}
			if (DrawClientDangerNavsForSlot) {
				DrawClientDangerNavsForCurrentSlot();
			}
			if (DrawClientExposedCoverForSlot) {
				DrawClientExposedCoverForCurrentSlot();
			}
			if (DrawCoverInteractionLocations) {
				DrawInteractionPositions();
			}
			if (DrawSpawnedLinks) {
				DrawSpawnedCoverLinks();
			}

			SetHudCanvas(nullptr);
		}

		UObject_ProcessEvent_orig(Context, Function, Parms, Result);
	}
}