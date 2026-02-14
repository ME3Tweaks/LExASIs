#include "Hooks.hpp"
#include <chrono>
#include <random>

namespace AceSlammer
{
	// ! Global variables
	// ========================================
	const int NEXTSTATE_SLAMMING = 1;
	const int NEXTSTATE_ZEROG = 2;
	const int NEXTSTATE_UPGRAV = 3;
	const int NEXTSTATE_NORMAL = 4;
	int nextState = NEXTSTATE_UPGRAV;
	long long nextStateTimeMs = 0;

	// Helper function to set the next state change time
	void SetNextTime(int minMs, int maxMs)
	{
		static std::random_device rd;
		static std::mt19937 gen(rd());
		std::uniform_int_distribution<> dist(minMs, maxMs);

		auto now = std::chrono::steady_clock::now();
		auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

		nextStateTimeMs = nowMs + dist(gen);
	}

	// ! UObject::ProcessEvent hook
	// ========================================

	t_UObject_ProcessEvent* UObject_ProcessEvent_orig = nullptr;
	void UObject_ProcessEvent_hook(UObject* Context, UFunction* Function, void* Parms, void* Result)
	{
		auto szName = Function->GetFullName();
#if defined(SDK_TARGET_LE1) || defined(SDK_TARGET_LE2)
		if (szName == L"Function Engine.PlayerController.PlayerTick") {
#else 
		if (szName == L"Function SFXGame.BioPlayerController.PlayerTick") {

#endif
			// Check if the nextStateTime has been reached.
			// If not, do nothing here,
			// if it has, run the following
			auto now = std::chrono::steady_clock::now();
			auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

			if (nowMs < nextStateTimeMs) {
				UObject_ProcessEvent_orig(Context, Function, Parms, Result);
				return;
			}

			bool nextStateSet = false;
			ABioPlayerController* bpc = reinterpret_cast<ABioPlayerController*>(Context);
			if (!nextStateSet && nextState == NEXTSTATE_SLAMMING)
			{
				bpc->WorldInfo->WorldGravityZ = -6000; //Slam it like Shaq
				nextState = NEXTSTATE_NORMAL;
				nextStateSet = true;
				SetNextTime(1000, 1200);
			}
			if (!nextStateSet && nextState == NEXTSTATE_NORMAL)
			{
				bpc->WorldInfo->WorldGravityZ = -981; //Normal
				nextState = NEXTSTATE_UPGRAV;
				nextStateSet = true;
				SetNextTime(8000, 20000);
			}
			if (!nextStateSet && nextState == NEXTSTATE_ZEROG)
			{
				bpc->WorldInfo->WorldGravityZ = 3; //just slightly up
				nextState = NEXTSTATE_SLAMMING;
				nextStateSet = true;
				SetNextTime(800, 1600);
			}
			if (!nextStateSet && nextState == NEXTSTATE_UPGRAV)
			{
				bpc->WorldInfo->WorldGravityZ = 700; //Going up
				nextState = NEXTSTATE_ZEROG;
				nextStateSet = true;
				SetNextTime(1400, 2000);
			}
		}

		UObject_ProcessEvent_orig(Context, Function, Parms, Result);
	}
}
