// WwiseV56DebugBuild: a thin re-export shim, same idea as WwiseIntercept/Bridge/BridgeStub.cpp
// but for the older Wwise v2010.3.3 build 3773 SDK (the version ME3 2012's original banks were
// authored against - "v56" bank format). Not used at runtime for anything; its only purpose is
// to be a real binary linking the real Debug-config SDK static libs, built with debug info, so
// it (plus its matching .pdb) can be loaded into Ghidra to get real, named decompiled v56-era
// engine code - mirroring the existing v134 WwiseDebugBridge, used all session to verify the
// BankConverter write side (WriteV134). This lets the same kind of ground-truth check happen for
// the read side (ReadV56).
//
// Unlike the v134 bridge, this isn't chasing a specific target .exe's export table - there's no
// "MassEffect3.exe (2012)" to intersect against locally, so Generated/WwiseV56Debug.def instead
// re-exports every public symbol found in the linked libs (see generate_v56_def.py). A .def
// EXPORTS entry is itself a valid "root reference" that pulls a symbol straight out of a linked
// static lib, so - as with the v134 bridge - no C++ glue code is needed for the re-exports
// themselves.
//
// The only real code here is the small set of allocation hooks AkMemoryMgr.lib requires the
// integrator to provide (documented in AK::AllocHook/FreeHook/VirtualAllocHook/VirtualFreeHook,
// AkTypes.h) - trivial passthroughs to the CRT/OS allocators. Unlike the v134 bridge, this SDK
// era (2010.3.3) has no AK_STATIC_LINK_PLUGIN force-link macro at all - plugins here are
// registered manually via AK::SoundEngine::RegisterPlugin(), and their Create*/Create*Params
// factory functions are already ordinary exported symbols. Since generate_v56_def.py's .def
// covers every public symbol in every linked lib (not just a curated subset), those factory
// functions - and therefore each plugin's real DSP/codec code - get pulled in by the .def
// EXPORTS entries alone, with no calling code needed here at all.
#include <AK/SoundEngine/Common/AkTypes.h>
#include <cstdlib>
#include <windows.h>

namespace AK
{
    void* AllocHook(size_t in_size)
    {
        return std::malloc(in_size);
    }

    void FreeHook(void* in_pMemAddress)
    {
        std::free(in_pMemAddress);
    }

    void* VirtualAllocHook(void* in_pMemAddress, size_t in_size, DWORD in_dwAllocationType, DWORD in_dwProtect)
    {
        return ::VirtualAlloc(in_pMemAddress, in_size, in_dwAllocationType, in_dwProtect);
    }

    void VirtualFreeHook(void* in_pMemAddress, size_t in_size, DWORD in_dwFreeType)
    {
        ::VirtualFree(in_pMemAddress, in_size, in_dwFreeType);
    }
}
