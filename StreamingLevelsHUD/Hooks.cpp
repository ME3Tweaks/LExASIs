#include <psapi.h>
#include <string>
#include "Hooks.hpp"
#include <LESDK/Headers.hpp>
#include <LESDK/Init.hpp>
#include "Common/Base.hpp"

namespace StreamingLevelsHUD
{
	// ! Variables ======================================================================
	// Maximum memory we've seen
	size_t MaxMemoryHit = 0;

	// If we should draw the SLH
	bool DrawSLH = true;

	// If we can draw the SLH right now (to prevent multiple toggles on key hold)
	bool CanToggleDrawSLH = false;

	// The string of the last touched trigger stream
	std::wstring lastTouchedTriggerStream;

	// The scale of the text
	float textScale = 1.0f;

	// The line height of the text so we don't draw over ourselves
	float lineHeight = 12.0f;

	// ProcessEvent hook
	// Renders the HUD for Streaming Levels
	// ======================================================================

	static void RenderTextSLH(std::wstring msg, const float x, const float y, const unsigned char r, const unsigned char g, const unsigned char b, UCanvas* can)
	{
		can->SetDrawColor(r, g, b, 255);
		can->SetPos(x, y + 64); //+ is Y start. To prevent overlay on top of the power bar thing
		can->DrawText(FString(msg.c_str()), 1, textScale, textScale, nullptr);
	}

	std::wstring FormatBytes(size_t bytes)
	{
		const wchar_t* sizes[4] = { L"B", L"KB", L"MB", L"GB" };

		int i;
		double dblByte = static_cast<double>(bytes);
		for (i = 0; i < 4 && bytes >= 1024; i++, bytes /= 1024)
			dblByte = bytes / 1024.0;

		return std::format(L"{:.2f}{}", dblByte, sizes[i]);
	}

	FString GetLinker(UObject* obj) {
		UObject* parent = obj->Outer;
		while (parent->Outer != nullptr) {
			parent = parent->Outer;
		}

		return parent->GetName();
	}

	int line = 0;
	PROCESS_MEMORY_COUNTERS pmc;

	void SetTextScale()
	{
		HWND activeWindow = FindWindowA(NULL, WINDOWNAME);
		if (activeWindow)
		{
			RECT rcOwner;
			if (GetWindowRect(activeWindow, &rcOwner))
			{
				long width = rcOwner.right - rcOwner.left;
				long height = rcOwner.bottom - rcOwner.top;

				if (width > 2560 && height > 1440)
				{
					textScale = 2.0f;
				}
				else if (width > 1920 && height > 1080)
				{
					textScale = 1.5f;
				}
				else
				{
					textScale = 1.0f;
				}
				lineHeight = 12.0f * textScale;
			}
		}
	}

