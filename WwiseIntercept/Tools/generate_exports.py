#!/usr/bin/env python3
"""
Generates WwiseIntercept's hook declarations/definitions and WwiseDebugBridge's
export .def file from MassEffect3.exe's AK:: export table.

Rerun manually whenever the exe or the target Wwise SDK version changes.
The output of this script is checked in (same convention as the hand-maintained
BUILTIN_*_RVA tables in External/LExSDKv2/Src/LESDK/Init.hpp) - it is NOT
regenerated at build time.

Usage:
    py generate_exports.py --exe "S:\\...\\MassEffect3.exe" --dumpbin "<path>\\dumpbin.exe" --undname "<path>\\undname.exe"
"""
import argparse
import re
import subprocess
import sys
from pathlib import Path

# The very first AK:: call the game makes (traced in Ghidra: called from the
# BioWare Wwise-init wrapper before anything else). This one gets installed
# eagerly during SPI attach; InstallRemainingHooks() installs everything else
# from inside its hook body. See WwiseIntercept plan / Hooks.hpp comments.
PRIORITY_MANGLED = "?Init@MemoryMgr@AK@@YA?AW4AKRESULT@@PEAUAkMemSettings@@@Z"

# Testing showed only ~35/295 hooks actually install in this session - the
# underlying (closed-source) hook engine appears to have a fixed budget
# (hook count or trampoline memory, we can't tell which from here), and
# everything past it fails, roughly in the table's existing order. Rather
# than leave that to alphabetical chance, these get installed first (after
# the priority entry above), in this order, so whatever budget exists gets
# spent on the functions that matter: the rest of the traced startup
# sequence (needed for the "one real engine instance" argument to hold),
# then the most commonly-used state-affecting API. A qualified name here
# matches any overload whose name starts with it. Everything not listed
# keeps its existing (alphabetical) relative order after these.
PRIORITY_QUALIFIED_NAMES = [
    "AK::StreamMgr::Create",
    "AK::SoundEngine::GetDefaultInitSettings",
    "AK::SoundEngine::GetDefaultPlatformInitSettings",
    "AK::SoundEngine::Init",
    "AK::MusicEngine::Init",
    "AK::SoundEngine::RegisterGlobalCallback",
    "AK::SoundEngine::RegisterGameObj",
    "AK::SoundEngine::SetDefaultListeners",
    "AK::SoundEngine::PostEvent",
    "AK::SoundEngine::LoadBank",
    "AK::SoundEngine::UnloadBank",
    "AK::SoundEngine::UnregisterGameObj",
    "AK::SoundEngine::SetSwitch",
    "AK::SoundEngine::SetState",
    "AK::SoundEngine::SetRTPCValue",
    "AK::SoundEngine::StopAll",
    "AK::SoundEngine::Term",
    "AK::MemoryMgr::Term",
]

# Confirmed via live testing to never successfully install: the hook engine's
# trampoline relocator can't fit their relocated prologue in the available
# trampoline slot (see the plan's "Runtime finding: not every export is
# hookable" section - e.g. ExecuteActionOnPlayingID needs 37 bytes, this
# fork's hook-chaining overhead leaves only 34 available). Both were also
# independently confirmed to have zero real call sites anywhere in the exe's
# own code (checked in Ghidra) - dead API surface the game never invokes -
# so excluding them costs nothing. Excluded here (rather than left to fail
# and get SEH-caught at install time every session) purely to stop the
# now-pointless "failed to install hook" warnings from PRIORITY_QUALIFIED_
# NAMES's own installation attempt. If a future SDK/exe update adds a call
# site to either of these, this is the place to revert that.
KNOWN_UNHOOKABLE_MANGLED = {
    "?ExecuteActionOnPlayingID@SoundEngine@AK@@YAXW4AkActionOnEventType@12@KJW4AkCurveInterpolation@@@Z",
    "?StopPlayingID@SoundEngine@AK@@YAXKJW4AkCurveInterpolation@@@Z",
}

