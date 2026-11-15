// WwiseDebugBridge is a thin re-export shim. Its actual behavior comes
// entirely from the Debug-config Wwise SDK static libraries it links and the
// generated Generated/WwiseDebugBridge.def, which re-exports the exact set
// of AK:: symbols WwiseIntercept hooks - a .def EXPORTS entry is itself a
// valid "root reference" that pulls a symbol straight out of a linked
// static lib, so no C++ glue code is needed for the re-exports themselves
// (and the CRT's default DllMain, pulled in automatically by the normal CRT
// link, is all that's needed there).
//
// The only real code this file has is the small set of allocation hooks
// AkMemoryMgr.lib requires the integrator to provide (documented in
// AK::AllocHook/FreeHook/VirtualAllocHook/VirtualFreeHook, AkTypes.h) -
// trivial passthroughs to the CRT/OS allocators.

#include <AK/SoundEngine/Common/AkTypes.h>
#include <AK/SoundEngine/Common/IAkPlugin.h>
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

    void* VirtualAllocHook(void* in_pMemAddress, size_t in_size, AkUInt32 in_dwAllocationType, AkUInt32 in_dwProtect)
    {
        return ::VirtualAlloc(in_pMemAddress, in_size, in_dwAllocationType, in_dwProtect);
    }

    void VirtualFreeHook(void* in_pMemAddress, size_t in_size, AkUInt32 in_dwFreeType)
    {
        ::VirtualFree(in_pMemAddress, in_size, in_dwFreeType);
    }
}

// Plugin/codec registration.
//
// Found the actual cause of total silence: "Codec Plugin not Registered"
// (AK::Monitor::ErrorCode_CodecNotRegistered) surfaced via the diagnostic
// callback wired up in WwiseIntercept. AkVorbisDecoder.lib (Vorbis is the
// LE3 template project's configured compression codec) is a SEPARATE
// library from the core AkSoundEngine, same as every FX plugin - none of
// them get pulled into the final binary just by being linked, since nothing
// in this otherwise-empty bridge stub referenced their object code. Each
// AK_STATIC_LINK_PLUGIN(name) below force-references that plugin's
// registration object so the linker keeps it; the plugin then registers
// itself with the sound engine automatically at Init() time - no other
// integration code needed. Registering the full catalog available in this
// SDK (all first-party + the two bundled McDSP plugins - confirmed present
// via both header and .lib) rather than trying to guess the exact subset
// the shipped game's real content needs; the LE3 template project (busses
// only, no real content) can't tell us that, and registering an unused
// plugin is harmless.
//
// No DEFINE_PLUGIN_REGISTER_HOOK here - AkSoundEngine.lib already defines
// g_pAKPluginList itself (LNK2005 duplicate-definition if added). And no
// AkSinkFactory.h - "AkSink" isn't a real standalone linkable plugin in
// this SDK layout (no corresponding registration object to resolve).
#include <AK/Plugin/AkAudioInputSourceFactory.h>
#include <AK/Plugin/AkCompressorFXFactory.h>
#include <AK/Plugin/AkDelayFXFactory.h>
#include <AK/Plugin/AkExpanderFXFactory.h>
#include <AK/Plugin/AkFlangerFXFactory.h>
#include <AK/Plugin/AkGainFXFactory.h>
#include <AK/Plugin/AkGuitarDistortionFXFactory.h>
#include <AK/Plugin/AkHarmonizerFXFactory.h>
#include <AK/Plugin/AkMatrixReverbFXFactory.h>
#include <AK/Plugin/AkMeterFXFactory.h>
#include <AK/Plugin/AkOpusDecoderFactory.h>
#include <AK/Plugin/AkParametricEQFXFactory.h>
#include <AK/Plugin/AkPeakLimiterFXFactory.h>
#include <AK/Plugin/AkPitchShifterFXFactory.h>
#include <AK/Plugin/AkRecorderFXFactory.h>
#include <AK/Plugin/AkRoomVerbFXFactory.h>
#include <AK/Plugin/AkSilenceSourceFactory.h>
#include <AK/Plugin/AkSineSourceFactory.h>
#include <AK/Plugin/AkStereoDelayFXFactory.h>
#include <AK/Plugin/AkSynthOneSourceFactory.h>
#include <AK/Plugin/AkTimeStretchFXFactory.h>
#include <AK/Plugin/AkToneSourceFactory.h>
#include <AK/Plugin/AkTremoloFXFactory.h>
#include <AK/Plugin/AkVorbisDecoderFactory.h>
#include <AK/Plugin/McDSPFutzBoxFXFactory.h>
#include <AK/Plugin/McDSPLimiterFXFactory.h>
