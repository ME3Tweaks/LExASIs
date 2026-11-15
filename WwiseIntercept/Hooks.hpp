#pragma once

#include <LESDK/Headers.hpp>
#include <LESDK/Init.hpp>
#include <windows.h>

#include <AK/SoundEngine/Common/AkTypes.h>

namespace WwiseIntercept
{
    // WwiseDebugBridge.dll, loaded from this ASI's own directory.
    extern HMODULE g_hBridge;

    // Generic entry logger shared by every generated hook. Deliberately
    // minimal for now (name only) - each generated hook already carries its
    // own real typed signature, so decoding/logging specific arguments for a
    // given function later is a small, local edit to that one function.
    void OnWwiseCall(char const* QualifiedName);

    // Entry-only logging can't distinguish "the call happened" from "the
    // call actually worked" - AK_FileNotFound looks identical to AK_Success
    // in the log otherwise. Used by the small set of functions in
    // generate_exports.py's LOG_RETURN_QUALIFIED_PREFIXES (currently
    // LoadBank, PostEvent) whose bridge return value gets logged too.
    void OnWwiseCallResult(char const* QualifiedName, long ResultValue);
    void OnWwiseCallResult(char const* QualifiedName, unsigned long ResultValue);

    // Installs only the priority hook (AK::MemoryMgr::Init, table index 0)
    // during SPI attach. The remaining ~294 hooks are installed lazily, once,
    // from inside that hook's body via InstallRemainingHooks() - see the
    // WwiseIntercept plan for why: it collapses the old eager-install /
    // stall-event design (needed only as a guard against a SEQATTACH-timing
    // race that turned out not to be the real problem) into something
    // simpler, and it means we don't pay for hooking functions the game
    // never ends up calling in a given session.
    void InitializeHooks(::LESDK::Initializer& Init);

    // Installs g_WwiseHooks[1..kWwiseHookCount) - everything except the
    // priority entry, which InitializeHooks already installed. Idempotent;
    // safe to call more than once (only the first call does anything).
    //
    // Some exports can't be hooked at all - the hook engine's trampoline
    // relocator can choke on certain prologue shapes (e.g. a short
    // conditional jump within the first few bytes, which is what broke
    // AK::SoundEngine::ExecuteActionOnPlayingID in testing: TEST+JZ rel8
    // right at +2). Each install is wrapped in a Windows SEH guard so a
    // failure there logs a warning and skips that one export instead of
    // taking the whole game down.
    void InstallRemainingHooks();

    // AK::SoundEngine::RegisterGameObj's 2-arg overload (id, name) shares its
    // address in this exe with the 1-arg overload (ICF folding - see the
    // plan's "ICF" section) - one real call site sets only the id argument,
    // leaving the name register as leftover garbage from an unrelated call,
    // not a deliberate null. Excluding this export entirely (the default
    // policy for ICF collisions) isn't safe either: the exe's own native
    // implementation dereferences uninitialized engine state unconditionally
    // and crashes on the very first startup call. So this one gets a
    // hand-written hook instead of the generated generic forward: it probes
    // whether the name pointer is actually readable memory before forwarding
    // it, substituting nullptr when it isn't, rather than trusting it
    // blindly either way. Installed as an explicit extra step inside
    // InstallRemainingHooks(), not part of the generated g_WwiseHooks table.
    void InstallRegisterGameObjHook();

    // Diagnostic: registers Wwise's own real bus-metering callback (peak/RMS,
    // computed by the bridge's actual Debug engine - not something we're
    // estimating) on the Master Audio Bus, so post-mix loudness can be
    // watched in near-real-time via '[BusMeter]' trace lines. Useful for
    // telling apart "a PostEvent succeeded but something (a duck, an RTPC, a
    // muted bus) is silencing it before the output stage" from "it never
    // reached the mix at all" - the two look identical from return-value
    // logging alone. The bus hierarchy doesn't exist until the project's
    // Init bank has loaded, so this is called after every
    // AK::SoundEngine::LoadBank (see generate_exports.py's
    // POST_CALL_QUALIFIED_NAMES) rather than once at startup; it's
    // idempotent (guarded by an atomic) and silently retries on each
    // subsequent LoadBank until the Master Audio Bus actually resolves.
    void TryRegisterMasterBusMetering();

    // Marks "a PostEvent just happened" so the bus-metering callback above
    // can burst to unthrottled (every-frame) logging for a short window
    // instead of its normal 500ms sample rate - see
    // WwiseBusMeteringCallback's comment for why. Called after every
    // AK::SoundEngine::PostEvent (see generate_exports.py's
    // POST_CALL_QUALIFIED_NAMES), regardless of which event or whether it
    // succeeded - the point is just "something was asked to play around
    // now," so the log gives a tight trace to eyeball against.
    void NotePostEventForBusMeterBurst();

    // --- Solo PostEvent testing mode (diagnostic) ---
    // When g_SoloPostEventMode (Hooks.cpp) is on, every PostEvent call is
    // routed through one of these before it's allowed to reach the bridge -
    // returning false makes the generated hook skip the bridge call entirely
    // (see generate_exports.py's PRE_CALL_GUARD_QUALIFIED_NAMES) instead of
    // just logging it and forwarding as normal. The point: isolate one
    // suspect event's actual contribution to the mix with literally nothing
    // else (footsteps, other barks, music, ambience) able to compete for it,
    // without needing a quiet test level or fiddling with in-game volume
    // sliders. The numeric-ID overload is compared against
    // g_SoloPostEventTargetID directly; the two string-named overloads are
    // unconditionally suppressed while solo mode is active, since none of
    // the events this mode exists to isolate are ever posted by name in this
    // game's traced call sites - not worth hashing the string to compare.
    bool ShouldForwardPostEvent(unsigned long EventID);
    bool ShouldForwardPostEvent(char const* EventName);
    bool ShouldForwardPostEvent(wchar_t const* EventName);

    // Logs each AkAuxSendValue entry passed to AK::SoundEngine::SetGameObjectAuxSendValues
    // (aux/reverb send levels, unrelated to dry-path bus volume) - see
    // generate_exports.py's build_arg_log for why this needed a hand-written
    // helper instead of a one-line LEASI_TRACE like the other arg-logged
    // calls: it has to dereference a variable-length array, not just format
    // scalar arguments.
    void LogAuxSendValues(unsigned __int64 GameObjectID, AkAuxSendValue* pValues, unsigned long NumValues);
}