# Entry-only logging can't tell "the call happened" from "the call actually
# worked" - e.g. AK_FileNotFound looks identical to AK_Success in the log.
# For these specific qualified-name prefixes, log the return value too (as a
# plain integer - enough to tell success (AKRESULT AK_Success == 1, AkPlayingID
# != 0) from failure at a glance). Restricted to prefixes whose return type is
# always one of the two forms handled below (LOG_RETURN_CAST) across every
# overload; add more prefixes here as needed, but check the return type is
# actually integer-like first, or generation will emit a bogus cast.
LOG_RETURN_QUALIFIED_PREFIXES = [
    "AK::SoundEngine::LoadBank",
    "AK::SoundEngine::PostEvent",
]

# How to turn each supported return type into a loggable integer. Add an
# entry here before adding a new prefix above with a different return type.
LOG_RETURN_CAST = {
    "enum AKRESULT": "static_cast<long>",
    "unsigned long": "static_cast<unsigned long>",
}

# Fires a hand-written C++ statement right after a specific exact-qualified-
# name's bridge call returns (only supported where log_return is already true
# for that name, since that's the code shape that gives us a `result` local
# to sit next to - see the emission loop below). Used for
# TryRegisterMasterBusMetering() (Hooks.cpp): the bus hierarchy doesn't exist
# until the project's Init bank has loaded, so registering bus-metering
# eagerly at startup would just fail - retrying it after every LoadBank call
# (idempotent, guarded by an atomic in Hooks.cpp) means it self-heals onto
# whichever LoadBank actually brings in the bus hierarchy without needing to
# know that bank's name ahead of time.
POST_CALL_QUALIFIED_NAMES = {
    "AK::SoundEngine::LoadBank": "TryRegisterMasterBusMetering();",
    "AK::SoundEngine::PostEvent": "NotePostEventForBusMeterBurst();",
}

# A boolean C++ expression (using this overload's own a0.. names, so it can
# inspect the real typed arguments) that gates whether the bridge call
# actually happens at all - unlike POST_CALL_QUALIFIED_NAMES, which only
# runs after a real call already completed. Only supported where log_return
# is already true (PostEvent/LoadBank), since that's the code shape with a
# `result` local to assign the skipped-call fallback into. Currently used
# for ShouldForwardPostEvent() (Hooks.cpp)'s "solo PostEvent" debug mode:
# swallow every PostEvent except one target event ID entirely before it
# reaches the bridge, so nothing else can add itself to the mix while
# isolating one suspect event - see the g_SoloPostEventMode comment in
# Hooks.cpp for how to point it at a different event.
PRE_CALL_GUARD_QUALIFIED_NAMES = {
    "AK::SoundEngine::PostEvent": "ShouldForwardPostEvent(a0)",
}

