#if defined(SDK_TARGET_LE3)
#include "Common/Base.hpp"
#include "DebugLogger/Hooks.hpp"
#include <cstring>
#include <cwchar>
#include <format>
#include <intrin.h>
#include <mutex>
#include <string>
#include <unordered_map>

namespace DebugLogger {
	namespace
	{
		constexpr uintptr_t AK_SoundEngine_PostEventById_RVA = 0xCAD360;
		constexpr uintptr_t AK_SoundEngine_PostEventByAnsiName_RVA = 0xCAD5A0;
		constexpr uintptr_t AK_SoundEngine_PostEventByWideName_RVA = 0xCAD670;

		std::mutex WwiseEventNameCacheMutex;
		std::unordered_map<uint32_t, std::wstring> WwiseEventNames;
		int WwiseEventNameCacheObjectCount = 0;

		// Wwise's numeric PostEvent API intentionally receives only the hashed
		// AkUniqueID. UE3 retains the readable asset name separately on loaded
		// UWwiseEvent objects, whose Id property is that same Wwise ID. Refresh
		// after package loads grow the object array so later-loaded DLC/map assets
		// become resolvable without scanning it for every audio post.
		std::wstring ResolveWwiseEventName(uint32_t eventId)
		{
			std::scoped_lock lock(WwiseEventNameCacheMutex);

			const int objectCount = UObject::GObjObjects != nullptr
				? static_cast<int>(UObject::GObjObjects->Count())
				: 0;
			if (objectCount > WwiseEventNameCacheObjectCount)
			{
				UClass* const wwiseEventClass = UWwiseEvent::StaticClass();
				for (int index = 0; index < objectCount; ++index)
				{
					UObject* const object = UObject::GObjObjects->GetData()[index];
					if (object == nullptr || !object->IsA(wwiseEventClass))
						continue;

					auto const* event = static_cast<UWwiseEvent const*>(object);
					if (event->Id == 0)
						continue;

					// Keep the package/outer path as well as the leaf name. This
					// distinguishes identically named events from different GUI,
					// DLC, or map packages in a runtime trace.
					WwiseEventNames.try_emplace(static_cast<uint32_t>(event->Id), event->GetFullPath().Chars());
				}
				WwiseEventNameCacheObjectCount = objectCount;
			}

			if (const auto entry = WwiseEventNames.find(eventId); entry != WwiseEventNames.end())
				return entry->second;

			return {};
		}

		// Return a stable module+RVA string without requiring PDBs. Shipping LE3
		// binaries normally have no symbols available, but this still identifies
		// the exact native callsite for Ghidra lookup.
		std::wstring DescribeWwiseEventCaller(void const* returnAddress)
		{
			HMODULE module = nullptr;
			if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
				GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
				reinterpret_cast<LPCWSTR>(returnAddress), &module))
			{
				return L"UNKNOWN";
			}

			wchar_t modulePath[MAX_PATH]{};
			const DWORD pathLength = GetModuleFileNameW(module, modulePath, ARRAYSIZE(modulePath));
			const wchar_t* moduleName = modulePath;
			if (pathLength != 0)
			{
				if (const wchar_t* lastSlash = wcsrchr(modulePath, L'\\'))
					moduleName = lastSlash + 1;
			}
			else
			{
				moduleName = L"UNKNOWN";
			}