	t_UObject_ProcessEvent* UObject_ProcessEvent_orig = nullptr;
	void UObject_ProcessEvent_hook(UObject* Context, UFunction* Function, void* Parms, void* Result)
	{
		auto funcFullName = Function->GetFullName();
		if (funcFullName.Equals(L"Function SFXGame.BioTriggerStream.Touch"))
		{
			auto rparms = reinterpret_cast<ABioTriggerStream_eventTouch_Parms*>(Parms);
			if (rparms->Other->IsA(ASFXPawn_Player::StaticClass()))
			{
				auto bts = reinterpret_cast<ABioTriggerStream*>(Context);
				lastTouchedTriggerStream = std::format(L"{}.{}", GetLinker(bts).Chars(), bts->GetFullName().Chars());
			}
		}
		else if (Function->GetName().Equals(L"DrawHUD")) // We do this on all DrawHUD calls since BioHUD may not be used
		{
			auto hud = reinterpret_cast<AHUD*>(Context);
			if (hud != nullptr)
			{
				hud->Canvas->SetDrawColor(0, 0, 0, 255);
				hud->Canvas->SetPos(0, 0);
				hud->Canvas->DrawBox(1920, 1080);
			}
		}
		else if (funcFullName.Equals(L"Function SFXGame.BioHUD.PostRender"))
		{
			// Toggling 
			line = 0;
			auto hud = reinterpret_cast<ABioHUD*>(Context);
			if (hud != nullptr)
			{
				// Toggle drawing/not drawing
				if ((GetKeyState('T') & 0x8000) && (GetKeyState(VK_CONTROL) & 0x8000)) {
					if (CanToggleDrawSLH) {
						CanToggleDrawSLH = false; // Will not activate combo again until you re-press combo
						DrawSLH = !DrawSLH;
					}
				}
				else
				{
					if (!(GetKeyState('T') & 0x8000) || !(GetKeyState(VK_CONTROL) & 0x8000)) {
						CanToggleDrawSLH = true; // can press key combo again
					}
				}

				// Render mem usage
				if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
				{
					unsigned char r = 0;
					unsigned char g = 255;

					// Flash if high crit
					if (DrawSLH)
					{
						auto currentMemory = FormatBytes(pmc.PagefileUsage);
						std::wstring objectName = std::format(L"Memory usage: {} ({} bytes)", currentMemory, pmc.PagefileUsage);
						RenderTextSLH(objectName, 5.0f, line * lineHeight, r, g, 0, hud->Canvas);
					}
					line++;

					// Max Hit
					if (pmc.PagefileUsage > MaxMemoryHit)
					{
						MaxMemoryHit = pmc.PagefileUsage;
					}

					if (DrawSLH) {
						auto maxMemoryStr = std::format(L"Max memory hit: {} ({} bytes)", FormatBytes(MaxMemoryHit), MaxMemoryHit);
						RenderTextSLH(maxMemoryStr, 5.0f, line * lineHeight, r, g, 0, hud->Canvas);
					}

					line++;
				}

				if (DrawSLH && hud->WorldInfo)
				{
					SetTextScale();
					int yIndex = 3; //Start at line 3 (starting at 0)

					auto lastHit = std::format(L"Last BioTriggerStream: {}", lastTouchedTriggerStream);
					RenderTextSLH(lastHit, 5.0f, yIndex * lineHeight, 0, 255, 64, hud->Canvas);
					yIndex++;

					if (hud->WorldInfo->StreamingLevels.Any()) {
						for (unsigned int i = 0; i < hud->WorldInfo->StreamingLevels.Count(); i++) {
							std::wstringstream ss;
							ULevelStreaming* sl = hud->WorldInfo->StreamingLevels.GetData()[i];
							if (sl->bShouldBeLoaded || sl->bIsVisible) {
								unsigned char r = 255;
								unsigned char g = 255;
								unsigned char b = 255;

								if (!sl->bIsVisible && sl->bHasLoadRequestPending)
								{
									ss << ">> ";
								}

								ss << sl->PackageName.GetName();
								if (sl->PackageName.Number > 0)
								{
									ss << "_" << sl->PackageName.Number - 1;
								}
								if (sl->bIsVisible)
								{
									ss << " Visible";
									r = 0;
									g = 255;
									b = 0;
								}
								else if (sl->bHasLoadRequestPending)
								{
									ss << " Loading";
								 r = 255;
									g = 229;
									b = 0;
								}
								else if (sl->bHasUnloadRequestPending)
								{
									ss << " Unloading";
								 r = 0;
									g = 255;
									b = 229;
								}
								else if (sl->bShouldBeLoaded && sl->LoadedLevel)
								{
									ss << " Loaded";
									r = 255;
									g = 255;
									b = 0;
								}
								else if (sl->bShouldBeLoaded)
								{
									ss << " Pending load";
									r = 185;
									g = 169;
									b = 0;
								}
								const std::wstring msg = ss.str();
								RenderTextSLH(msg, 5, yIndex * lineHeight, r, g, b, hud->Canvas);
								yIndex++;
							}
						}
					}
				}
			}
		}

		UObject_ProcessEvent_orig(Context, Function, Parms, Result);
	}
}