# Entry-only logging (OnWwiseCall) just prints the function name - fine for
# most of the 270, but for the handful that actually drive what's happening
# (which sound got asked for, which RTPC/object a call targets) the name
# alone isn't useful for tracing a real issue back to content. This is a
# curated allowlist, not an attempt to describe every export's arguments -
# matched by exact qualified name (not prefix, since e.g. GetIDFromString's
# char*/wchar_t* overloads need different format strings for the same name)
# plus a lightweight shape check on the specific overload's params.
def build_arg_log(qualified_name: str, params: list) -> str | None:
    def is_str(ptype: str) -> bool:
        return ptype in ("char const *", "wchar_t const *")

    def wide(ptype: str) -> str:
        return "L" if ptype == "wchar_t const *" else ""

    if qualified_name == "AK::SoundEngine::GetIDFromString" and len(params) == 1 and is_str(params[0]):
        return f'    LEASI_TRACE({wide(params[0])}"  -> hashing: {{}}", a0);'

    if qualified_name == "AK::SoundEngine::RegisterGameObj" and len(params) == 2 and is_str(params[1]):
        return f'    LEASI_TRACE({wide(params[1])}"  -> id={{}} name={{}}", a0, a1);'

    if qualified_name in ("AK::SoundEngine::SetRTPCValue", "AK::SoundEngine::SetRTPCValueByPlayingID") \
            and len(params) >= 3:
        target = "playingID" if qualified_name.endswith("ByPlayingID") else "gameObj"
        if is_str(params[0]):
            return f'    LEASI_TRACE({wide(params[0])}"  -> rtpc={{}} value={{}} {target}={{}}", a0, a1, a2);'
        return f'    LEASI_TRACE("  -> rtpcID={{}} value={{}} {target}={{}}", a0, a1, a2);'

    if qualified_name == "AK::SoundEngine::PostEvent" and len(params) >= 2:
        if is_str(params[0]):
            return f'    LEASI_TRACE({wide(params[0])}"  -> event={{}} gameObj={{}}", a0, a1);'
        return f'    LEASI_TRACE("  -> eventID={{}} gameObj={{}}", a0, a1);'

    # Volume-affecting calls that don't go through RTPC/bus routing at all -
    # a direct per-listener gain multiplier and per-listener obstruction/
    # occlusion attenuation+lowpass, respectively. Both silently override
    # whatever the bus hierarchy/RTPCs would otherwise produce, so they're
    # worth seeing even though nothing else in this codegen currently
    # decodes them.
    if qualified_name == "AK::SoundEngine::SetGameObjectOutputBusVolume" and len(params) == 3:
        return '    LEASI_TRACE("  -> emitter={} listener={} controlValue={}", a0, a1, a2);'

    if qualified_name == "AK::SoundEngine::SetObjectObstructionAndOcclusion" and len(params) == 4:
        return '    LEASI_TRACE("  -> emitter={} listener={} obstruction={} occlusion={}", a0, a1, a2, a3);'

    if qualified_name == "AK::SoundEngine::SetGameObjectAuxSendValues" and len(params) == 3:
        return (
            '    LogAuxSendValues(a0, a1, a2);'
        )

    # UnregisterGameObj force-stops anything still playing on that game
    # object (per Wwise's own semantics) - worth knowing exactly which
    # object it targets and when, since a PostEvent immediately followed by
    # UnregisterGameObj on the same object would silently kill the voice
    # before it ever renders audio, with no error anywhere in the normal
    # call-succeeded/return-value logging.
    if qualified_name == "AK::SoundEngine::UnregisterGameObj" and len(params) == 1:
        return '    LEASI_TRACE("  -> gameObj={}", a0);'

    return None

EXPORT_LINE_RE = re.compile(
    r"^\s*(\d+)\s+[0-9A-Fa-f]+\s+([0-9A-Fa-f]{8})\s+(\S+)(?:\s+=\s+(.+?))?\s*$"
)
UNDNAME_RESULT_RE = re.compile(r'is :-\s*"(.*)"\s*$', re.MULTILINE | re.DOTALL)


def run_dumpbin_exports(dumpbin: str, exe: str):
    proc = subprocess.run([dumpbin, "/exports", exe], capture_output=True, text=True, check=True)
    entries = []
    for line in proc.stdout.splitlines():
        m = EXPORT_LINE_RE.match(line)
        if not m:
            continue
        ordinal, rva, mangled, demangled = m.groups()
        entries.append({"ordinal": int(ordinal), "rva": rva, "mangled": mangled})
    return entries


def filter_ak(entries):
    return [e for e in entries if "@AK@" in e["mangled"]]


def undname(undname_exe: str, mangled: str) -> str:
    proc = subprocess.run([undname_exe, mangled], capture_output=True, text=True, check=True)
    m = UNDNAME_RESULT_RE.search(proc.stdout)
    if not m:
        raise RuntimeError(f"undname produced unexpected output for {mangled}:\n{proc.stdout}")
    return m.group(1).strip()


_OPEN_BRACKETS = "(<["
_CLOSE_BRACKETS = ")>]"


def split_top_level_commas(text: str):
    """
    Split on commas at bracket-depth 0, so nested function-pointer params AND
    template argument lists (AkArray<A, B, C>) survive intact.
    """
    if text.strip() == "void" or text.strip() == "":
        return []
    parts = []
    depth = 0
    start = 0
    for i, ch in enumerate(text):
        if ch in _OPEN_BRACKETS:
            depth += 1
        elif ch in _CLOSE_BRACKETS:
            depth -= 1
        elif ch == "," and depth == 0:
            parts.append(text[start:i].strip())
            start = i + 1
    parts.append(text[start:].strip())
    return [p for p in parts if p]


