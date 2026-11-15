#include "WwiseIntercept/Hooks.hpp"
#include "Common/Base.hpp"
#include "WwiseIntercept/Generated/WwiseExports.gen.hpp"

#include <atomic>
#include <cmath>
#include <cstdint>
#include <format>
#include <iterator>
#include <string>
#include <string_view>
#include <unordered_set>

namespace WwiseIntercept
{
    HMODULE g_hBridge = nullptr;

    namespace
    {
        ::LESDK::Initializer* g_pInit = nullptr;
        HMODULE g_hGame = nullptr;
        std::atomic<bool> g_bRemainingInstalled{ false };

        // Called every frame (or more) per audio object, per real session
        // measurement (a few minutes of play produced 95k IsInitialized
        // calls, 66k/61k WriteBytesCount::Count/Reserve, 10k+ RenderAudio) -
        // logging these drowns out everything else. Everything not listed
        // here still logs normally; add to this set if something else turns
        // out to be similarly noisy.
        std::unordered_set<std::string_view> const kNoisyFunctions = {
            "AK::SoundEngine::IsInitialized",
            "AK::SoundEngine::RenderAudio",
            "AK::SoundEngine::Query::GetPositioningInfo",
            "AK::SoundEngine::SetPosition",
            "AK::SoundEngine::Query::QueryAudioObjectIDs",
            "AK::WriteBytesCount::Count",
            "AK::WriteBytesCount::Reserve",
            "AK::WriteBytesMem::Bytes",
        };
    }

    // --- Solo PostEvent testing mode (diagnostic) ---
    // Isolates one suspect event's actual contribution to the mix by
    // swallowing every OTHER PostEvent before it ever reaches the bridge -
    // not just muted client-side, never mixed at all - so literally nothing
    // else (footsteps, other barks, music, ambience) can compete for it.
    // This is a throwaway debugging knob, not meant to ship enabled: flip
    // g_SoloPostEventMode to true and set g_SoloPostEventTargetID to
    // whichever AkUniqueID you're chasing (visible in the log's
    // "PostEvent -> eventID=..." trace line), then rebuild.
    namespace
    {
        bool constexpr g_SoloPostEventMode = false;
        unsigned long constexpr g_SoloPostEventTargetID = 1982137765; // SS_ss_mp_n7m1_player_..._m_Play

        std::atomic<uint64_t> g_SuppressedPostEventCount{ 0 };
    }

    void OnWwiseCall(char const* QualifiedName)
    {
        if (kNoisyFunctions.contains(QualifiedName))
        {
            return;
        }
        LEASI_TRACE("{}", QualifiedName);
    }

    // AKRESULT-returning overload (AK_Success == 1) - e.g. LoadBank.
    void OnWwiseCallResult(char const* QualifiedName, long ResultValue)
    {
        if (ResultValue == 1)
        {
            LEASI_TRACE("{} -> AK_Success", QualifiedName);
        }
        else
        {
            LEASI_TRACE("{} -> FAILED (AKRESULT={})", QualifiedName, ResultValue);
        }
    }

    // AkPlayingID-returning overload (0 == AK_INVALID_PLAYING_ID) - PostEvent.
    void OnWwiseCallResult(char const* QualifiedName, unsigned long ResultValue)
    {
        if (ResultValue != 0)
        {
            LEASI_TRACE("{} -> playingID={}", QualifiedName, ResultValue);
            if constexpr (g_SoloPostEventMode)
            {
                LEASI_INFO("WwiseIntercept: [solo mode] target event reached the bridge - {} other "
                           "PostEvent call(s) suppressed so far this session", g_SuppressedPostEventCount.load());
            }
        }
        else
        {
            LEASI_TRACE("{} -> FAILED (AK_INVALID_PLAYING_ID)", QualifiedName);
        }
    }

    bool ShouldForwardPostEvent(unsigned long EventID)
    {
        if (!g_SoloPostEventMode || EventID == g_SoloPostEventTargetID)
        {
            return true;
        }
        ++g_SuppressedPostEventCount;
        return false;
    }

    // String-named PostEvent calls aren't hash-compared against the target
    // ID - none of the events this mode exists to isolate are ever posted
    // by name in this game's traced call sites, so just suppress all of
    // them unconditionally while solo mode is active rather than bothering
    // to replicate Wwise's own string-hashing to compare.
    bool ShouldForwardPostEvent(char const*)
    {
        if (!g_SoloPostEventMode)
        {
            return true;
        }
        ++g_SuppressedPostEventCount;
        return false;
    }