			const auto base = reinterpret_cast<uintptr_t>(module);
			const auto address = reinterpret_cast<uintptr_t>(returnAddress);
			return std::format(L"{}+0x{:X}", moduleName, address - base);
		}

		void LogWwiseEvent(uint32_t eventId, uint64_t gameObjectId, uint32_t playingId, void const* returnAddress)
		{
			const std::wstring eventName = ResolveWwiseEventName(eventId);
			LEASI_INFO(L"WwiseEvent event_id={} (0x{:08X}) event_name={} game_object_id={} "
				L"playing_id={} caller={}", eventId, eventId,
				eventName.empty() ? L"UNKNOWN" : eventName, gameObjectId, playingId,
				DescribeWwiseEventCaller(returnAddress));
		}

		void LogWwiseEvent(char const* eventName, uint64_t gameObjectId, uint32_t playingId, void const* returnAddress)
		{
			// Wwise event names in this vintage of the API are ASCII identifiers.
			// Convert only for the logger's wide formatting path; the numeric hook
			// remains the authoritative record for non-string game submissions.
			const std::wstring wideEventName = eventName != nullptr
				? std::wstring(eventName, eventName + strlen(eventName))
				: L"<null>";
			LEASI_INFO(L"WwiseEvent event_name=\"{}\" game_object_id={} playing_id={} caller={}",
				wideEventName, gameObjectId, playingId,
				DescribeWwiseEventCaller(returnAddress));
		}

		void LogWwiseEvent(wchar_t const* eventName, uint64_t gameObjectId, uint32_t playingId, void const* returnAddress)
		{
			LEASI_INFO(L"WwiseEvent event_name=\"{}\" game_object_id={} playing_id={} caller={}",
				eventName != nullptr ? eventName : L"<null>", gameObjectId, playingId,
				DescribeWwiseEventCaller(returnAddress));
		}
	}

	tPostWwiseEventById* PostWwiseEventById_orig = nullptr;
	tPostWwiseEventByAnsiName* PostWwiseEventByAnsiName_orig = nullptr;
	tPostWwiseEventByWideName* PostWwiseEventByWideName_orig = nullptr;

	void InstallGameSpecificHooks(::LESDK::Initializer& Init) {
		// WwiseBankParsingLoop
		// This function seems to handle the actual loading of bank data.
		// ----------------------------------------
		/*auto const wwiseParsingBankLoop_target = Init.ResolveTyped<tParseWwiseBank>(LEASI_RVA(0xccec40));
		CHECK_RESOLVED(wwiseParsingBankLoop_target);
		ParseWwiseBank_orig = (tParseWwiseBank*)Init.InstallHook("ParseWwiseBank", wwiseParsingBankLoop_target, ParseWwiseBank_hook);
		CHECK_RESOLVED(ParseWwiseBank_orig);*/

		// appThrowf
		// Logs the formatted exception message before the game throws it.
		// ----------------------------------------
		auto const appThrowF_target = Init.ResolveTyped<tAppThrowF>(LEASI_RVA(0x11fcb0));
		CHECK_RESOLVED(appThrowF_target);
		AppThrowF_orig = (tAppThrowF*)Init.InstallHook("AppThrowF", appThrowF_target, AppThrowF_hook);
		CHECK_RESOLVED(AppThrowF_orig);

		// FMaterial::InitShaderMap
		// Detects materials that silently fall back to GEngine->DefaultMaterial (the purple/white
		// checkerboard) because their shader map is missing/incomplete on this shipping build.
		// This function has an unconventional prologue that isn't safe to detour at its very
		// first instruction, so like ShaderClient we hook it via a post-prologue byte pattern
		// instead of a plain RVA.
		// ----------------------------------------
		auto const initShaderMap_target = Init.ResolveTyped<tInitShaderMap>(::LESDK::Address::FromPostHook(
			"54 41 55 41 56 41 57 48 81 ec a0 00 00 00 48 c7 44 24 40 fe ff ff ff 49 89 5b 10 49 89 6b 18 49 89 73 20 45 8b e1 45 8b f0 48 8b ea"));
		CHECK_RESOLVED(initShaderMap_target);
		InitShaderMap_orig = (tInitShaderMap*)Init.InstallHook("InitShaderMap", initShaderMap_target, InitShaderMap_hook);
		CHECK_RESOLVED(InitShaderMap_orig);

		// FMaterial::GetFriendlyName - just resolved directly, not hooked, so we can name the
		// material in the InitShaderMap log above.
		FMaterial_GetFriendlyName = Init.ResolveTyped<tFGetMaterialFriendlyName>(LEASI_RVA(0x2BDEC0));
		CHECK_RESOLVED(FMaterial_GetFriendlyName);

		// Wwise event submissions are intentionally opt-in: gameplay can post
		// many events per second, and the raw IDs are most useful during a
		// focused investigation. These RVAs are for LE3 11.3.2; LMP uses the
		// same LE3 executable layout/SDK target.
		if (std::wcsstr(GetCommandLineW(), L"-wwiseevent"))
		{
			auto const postEventById_target = Init.ResolveTyped<tPostWwiseEventById>(LEASI_RVA(AK_SoundEngine_PostEventById_RVA));
			CHECK_RESOLVED(postEventById_target);
			PostWwiseEventById_orig = reinterpret_cast<tPostWwiseEventById*>(Init.InstallHook(
				"AK_SoundEngine_PostEventById", postEventById_target, PostWwiseEventById_hook));
			CHECK_RESOLVED(PostWwiseEventById_orig);

			auto const postEventByAnsiName_target = Init.ResolveTyped<tPostWwiseEventByAnsiName>(LEASI_RVA(AK_SoundEngine_PostEventByAnsiName_RVA));
			CHECK_RESOLVED(postEventByAnsiName_target);
			PostWwiseEventByAnsiName_orig = reinterpret_cast<tPostWwiseEventByAnsiName*>(Init.InstallHook(
				"AK_SoundEngine_PostEventByAnsiName", postEventByAnsiName_target, PostWwiseEventByAnsiName_hook));
			CHECK_RESOLVED(PostWwiseEventByAnsiName_orig);

			auto const postEventByWideName_target = Init.ResolveTyped<tPostWwiseEventByWideName>(LEASI_RVA(AK_SoundEngine_PostEventByWideName_RVA));
			CHECK_RESOLVED(postEventByWideName_target);
			PostWwiseEventByWideName_orig = reinterpret_cast<tPostWwiseEventByWideName*>(Init.InstallHook(
				"AK_SoundEngine_PostEventByWideName", postEventByWideName_target, PostWwiseEventByWideName_hook));
			CHECK_RESOLVED(PostWwiseEventByWideName_orig);

			LEASI_INFO("Wwise event logging enabled (-wwiseevent): numeric and string AK::SoundEngine::PostEvent hooks installed");
		}

	}

	uint32_t PostWwiseEventById_hook(uint32_t eventId, uint64_t gameObjectId,
		uint32_t flags, tAkCallbackFunc callback, void* cookie, uint32_t externalSourceCount,
		void* externalSources, uint32_t playingId)
	{
		void const* const returnAddress = _ReturnAddress();
		const uint32_t result = PostWwiseEventById_orig(eventId, gameObjectId, flags, callback, cookie,
			externalSourceCount, externalSources, playingId);
		LogWwiseEvent(eventId, gameObjectId, result, returnAddress);
		return result;
	}

	uint32_t PostWwiseEventByAnsiName_hook(char const* eventName, uint64_t gameObjectId,
		uint32_t flags, tAkCallbackFunc callback, void* cookie, uint32_t externalSourceCount,
		void* externalSources, uint32_t playingId)
	{
		void const* const returnAddress = _ReturnAddress();
		const uint32_t result = PostWwiseEventByAnsiName_orig(eventName, gameObjectId, flags, callback, cookie,
			externalSourceCount, externalSources, playingId);
		LogWwiseEvent(eventName, gameObjectId, result, returnAddress);
		return result;
	}

	uint32_t PostWwiseEventByWideName_hook(wchar_t const* eventName, uint64_t gameObjectId,
		uint32_t flags, tAkCallbackFunc callback, void* cookie, uint32_t externalSourceCount,
		void* externalSources, uint32_t playingId)
	{
		void const* const returnAddress = _ReturnAddress();
		const uint32_t result = PostWwiseEventByWideName_orig(eventName, gameObjectId, flags, callback, cookie,
			externalSourceCount, externalSources, playingId);
		LogWwiseEvent(eventName, gameObjectId, result, returnAddress);
		return result;
	}

	tFGetMaterialFriendlyName* FMaterial_GetFriendlyName = nullptr;

	tInitShaderMap* InitShaderMap_orig = nullptr;
	UBOOL InitShaderMap_hook(FMaterial* fmaterial, void* StaticParameters, EShaderPlatform Platform, int Quality)
	{
		auto result = InitShaderMap_orig(fmaterial, StaticParameters, Platform, Quality);
		if (!result)
		{
			FString name;
			FMaterial_GetFriendlyName(fmaterial, &name);
			LEASI_WARN(L"Shader map missing/incomplete for material {} - default material (checkerboard) will be substituted", name.Chars());
		}
		return result;
	}
}

#endif