_PTR64_RE = re.compile(r"\s*\b__ptr64\b")


def strip_ptr64(text: str) -> str:
    """
    __ptr64 is a redundant no-op annotation on x64 builds, but MSVC's /W4
    flags it as an "anachronism" (C4227) when it lands on a reference type
    (e.g. "struct Foo const & __ptr64", which undname emits routinely) - and
    this repo builds with /WX, so it needs to go rather than be suppressed.
    """
    return _PTR64_RE.sub("", text).strip()


class ParseSkip(Exception):
    pass


def parse_signature(sig: str, mangled: str):
    """
    Parse undname's demangled text into (is_member, class_name, qualified_name,
    short_name, return_type, param_types[]).　Raises ParseSkip for shapes we
    deliberately don't handle yet (constructors/destructors, anything odd).
    """
    text = sig.strip()

    is_member = False
    for prefix in ("public: ", "protected: ", "private: "):
        if text.startswith(prefix):
            is_member = True
            text = text[len(prefix):]
            break

    if text.startswith("static "):
        # Not expected for the AK:: surface we kept (only free functions and
        # plain instance methods survived the @AK@ filter) - skip if it shows up.
        raise ParseSkip("static member function (unexpected shape)")

    if text.startswith("virtual "):
        # "virtual" is a declaration-context keyword, not part of the callable
        # signature - resolving by exact mangled name already gives us this
        # specific override's address directly, so a plain (non-virtual-dispatch)
        # call through that address is correct; just drop the keyword here.
        text = text[len("virtual "):]

    # Split on the FIRST "__cdecl " marker, not the first "AK::" - the return
    # type itself can contain "AK::" (e.g. a function returning IAkStreamMgr*,
    # which is AK::IAkStreamMgr*), which would otherwise truncate it.
    cdecl_marker = "__cdecl "
    cdecl_idx = text.find(cdecl_marker)
    if cdecl_idx == -1:
        # Constructors/destructors have no return type in undname's output,
        # so no leading "__cdecl " marker either.
        raise ParseSkip("no return type (likely a constructor/destructor)")

    return_type = text[:cdecl_idx].strip()
    if not return_type:
        raise ParseSkip("no return type (likely a constructor/destructor)")

    rest = text[cdecl_idx + len(cdecl_marker):]
    if not rest.startswith("AK::"):
        raise ParseSkip("qualified name after __cdecl does not start with AK::")

    paren_idx = rest.find("(")
    if paren_idx == -1:
        raise ParseSkip("no parameter list found")
    qualified_name = rest[:paren_idx].strip()

    last_paren = rest.rfind(")")
    if last_paren == -1 or last_paren < paren_idx:
        raise ParseSkip("unbalanced parameter list")
    param_text = rest[paren_idx + 1:last_paren]

    if "~" in qualified_name:
        raise ParseSkip("destructor")
    short_name = qualified_name.rsplit("::", 1)[-1]
    class_name = qualified_name.rsplit("::", 1)[0] if "::" in qualified_name else ""
    if class_name and short_name == class_name.rsplit("::", 1)[-1]:
        raise ParseSkip("constructor")

    params = [strip_ptr64(p) for p in split_top_level_commas(param_text)]

    if is_member:
        if not class_name:
            raise ParseSkip("member function without a class name")
        params = [f"{class_name}*"] + params

    return {
        "is_member": is_member,
        "class_name": class_name,
        "qualified_name": qualified_name,
        "short_name": short_name,
        "return_type": strip_ptr64(return_type),
        "params": params,
        "mangled": mangled,
    }


def make_ident(parsed: dict, ordinal: int) -> str:
    raw = parsed["qualified_name"].replace("::", "_")
    ident = re.sub(r"[^A-Za-z0-9_]", "_", raw)
    return f"{ident}_{ordinal}"


_FUNC_PTR_RE = re.compile(r"\(__cdecl\*\)")