    bool ShouldForwardPostEvent(wchar_t const*)
    {
        if (!g_SoloPostEventMode)
        {
            return true;
        }
        ++g_SuppressedPostEventCount;
        return false;
    }

    // Resolves this DLL's own module handle via the address of a function
    // known to live inside it - there's no DllMain in this SPI plugin shape
    // to capture hInstance from directly.
    static HMODULE GetSelfModule()
    {
        HMODULE hSelf = nullptr;
        BOOL const ok = GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&GetSelfModule),
            &hSelf);
        LEASI_VERIFYA(ok && hSelf != nullptr, "failed to resolve WwiseIntercept's own module handle", "");
        return hSelf;
    }

    // Loads WwiseDebugBridge.dll from the same directory as WwiseIntercept.asi
    // itself, rather than relying on the game's DLL search path.
    static HMODULE LoadBridge()
    {
        wchar_t path[MAX_PATH];
        DWORD const len = GetModuleFileNameW(GetSelfModule(), path, MAX_PATH);
        LEASI_VERIFYA(len > 0 && len < MAX_PATH, "failed to resolve WwiseIntercept.asi's own path", "");

        std::wstring dir(path);
        size_t const slash = dir.find_last_of(L"\\/");
        LEASI_VERIFYA(slash != std::wstring::npos, "unexpected module path shape", "");
        dir.resize(slash + 1);
        dir += L"WwiseDebugBridge.dll";

        HMODULE const hBridge = LoadLibraryW(dir.c_str());
        LEASI_VERIFYW(hBridge != nullptr, L"failed to load {}", dir);
        return hBridge;
    }

    // Isolated in its own function so the __try block never shares a stack
    // frame with a C++ object that requires unwinding (MSVC forbids mixing
    // the two - error C2712). This is what actually recovers from a hook
    // engine failure instead of letting its __debugbreak()/access violation
    // take the whole game down - see Hooks.hpp for why some exports can fail
    // to hook at all (e.g. AK::SoundEngine::ExecuteActionOnPlayingID, whose
    // prologue starts with a short/rel8 conditional jump the trampoline
    // relocator can't handle).
    static bool TryInstallHook(::LESDK::Initializer& Init, WwiseHookEntry const& entry, void* target)
    {
        __try
        {
            *entry.OrigSlot = Init.InstallHook(entry.MangledName, target, entry.HookFn);
            return *entry.OrigSlot != nullptr;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
    }

    static bool InstallOne(WwiseHookEntry const& entry)
    {
        void* const target = reinterpret_cast<void*>(GetProcAddress(g_hGame, entry.MangledName));
        if (target == nullptr)
        {
            LEASI_WARN("WwiseIntercept: export {} not found in main module, skipping", entry.MangledName);
            return false;
        }

        *entry.BridgeSlot = reinterpret_cast<void*>(GetProcAddress(g_hBridge, entry.MangledName));
        if (*entry.BridgeSlot == nullptr)
        {
            LEASI_WARN("WwiseIntercept: export {} not found in WwiseDebugBridge.dll, skipping", entry.MangledName);
            return false;
        }

        if (!TryInstallHook(*g_pInit, entry, target))
        {
            LEASI_WARN("WwiseIntercept: failed to install hook for {} (hook engine rejected the prologue), skipping - "
                       "calls to this export will run the game's own unredirected implementation", entry.MangledName);
            return false;
        }

        return true;
    }

    // --- AK::SoundEngine::RegisterGameObj hand-written special case ---
    // See Hooks.hpp for why: this export's address is ICF-folded with its
    // 1-arg sibling, and one real call site in MassEffect3.exe only ever
    // sets the id argument - the name register holds whatever an unrelated
    // CALL happened to leave behind, not a deliberate null. Confirmed live:
    // leaving this export unhooked (the default ICF-collision policy)
    // crashes immediately - the exe's own native implementation
    // dereferences uninitialized engine state unconditionally.
    namespace
    {
        char const* const kRegisterGameObjMangled =
            "?RegisterGameObj@SoundEngine@AK@@YA?AW4AKRESULT@@_KPEBD@Z";

        using t_RegisterGameObj_Special = AKRESULT(unsigned __int64, char const*);
        t_RegisterGameObj_Special* RegisterGameObj_Special_orig = nullptr;
        t_RegisterGameObj_Special* RegisterGameObj_Special_bridge = nullptr;

        // Best-effort "is this actually a valid, readable pointer" probe -
        // enough to catch the garbage-register case (observed as literal
        // address 0x1) without needing to know the string's real length.
        bool IsReadableString(void const* p)
        {
            if (p == nullptr)
            {
                return false;
            }
            MEMORY_BASIC_INFORMATION mbi{};
            if (VirtualQuery(p, &mbi, sizeof(mbi)) == 0)
            {
                return false;
            }
            if (mbi.State != MEM_COMMIT || (mbi.Protect & PAGE_GUARD) != 0)
            {
                return false;
            }
            constexpr DWORD kReadableFlags = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY |
                PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
            return (mbi.Protect & kReadableFlags) != 0;
        }

        // Isolated so __try never shares a frame with a C++ object requiring
        // unwinding (MSVC C2712) - a second line of defense in case the
        // pointer probe above isn't enough for some other garbage shape.
        bool TryForwardRegisterGameObj(unsigned __int64 id, char const* name, AKRESULT& outResult)
        {
            __try
            {
                outResult = RegisterGameObj_Special_bridge(id, name);
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        AKRESULT RegisterGameObj_Special_hook(unsigned __int64 in_GameObjID, char const* in_pszName)
        {
            char const* const safeName = IsReadableString(in_pszName) ? in_pszName : nullptr;
            OnWwiseCall("AK::SoundEngine::RegisterGameObj");
            if (safeName != nullptr)
            {
                LEASI_TRACE("  -> id={} name={}", in_GameObjID, safeName);
            }
            else
            {
                LEASI_TRACE("  -> id={}", in_GameObjID);
            }

            AKRESULT result = AK_Fail;
            if (!TryForwardRegisterGameObj(in_GameObjID, safeName, result))
            {
                LEASI_WARN("WwiseIntercept: AK::SoundEngine::RegisterGameObj threw even after "
                           "name-pointer validation (id={}), returning AK_Fail", in_GameObjID);
                result = AK_Fail;
            }
            return result;
        }
    }

    void InstallRegisterGameObjHook()
    {
        WwiseHookEntry entry{
            kRegisterGameObjMangled,
            reinterpret_cast<void**>(&RegisterGameObj_Special_orig),
            reinterpret_cast<void**>(&RegisterGameObj_Special_bridge),
            reinterpret_cast<void*>(&RegisterGameObj_Special_hook)
        };
        if (!InstallOne(entry))
        {
            LEASI_WARN("WwiseIntercept: failed to install the hand-written RegisterGameObj hook - "
                       "game object registration will hit the exe's own uninitialized engine and likely crash");
        }
    }

    // --- AK::Monitor diagnostic output ---
    // AK::Monitor::SetLocalOutput/PostCode/PostString are documented by the
    // SDK itself to be no-ops in Release/Optimized Wwise builds ("does
    // nothing... in the optimized/release configuration") - which is exactly
    // why they got ICF-folded together with unrelated stubs in the shipped
    // exe (see the plan's ICF section) and why hooking them from the exe
    // side would be pointless even if they weren't collision-excluded. The
    // bridge is a real Debug build though, so its implementation is fully
    // functional - call it DIRECTLY against the bridge (no hook, no
    // redirect, just a plain function call) to register our own callback
    // and surface whatever internal diagnostics Wwise generates on its own
    // (streaming starvation, decode failures, missing plugins, output
    // device init failures, etc.) that never show up as a return value from
    // any hooked call.
    namespace
    {
        char const* const kSetLocalOutputMangled =
            "?SetLocalOutput@Monitor@AK@@YA?AW4AKRESULT@@KP6AXW4ErrorCode@12@PEB_WW4ErrorLevel@12@K_K@Z@Z";

        void WwiseMonitorCallback(
            AK::Monitor::ErrorCode in_eErrorCode,
            AkOSChar const* in_pszError,
            AK::Monitor::ErrorLevel in_eErrorLevel,
            AkPlayingID in_playingID,
            AkGameObjectID in_gameObjID)
        {
            if ((in_eErrorLevel & AK::Monitor::ErrorLevel_Error) != 0)
            {
                LEASI_WARN(L"WwiseIntercept [Wwise]: {} (code={}, playingID={}, gameObjID={})",
                    in_pszError, static_cast<int>(in_eErrorCode), in_playingID, in_gameObjID);
            }
            else
            {
                LEASI_TRACE(L"WwiseIntercept [Wwise]: {} (code={})", in_pszError, static_cast<int>(in_eErrorCode));
            }
        }
    }

    void InstallWwiseMonitorOutput()
    {
        using t_SetLocalOutput = AKRESULT(unsigned long, AK::Monitor::LocalOutputFunc);
        auto const setLocalOutput = reinterpret_cast<t_SetLocalOutput*>(
            GetProcAddress(g_hBridge, kSetLocalOutputMangled));
        if (setLocalOutput == nullptr)
        {
            LEASI_WARN("WwiseIntercept: {} not found in WwiseDebugBridge.dll - Wwise's own internal "
                       "diagnostics (streaming/decode/output-device errors) won't be visible", kSetLocalOutputMangled);
            return;
        }

        AKRESULT const result = setLocalOutput(AK::Monitor::ErrorLevel_All, &WwiseMonitorCallback);
        if (result == AK_Success)
        {
            LEASI_INFO("WwiseIntercept: registered Wwise's internal diagnostic callback (AK::Monitor::SetLocalOutput)");
        }
        else
        {
            LEASI_WARN("WwiseIntercept: AK::Monitor::SetLocalOutput returned {} (not AK_Success) - Wwise's own "
                       "internal diagnostics may not be visible", static_cast<long>(result));
        }
    }

    // --- Master Audio Bus metering (diagnostic) ---
    // Reuses the already-resolved bridge function pointers from the normal
    // generated hooks for AK::SoundEngine::GetIDFromString and
    // ::RegisterBusMeteringCallback (both installed during
    // InstallRemainingHooks(), which always completes before
    // AK::SoundEngine::Init/LoadBank are ever reached - see the traced
    // startup order in the plan) rather than re-resolving them by mangled
    // name - one less place that name string can silently drift out of sync.
    extern t_AK_SoundEngine_GetIDFromString_82* AK_SoundEngine_GetIDFromString_82_bridge;
    extern t_AK_SoundEngine_RegisterBusMeteringCallback_195* AK_SoundEngine_RegisterBusMeteringCallback_195_bridge;

    namespace
    {
        // Timestamp of the most recent AK::SoundEngine::PostEvent call (any
        // event) - see NotePostEventForBusMeterBurst(). A steady 500ms
        // sample rate is too coarse to tell "this specific voice" apart from
        // everything else sharing a bus (weapons/footsteps/ambience in a
        // busy MP match keep Master itself essentially never silent);
        // bursting to unthrottled logging for a short window after each
        // PostEvent gives a tight trace tied to that specific call instead.
        std::atomic<ULONGLONG> g_lastPostEventMs{ 0 };

        float LinearToDb(float linear)
        {
            constexpr float kFloorDb = -100.0f;
            if (linear <= 0.0f)
            {
                return kFloorDb;
            }
            float const db = 20.0f * log10f(linear);
            return db < kFloorDb ? kFloorDb : db;
        }

        void LogBusMeter(char const* busLabel, AK::IAkMetering* in_pMetering, AkChannelConfig in_channelConfig, ULONGLONG& io_lastLogMs)
        {
            // Fires once per audio frame (~every 21ms). Normally throttled
            // to one line per 500ms per bus so this doesn't drown out
            // everything else; for 1.5s after any PostEvent, log every
            // frame instead so a specific event's contribution is visible
            // as a tight trace.
            ULONGLONG const nowMs = GetTickCount64();
            bool const bursting = nowMs - g_lastPostEventMs.load() < 1500;
            if (!bursting && nowMs - io_lastLogMs < 500)
            {
                return;
            }
            io_lastLogMs = nowMs;

            AkUInt32 const numChannels = in_channelConfig.uNumChannels;
            AK::SpeakerVolumes::ConstVectorPtr const peaks = in_pMetering->GetPeak();
            AK::SpeakerVolumes::ConstVectorPtr const rms = in_pMetering->GetRMS();

            if (peaks == nullptr || numChannels == 0)
            {
                LEASI_TRACE("WwiseIntercept [BusMeter] {}: no active channels", busLabel);
                return;
            }

            // Per-channel breakdown, not just an aggregate max - a corrupted
            // positioning/channel-routing field somewhere upstream (e.g. in
            // the ActorMixer hierarchy a ported bank's voice runs through)
            // could park real signal on a channel a stereo/2.1 setup never
            // renders (LFE, a rear/height channel) while still reading as a
            // healthy "peak" in an aggregate max - that would be invisible
            // without seeing each channel separately. uChannelMask is logged
            // alongside so a specific channel index can be mapped back to a
            // speaker position (bit definitions in AkSpeakerConfig.h) if one
            // stands out.
            float peakDb = -100.0f;
            std::string channelPeaks;
            for (AkUInt32 i = 0; i < numChannels; ++i)
            {
                float const db = LinearToDb(peaks[i]);
                if (db > peakDb) peakDb = db;
                if (i > 0) channelPeaks += ",";
                channelPeaks += std::format("ch{}={:.1f}", i, db);
            }
            float rmsDb = -100.0f;
            if (rms != nullptr)
            {
                for (AkUInt32 i = 0; i < numChannels; ++i)
                {
                    float const db = LinearToDb(rms[i]);
                    if (db > rmsDb) rmsDb = db;
                }
            }

            LEASI_TRACE("WwiseIntercept [BusMeter] {}: peak={:.1f}dB rms={:.1f}dB mask=0x{:x} [{}]",
                busLabel, peakDb, rmsDb, static_cast<unsigned int>(in_channelConfig.uChannelMask), channelPeaks);
        }

        // AkBusMeteringCallbackFunc carries no user-data/cookie parameter, so
        // each monitored bus needs its own trampoline function to know which
        // bus (and which throttle timer) it's reporting for - LogBusMeter()
        // above holds the one copy of the actual metering/formatting logic.
#define WWISE_DEFINE_BUS_METER_CALLBACK(FuncName, Label)                                                      \
        void FuncName(AK::IAkMetering* in_pMetering, AkChannelConfig in_channelConfig, AkMeteringFlags)       \
        {                                                                                                     \
            static ULONGLONG lastLogMs = 0;                                                                  \
            LogBusMeter(Label, in_pMetering, in_channelConfig, lastLogMs);                                    \
        }

        // Master is always present and is the final sanity check ("is
        // anything coming out at all"). The rest are the VO/dialogue-routing
        // candidates pulled from LE3TemplateProject's actual Master-Mixer
        // hierarchy (Ducking Control Buses / Env-VO / NonEnv-VO subtrees) -
        // registering all of them at once avoids having to first prove which
        // one a given bark actually routes through by tracing the
        // Actor-Mixer hierarchy offline. Whichever one shows a real peak
        // exactly when a PostEvent fires is the answer.
        WWISE_DEFINE_BUS_METER_CALLBACK(WwiseBusMeter_Master, "Master Audio Bus")
        WWISE_DEFINE_BUS_METER_CALLBACK(WwiseBusMeter_EnvVOSoundSetDuck, "Env-VO-SoundSet-Duck")
        WWISE_DEFINE_BUS_METER_CALLBACK(WwiseBusMeter_EnvVOExertions, "Env-VO-Exertions")
        WWISE_DEFINE_BUS_METER_CALLBACK(WwiseBusMeter_EnvVOConversation, "Env-VO-Conversation")
        WWISE_DEFINE_BUS_METER_CALLBACK(WwiseBusMeter_EnvVOAmbientDuck, "Env-VO-Ambient-Duck")
        WWISE_DEFINE_BUS_METER_CALLBACK(WwiseBusMeter_NonEnvVO, "NonEnv-VO")
        WWISE_DEFINE_BUS_METER_CALLBACK(WwiseBusMeter_NonSlowdownDialog, "NonSlowdown-Dialog")
#undef WWISE_DEFINE_BUS_METER_CALLBACK

        struct BusMeterTarget
        {
            char const* Name;
            AkBusMeteringCallbackFunc Callback;
        };

        BusMeterTarget const kBusMeterTargets[] = {
            { "Master Audio Bus",       &WwiseBusMeter_Master },
            { "Env-VO-SoundSet-Duck",   &WwiseBusMeter_EnvVOSoundSetDuck },
            { "Env-VO-Exertions",       &WwiseBusMeter_EnvVOExertions },
            { "Env-VO-Conversation",    &WwiseBusMeter_EnvVOConversation },
            { "Env-VO-Ambient-Duck",    &WwiseBusMeter_EnvVOAmbientDuck },
            { "NonEnv-VO",              &WwiseBusMeter_NonEnvVO },
            { "NonSlowdown-Dialog",     &WwiseBusMeter_NonSlowdownDialog },
        };

        std::atomic<bool> g_busMeteringDone[std::size(kBusMeterTargets)]{};
    }

    void NotePostEventForBusMeterBurst()
    {
        g_lastPostEventMs = GetTickCount64();
    }

    void TryRegisterMasterBusMetering()
    {
        if (AK_SoundEngine_GetIDFromString_82_bridge == nullptr ||
            AK_SoundEngine_RegisterBusMeteringCallback_195_bridge == nullptr)
        {
            // One or both of these two specific hooks failed to install -
            // nothing to call through. Not expected to be transient, so
            // don't bother retrying.
            LEASI_WARN("WwiseIntercept: can't register bus metering - GetIDFromString/"
                       "RegisterBusMeteringCallback bridge pointer(s) missing");
            return;
        }

        for (size_t i = 0; i < std::size(kBusMeterTargets); ++i)
        {
            bool expected = false;
            if (!g_busMeteringDone[i].compare_exchange_strong(expected, true))
            {
                continue;
            }

            BusMeterTarget const& target = kBusMeterTargets[i];
            AkUniqueID const busID = AK_SoundEngine_GetIDFromString_82_bridge(target.Name);
            AKRESULT const result = AK_SoundEngine_RegisterBusMeteringCallback_195_bridge(
                busID, target.Callback,
                static_cast<AkMeteringFlags>(AK_EnableBusMeter_Peak | AK_EnableBusMeter_RMS));

            if (result == AK_Success)
            {
                LEASI_INFO("WwiseIntercept: registered bus metering on '{}' - watch for "
                           "'[BusMeter] {}:' trace lines", target.Name, target.Name);
            }
            else
            {
                // Most likely AK_IDNotFound - either the bus hierarchy
                // (Init bank) hasn't loaded yet, or this bus name doesn't
                // exist in the actual MP project (it was pulled from
                // LE3TemplateProject's SP hierarchy as a best guess). Allow
                // a retry on the next LoadBank call either way.
                g_busMeteringDone[i] = false;
            }
        }
    }

    void LogAuxSendValues(unsigned __int64 GameObjectID, AkAuxSendValue* pValues, unsigned long NumValues)
    {
        if (pValues == nullptr || NumValues == 0)
        {
            LEASI_TRACE("  -> gameObj={} clears all aux sends", GameObjectID);
            return;
        }
        for (unsigned long i = 0; i < NumValues; ++i)
        {
            LEASI_TRACE("  -> gameObj={} auxBus={} listener={} controlValue={}",
                GameObjectID, pValues[i].auxBusID, pValues[i].listenerID, pValues[i].fControlValue);
        }
    }

    void InstallRemainingHooks()
    {
        bool expected = false;
        if (!g_bRemainingInstalled.compare_exchange_strong(expected, true))
        {
            return;
        }

        size_t installedCount = 0;
        for (size_t i = 1; i < kWwiseHookCount; ++i)
        {
            if (InstallOne(g_WwiseHooks[i]))
            {
                ++installedCount;
            }
        }

        InstallRegisterGameObjHook();

        LEASI_INFO("WwiseIntercept: installed {}/{} remaining AK:: hooks ({} total)",
            installedCount, kWwiseHookCount - 1, kWwiseHookCount);
    }

    void InitializeHooks(::LESDK::Initializer& Init)
    {
        g_hBridge = LoadBridge();
        CHECK_RESOLVED(g_hBridge);

        if constexpr (g_SoloPostEventMode)
        {
            LEASI_WARN("WwiseIntercept: SOLO POST EVENT MODE ACTIVE - only eventID={} will actually "
                       "reach the bridge; every other PostEvent call is being swallowed before it can "
                       "mix in. Not meant to be left on - see g_SoloPostEventMode in Hooks.cpp.",
                       g_SoloPostEventTargetID);
        }

        InstallWwiseMonitorOutput();

        g_hGame = reinterpret_cast<HMODULE>(Init.ModuleBase);
        g_pInit = new ::LESDK::Initializer(Init);

        bool const ok = InstallOne(g_WwiseHooks[0]);
        LEASI_VERIFYA(ok, "failed to install the priority AK::MemoryMgr::Init hook - cannot continue", "");

        LEASI_INFO("WwiseIntercept: priority hook installed; remaining {} hooks install on the first "
                   "AK::MemoryMgr::Init call", kWwiseHookCount - 1);
    }
}