def make_param_decl(ptype: str, name: str) -> str:
    """
    Builds a "Type name" declaration, handling the one non-trivial C/C++
    declarator shape that shows up in this SDK: function-pointer parameters
    like "void (__cdecl*)(enum AkCallbackType,...)" (AkCallbackFunc), where
    the name has to go *inside* the parens right after '*', not appended
    after the whole type text.
    """
    if _FUNC_PTR_RE.search(ptype):
        return _FUNC_PTR_RE.sub(f"(__cdecl* {name})", ptype, count=1)
    return f"{ptype} {name}"


def generate(entries_parsed, out_dir: Path, bridge_def_path: Path):
    hpp_lines = []
    cpp_lines = []
    table_lines = []

    hpp_lines.append("// GENERATED FILE - see WwiseIntercept/Tools/generate_exports.py. Do not hand-edit.")
    hpp_lines.append("#pragma once")
    hpp_lines.append("")
    hpp_lines.append('#include "Common/Base.hpp"')
    hpp_lines.append('#include <AK/SoundEngine/Common/AkSoundEngine.h>')
    hpp_lines.append('#include <AK/SoundEngine/Common/AkModule.h>')
    hpp_lines.append('#include <AK/SoundEngine/Common/AkStreamMgrModule.h>')
    hpp_lines.append('#include <AK/MusicEngine/Common/AkMusicEngine.h>')
    hpp_lines.append('#include <AK/SoundEngine/Common/AkBytesMem.h>')
    hpp_lines.append('#include <AK/SoundEngine/Common/AkBytesCount.h>')
    hpp_lines.append('#include <AK/Tools/Common/AkArray.h>')
    hpp_lines.append('#include <AK/Tools/Common/AkMonitorError.h>')
    hpp_lines.append('#include <AK/SoundEngine/Common/AkQueryParameters.h>')
    hpp_lines.append('#include <AK/SoundEngine/Common/AkDynamicSequence.h>')
    hpp_lines.append("")
    hpp_lines.append("namespace WwiseIntercept {")
    hpp_lines.append("")
    hpp_lines.append("struct WwiseHookEntry {")
    hpp_lines.append("    char const* MangledName;")
    hpp_lines.append("    void** OrigSlot;")
    hpp_lines.append("    void** BridgeSlot;")
    hpp_lines.append("    void* HookFn;")
    hpp_lines.append("};")
    hpp_lines.append("")
    hpp_lines.append(f"inline constexpr size_t kWwiseHookCount = {len(entries_parsed)};")
    hpp_lines.append("extern WwiseHookEntry g_WwiseHooks[kWwiseHookCount];")
    hpp_lines.append("")

    cpp_lines.append("// GENERATED FILE - see WwiseIntercept/Tools/generate_exports.py. Do not hand-edit.")
    cpp_lines.append('#include "WwiseExports.gen.hpp"')
    cpp_lines.append('#include "WwiseIntercept/Hooks.hpp"')
    cpp_lines.append("")
    cpp_lines.append("namespace WwiseIntercept {")
    cpp_lines.append("")

    def_lines = ["; GENERATED FILE - see WwiseIntercept/Tools/generate_exports.py. Do not hand-edit.",
                 "LIBRARY WwiseDebugBridge", "EXPORTS"]

    priority_entry = None
    normal_entries = []

    for parsed in entries_parsed:
        ordinal = parsed["_ordinal"]
        ident = make_ident(parsed, ordinal)
        ret = parsed["return_type"]
        params = parsed["params"]
        # Omit the trailing ", " entirely when there are no params, rather than
        # passing an explicitly-empty variadic argument (avoids relying on
        # C++20 empty-__VA_ARGS__ behavior - just call the macro with 2 args).
        if params:
            hpp_lines.append(f"LEASI_HOOKDEF({ident}, {ret}, {', '.join(params)})")
        else:
            hpp_lines.append(f"LEASI_HOOKDEF({ident}, {ret})")

        param_names = ", ".join(f"a{i}" for i in range(len(params)))
        param_decls = ", ".join(make_param_decl(ptype, f"a{i}") for i, ptype in enumerate(params))

        # LEASI_HOOKDEF only extern-declares _orig; this is the one definition.
        cpp_lines.append(f"t_{ident}* {ident}_orig = nullptr;")
        cpp_lines.append(f"t_{ident}* {ident}_bridge = nullptr;")
        cpp_lines.append(f"{ret} {ident}_hook({param_decls})")
        cpp_lines.append("{")
        if parsed["mangled"] == PRIORITY_MANGLED:
            # First AK:: call the game makes (see plan) - this is where the
            # remaining ~294 hooks get installed, lazily and exactly once,
            # before we forward to the bridge's own Init.
            cpp_lines.append("    InstallRemainingHooks();")
        cpp_lines.append(f'    OnWwiseCall("{parsed["qualified_name"]}");')
        arg_log = build_arg_log(parsed["qualified_name"], params)
        if arg_log is not None:
            cpp_lines.append(arg_log)

        log_return = (
            ret in LOG_RETURN_CAST
            and any(parsed["qualified_name"].startswith(p) for p in LOG_RETURN_QUALIFIED_PREFIXES)
        )
        if log_return:
            cast = LOG_RETURN_CAST[ret]
            guard = PRE_CALL_GUARD_QUALIFIED_NAMES.get(parsed["qualified_name"])
            if guard is not None:
                cpp_lines.append(f"    auto const result = ({guard}) ? {ident}_bridge({param_names}) : static_cast<{ret}>(0);")
            else:
                cpp_lines.append(f"    auto const result = {ident}_bridge({param_names});")
            cpp_lines.append(f'    OnWwiseCallResult("{parsed["qualified_name"]}", {cast}(result));')
            post_call = POST_CALL_QUALIFIED_NAMES.get(parsed["qualified_name"])
            if post_call is not None:
                cpp_lines.append(f"    {post_call}")
            cpp_lines.append("    return result;")
        else:
            cpp_lines.append(f"    return {ident}_bridge({param_names});")
        cpp_lines.append("}")
        cpp_lines.append("")

        def_lines.append(f"    {parsed['mangled']}")

        entry_line = (
            f'    {{ "{parsed["mangled"]}", (void**)&{ident}_orig, '
            f"(void**)&{ident}_bridge, (void*)&{ident}_hook }},"
        )
        if parsed["mangled"] == PRIORITY_MANGLED:
            priority_entry = entry_line
        else:
            normal_entries.append(entry_line)

    hpp_lines.append("")
    hpp_lines.append("}")

    if priority_entry is None:
        raise RuntimeError("priority export (AK::MemoryMgr::Init) was not found/parsed - aborting")

    cpp_lines.append("WwiseHookEntry g_WwiseHooks[kWwiseHookCount] = {")
    cpp_lines.append(priority_entry)
    cpp_lines.extend(normal_entries)
    cpp_lines.append("};")
    cpp_lines.append("")
    cpp_lines.append("}")

    (out_dir / "WwiseExports.gen.hpp").write_text("\n".join(hpp_lines), encoding="utf-8")
    (out_dir / "WwiseExports.gen.cpp").write_text("\n".join(cpp_lines), encoding="utf-8")
    bridge_def_path.write_text("\n".join(def_lines) + "\n", encoding="utf-8")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--exe", required=True)
    ap.add_argument("--dumpbin", required=True)
    ap.add_argument("--undname", required=True)
    ap.add_argument("--out-dir", default=str(Path(__file__).resolve().parent.parent / "Generated"))
    ap.add_argument("--bridge-def", default=str(Path(__file__).resolve().parent.parent / "Bridge" / "Generated" / "WwiseDebugBridge.def"))
    args = ap.parse_args()

    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    bridge_def_path = Path(args.bridge_def)
    bridge_def_path.parent.mkdir(parents=True, exist_ok=True)

    all_entries = run_dumpbin_exports(args.dumpbin, args.exe)
    ak_entries = filter_ak(all_entries)
    print(f"dumpbin: {len(all_entries)} total exports, {len(ak_entries)} in AK:: namespace")

    # Identical COMDAT Folding (/OPT:ICF): the Release linker merges functions
    # whose compiled bytes are byte-identical (typically trivial stubs) into
    # one shared address, regardless of their real signatures. Confirmed live:
    # AK::MemoryMgr::SetMonitoring(long,bool) and SetPoolName(long,wchar_t*)
    # both landed on the same address as an unrelated ReturnTrue() stub -
    # hooking either one with its own typed signature means a caller of the
    # OTHER folded identity gets its register contents misread as arguments,
    # which then get forwarded to a REAL (non-folded) bridge implementation
    # that dutifully validates them and crashes on the garbage. There is no
    # way to safely disambiguate this at the callee side - hooking is by
    # address, and once patched, every caller reaching that address is
    # intercepted regardless of which exported name they conceptually meant.
    # So: any RVA shared by more than one export (AK:: or not - the "other"
    # identity doesn't have to be one of ours) gets excluded entirely.
    #
    # A "narrower calls wider with a reliable default" exception was tried
    # here for AK::SoundEngine::RegisterGameObj (its 1-arg overload looked,
    # in a clean non-folded reference build, like a thunk that zeroes RDX
    # before jumping into the 2-arg body). Verified against the ACTUAL
    # MassEffect3.exe call sites in Ghidra and it does NOT hold there: one
    # real call site (FUN_7ff74b173410's "no name" branch, reached whenever
    # the object has no extra data - not a rare path) sets only RCX before
    # calling; RDX is whatever an intervening CALL happened to leave behind
    # (a volatile, caller-doesn't-care register per the x64 ABI), not a
    # deliberate null. That's the exact same failure shape as the
    # SetPoolName crash. So: no exceptions. Every ICF collision, including
    # this one, stays excluded - verify against the real binary before ever
    # reinstating something like this, a clean reference build isn't proof
    # the shipped build's compiler made the same choice.
    COLLISION_KEEP_WIDER = set()

    rva_counts = {}
    for e in all_entries:
        rva_counts[e["rva"]] = rva_counts.get(e["rva"], 0) + 1

    parsed_entries = []
    skipped = []
    for e in ak_entries:
        if e["mangled"] in KNOWN_UNHOOKABLE_MANGLED:
            skipped.append((e["mangled"], "(confirmed unhookable + zero real callers - see KNOWN_UNHOOKABLE_MANGLED)",
                             "excluded by KNOWN_UNHOOKABLE_MANGLED"))
            continue
        if rva_counts[e["rva"]] > 1 and e["mangled"] not in COLLISION_KEEP_WIDER:
            skipped.append((e["mangled"], "(address shared with another export - ICF folding)",
                             f"RVA {e['rva']} shared by {rva_counts[e['rva']]} exports"))
            continue
        sig = undname(args.undname, e["mangled"])
        try:
            parsed = parse_signature(sig, e["mangled"])
            parsed["_ordinal"] = e["ordinal"]
            parsed_entries.append(parsed)
        except ParseSkip as ex:
            skipped.append((e["mangled"], sig, str(ex)))

    print(f"parsed: {len(parsed_entries)} hookable exports, skipped: {len(skipped)}")
    if skipped:
        print("\nSkipped (excluded from generated output):")
        for mangled, sig, reason in skipped:
            print(f"  [{reason}] {mangled}\n      -> {sig}")

    has_priority = any(p["mangled"] == PRIORITY_MANGLED for p in parsed_entries)
    if not has_priority:
        print("\nERROR: priority export (AK::MemoryMgr::Init) was skipped or not found - fix parser before proceeding.", file=sys.stderr)
        sys.exit(1)

    def priority_rank(parsed):
        qn = parsed["qualified_name"]
        for i, prefix in enumerate(PRIORITY_QUALIFIED_NAMES):
            if qn.startswith(prefix):
                return i
        return len(PRIORITY_QUALIFIED_NAMES)

    # Stable sort: preserves existing (alphabetical) order within each rank.
    parsed_entries.sort(key=priority_rank)

    generate(parsed_entries, out_dir, bridge_def_path)
    print(f"\nWrote {out_dir / 'WwiseExports.gen.hpp'}")
    print(f"Wrote {out_dir / 'WwiseExports.gen.cpp'}")
    print(f"Wrote {bridge_def_path}")


if __name__ == "__main__":
    main()
