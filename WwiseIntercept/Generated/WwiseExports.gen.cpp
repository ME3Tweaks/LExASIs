// GENERATED FILE - see WwiseIntercept/Tools/generate_exports.py. Do not hand-edit.
#include "WwiseExports.gen.hpp"
#include "WwiseIntercept/Hooks.hpp"

namespace WwiseIntercept {

t_AK_StreamMgr_Create_37* AK_StreamMgr_Create_37_orig = nullptr;
t_AK_StreamMgr_Create_37* AK_StreamMgr_Create_37_bridge = nullptr;
class AK::IAkStreamMgr * AK_StreamMgr_Create_37_hook(struct AkStreamMgrSettings const & a0)
{
    OnWwiseCall("AK::StreamMgr::Create");
    return AK_StreamMgr_Create_37_bridge(a0);
}

t_AK_StreamMgr_CreateDevice_38* AK_StreamMgr_CreateDevice_38_orig = nullptr;
t_AK_StreamMgr_CreateDevice_38* AK_StreamMgr_CreateDevice_38_bridge = nullptr;
unsigned long AK_StreamMgr_CreateDevice_38_hook(struct AkDeviceSettings const & a0, class AK::StreamMgr::IAkLowLevelIOHook * a1)
{
    OnWwiseCall("AK::StreamMgr::CreateDevice");
    return AK_StreamMgr_CreateDevice_38_bridge(a0, a1);
}

t_AK_SoundEngine_GetDefaultInitSettings_67* AK_SoundEngine_GetDefaultInitSettings_67_orig = nullptr;
t_AK_SoundEngine_GetDefaultInitSettings_67* AK_SoundEngine_GetDefaultInitSettings_67_bridge = nullptr;
void AK_SoundEngine_GetDefaultInitSettings_67_hook(struct AkInitSettings & a0)
{
    OnWwiseCall("AK::SoundEngine::GetDefaultInitSettings");
    return AK_SoundEngine_GetDefaultInitSettings_67_bridge(a0);
}

t_AK_SoundEngine_GetDefaultPlatformInitSettings_68* AK_SoundEngine_GetDefaultPlatformInitSettings_68_orig = nullptr;
t_AK_SoundEngine_GetDefaultPlatformInitSettings_68* AK_SoundEngine_GetDefaultPlatformInitSettings_68_bridge = nullptr;
void AK_SoundEngine_GetDefaultPlatformInitSettings_68_hook(struct AkPlatformInitSettings & a0)
{
    OnWwiseCall("AK::SoundEngine::GetDefaultPlatformInitSettings");
    return AK_SoundEngine_GetDefaultPlatformInitSettings_68_bridge(a0);
}

t_AK_SoundEngine_Init_130* AK_SoundEngine_Init_130_orig = nullptr;
t_AK_SoundEngine_Init_130* AK_SoundEngine_Init_130_bridge = nullptr;
enum AKRESULT AK_SoundEngine_Init_130_hook(struct AkInitSettings * a0, struct AkPlatformInitSettings * a1)
{
    OnWwiseCall("AK::SoundEngine::Init");
    return AK_SoundEngine_Init_130_bridge(a0, a1);
}

t_AK_MusicEngine_Init_129* AK_MusicEngine_Init_129_orig = nullptr;
t_AK_MusicEngine_Init_129* AK_MusicEngine_Init_129_bridge = nullptr;
enum AKRESULT AK_MusicEngine_Init_129_hook(struct AkMusicSettings * a0)
{
    OnWwiseCall("AK::MusicEngine::Init");
    return AK_MusicEngine_Init_129_bridge(a0);
}

t_AK_SoundEngine_RegisterGlobalCallback_200* AK_SoundEngine_RegisterGlobalCallback_200_orig = nullptr;
t_AK_SoundEngine_RegisterGlobalCallback_200* AK_SoundEngine_RegisterGlobalCallback_200_bridge = nullptr;
enum AKRESULT AK_SoundEngine_RegisterGlobalCallback_200_hook(void (__cdecl* a0)(class AK::IAkGlobalPluginContext *,enum AkGlobalCallbackLocation,void *), unsigned long a1, void * a2, enum AkPluginType a3, unsigned long a4, unsigned long a5)
{
    OnWwiseCall("AK::SoundEngine::RegisterGlobalCallback");
    return AK_SoundEngine_RegisterGlobalCallback_200_bridge(a0, a1, a2, a3, a4, a5);
}

t_AK_SoundEngine_SetDefaultListeners_242* AK_SoundEngine_SetDefaultListeners_242_orig = nullptr;
t_AK_SoundEngine_SetDefaultListeners_242* AK_SoundEngine_SetDefaultListeners_242_bridge = nullptr;
enum AKRESULT AK_SoundEngine_SetDefaultListeners_242_hook(unsigned __int64 const * a0, unsigned long a1)
{
    OnWwiseCall("AK::SoundEngine::SetDefaultListeners");
    return AK_SoundEngine_SetDefaultListeners_242_bridge(a0, a1);
}

t_AK_SoundEngine_PostEvent_161* AK_SoundEngine_PostEvent_161_orig = nullptr;
t_AK_SoundEngine_PostEvent_161* AK_SoundEngine_PostEvent_161_bridge = nullptr;
unsigned long AK_SoundEngine_PostEvent_161_hook(unsigned long a0, unsigned __int64 a1, unsigned long a2, void (__cdecl* a3)(enum AkCallbackType,struct AkCallbackInfo *), void * a4, unsigned long a5, struct AkExternalSourceInfo * a6, unsigned long a7)
{
    OnWwiseCall("AK::SoundEngine::PostEvent");
    LEASI_TRACE("  -> eventID={} gameObj={}", a0, a1);
    auto const result = (ShouldForwardPostEvent(a0)) ? AK_SoundEngine_PostEvent_161_bridge(a0, a1, a2, a3, a4, a5, a6, a7) : static_cast<unsigned long>(0);
    OnWwiseCallResult("AK::SoundEngine::PostEvent", static_cast<unsigned long>(result));
    NotePostEventForBusMeterBurst();
    return result;
}

t_AK_SoundEngine_PostEvent_162* AK_SoundEngine_PostEvent_162_orig = nullptr;
t_AK_SoundEngine_PostEvent_162* AK_SoundEngine_PostEvent_162_bridge = nullptr;
unsigned long AK_SoundEngine_PostEvent_162_hook(char const * a0, unsigned __int64 a1, unsigned long a2, void (__cdecl* a3)(enum AkCallbackType,struct AkCallbackInfo *), void * a4, unsigned long a5, struct AkExternalSourceInfo * a6, unsigned long a7)
{
    OnWwiseCall("AK::SoundEngine::PostEvent");
    LEASI_TRACE("  -> event={} gameObj={}", a0, a1);
    auto const result = (ShouldForwardPostEvent(a0)) ? AK_SoundEngine_PostEvent_162_bridge(a0, a1, a2, a3, a4, a5, a6, a7) : static_cast<unsigned long>(0);
    OnWwiseCallResult("AK::SoundEngine::PostEvent", static_cast<unsigned long>(result));
    NotePostEventForBusMeterBurst();
    return result;
}

t_AK_SoundEngine_PostEvent_163* AK_SoundEngine_PostEvent_163_orig = nullptr;
t_AK_SoundEngine_PostEvent_163* AK_SoundEngine_PostEvent_163_bridge = nullptr;
unsigned long AK_SoundEngine_PostEvent_163_hook(wchar_t const * a0, unsigned __int64 a1, unsigned long a2, void (__cdecl* a3)(enum AkCallbackType,struct AkCallbackInfo *), void * a4, unsigned long a5, struct AkExternalSourceInfo * a6, unsigned long a7)
{
    OnWwiseCall("AK::SoundEngine::PostEvent");
    LEASI_TRACE(L"  -> event={} gameObj={}", a0, a1);
    auto const result = (ShouldForwardPostEvent(a0)) ? AK_SoundEngine_PostEvent_163_bridge(a0, a1, a2, a3, a4, a5, a6, a7) : static_cast<unsigned long>(0);
    OnWwiseCallResult("AK::SoundEngine::PostEvent", static_cast<unsigned long>(result));
    NotePostEventForBusMeterBurst();
    return result;
}

t_AK_SoundEngine_LoadBank_134* AK_SoundEngine_LoadBank_134_orig = nullptr;
t_AK_SoundEngine_LoadBank_134* AK_SoundEngine_LoadBank_134_bridge = nullptr;
enum AKRESULT AK_SoundEngine_LoadBank_134_hook(unsigned long a0, long a1)
{
    OnWwiseCall("AK::SoundEngine::LoadBank");
    auto const result = AK_SoundEngine_LoadBank_134_bridge(a0, a1);
    OnWwiseCallResult("AK::SoundEngine::LoadBank", static_cast<long>(result));
    TryRegisterMasterBusMetering();
    return result;
}

t_AK_SoundEngine_LoadBank_135* AK_SoundEngine_LoadBank_135_orig = nullptr;
t_AK_SoundEngine_LoadBank_135* AK_SoundEngine_LoadBank_135_bridge = nullptr;
enum AKRESULT AK_SoundEngine_LoadBank_135_hook(unsigned long a0, void (__cdecl* a1)(unsigned long,void const *,enum AKRESULT,long,void *), void * a2, long a3)
{
    OnWwiseCall("AK::SoundEngine::LoadBank");
    auto const result = AK_SoundEngine_LoadBank_135_bridge(a0, a1, a2, a3);
    OnWwiseCallResult("AK::SoundEngine::LoadBank", static_cast<long>(result));
    TryRegisterMasterBusMetering();
    return result;
}

t_AK_SoundEngine_LoadBank_136* AK_SoundEngine_LoadBank_136_orig = nullptr;
t_AK_SoundEngine_LoadBank_136* AK_SoundEngine_LoadBank_136_bridge = nullptr;
enum AKRESULT AK_SoundEngine_LoadBank_136_hook(char const * a0, long a1, unsigned long & a2)
{
    OnWwiseCall("AK::SoundEngine::LoadBank");
    auto const result = AK_SoundEngine_LoadBank_136_bridge(a0, a1, a2);
    OnWwiseCallResult("AK::SoundEngine::LoadBank", static_cast<long>(result));
    TryRegisterMasterBusMetering();
    return result;
}

t_AK_SoundEngine_LoadBank_137* AK_SoundEngine_LoadBank_137_orig = nullptr;
t_AK_SoundEngine_LoadBank_137* AK_SoundEngine_LoadBank_137_bridge = nullptr;
enum AKRESULT AK_SoundEngine_LoadBank_137_hook(char const * a0, void (__cdecl* a1)(unsigned long,void const *,enum AKRESULT,long,void *), void * a2, long a3, unsigned long & a4)
{
    OnWwiseCall("AK::SoundEngine::LoadBank");
    auto const result = AK_SoundEngine_LoadBank_137_bridge(a0, a1, a2, a3, a4);
    OnWwiseCallResult("AK::SoundEngine::LoadBank", static_cast<long>(result));
    TryRegisterMasterBusMetering();
    return result;
}

t_AK_SoundEngine_LoadBank_138* AK_SoundEngine_LoadBank_138_orig = nullptr;
t_AK_SoundEngine_LoadBank_138* AK_SoundEngine_LoadBank_138_bridge = nullptr;
enum AKRESULT AK_SoundEngine_LoadBank_138_hook(void const * a0, unsigned long a1, unsigned long & a2)
{
    OnWwiseCall("AK::SoundEngine::LoadBank");
    auto const result = AK_SoundEngine_LoadBank_138_bridge(a0, a1, a2);
    OnWwiseCallResult("AK::SoundEngine::LoadBank", static_cast<long>(result));
    TryRegisterMasterBusMetering();
    return result;
}

t_AK_SoundEngine_LoadBank_139* AK_SoundEngine_LoadBank_139_orig = nullptr;
t_AK_SoundEngine_LoadBank_139* AK_SoundEngine_LoadBank_139_bridge = nullptr;
enum AKRESULT AK_SoundEngine_LoadBank_139_hook(void const * a0, unsigned long a1, long a2, unsigned long & a3)
{
    OnWwiseCall("AK::SoundEngine::LoadBank");
    auto const result = AK_SoundEngine_LoadBank_139_bridge(a0, a1, a2, a3);
    OnWwiseCallResult("AK::SoundEngine::LoadBank", static_cast<long>(result));
    TryRegisterMasterBusMetering();
    return result;
}

t_AK_SoundEngine_LoadBank_140* AK_SoundEngine_LoadBank_140_orig = nullptr;
t_AK_SoundEngine_LoadBank_140* AK_SoundEngine_LoadBank_140_bridge = nullptr;
enum AKRESULT AK_SoundEngine_LoadBank_140_hook(void const * a0, unsigned long a1, void (__cdecl* a2)(unsigned long,void const *,enum AKRESULT,long,void *), void * a3, unsigned long & a4)
{
    OnWwiseCall("AK::SoundEngine::LoadBank");
    auto const result = AK_SoundEngine_LoadBank_140_bridge(a0, a1, a2, a3, a4);
    OnWwiseCallResult("AK::SoundEngine::LoadBank", static_cast<long>(result));
    TryRegisterMasterBusMetering();
    return result;
}

t_AK_SoundEngine_LoadBank_141* AK_SoundEngine_LoadBank_141_orig = nullptr;
t_AK_SoundEngine_LoadBank_141* AK_SoundEngine_LoadBank_141_bridge = nullptr;
enum AKRESULT AK_SoundEngine_LoadBank_141_hook(void const * a0, unsigned long a1, void (__cdecl* a2)(unsigned long,void const *,enum AKRESULT,long,void *), void * a3, long a4, unsigned long & a5)
{
    OnWwiseCall("AK::SoundEngine::LoadBank");
    auto const result = AK_SoundEngine_LoadBank_141_bridge(a0, a1, a2, a3, a4, a5);
    OnWwiseCallResult("AK::SoundEngine::LoadBank", static_cast<long>(result));
    TryRegisterMasterBusMetering();
    return result;
}

t_AK_SoundEngine_LoadBank_142* AK_SoundEngine_LoadBank_142_orig = nullptr;
t_AK_SoundEngine_LoadBank_142* AK_SoundEngine_LoadBank_142_bridge = nullptr;
enum AKRESULT AK_SoundEngine_LoadBank_142_hook(wchar_t const * a0, long a1, unsigned long & a2)
{
    OnWwiseCall("AK::SoundEngine::LoadBank");
    auto const result = AK_SoundEngine_LoadBank_142_bridge(a0, a1, a2);
    OnWwiseCallResult("AK::SoundEngine::LoadBank", static_cast<long>(result));
    TryRegisterMasterBusMetering();
    return result;
}

t_AK_SoundEngine_LoadBank_143* AK_SoundEngine_LoadBank_143_orig = nullptr;
t_AK_SoundEngine_LoadBank_143* AK_SoundEngine_LoadBank_143_bridge = nullptr;
enum AKRESULT AK_SoundEngine_LoadBank_143_hook(wchar_t const * a0, void (__cdecl* a1)(unsigned long,void const *,enum AKRESULT,long,void *), void * a2, long a3, unsigned long & a4)
{
    OnWwiseCall("AK::SoundEngine::LoadBank");
    auto const result = AK_SoundEngine_LoadBank_143_bridge(a0, a1, a2, a3, a4);
    OnWwiseCallResult("AK::SoundEngine::LoadBank", static_cast<long>(result));
    TryRegisterMasterBusMetering();
    return result;
}

t_AK_SoundEngine_UnloadBank_300* AK_SoundEngine_UnloadBank_300_orig = nullptr;
t_AK_SoundEngine_UnloadBank_300* AK_SoundEngine_UnloadBank_300_bridge = nullptr;
enum AKRESULT AK_SoundEngine_UnloadBank_300_hook(unsigned long a0, void const * a1, void (__cdecl* a2)(unsigned long,void const *,enum AKRESULT,long,void *), void * a3)
{
    OnWwiseCall("AK::SoundEngine::UnloadBank");
    return AK_SoundEngine_UnloadBank_300_bridge(a0, a1, a2, a3);
}

t_AK_SoundEngine_UnloadBank_301* AK_SoundEngine_UnloadBank_301_orig = nullptr;
t_AK_SoundEngine_UnloadBank_301* AK_SoundEngine_UnloadBank_301_bridge = nullptr;
enum AKRESULT AK_SoundEngine_UnloadBank_301_hook(unsigned long a0, void const * a1, long * a2)
{
    OnWwiseCall("AK::SoundEngine::UnloadBank");
    return AK_SoundEngine_UnloadBank_301_bridge(a0, a1, a2);
}

t_AK_SoundEngine_UnloadBank_302* AK_SoundEngine_UnloadBank_302_orig = nullptr;
t_AK_SoundEngine_UnloadBank_302* AK_SoundEngine_UnloadBank_302_bridge = nullptr;
enum AKRESULT AK_SoundEngine_UnloadBank_302_hook(char const * a0, void const * a1, void (__cdecl* a2)(unsigned long,void const *,enum AKRESULT,long,void *), void * a3)
{
    OnWwiseCall("AK::SoundEngine::UnloadBank");
    return AK_SoundEngine_UnloadBank_302_bridge(a0, a1, a2, a3);
}

t_AK_SoundEngine_UnloadBank_303* AK_SoundEngine_UnloadBank_303_orig = nullptr;
t_AK_SoundEngine_UnloadBank_303* AK_SoundEngine_UnloadBank_303_bridge = nullptr;
enum AKRESULT AK_SoundEngine_UnloadBank_303_hook(char const * a0, void const * a1, long * a2)
{
    OnWwiseCall("AK::SoundEngine::UnloadBank");
    return AK_SoundEngine_UnloadBank_303_bridge(a0, a1, a2);
}

t_AK_SoundEngine_UnloadBank_304* AK_SoundEngine_UnloadBank_304_orig = nullptr;
t_AK_SoundEngine_UnloadBank_304* AK_SoundEngine_UnloadBank_304_bridge = nullptr;
enum AKRESULT AK_SoundEngine_UnloadBank_304_hook(wchar_t const * a0, void const * a1, void (__cdecl* a2)(unsigned long,void const *,enum AKRESULT,long,void *), void * a3)
{
    OnWwiseCall("AK::SoundEngine::UnloadBank");
    return AK_SoundEngine_UnloadBank_304_bridge(a0, a1, a2, a3);
}

t_AK_SoundEngine_UnloadBank_305* AK_SoundEngine_UnloadBank_305_orig = nullptr;
t_AK_SoundEngine_UnloadBank_305* AK_SoundEngine_UnloadBank_305_bridge = nullptr;
enum AKRESULT AK_SoundEngine_UnloadBank_305_hook(wchar_t const * a0, void const * a1, long * a2)
{
    OnWwiseCall("AK::SoundEngine::UnloadBank");
    return AK_SoundEngine_UnloadBank_305_bridge(a0, a1, a2);
}

t_AK_SoundEngine_UnregisterGameObj_312* AK_SoundEngine_UnregisterGameObj_312_orig = nullptr;
t_AK_SoundEngine_UnregisterGameObj_312* AK_SoundEngine_UnregisterGameObj_312_bridge = nullptr;
enum AKRESULT AK_SoundEngine_UnregisterGameObj_312_hook(unsigned __int64 a0)
{
    OnWwiseCall("AK::SoundEngine::UnregisterGameObj");
    LEASI_TRACE("  -> gameObj={}", a0);
    return AK_SoundEngine_UnregisterGameObj_312_bridge(a0);
}

t_AK_SoundEngine_SetSwitch_281* AK_SoundEngine_SetSwitch_281_orig = nullptr;
t_AK_SoundEngine_SetSwitch_281* AK_SoundEngine_SetSwitch_281_bridge = nullptr;
enum AKRESULT AK_SoundEngine_SetSwitch_281_hook(unsigned long a0, unsigned long a1, unsigned __int64 a2)
{
    OnWwiseCall("AK::SoundEngine::SetSwitch");
    return AK_SoundEngine_SetSwitch_281_bridge(a0, a1, a2);
}

t_AK_SoundEngine_SetSwitch_282* AK_SoundEngine_SetSwitch_282_orig = nullptr;
t_AK_SoundEngine_SetSwitch_282* AK_SoundEngine_SetSwitch_282_bridge = nullptr;
enum AKRESULT AK_SoundEngine_SetSwitch_282_hook(char const * a0, char const * a1, unsigned __int64 a2)
{
    OnWwiseCall("AK::SoundEngine::SetSwitch");
    return AK_SoundEngine_SetSwitch_282_bridge(a0, a1, a2);
}

t_AK_SoundEngine_SetSwitch_283* AK_SoundEngine_SetSwitch_283_orig = nullptr;
t_AK_SoundEngine_SetSwitch_283* AK_SoundEngine_SetSwitch_283_bridge = nullptr;
enum AKRESULT AK_SoundEngine_SetSwitch_283_hook(wchar_t const * a0, wchar_t const * a1, unsigned __int64 a2)
{
    OnWwiseCall("AK::SoundEngine::SetSwitch");
    return AK_SoundEngine_SetSwitch_283_bridge(a0, a1, a2);
}

t_AK_SoundEngine_SetState_278* AK_SoundEngine_SetState_278_orig = nullptr;
t_AK_SoundEngine_SetState_278* AK_SoundEngine_SetState_278_bridge = nullptr;
enum AKRESULT AK_SoundEngine_SetState_278_hook(unsigned long a0, unsigned long a1)
{
    OnWwiseCall("AK::SoundEngine::SetState");
    return AK_SoundEngine_SetState_278_bridge(a0, a1);
}

t_AK_SoundEngine_SetState_279* AK_SoundEngine_SetState_279_orig = nullptr;
t_AK_SoundEngine_SetState_279* AK_SoundEngine_SetState_279_bridge = nullptr;
enum AKRESULT AK_SoundEngine_SetState_279_hook(char const * a0, char const * a1)
{
    OnWwiseCall("AK::SoundEngine::SetState");
    return AK_SoundEngine_SetState_279_bridge(a0, a1);
}

t_AK_SoundEngine_SetState_280* AK_SoundEngine_SetState_280_orig = nullptr;
t_AK_SoundEngine_SetState_280* AK_SoundEngine_SetState_280_bridge = nullptr;
enum AKRESULT AK_SoundEngine_SetState_280_hook(wchar_t const * a0, wchar_t const * a1)
{
    OnWwiseCall("AK::SoundEngine::SetState");
    return AK_SoundEngine_SetState_280_bridge(a0, a1);
}

t_AK_SoundEngine_SetRTPCValue_269* AK_SoundEngine_SetRTPCValue_269_orig = nullptr;
t_AK_SoundEngine_SetRTPCValue_269* AK_SoundEngine_SetRTPCValue_269_bridge = nullptr;
enum AKRESULT AK_SoundEngine_SetRTPCValue_269_hook(unsigned long a0, float a1, unsigned __int64 a2, long a3, enum AkCurveInterpolation a4, bool a5)
{
    OnWwiseCall("AK::SoundEngine::SetRTPCValue");
    LEASI_TRACE("  -> rtpcID={} value={} gameObj={}", a0, a1, a2);
    return AK_SoundEngine_SetRTPCValue_269_bridge(a0, a1, a2, a3, a4, a5);
}

t_AK_SoundEngine_SetRTPCValue_270* AK_SoundEngine_SetRTPCValue_270_orig = nullptr;
t_AK_SoundEngine_SetRTPCValue_270* AK_SoundEngine_SetRTPCValue_270_bridge = nullptr;
enum AKRESULT AK_SoundEngine_SetRTPCValue_270_hook(char const * a0, float a1, unsigned __int64 a2, long a3, enum AkCurveInterpolation a4, bool a5)
{
    OnWwiseCall("AK::SoundEngine::SetRTPCValue");
    LEASI_TRACE("  -> rtpc={} value={} gameObj={}", a0, a1, a2);
    return AK_SoundEngine_SetRTPCValue_270_bridge(a0, a1, a2, a3, a4, a5);
}

t_AK_SoundEngine_SetRTPCValue_271* AK_SoundEngine_SetRTPCValue_271_orig = nullptr;
t_AK_SoundEngine_SetRTPCValue_271* AK_SoundEngine_SetRTPCValue_271_bridge = nullptr;
enum AKRESULT AK_SoundEngine_SetRTPCValue_271_hook(wchar_t const * a0, float a1, unsigned __int64 a2, long a3, enum AkCurveInterpolation a4, bool a5)
{
    OnWwiseCall("AK::SoundEngine::SetRTPCValue");
    LEASI_TRACE(L"  -> rtpc={} value={} gameObj={}", a0, a1, a2);
    return AK_SoundEngine_SetRTPCValue_271_bridge(a0, a1, a2, a3, a4, a5);
}

t_AK_SoundEngine_SetRTPCValueByPlayingID_272* AK_SoundEngine_SetRTPCValueByPlayingID_272_orig = nullptr;
t_AK_SoundEngine_SetRTPCValueByPlayingID_272* AK_SoundEngine_SetRTPCValueByPlayingID_272_bridge = nullptr;
enum AKRESULT AK_SoundEngine_SetRTPCValueByPlayingID_272_hook(unsigned long a0, float a1, unsigned long a2, long a3, enum AkCurveInterpolation a4, bool a5)
{
    OnWwiseCall("AK::SoundEngine::SetRTPCValueByPlayingID");
    LEASI_TRACE("  -> rtpcID={} value={} playingID={}", a0, a1, a2);
    return AK_SoundEngine_SetRTPCValueByPlayingID_272_bridge(a0, a1, a2, a3, a4, a5);
}

t_AK_SoundEngine_SetRTPCValueByPlayingID_273* AK_SoundEngine_SetRTPCValueByPlayingID_273_orig = nullptr;
t_AK_SoundEngine_SetRTPCValueByPlayingID_273* AK_SoundEngine_SetRTPCValueByPlayingID_273_bridge = nullptr;
enum AKRESULT AK_SoundEngine_SetRTPCValueByPlayingID_273_hook(char const * a0, float a1, unsigned long a2, long a3, enum AkCurveInterpolation a4, bool a5)
{
    OnWwiseCall("AK::SoundEngine::SetRTPCValueByPlayingID");
    LEASI_TRACE("  -> rtpc={} value={} playingID={}", a0, a1, a2);
    return AK_SoundEngine_SetRTPCValueByPlayingID_273_bridge(a0, a1, a2, a3, a4, a5);
}

t_AK_SoundEngine_SetRTPCValueByPlayingID_274* AK_SoundEngine_SetRTPCValueByPlayingID_274_orig = nullptr;
t_AK_SoundEngine_SetRTPCValueByPlayingID_274* AK_SoundEngine_SetRTPCValueByPlayingID_274_bridge = nullptr;
enum AKRESULT AK_SoundEngine_SetRTPCValueByPlayingID_274_hook(wchar_t const * a0, float a1, unsigned long a2, long a3, enum AkCurveInterpolation a4, bool a5)
{
    OnWwiseCall("AK::SoundEngine::SetRTPCValueByPlayingID");
    LEASI_TRACE(L"  -> rtpc={} value={} playingID={}", a0, a1, a2);
    return AK_SoundEngine_SetRTPCValueByPlayingID_274_bridge(a0, a1, a2, a3, a4, a5);
}

t_AK_SoundEngine_StopAll_289* AK_SoundEngine_StopAll_289_orig = nullptr;
t_AK_SoundEngine_StopAll_289* AK_SoundEngine_StopAll_289_bridge = nullptr;
void AK_SoundEngine_StopAll_289_hook(unsigned __int64 a0)
{
    OnWwiseCall("AK::SoundEngine::StopAll");
    return AK_SoundEngine_StopAll_289_bridge(a0);
}

t_AK_SoundEngine_Term_299* AK_SoundEngine_Term_299_orig = nullptr;
t_AK_SoundEngine_Term_299* AK_SoundEngine_Term_299_bridge = nullptr;
void AK_SoundEngine_Term_299_hook()
{
    OnWwiseCall("AK::SoundEngine::Term");
    return AK_SoundEngine_Term_299_bridge();
}

t_AK_MemoryMgr_Term_297* AK_MemoryMgr_Term_297_orig = nullptr;
t_AK_MemoryMgr_Term_297* AK_MemoryMgr_Term_297_bridge = nullptr;
void AK_MemoryMgr_Term_297_hook()
{
    OnWwiseCall("AK::MemoryMgr::Term");
    return AK_MemoryMgr_Term_297_bridge();
}

t_AK_SoundEngine_AddDefaultListener_14* AK_SoundEngine_AddDefaultListener_14_orig = nullptr;
t_AK_SoundEngine_AddDefaultListener_14* AK_SoundEngine_AddDefaultListener_14_bridge = nullptr;
enum AKRESULT AK_SoundEngine_AddDefaultListener_14_hook(unsigned __int64 a0)
{
    OnWwiseCall("AK::SoundEngine::AddDefaultListener");
    return AK_SoundEngine_AddDefaultListener_14_bridge(a0);
}

t_AK_StreamMgr_AddLanguageChangeObserver_15* AK_StreamMgr_AddLanguageChangeObserver_15_orig = nullptr;
t_AK_StreamMgr_AddLanguageChangeObserver_15* AK_StreamMgr_AddLanguageChangeObserver_15_bridge = nullptr;
enum AKRESULT AK_StreamMgr_AddLanguageChangeObserver_15_hook(void (__cdecl* a0)(wchar_t const * const,void *), void * a1)
{
    OnWwiseCall("AK::StreamMgr::AddLanguageChangeObserver");
    return AK_StreamMgr_AddLanguageChangeObserver_15_bridge(a0, a1);
}

t_AK_SoundEngine_AddListener_16* AK_SoundEngine_AddListener_16_orig = nullptr;
t_AK_SoundEngine_AddListener_16* AK_SoundEngine_AddListener_16_bridge = nullptr;
enum AKRESULT AK_SoundEngine_AddListener_16_hook(unsigned __int64 a0, unsigned __int64 a1)
{
    OnWwiseCall("AK::SoundEngine::AddListener");
    return AK_SoundEngine_AddListener_16_bridge(a0, a1);
}

t_AK_SoundEngine_AddOutput_17* AK_SoundEngine_AddOutput_17_orig = nullptr;
t_AK_SoundEngine_AddOutput_17* AK_SoundEngine_AddOutput_17_bridge = nullptr;
enum AKRESULT AK_SoundEngine_AddOutput_17_hook(struct AkOutputSettings const & a0, unsigned __int64 * a1, unsigned __int64 const * a2, unsigned long a3)
{
    OnWwiseCall("AK::SoundEngine::AddOutput");
    return AK_SoundEngine_AddOutput_17_bridge(a0, a1, a2, a3);
}

t_AK_SoundEngine_AddOutputCaptureMarker_18* AK_SoundEngine_AddOutputCaptureMarker_18_orig = nullptr;
t_AK_SoundEngine_AddOutputCaptureMarker_18* AK_SoundEngine_AddOutputCaptureMarker_18_bridge = nullptr;
enum AKRESULT AK_SoundEngine_AddOutputCaptureMarker_18_hook(char const * a0)
{
    OnWwiseCall("AK::SoundEngine::AddOutputCaptureMarker");
    return AK_SoundEngine_AddOutputCaptureMarker_18_bridge(a0);
}

t_AK_SoundEngine_DynamicSequence_Break_21* AK_SoundEngine_DynamicSequence_Break_21_orig = nullptr;
t_AK_SoundEngine_DynamicSequence_Break_21* AK_SoundEngine_DynamicSequence_Break_21_bridge = nullptr;
enum AKRESULT AK_SoundEngine_DynamicSequence_Break_21_hook(unsigned long a0)
{
    OnWwiseCall("AK::SoundEngine::DynamicSequence::Break");
    return AK_SoundEngine_DynamicSequence_Break_21_bridge(a0);
}

t_AK_WriteBytesMem_Bytes_23* AK_WriteBytesMem_Bytes_23_orig = nullptr;
t_AK_WriteBytesMem_Bytes_23* AK_WriteBytesMem_Bytes_23_bridge = nullptr;
unsigned char * AK_WriteBytesMem_Bytes_23_hook(AK::WriteBytesMem* a0)
{
    OnWwiseCall("AK::WriteBytesMem::Bytes");
    return AK_WriteBytesMem_Bytes_23_bridge(a0);
}

t_AK_SoundEngine_CancelBankCallbackCookie_24* AK_SoundEngine_CancelBankCallbackCookie_24_orig = nullptr;
t_AK_SoundEngine_CancelBankCallbackCookie_24* AK_SoundEngine_CancelBankCallbackCookie_24_bridge = nullptr;
void AK_SoundEngine_CancelBankCallbackCookie_24_hook(void * a0)
{
    OnWwiseCall("AK::SoundEngine::CancelBankCallbackCookie");
    return AK_SoundEngine_CancelBankCallbackCookie_24_bridge(a0);
}

t_AK_SoundEngine_CancelEventCallback_25* AK_SoundEngine_CancelEventCallback_25_orig = nullptr;
t_AK_SoundEngine_CancelEventCallback_25* AK_SoundEngine_CancelEventCallback_25_bridge = nullptr;
void AK_SoundEngine_CancelEventCallback_25_hook(unsigned long a0)
{
    OnWwiseCall("AK::SoundEngine::CancelEventCallback");
    return AK_SoundEngine_CancelEventCallback_25_bridge(a0);
}

t_AK_SoundEngine_CancelEventCallbackCookie_26* AK_SoundEngine_CancelEventCallbackCookie_26_orig = nullptr;
t_AK_SoundEngine_CancelEventCallbackCookie_26* AK_SoundEngine_CancelEventCallbackCookie_26_bridge = nullptr;
void AK_SoundEngine_CancelEventCallbackCookie_26_hook(void * a0)
{
    OnWwiseCall("AK::SoundEngine::CancelEventCallbackCookie");
    return AK_SoundEngine_CancelEventCallbackCookie_26_bridge(a0);
}

t_AK_SoundEngine_CancelEventCallbackGameObject_27* AK_SoundEngine_CancelEventCallbackGameObject_27_orig = nullptr;
t_AK_SoundEngine_CancelEventCallbackGameObject_27* AK_SoundEngine_CancelEventCallbackGameObject_27_bridge = nullptr;
void AK_SoundEngine_CancelEventCallbackGameObject_27_hook(unsigned __int64 a0)
{
    OnWwiseCall("AK::SoundEngine::CancelEventCallbackGameObject");
    return AK_SoundEngine_CancelEventCallbackGameObject_27_bridge(a0);
}

t_AK_MemoryMgr_CheckPoolId_28* AK_MemoryMgr_CheckPoolId_28_orig = nullptr;
t_AK_MemoryMgr_CheckPoolId_28* AK_MemoryMgr_CheckPoolId_28_bridge = nullptr;
enum AKRESULT AK_MemoryMgr_CheckPoolId_28_hook(long a0)
{
    OnWwiseCall("AK::MemoryMgr::CheckPoolId");
    return AK_MemoryMgr_CheckPoolId_28_bridge(a0);
}

t_AK_WriteBytesCount_Clear_29* AK_WriteBytesCount_Clear_29_orig = nullptr;
t_AK_WriteBytesCount_Clear_29* AK_WriteBytesCount_Clear_29_bridge = nullptr;
void AK_WriteBytesCount_Clear_29_hook(AK::WriteBytesCount* a0)
{
    OnWwiseCall("AK::WriteBytesCount::Clear");
    return AK_WriteBytesCount_Clear_29_bridge(a0);
}

t_AK_WriteBytesMem_Clear_30* AK_WriteBytesMem_Clear_30_orig = nullptr;
t_AK_WriteBytesMem_Clear_30* AK_WriteBytesMem_Clear_30_bridge = nullptr;
void AK_WriteBytesMem_Clear_30_hook(AK::WriteBytesMem* a0)
{
    OnWwiseCall("AK::WriteBytesMem::Clear");
    return AK_WriteBytesMem_Clear_30_bridge(a0);
}

t_AK_SoundEngine_ClearBanks_31* AK_SoundEngine_ClearBanks_31_orig = nullptr;
t_AK_SoundEngine_ClearBanks_31* AK_SoundEngine_ClearBanks_31_bridge = nullptr;
enum AKRESULT AK_SoundEngine_ClearBanks_31_hook()
{
    OnWwiseCall("AK::SoundEngine::ClearBanks");
    return AK_SoundEngine_ClearBanks_31_bridge();
}

t_AK_SoundEngine_ClearPreparedEvents_32* AK_SoundEngine_ClearPreparedEvents_32_orig = nullptr;
t_AK_SoundEngine_ClearPreparedEvents_32* AK_SoundEngine_ClearPreparedEvents_32_bridge = nullptr;
enum AKRESULT AK_SoundEngine_ClearPreparedEvents_32_hook()
{
    OnWwiseCall("AK::SoundEngine::ClearPreparedEvents");
    return AK_SoundEngine_ClearPreparedEvents_32_bridge();
}

t_AK_SoundEngine_DynamicSequence_Close_33* AK_SoundEngine_DynamicSequence_Close_33_orig = nullptr;
t_AK_SoundEngine_DynamicSequence_Close_33* AK_SoundEngine_DynamicSequence_Close_33_bridge = nullptr;
enum AKRESULT AK_SoundEngine_DynamicSequence_Close_33_hook(unsigned long a0)
{
    OnWwiseCall("AK::SoundEngine::DynamicSequence::Close");
    return AK_SoundEngine_DynamicSequence_Close_33_bridge(a0);
}

t_AK_WriteBytesCount_Count_35* AK_WriteBytesCount_Count_35_orig = nullptr;
t_AK_WriteBytesCount_Count_35* AK_WriteBytesCount_Count_35_bridge = nullptr;
long AK_WriteBytesCount_Count_35_hook(AK::WriteBytesCount* a0)
{
    OnWwiseCall("AK::WriteBytesCount::Count");
    return AK_WriteBytesCount_Count_35_bridge(a0);
}

t_AK_MemoryMgr_CreatePool_39* AK_MemoryMgr_CreatePool_39_orig = nullptr;
t_AK_MemoryMgr_CreatePool_39* AK_MemoryMgr_CreatePool_39_bridge = nullptr;
long AK_MemoryMgr_CreatePool_39_hook(void * a0, unsigned long a1, unsigned long a2, unsigned long a3, unsigned long a4)
{
    OnWwiseCall("AK::MemoryMgr::CreatePool");
    return AK_MemoryMgr_CreatePool_39_bridge(a0, a1, a2, a3, a4);
}

t_AK_SoundEngine_DecodeBank_40* AK_SoundEngine_DecodeBank_40_orig = nullptr;
t_AK_SoundEngine_DecodeBank_40* AK_SoundEngine_DecodeBank_40_bridge = nullptr;
enum AKRESULT AK_SoundEngine_DecodeBank_40_hook(void const * a0, unsigned long a1, long a2, void * & a3, unsigned long & a4)
{
    OnWwiseCall("AK::SoundEngine::DecodeBank");
    return AK_SoundEngine_DecodeBank_40_bridge(a0, a1, a2, a3, a4);
}

t_AK_StreamMgr_DestroyDevice_41* AK_StreamMgr_DestroyDevice_41_orig = nullptr;
t_AK_StreamMgr_DestroyDevice_41* AK_StreamMgr_DestroyDevice_41_bridge = nullptr;
enum AKRESULT AK_StreamMgr_DestroyDevice_41_hook(unsigned long a0)
{
    OnWwiseCall("AK::StreamMgr::DestroyDevice");
    return AK_StreamMgr_DestroyDevice_41_bridge(a0);
}

t_AK_MemoryMgr_DestroyPool_42* AK_MemoryMgr_DestroyPool_42_orig = nullptr;
t_AK_MemoryMgr_DestroyPool_42* AK_MemoryMgr_DestroyPool_42_bridge = nullptr;
enum AKRESULT AK_MemoryMgr_DestroyPool_42_hook(long a0)
{
    OnWwiseCall("AK::MemoryMgr::DestroyPool");
    return AK_MemoryMgr_DestroyPool_42_bridge(a0);
}

t_AK_WriteBytesMem_Detach_44* AK_WriteBytesMem_Detach_44_orig = nullptr;
t_AK_WriteBytesMem_Detach_44* AK_WriteBytesMem_Detach_44_bridge = nullptr;
unsigned char * AK_WriteBytesMem_Detach_44_hook(AK::WriteBytesMem* a0)
{
    OnWwiseCall("AK::WriteBytesMem::Detach");
    return AK_WriteBytesMem_Detach_44_bridge(a0);
}

t_AK_SoundEngine_ExecuteActionOnEvent_45* AK_SoundEngine_ExecuteActionOnEvent_45_orig = nullptr;
t_AK_SoundEngine_ExecuteActionOnEvent_45* AK_SoundEngine_ExecuteActionOnEvent_45_bridge = nullptr;
enum AKRESULT AK_SoundEngine_ExecuteActionOnEvent_45_hook(unsigned long a0, enum AK::SoundEngine::AkActionOnEventType a1, unsigned __int64 a2, long a3, enum AkCurveInterpolation a4, unsigned long a5)
{
    OnWwiseCall("AK::SoundEngine::ExecuteActionOnEvent");
    return AK_SoundEngine_ExecuteActionOnEvent_45_bridge(a0, a1, a2, a3, a4, a5);
}

t_AK_SoundEngine_ExecuteActionOnEvent_46* AK_SoundEngine_ExecuteActionOnEvent_46_orig = nullptr;
t_AK_SoundEngine_ExecuteActionOnEvent_46* AK_SoundEngine_ExecuteActionOnEvent_46_bridge = nullptr;
enum AKRESULT AK_SoundEngine_ExecuteActionOnEvent_46_hook(char const * a0, enum AK::SoundEngine::AkActionOnEventType a1, unsigned __int64 a2, long a3, enum AkCurveInterpolation a4, unsigned long a5)
{
    OnWwiseCall("AK::SoundEngine::ExecuteActionOnEvent");
    return AK_SoundEngine_ExecuteActionOnEvent_46_bridge(a0, a1, a2, a3, a4, a5);
}

t_AK_SoundEngine_ExecuteActionOnEvent_47* AK_SoundEngine_ExecuteActionOnEvent_47_orig = nullptr;
t_AK_SoundEngine_ExecuteActionOnEvent_47* AK_SoundEngine_ExecuteActionOnEvent_47_bridge = nullptr;
enum AKRESULT AK_SoundEngine_ExecuteActionOnEvent_47_hook(wchar_t const * a0, enum AK::SoundEngine::AkActionOnEventType a1, unsigned __int64 a2, long a3, enum AkCurveInterpolation a4, unsigned long a5)
{
    OnWwiseCall("AK::SoundEngine::ExecuteActionOnEvent");
    return AK_SoundEngine_ExecuteActionOnEvent_47_bridge(a0, a1, a2, a3, a4, a5);
}

t_AK_MemoryMgr_Falign_49* AK_MemoryMgr_Falign_49_orig = nullptr;
t_AK_MemoryMgr_Falign_49* AK_MemoryMgr_Falign_49_bridge = nullptr;
enum AKRESULT AK_MemoryMgr_Falign_49_hook(long a0, void * a1)
{
    OnWwiseCall("AK::MemoryMgr::Falign");
    return AK_MemoryMgr_Falign_49_bridge(a0, a1);
}

t_AK_StreamMgr_FlushAllCaches_50* AK_StreamMgr_FlushAllCaches_50_orig = nullptr;
t_AK_StreamMgr_FlushAllCaches_50* AK_StreamMgr_FlushAllCaches_50_bridge = nullptr;
void AK_StreamMgr_FlushAllCaches_50_hook()
{
    OnWwiseCall("AK::StreamMgr::FlushAllCaches");
    return AK_StreamMgr_FlushAllCaches_50_bridge();
}

t_AK_MemoryMgr_Free_51* AK_MemoryMgr_Free_51_orig = nullptr;
t_AK_MemoryMgr_Free_51* AK_MemoryMgr_Free_51_bridge = nullptr;
enum AKRESULT AK_MemoryMgr_Free_51_hook(long a0, void * a1)
{
    OnWwiseCall("AK::MemoryMgr::Free");
    return AK_MemoryMgr_Free_51_bridge(a0, a1);
}

t_AK_SoundEngine_Query_GetActiveGameObjects_52* AK_SoundEngine_Query_GetActiveGameObjects_52_orig = nullptr;
t_AK_SoundEngine_Query_GetActiveGameObjects_52* AK_SoundEngine_Query_GetActiveGameObjects_52_bridge = nullptr;
enum AKRESULT AK_SoundEngine_Query_GetActiveGameObjects_52_hook(class AkArray<unsigned __int64,unsigned __int64,struct AkArrayAllocatorNoAlign<struct _ArrayPoolDefault>,32,struct AkAssignmentMovePolicy<unsigned __int64> > & a0)
{
    OnWwiseCall("AK::SoundEngine::Query::GetActiveGameObjects");
    return AK_SoundEngine_Query_GetActiveGameObjects_52_bridge(a0);
}

t_AK_SoundEngine_GetAudioSettings_53* AK_SoundEngine_GetAudioSettings_53_orig = nullptr;
t_AK_SoundEngine_GetAudioSettings_53* AK_SoundEngine_GetAudioSettings_53_bridge = nullptr;
enum AKRESULT AK_SoundEngine_GetAudioSettings_53_hook(struct AkAudioSettings & a0)
{
    OnWwiseCall("AK::SoundEngine::GetAudioSettings");
    return AK_SoundEngine_GetAudioSettings_53_bridge(a0);
}

t_AK_SoundEngine_GetBackgroundMusicMute_54* AK_SoundEngine_GetBackgroundMusicMute_54_orig = nullptr;
t_AK_SoundEngine_GetBackgroundMusicMute_54* AK_SoundEngine_GetBackgroundMusicMute_54_bridge = nullptr;
bool AK_SoundEngine_GetBackgroundMusicMute_54_hook()
{
    OnWwiseCall("AK::SoundEngine::GetBackgroundMusicMute");
    return AK_SoundEngine_GetBackgroundMusicMute_54_bridge();
}

t_AK_MemoryMgr_GetBlock_55* AK_MemoryMgr_GetBlock_55_orig = nullptr;
t_AK_MemoryMgr_GetBlock_55* AK_MemoryMgr_GetBlock_55_bridge = nullptr;
void * AK_MemoryMgr_GetBlock_55_hook(long a0)
{
    OnWwiseCall("AK::MemoryMgr::GetBlock");
    return AK_MemoryMgr_GetBlock_55_bridge(a0);
}

t_AK_MemoryMgr_GetBlockSize_56* AK_MemoryMgr_GetBlockSize_56_orig = nullptr;
t_AK_MemoryMgr_GetBlockSize_56* AK_MemoryMgr_GetBlockSize_56_bridge = nullptr;
unsigned long AK_MemoryMgr_GetBlockSize_56_hook(long a0)
{
    OnWwiseCall("AK::MemoryMgr::GetBlockSize");
    return AK_MemoryMgr_GetBlockSize_56_bridge(a0);
}

t_AK_SoundEngine_GetBufferStatusForPinnedEvent_57* AK_SoundEngine_GetBufferStatusForPinnedEvent_57_orig = nullptr;
t_AK_SoundEngine_GetBufferStatusForPinnedEvent_57* AK_SoundEngine_GetBufferStatusForPinnedEvent_57_bridge = nullptr;
enum AKRESULT AK_SoundEngine_GetBufferStatusForPinnedEvent_57_hook(unsigned long a0, float & a1, bool & a2)
{
    OnWwiseCall("AK::SoundEngine::GetBufferStatusForPinnedEvent");
    return AK_SoundEngine_GetBufferStatusForPinnedEvent_57_bridge(a0, a1, a2);
}

t_AK_SoundEngine_GetBufferStatusForPinnedEvent_58* AK_SoundEngine_GetBufferStatusForPinnedEvent_58_orig = nullptr;
t_AK_SoundEngine_GetBufferStatusForPinnedEvent_58* AK_SoundEngine_GetBufferStatusForPinnedEvent_58_bridge = nullptr;
enum AKRESULT AK_SoundEngine_GetBufferStatusForPinnedEvent_58_hook(char const * a0, float & a1, bool & a2)
{
    OnWwiseCall("AK::SoundEngine::GetBufferStatusForPinnedEvent");
    return AK_SoundEngine_GetBufferStatusForPinnedEvent_58_bridge(a0, a1, a2);
}

t_AK_SoundEngine_GetBufferStatusForPinnedEvent_59* AK_SoundEngine_GetBufferStatusForPinnedEvent_59_orig = nullptr;
t_AK_SoundEngine_GetBufferStatusForPinnedEvent_59* AK_SoundEngine_GetBufferStatusForPinnedEvent_59_bridge = nullptr;
enum AKRESULT AK_SoundEngine_GetBufferStatusForPinnedEvent_59_hook(wchar_t const * a0, float & a1, bool & a2)
{
    OnWwiseCall("AK::SoundEngine::GetBufferStatusForPinnedEvent");
    return AK_SoundEngine_GetBufferStatusForPinnedEvent_59_bridge(a0, a1, a2);
}

t_AK_SoundEngine_GetBufferTick_60* AK_SoundEngine_GetBufferTick_60_orig = nullptr;
t_AK_SoundEngine_GetBufferTick_60* AK_SoundEngine_GetBufferTick_60_bridge = nullptr;
unsigned long AK_SoundEngine_GetBufferTick_60_hook()
{
    OnWwiseCall("AK::SoundEngine::GetBufferTick");
    return AK_SoundEngine_GetBufferTick_60_bridge();
}

t_AK_SoundEngine_GetContainerHistory_61* AK_SoundEngine_GetContainerHistory_61_orig = nullptr;
t_AK_SoundEngine_GetContainerHistory_61* AK_SoundEngine_GetContainerHistory_61_bridge = nullptr;
enum AKRESULT AK_SoundEngine_GetContainerHistory_61_hook(class AK::IWriteBytes * a0)
{
    OnWwiseCall("AK::SoundEngine::GetContainerHistory");
    return AK_SoundEngine_GetContainerHistory_61_bridge(a0);
}

t_AK_StreamMgr_GetCurrentLanguage_62* AK_StreamMgr_GetCurrentLanguage_62_orig = nullptr;
t_AK_StreamMgr_GetCurrentLanguage_62* AK_StreamMgr_GetCurrentLanguage_62_bridge = nullptr;
wchar_t const * AK_StreamMgr_GetCurrentLanguage_62_hook()
{
    OnWwiseCall("AK::StreamMgr::GetCurrentLanguage");
    return AK_StreamMgr_GetCurrentLanguage_62_bridge();
}

t_AK_StreamMgr_GetDefaultDeviceSettings_65* AK_StreamMgr_GetDefaultDeviceSettings_65_orig = nullptr;
t_AK_StreamMgr_GetDefaultDeviceSettings_65* AK_StreamMgr_GetDefaultDeviceSettings_65_bridge = nullptr;
void AK_StreamMgr_GetDefaultDeviceSettings_65_hook(struct AkDeviceSettings & a0)
{
    OnWwiseCall("AK::StreamMgr::GetDefaultDeviceSettings");
    return AK_StreamMgr_GetDefaultDeviceSettings_65_bridge(a0);
}

t_AK_MusicEngine_GetDefaultInitSettings_66* AK_MusicEngine_GetDefaultInitSettings_66_orig = nullptr;
t_AK_MusicEngine_GetDefaultInitSettings_66* AK_MusicEngine_GetDefaultInitSettings_66_bridge = nullptr;
void AK_MusicEngine_GetDefaultInitSettings_66_hook(struct AkMusicSettings & a0)
{
    OnWwiseCall("AK::MusicEngine::GetDefaultInitSettings");
    return AK_MusicEngine_GetDefaultInitSettings_66_bridge(a0);
}

t_AK_StreamMgr_GetDefaultSettings_69* AK_StreamMgr_GetDefaultSettings_69_orig = nullptr;
t_AK_StreamMgr_GetDefaultSettings_69* AK_StreamMgr_GetDefaultSettings_69_bridge = nullptr;
void AK_StreamMgr_GetDefaultSettings_69_hook(struct AkStreamMgrSettings & a0)
{
    OnWwiseCall("AK::StreamMgr::GetDefaultSettings");
    return AK_StreamMgr_GetDefaultSettings_69_bridge(a0);
}

t_AK_GetDeviceID_70* AK_GetDeviceID_70_orig = nullptr;
t_AK_GetDeviceID_70* AK_GetDeviceID_70_bridge = nullptr;
unsigned long AK_GetDeviceID_70_hook(struct IMMDevice * a0)
{
    OnWwiseCall("AK::GetDeviceID");
    return AK_GetDeviceID_70_bridge(a0);
}

t_AK_GetDeviceIDFromName_71* AK_GetDeviceIDFromName_71_orig = nullptr;
t_AK_GetDeviceIDFromName_71* AK_GetDeviceIDFromName_71_bridge = nullptr;
unsigned long AK_GetDeviceIDFromName_71_hook(wchar_t * a0)
{
    OnWwiseCall("AK::GetDeviceIDFromName");
    return AK_GetDeviceIDFromName_71_bridge(a0);
}

t_AK_SoundEngine_GetDeviceSpatialAudioSupport_72* AK_SoundEngine_GetDeviceSpatialAudioSupport_72_orig = nullptr;
t_AK_SoundEngine_GetDeviceSpatialAudioSupport_72* AK_SoundEngine_GetDeviceSpatialAudioSupport_72_bridge = nullptr;
enum AKRESULT AK_SoundEngine_GetDeviceSpatialAudioSupport_72_hook(unsigned long a0)
{
    OnWwiseCall("AK::SoundEngine::GetDeviceSpatialAudioSupport");
    return AK_SoundEngine_GetDeviceSpatialAudioSupport_72_bridge(a0);
}

t_AK_SoundEngine_DynamicDialogue_GetDialogueEventCustomPropertyValue_73* AK_SoundEngine_DynamicDialogue_GetDialogueEventCustomPropertyValue_73_orig = nullptr;
t_AK_SoundEngine_DynamicDialogue_GetDialogueEventCustomPropertyValue_73* AK_SoundEngine_DynamicDialogue_GetDialogueEventCustomPropertyValue_73_bridge = nullptr;
enum AKRESULT AK_SoundEngine_DynamicDialogue_GetDialogueEventCustomPropertyValue_73_hook(unsigned long a0, unsigned long a1, long & a2)
{
    OnWwiseCall("AK::SoundEngine::DynamicDialogue::GetDialogueEventCustomPropertyValue");
    return AK_SoundEngine_DynamicDialogue_GetDialogueEventCustomPropertyValue_73_bridge(a0, a1, a2);
}

t_AK_SoundEngine_DynamicDialogue_GetDialogueEventCustomPropertyValue_74* AK_SoundEngine_DynamicDialogue_GetDialogueEventCustomPropertyValue_74_orig = nullptr;
t_AK_SoundEngine_DynamicDialogue_GetDialogueEventCustomPropertyValue_74* AK_SoundEngine_DynamicDialogue_GetDialogueEventCustomPropertyValue_74_bridge = nullptr;
enum AKRESULT AK_SoundEngine_DynamicDialogue_GetDialogueEventCustomPropertyValue_74_hook(unsigned long a0, unsigned long a1, float & a2)
{
    OnWwiseCall("AK::SoundEngine::DynamicDialogue::GetDialogueEventCustomPropertyValue");
    return AK_SoundEngine_DynamicDialogue_GetDialogueEventCustomPropertyValue_74_bridge(a0, a1, a2);
}

t_AK_GetDirectSoundInstance_75* AK_GetDirectSoundInstance_75_orig = nullptr;
t_AK_GetDirectSoundInstance_75* AK_GetDirectSoundInstance_75_bridge = nullptr;
struct IDirectSound8 * AK_GetDirectSoundInstance_75_hook()
{
    OnWwiseCall("AK::GetDirectSoundInstance");
    return AK_GetDirectSoundInstance_75_bridge();
}

t_AK_SoundEngine_Query_GetEventIDFromPlayingID_76* AK_SoundEngine_Query_GetEventIDFromPlayingID_76_orig = nullptr;
t_AK_SoundEngine_Query_GetEventIDFromPlayingID_76* AK_SoundEngine_Query_GetEventIDFromPlayingID_76_bridge = nullptr;
unsigned long AK_SoundEngine_Query_GetEventIDFromPlayingID_76_hook(unsigned long a0)
{
    OnWwiseCall("AK::SoundEngine::Query::GetEventIDFromPlayingID");
    return AK_SoundEngine_Query_GetEventIDFromPlayingID_76_bridge(a0);
}

t_AK_StreamMgr_GetFileLocationResolver_77* AK_StreamMgr_GetFileLocationResolver_77_orig = nullptr;
t_AK_StreamMgr_GetFileLocationResolver_77* AK_StreamMgr_GetFileLocationResolver_77_bridge = nullptr;
class AK::StreamMgr::IAkFileLocationResolver * AK_StreamMgr_GetFileLocationResolver_77_hook()
{
    OnWwiseCall("AK::StreamMgr::GetFileLocationResolver");
    return AK_StreamMgr_GetFileLocationResolver_77_bridge();
}

t_AK_SoundEngine_Query_GetGameObjectAuxSendValues_78* AK_SoundEngine_Query_GetGameObjectAuxSendValues_78_orig = nullptr;
t_AK_SoundEngine_Query_GetGameObjectAuxSendValues_78* AK_SoundEngine_Query_GetGameObjectAuxSendValues_78_bridge = nullptr;
enum AKRESULT AK_SoundEngine_Query_GetGameObjectAuxSendValues_78_hook(unsigned __int64 a0, struct AkAuxSendValue * a1, unsigned long & a2)
{
    OnWwiseCall("AK::SoundEngine::Query::GetGameObjectAuxSendValues");
    return AK_SoundEngine_Query_GetGameObjectAuxSendValues_78_bridge(a0, a1, a2);
}

t_AK_SoundEngine_Query_GetGameObjectDryLevelValue_79* AK_SoundEngine_Query_GetGameObjectDryLevelValue_79_orig = nullptr;
t_AK_SoundEngine_Query_GetGameObjectDryLevelValue_79* AK_SoundEngine_Query_GetGameObjectDryLevelValue_79_bridge = nullptr;
enum AKRESULT AK_SoundEngine_Query_GetGameObjectDryLevelValue_79_hook(unsigned __int64 a0, unsigned __int64 a1, float & a2)
{
    OnWwiseCall("AK::SoundEngine::Query::GetGameObjectDryLevelValue");
    return AK_SoundEngine_Query_GetGameObjectDryLevelValue_79_bridge(a0, a1, a2);
}

t_AK_SoundEngine_Query_GetGameObjectFromPlayingID_80* AK_SoundEngine_Query_GetGameObjectFromPlayingID_80_orig = nullptr;
t_AK_SoundEngine_Query_GetGameObjectFromPlayingID_80* AK_SoundEngine_Query_GetGameObjectFromPlayingID_80_bridge = nullptr;
unsigned __int64 AK_SoundEngine_Query_GetGameObjectFromPlayingID_80_hook(unsigned long a0)
{
    OnWwiseCall("AK::SoundEngine::Query::GetGameObjectFromPlayingID");
    return AK_SoundEngine_Query_GetGameObjectFromPlayingID_80_bridge(a0);
}

t_AK_SoundEngine_GetGlobalPluginContext_81* AK_SoundEngine_GetGlobalPluginContext_81_orig = nullptr;
t_AK_SoundEngine_GetGlobalPluginContext_81* AK_SoundEngine_GetGlobalPluginContext_81_bridge = nullptr;
class AK::IAkGlobalPluginContext * AK_SoundEngine_GetGlobalPluginContext_81_hook()
{
    OnWwiseCall("AK::SoundEngine::GetGlobalPluginContext");
    return AK_SoundEngine_GetGlobalPluginContext_81_bridge();
}

t_AK_SoundEngine_GetIDFromString_82* AK_SoundEngine_GetIDFromString_82_orig = nullptr;
t_AK_SoundEngine_GetIDFromString_82* AK_SoundEngine_GetIDFromString_82_bridge = nullptr;
unsigned long AK_SoundEngine_GetIDFromString_82_hook(char const * a0)
{
    OnWwiseCall("AK::SoundEngine::GetIDFromString");
    LEASI_TRACE("  -> hashing: {}", a0);
    return AK_SoundEngine_GetIDFromString_82_bridge(a0);
}

t_AK_SoundEngine_GetIDFromString_83* AK_SoundEngine_GetIDFromString_83_orig = nullptr;
t_AK_SoundEngine_GetIDFromString_83* AK_SoundEngine_GetIDFromString_83_bridge = nullptr;
unsigned long AK_SoundEngine_GetIDFromString_83_hook(wchar_t const * a0)
{
    OnWwiseCall("AK::SoundEngine::GetIDFromString");
    LEASI_TRACE(L"  -> hashing: {}", a0);
    return AK_SoundEngine_GetIDFromString_83_bridge(a0);
}

t_AK_SoundEngine_Query_GetIsGameObjectActive_84* AK_SoundEngine_Query_GetIsGameObjectActive_84_orig = nullptr;
t_AK_SoundEngine_Query_GetIsGameObjectActive_84* AK_SoundEngine_Query_GetIsGameObjectActive_84_bridge = nullptr;
bool AK_SoundEngine_Query_GetIsGameObjectActive_84_hook(unsigned __int64 a0)
{
    OnWwiseCall("AK::SoundEngine::Query::GetIsGameObjectActive");
    return AK_SoundEngine_Query_GetIsGameObjectActive_84_bridge(a0);
}

t_AK_SoundEngine_Query_GetListenerPosition_85* AK_SoundEngine_Query_GetListenerPosition_85_orig = nullptr;
t_AK_SoundEngine_Query_GetListenerPosition_85* AK_SoundEngine_Query_GetListenerPosition_85_bridge = nullptr;
enum AKRESULT AK_SoundEngine_Query_GetListenerPosition_85_hook(unsigned __int64 a0, class AkTransform & a1)
{
    OnWwiseCall("AK::SoundEngine::Query::GetListenerPosition");
    return AK_SoundEngine_Query_GetListenerPosition_85_bridge(a0, a1);
}

t_AK_SoundEngine_Query_GetListenerSpatialization_86* AK_SoundEngine_Query_GetListenerSpatialization_86_orig = nullptr;
t_AK_SoundEngine_Query_GetListenerSpatialization_86* AK_SoundEngine_Query_GetListenerSpatialization_86_bridge = nullptr;
enum AKRESULT AK_SoundEngine_Query_GetListenerSpatialization_86_hook(unsigned long a0, bool & a1, float * & a2, struct AkChannelConfig & a3)
{
    OnWwiseCall("AK::SoundEngine::Query::GetListenerSpatialization");
    return AK_SoundEngine_Query_GetListenerSpatialization_86_bridge(a0, a1, a2, a3);
}

t_AK_SoundEngine_Query_GetListeners_87* AK_SoundEngine_Query_GetListeners_87_orig = nullptr;
t_AK_SoundEngine_Query_GetListeners_87* AK_SoundEngine_Query_GetListeners_87_bridge = nullptr;
enum AKRESULT AK_SoundEngine_Query_GetListeners_87_hook(unsigned __int64 a0, unsigned __int64 * a1, unsigned long & a2)
{
    OnWwiseCall("AK::SoundEngine::Query::GetListeners");
    return AK_SoundEngine_Query_GetListeners_87_bridge(a0, a1, a2);
}

t_AK_MemoryMgr_GetMaxPools_88* AK_MemoryMgr_GetMaxPools_88_orig = nullptr;
t_AK_MemoryMgr_GetMaxPools_88* AK_MemoryMgr_GetMaxPools_88_bridge = nullptr;
long AK_MemoryMgr_GetMaxPools_88_hook()
{
    OnWwiseCall("AK::MemoryMgr::GetMaxPools");
    return AK_MemoryMgr_GetMaxPools_88_bridge();
}

t_AK_SoundEngine_Query_GetMaxRadius_89* AK_SoundEngine_Query_GetMaxRadius_89_orig = nullptr;
t_AK_SoundEngine_Query_GetMaxRadius_89* AK_SoundEngine_Query_GetMaxRadius_89_bridge = nullptr;
enum AKRESULT AK_SoundEngine_Query_GetMaxRadius_89_hook(class AkArray<struct AK::SoundEngine::Query::GameObjDst,struct AK::SoundEngine::Query::GameObjDst const &,struct AkArrayAllocatorNoAlign<struct _ArrayPoolDefault>,32,struct AkAssignmentMovePolicy<struct AK::SoundEngine::Query::GameObjDst> > & a0)
{
    OnWwiseCall("AK::SoundEngine::Query::GetMaxRadius");
    return AK_SoundEngine_Query_GetMaxRadius_89_bridge(a0);
}

t_AK_SoundEngine_Query_GetMaxRadius_90* AK_SoundEngine_Query_GetMaxRadius_90_orig = nullptr;
t_AK_SoundEngine_Query_GetMaxRadius_90* AK_SoundEngine_Query_GetMaxRadius_90_bridge = nullptr;
float AK_SoundEngine_Query_GetMaxRadius_90_hook(unsigned __int64 a0)
{
    OnWwiseCall("AK::SoundEngine::Query::GetMaxRadius");
    return AK_SoundEngine_Query_GetMaxRadius_90_bridge(a0);
}

t_AK_MemoryMgr_GetNumPools_91* AK_MemoryMgr_GetNumPools_91_orig = nullptr;
t_AK_MemoryMgr_GetNumPools_91* AK_MemoryMgr_GetNumPools_91_bridge = nullptr;
long AK_MemoryMgr_GetNumPools_91_hook()
{
    OnWwiseCall("AK::MemoryMgr::GetNumPools");
    return AK_MemoryMgr_GetNumPools_91_bridge();
}

t_AK_SoundEngine_Query_GetObjectObstructionAndOcclusion_93* AK_SoundEngine_Query_GetObjectObstructionAndOcclusion_93_orig = nullptr;
t_AK_SoundEngine_Query_GetObjectObstructionAndOcclusion_93* AK_SoundEngine_Query_GetObjectObstructionAndOcclusion_93_bridge = nullptr;
enum AKRESULT AK_SoundEngine_Query_GetObjectObstructionAndOcclusion_93_hook(unsigned __int64 a0, unsigned __int64 a1, float & a2, float & a3)
{
    OnWwiseCall("AK::SoundEngine::Query::GetObjectObstructionAndOcclusion");
    return AK_SoundEngine_Query_GetObjectObstructionAndOcclusion_93_bridge(a0, a1, a2, a3);
}

t_AK_SoundEngine_GetOutputID_94* AK_SoundEngine_GetOutputID_94_orig = nullptr;
t_AK_SoundEngine_GetOutputID_94* AK_SoundEngine_GetOutputID_94_bridge = nullptr;
unsigned __int64 AK_SoundEngine_GetOutputID_94_hook(unsigned long a0, unsigned long a1)
{
    OnWwiseCall("AK::SoundEngine::GetOutputID");
    return AK_SoundEngine_GetOutputID_94_bridge(a0, a1);
}

t_AK_SoundEngine_GetOutputID_95* AK_SoundEngine_GetOutputID_95_orig = nullptr;
t_AK_SoundEngine_GetOutputID_95* AK_SoundEngine_GetOutputID_95_bridge = nullptr;
unsigned __int64 AK_SoundEngine_GetOutputID_95_hook(char const * a0, unsigned long a1)
{
    OnWwiseCall("AK::SoundEngine::GetOutputID");
    return AK_SoundEngine_GetOutputID_95_bridge(a0, a1);
}

t_AK_SoundEngine_GetOutputID_96* AK_SoundEngine_GetOutputID_96_orig = nullptr;
t_AK_SoundEngine_GetOutputID_96* AK_SoundEngine_GetOutputID_96_bridge = nullptr;
unsigned __int64 AK_SoundEngine_GetOutputID_96_hook(wchar_t const * a0, unsigned long a1)
{
    OnWwiseCall("AK::SoundEngine::GetOutputID");
    return AK_SoundEngine_GetOutputID_96_bridge(a0, a1);
}

t_AK_SoundEngine_GetPanningRule_97* AK_SoundEngine_GetPanningRule_97_orig = nullptr;
t_AK_SoundEngine_GetPanningRule_97* AK_SoundEngine_GetPanningRule_97_bridge = nullptr;
enum AKRESULT AK_SoundEngine_GetPanningRule_97_hook(enum AkPanningRule & a0, unsigned __int64 a1)
{
    OnWwiseCall("AK::SoundEngine::GetPanningRule");
    return AK_SoundEngine_GetPanningRule_97_bridge(a0, a1);
}

t_AK_SoundEngine_DynamicSequence_GetPauseTimes_98* AK_SoundEngine_DynamicSequence_GetPauseTimes_98_orig = nullptr;
t_AK_SoundEngine_DynamicSequence_GetPauseTimes_98* AK_SoundEngine_DynamicSequence_GetPauseTimes_98_bridge = nullptr;
enum AKRESULT AK_SoundEngine_DynamicSequence_GetPauseTimes_98_hook(unsigned long a0, unsigned long & a1, unsigned long & a2)
{
    OnWwiseCall("AK::SoundEngine::DynamicSequence::GetPauseTimes");
    return AK_SoundEngine_DynamicSequence_GetPauseTimes_98_bridge(a0, a1, a2);
}

t_AK_SoundEngine_Query_GetPlayingIDsFromGameObject_99* AK_SoundEngine_Query_GetPlayingIDsFromGameObject_99_orig = nullptr;
t_AK_SoundEngine_Query_GetPlayingIDsFromGameObject_99* AK_SoundEngine_Query_GetPlayingIDsFromGameObject_99_bridge = nullptr;
enum AKRESULT AK_SoundEngine_Query_GetPlayingIDsFromGameObject_99_hook(unsigned __int64 a0, unsigned long & a1, unsigned long * a2)
{
    OnWwiseCall("AK::SoundEngine::Query::GetPlayingIDsFromGameObject");
    return AK_SoundEngine_Query_GetPlayingIDsFromGameObject_99_bridge(a0, a1, a2);
}

t_AK_SoundEngine_DynamicSequence_GetPlayingItem_100* AK_SoundEngine_DynamicSequence_GetPlayingItem_100_orig = nullptr;
t_AK_SoundEngine_DynamicSequence_GetPlayingItem_100* AK_SoundEngine_DynamicSequence_GetPlayingItem_100_bridge = nullptr;
enum AKRESULT AK_SoundEngine_DynamicSequence_GetPlayingItem_100_hook(unsigned long a0, unsigned long & a1, void * & a2)
{
    OnWwiseCall("AK::SoundEngine::DynamicSequence::GetPlayingItem");
    return AK_SoundEngine_DynamicSequence_GetPlayingItem_100_bridge(a0, a1, a2);
}

t_AK_MusicEngine_GetPlayingSegmentInfo_101* AK_MusicEngine_GetPlayingSegmentInfo_101_orig = nullptr;
t_AK_MusicEngine_GetPlayingSegmentInfo_101* AK_MusicEngine_GetPlayingSegmentInfo_101_bridge = nullptr;
enum AKRESULT AK_MusicEngine_GetPlayingSegmentInfo_101_hook(unsigned long a0, struct AkSegmentInfo & a1, bool a2)
{
    OnWwiseCall("AK::MusicEngine::GetPlayingSegmentInfo");
    return AK_MusicEngine_GetPlayingSegmentInfo_101_bridge(a0, a1, a2);
}

t_AK_MemoryMgr_GetPoolAttributes_102* AK_MemoryMgr_GetPoolAttributes_102_orig = nullptr;
t_AK_MemoryMgr_GetPoolAttributes_102* AK_MemoryMgr_GetPoolAttributes_102_bridge = nullptr;
enum AkMemPoolAttributes AK_MemoryMgr_GetPoolAttributes_102_hook(long a0)
{
    OnWwiseCall("AK::MemoryMgr::GetPoolAttributes");
    return AK_MemoryMgr_GetPoolAttributes_102_bridge(a0);
}

t_AK_StreamMgr_GetPoolID_103* AK_StreamMgr_GetPoolID_103_orig = nullptr;
t_AK_StreamMgr_GetPoolID_103* AK_StreamMgr_GetPoolID_103_bridge = nullptr;
long AK_StreamMgr_GetPoolID_103_hook()
{
    OnWwiseCall("AK::StreamMgr::GetPoolID");
    return AK_StreamMgr_GetPoolID_103_bridge();
}

t_AK_MemoryMgr_GetPoolMemoryUsed_104* AK_MemoryMgr_GetPoolMemoryUsed_104_orig = nullptr;
t_AK_MemoryMgr_GetPoolMemoryUsed_104* AK_MemoryMgr_GetPoolMemoryUsed_104_bridge = nullptr;
void AK_MemoryMgr_GetPoolMemoryUsed_104_hook(long a0, struct AK::MemoryMgr::PoolMemInfo & a1)
{
    OnWwiseCall("AK::MemoryMgr::GetPoolMemoryUsed");
    return AK_MemoryMgr_GetPoolMemoryUsed_104_bridge(a0, a1);
}

t_AK_MemoryMgr_GetPoolStats_106* AK_MemoryMgr_GetPoolStats_106_orig = nullptr;
t_AK_MemoryMgr_GetPoolStats_106* AK_MemoryMgr_GetPoolStats_106_bridge = nullptr;
enum AKRESULT AK_MemoryMgr_GetPoolStats_106_hook(long a0, struct AK::MemoryMgr::PoolStats & a1)
{
    OnWwiseCall("AK::MemoryMgr::GetPoolStats");
    return AK_MemoryMgr_GetPoolStats_106_bridge(a0, a1);
}

t_AK_SoundEngine_Query_GetPosition_107* AK_SoundEngine_Query_GetPosition_107_orig = nullptr;
t_AK_SoundEngine_Query_GetPosition_107* AK_SoundEngine_Query_GetPosition_107_bridge = nullptr;
enum AKRESULT AK_SoundEngine_Query_GetPosition_107_hook(unsigned __int64 a0, class AkTransform & a1)
{
    OnWwiseCall("AK::SoundEngine::Query::GetPosition");
    return AK_SoundEngine_Query_GetPosition_107_bridge(a0, a1);
}

t_AK_SoundEngine_Query_GetPositioningInfo_108* AK_SoundEngine_Query_GetPositioningInfo_108_orig = nullptr;
t_AK_SoundEngine_Query_GetPositioningInfo_108* AK_SoundEngine_Query_GetPositioningInfo_108_bridge = nullptr;
enum AKRESULT AK_SoundEngine_Query_GetPositioningInfo_108_hook(unsigned long a0, struct AkPositioningInfo & a1)
{
    OnWwiseCall("AK::SoundEngine::Query::GetPositioningInfo");
    return AK_SoundEngine_Query_GetPositioningInfo_108_bridge(a0, a1);
}

t_AK_SoundEngine_Query_GetRTPCValue_109* AK_SoundEngine_Query_GetRTPCValue_109_orig = nullptr;
t_AK_SoundEngine_Query_GetRTPCValue_109* AK_SoundEngine_Query_GetRTPCValue_109_bridge = nullptr;
enum AKRESULT AK_SoundEngine_Query_GetRTPCValue_109_hook(unsigned long a0, unsigned __int64 a1, unsigned long a2, float & a3, enum AK::SoundEngine::Query::RTPCValue_type & a4)
{
    OnWwiseCall("AK::SoundEngine::Query::GetRTPCValue");
    return AK_SoundEngine_Query_GetRTPCValue_109_bridge(a0, a1, a2, a3, a4);
}

t_AK_SoundEngine_Query_GetRTPCValue_110* AK_SoundEngine_Query_GetRTPCValue_110_orig = nullptr;
t_AK_SoundEngine_Query_GetRTPCValue_110* AK_SoundEngine_Query_GetRTPCValue_110_bridge = nullptr;
enum AKRESULT AK_SoundEngine_Query_GetRTPCValue_110_hook(char const * a0, unsigned __int64 a1, unsigned long a2, float & a3, enum AK::SoundEngine::Query::RTPCValue_type & a4)
{
    OnWwiseCall("AK::SoundEngine::Query::GetRTPCValue");
    return AK_SoundEngine_Query_GetRTPCValue_110_bridge(a0, a1, a2, a3, a4);
}

t_AK_SoundEngine_Query_GetRTPCValue_111* AK_SoundEngine_Query_GetRTPCValue_111_orig = nullptr;
t_AK_SoundEngine_Query_GetRTPCValue_111* AK_SoundEngine_Query_GetRTPCValue_111_bridge = nullptr;
enum AKRESULT AK_SoundEngine_Query_GetRTPCValue_111_hook(wchar_t const * a0, unsigned __int64 a1, unsigned long a2, float & a3, enum AK::SoundEngine::Query::RTPCValue_type & a4)
{
    OnWwiseCall("AK::SoundEngine::Query::GetRTPCValue");
    return AK_SoundEngine_Query_GetRTPCValue_111_bridge(a0, a1, a2, a3, a4);
}

t_AK_SoundEngine_GetSourcePlayPosition_112* AK_SoundEngine_GetSourcePlayPosition_112_orig = nullptr;
t_AK_SoundEngine_GetSourcePlayPosition_112* AK_SoundEngine_GetSourcePlayPosition_112_bridge = nullptr;
enum AKRESULT AK_SoundEngine_GetSourcePlayPosition_112_hook(unsigned long a0, long * a1, bool a2)
{
    OnWwiseCall("AK::SoundEngine::GetSourcePlayPosition");
    return AK_SoundEngine_GetSourcePlayPosition_112_bridge(a0, a1, a2);
}

t_AK_SoundEngine_GetSourcePlayPositions_113* AK_SoundEngine_GetSourcePlayPositions_113_orig = nullptr;
t_AK_SoundEngine_GetSourcePlayPositions_113* AK_SoundEngine_GetSourcePlayPositions_113_bridge = nullptr;
enum AKRESULT AK_SoundEngine_GetSourcePlayPositions_113_hook(unsigned long a0, struct AkSourcePosition * a1, unsigned long * a2, bool a3)
{
    OnWwiseCall("AK::SoundEngine::GetSourcePlayPositions");
    return AK_SoundEngine_GetSourcePlayPositions_113_bridge(a0, a1, a2, a3);
}

t_AK_SoundEngine_GetSourceStreamBuffering_114* AK_SoundEngine_GetSourceStreamBuffering_114_orig = nullptr;
t_AK_SoundEngine_GetSourceStreamBuffering_114* AK_SoundEngine_GetSourceStreamBuffering_114_bridge = nullptr;
enum AKRESULT AK_SoundEngine_GetSourceStreamBuffering_114_hook(unsigned long a0, long & a1, bool & a2)
{
    OnWwiseCall("AK::SoundEngine::GetSourceStreamBuffering");
    return AK_SoundEngine_GetSourceStreamBuffering_114_bridge(a0, a1, a2);
}

t_AK_SoundEngine_GetSpeakerAngles_115* AK_SoundEngine_GetSpeakerAngles_115_orig = nullptr;
t_AK_SoundEngine_GetSpeakerAngles_115* AK_SoundEngine_GetSpeakerAngles_115_bridge = nullptr;
enum AKRESULT AK_SoundEngine_GetSpeakerAngles_115_hook(float * a0, unsigned long & a1, float & a2, unsigned __int64 a3)
{
    OnWwiseCall("AK::SoundEngine::GetSpeakerAngles");
    return AK_SoundEngine_GetSpeakerAngles_115_bridge(a0, a1, a2, a3);
}

t_AK_SoundEngine_GetSpeakerConfiguration_116* AK_SoundEngine_GetSpeakerConfiguration_116_orig = nullptr;
t_AK_SoundEngine_GetSpeakerConfiguration_116* AK_SoundEngine_GetSpeakerConfiguration_116_bridge = nullptr;
struct AkChannelConfig AK_SoundEngine_GetSpeakerConfiguration_116_hook(unsigned __int64 a0)
{
    OnWwiseCall("AK::SoundEngine::GetSpeakerConfiguration");
    return AK_SoundEngine_GetSpeakerConfiguration_116_bridge(a0);
}

t_AK_SoundEngine_Query_GetState_117* AK_SoundEngine_Query_GetState_117_orig = nullptr;
t_AK_SoundEngine_Query_GetState_117* AK_SoundEngine_Query_GetState_117_bridge = nullptr;
enum AKRESULT AK_SoundEngine_Query_GetState_117_hook(unsigned long a0, unsigned long & a1)
{
    OnWwiseCall("AK::SoundEngine::Query::GetState");
    return AK_SoundEngine_Query_GetState_117_bridge(a0, a1);
}

t_AK_SoundEngine_Query_GetState_118* AK_SoundEngine_Query_GetState_118_orig = nullptr;
t_AK_SoundEngine_Query_GetState_118* AK_SoundEngine_Query_GetState_118_bridge = nullptr;
enum AKRESULT AK_SoundEngine_Query_GetState_118_hook(char const * a0, unsigned long & a1)
{
    OnWwiseCall("AK::SoundEngine::Query::GetState");
    return AK_SoundEngine_Query_GetState_118_bridge(a0, a1);
}

t_AK_SoundEngine_Query_GetState_119* AK_SoundEngine_Query_GetState_119_orig = nullptr;
t_AK_SoundEngine_Query_GetState_119* AK_SoundEngine_Query_GetState_119_bridge = nullptr;
enum AKRESULT AK_SoundEngine_Query_GetState_119_hook(wchar_t const * a0, unsigned long & a1)
{
    OnWwiseCall("AK::SoundEngine::Query::GetState");
    return AK_SoundEngine_Query_GetState_119_bridge(a0, a1);
}

t_AK_SoundEngine_Query_GetSwitch_120* AK_SoundEngine_Query_GetSwitch_120_orig = nullptr;
t_AK_SoundEngine_Query_GetSwitch_120* AK_SoundEngine_Query_GetSwitch_120_bridge = nullptr;
enum AKRESULT AK_SoundEngine_Query_GetSwitch_120_hook(unsigned long a0, unsigned __int64 a1, unsigned long & a2)
{
    OnWwiseCall("AK::SoundEngine::Query::GetSwitch");
    return AK_SoundEngine_Query_GetSwitch_120_bridge(a0, a1, a2);
}

t_AK_SoundEngine_Query_GetSwitch_121* AK_SoundEngine_Query_GetSwitch_121_orig = nullptr;
t_AK_SoundEngine_Query_GetSwitch_121* AK_SoundEngine_Query_GetSwitch_121_bridge = nullptr;
enum AKRESULT AK_SoundEngine_Query_GetSwitch_121_hook(char const * a0, unsigned __int64 a1, unsigned long & a2)
{
    OnWwiseCall("AK::SoundEngine::Query::GetSwitch");
    return AK_SoundEngine_Query_GetSwitch_121_bridge(a0, a1, a2);
}

t_AK_SoundEngine_Query_GetSwitch_122* AK_SoundEngine_Query_GetSwitch_122_orig = nullptr;
t_AK_SoundEngine_Query_GetSwitch_122* AK_SoundEngine_Query_GetSwitch_122_bridge = nullptr;
enum AKRESULT AK_SoundEngine_Query_GetSwitch_122_hook(wchar_t const * a0, unsigned __int64 a1, unsigned long & a2)
{
    OnWwiseCall("AK::SoundEngine::Query::GetSwitch");
    return AK_SoundEngine_Query_GetSwitch_122_bridge(a0, a1, a2);
}

t_AK_GetWindowsDevice_124* AK_GetWindowsDevice_124_orig = nullptr;
t_AK_GetWindowsDevice_124* AK_GetWindowsDevice_124_bridge = nullptr;
bool AK_GetWindowsDevice_124_hook(long a0, unsigned long & a1, struct IMMDevice * * a2, enum AkAudioDeviceState a3)
{
    OnWwiseCall("AK::GetWindowsDevice");
    return AK_GetWindowsDevice_124_bridge(a0, a1, a2, a3);
}

t_AK_GetWindowsDeviceCount_125* AK_GetWindowsDeviceCount_125_orig = nullptr;
t_AK_GetWindowsDeviceCount_125* AK_GetWindowsDeviceCount_125_bridge = nullptr;
unsigned long AK_GetWindowsDeviceCount_125_hook(enum AkAudioDeviceState a0)
{
    OnWwiseCall("AK::GetWindowsDeviceCount");
    return AK_GetWindowsDeviceCount_125_bridge(a0);
}

t_AK_GetWindowsDeviceName_126* AK_GetWindowsDeviceName_126_orig = nullptr;
t_AK_GetWindowsDeviceName_126* AK_GetWindowsDeviceName_126_bridge = nullptr;
wchar_t const * AK_GetWindowsDeviceName_126_hook(long a0, unsigned long & a1, enum AkAudioDeviceState a2)
{
    OnWwiseCall("AK::GetWindowsDeviceName");
    return AK_GetWindowsDeviceName_126_bridge(a0, a1, a2);
}

t_AK_GetWwiseXAudio2Interface_127* AK_GetWwiseXAudio2Interface_127_orig = nullptr;
t_AK_GetWwiseXAudio2Interface_127* AK_GetWwiseXAudio2Interface_127_bridge = nullptr;
struct IUnknown * AK_GetWwiseXAudio2Interface_127_hook()
{
    OnWwiseCall("AK::GetWwiseXAudio2Interface");
    return AK_GetWwiseXAudio2Interface_127_bridge();
}

t_AK_MemoryMgr_Init_128* AK_MemoryMgr_Init_128_orig = nullptr;
t_AK_MemoryMgr_Init_128* AK_MemoryMgr_Init_128_bridge = nullptr;
enum AKRESULT AK_MemoryMgr_Init_128_hook(struct AkMemSettings * a0)
{
    InstallRemainingHooks();
    OnWwiseCall("AK::MemoryMgr::Init");
    return AK_MemoryMgr_Init_128_bridge(a0);
}

t_AK_MemoryMgr_IsInitialized_132* AK_MemoryMgr_IsInitialized_132_orig = nullptr;
t_AK_MemoryMgr_IsInitialized_132* AK_MemoryMgr_IsInitialized_132_bridge = nullptr;
bool AK_MemoryMgr_IsInitialized_132_hook()
{
    OnWwiseCall("AK::MemoryMgr::IsInitialized");
    return AK_MemoryMgr_IsInitialized_132_bridge();
}

t_AK_SoundEngine_IsInitialized_133* AK_SoundEngine_IsInitialized_133_orig = nullptr;
t_AK_SoundEngine_IsInitialized_133* AK_SoundEngine_IsInitialized_133_bridge = nullptr;
bool AK_SoundEngine_IsInitialized_133_hook()
{
    OnWwiseCall("AK::SoundEngine::IsInitialized");
    return AK_SoundEngine_IsInitialized_133_bridge();
}

t_AK_SoundEngine_DynamicSequence_LockPlaylist_144* AK_SoundEngine_DynamicSequence_LockPlaylist_144_orig = nullptr;
t_AK_SoundEngine_DynamicSequence_LockPlaylist_144* AK_SoundEngine_DynamicSequence_LockPlaylist_144_bridge = nullptr;
class AK::SoundEngine::DynamicSequence::Playlist * AK_SoundEngine_DynamicSequence_LockPlaylist_144_hook(unsigned long a0)
{
    OnWwiseCall("AK::SoundEngine::DynamicSequence::LockPlaylist");
    return AK_SoundEngine_DynamicSequence_LockPlaylist_144_bridge(a0);
}

t_AK_MemoryMgr_Malign_145* AK_MemoryMgr_Malign_145_orig = nullptr;
t_AK_MemoryMgr_Malign_145* AK_MemoryMgr_Malign_145_bridge = nullptr;
void * AK_MemoryMgr_Malign_145_hook(long a0, unsigned __int64 a1, unsigned long a2)
{
    OnWwiseCall("AK::MemoryMgr::Malign");
    return AK_MemoryMgr_Malign_145_bridge(a0, a1, a2);
}

t_AK_MemoryMgr_Malloc_146* AK_MemoryMgr_Malloc_146_orig = nullptr;
t_AK_MemoryMgr_Malloc_146* AK_MemoryMgr_Malloc_146_bridge = nullptr;
void * AK_MemoryMgr_Malloc_146_hook(long a0, unsigned __int64 a1)
{
    OnWwiseCall("AK::MemoryMgr::Malloc");
    return AK_MemoryMgr_Malloc_146_bridge(a0, a1);
}

t_AK_SoundEngine_MuteBackgroundMusic_152* AK_SoundEngine_MuteBackgroundMusic_152_orig = nullptr;
t_AK_SoundEngine_MuteBackgroundMusic_152* AK_SoundEngine_MuteBackgroundMusic_152_bridge = nullptr;
void AK_SoundEngine_MuteBackgroundMusic_152_hook(bool a0)
{
    OnWwiseCall("AK::SoundEngine::MuteBackgroundMusic");
    return AK_SoundEngine_MuteBackgroundMusic_152_bridge(a0);
}

t_AK_SoundEngine_DynamicSequence_Open_153* AK_SoundEngine_DynamicSequence_Open_153_orig = nullptr;
t_AK_SoundEngine_DynamicSequence_Open_153* AK_SoundEngine_DynamicSequence_Open_153_bridge = nullptr;
unsigned long AK_SoundEngine_DynamicSequence_Open_153_hook(unsigned __int64 a0, unsigned long a1, void (__cdecl* a2)(enum AkCallbackType,struct AkCallbackInfo *), void * a3, enum AK::SoundEngine::DynamicSequence::DynamicSequenceType a4)
{
    OnWwiseCall("AK::SoundEngine::DynamicSequence::Open");
    return AK_SoundEngine_DynamicSequence_Open_153_bridge(a0, a1, a2, a3, a4);
}

t_AK_SoundEngine_DynamicSequence_Pause_154* AK_SoundEngine_DynamicSequence_Pause_154_orig = nullptr;
t_AK_SoundEngine_DynamicSequence_Pause_154* AK_SoundEngine_DynamicSequence_Pause_154_bridge = nullptr;
enum AKRESULT AK_SoundEngine_DynamicSequence_Pause_154_hook(unsigned long a0, long a1, enum AkCurveInterpolation a2)
{
    OnWwiseCall("AK::SoundEngine::DynamicSequence::Pause");
    return AK_SoundEngine_DynamicSequence_Pause_154_bridge(a0, a1, a2);
}

t_AK_SoundEngine_PinEventInStreamCache_155* AK_SoundEngine_PinEventInStreamCache_155_orig = nullptr;
t_AK_SoundEngine_PinEventInStreamCache_155* AK_SoundEngine_PinEventInStreamCache_155_bridge = nullptr;
enum AKRESULT AK_SoundEngine_PinEventInStreamCache_155_hook(unsigned long a0, char a1, char a2)
{
    OnWwiseCall("AK::SoundEngine::PinEventInStreamCache");
    return AK_SoundEngine_PinEventInStreamCache_155_bridge(a0, a1, a2);
}

t_AK_SoundEngine_PinEventInStreamCache_156* AK_SoundEngine_PinEventInStreamCache_156_orig = nullptr;
t_AK_SoundEngine_PinEventInStreamCache_156* AK_SoundEngine_PinEventInStreamCache_156_bridge = nullptr;
enum AKRESULT AK_SoundEngine_PinEventInStreamCache_156_hook(char const * a0, char a1, char a2)
{
    OnWwiseCall("AK::SoundEngine::PinEventInStreamCache");
    return AK_SoundEngine_PinEventInStreamCache_156_bridge(a0, a1, a2);
}

t_AK_SoundEngine_PinEventInStreamCache_157* AK_SoundEngine_PinEventInStreamCache_157_orig = nullptr;
t_AK_SoundEngine_PinEventInStreamCache_157* AK_SoundEngine_PinEventInStreamCache_157_bridge = nullptr;
enum AKRESULT AK_SoundEngine_PinEventInStreamCache_157_hook(wchar_t const * a0, char a1, char a2)
{
    OnWwiseCall("AK::SoundEngine::PinEventInStreamCache");
    return AK_SoundEngine_PinEventInStreamCache_157_bridge(a0, a1, a2);
}

t_AK_SoundEngine_DynamicSequence_Play_158* AK_SoundEngine_DynamicSequence_Play_158_orig = nullptr;
t_AK_SoundEngine_DynamicSequence_Play_158* AK_SoundEngine_DynamicSequence_Play_158_bridge = nullptr;
enum AKRESULT AK_SoundEngine_DynamicSequence_Play_158_hook(unsigned long a0, long a1, enum AkCurveInterpolation a2)
{
    OnWwiseCall("AK::SoundEngine::DynamicSequence::Play");
    return AK_SoundEngine_DynamicSequence_Play_158_bridge(a0, a1, a2);
}

t_AK_SoundEngine_PlaySourcePlugin_159* AK_SoundEngine_PlaySourcePlugin_159_orig = nullptr;
t_AK_SoundEngine_PlaySourcePlugin_159* AK_SoundEngine_PlaySourcePlugin_159_bridge = nullptr;
unsigned long AK_SoundEngine_PlaySourcePlugin_159_hook(unsigned long a0, unsigned long a1, unsigned __int64 a2)
{
    OnWwiseCall("AK::SoundEngine::PlaySourcePlugin");
    return AK_SoundEngine_PlaySourcePlugin_159_bridge(a0, a1, a2);
}

t_AK_SoundEngine_PostMIDIOnEvent_164* AK_SoundEngine_PostMIDIOnEvent_164_orig = nullptr;
t_AK_SoundEngine_PostMIDIOnEvent_164* AK_SoundEngine_PostMIDIOnEvent_164_bridge = nullptr;
enum AKRESULT AK_SoundEngine_PostMIDIOnEvent_164_hook(unsigned long a0, unsigned __int64 a1, struct AkMIDIPost * a2, unsigned short a3)
{
    OnWwiseCall("AK::SoundEngine::PostMIDIOnEvent");
    return AK_SoundEngine_PostMIDIOnEvent_164_bridge(a0, a1, a2, a3);
}

t_AK_SoundEngine_PostTrigger_167* AK_SoundEngine_PostTrigger_167_orig = nullptr;
t_AK_SoundEngine_PostTrigger_167* AK_SoundEngine_PostTrigger_167_bridge = nullptr;
enum AKRESULT AK_SoundEngine_PostTrigger_167_hook(unsigned long a0, unsigned __int64 a1)
{
    OnWwiseCall("AK::SoundEngine::PostTrigger");
    return AK_SoundEngine_PostTrigger_167_bridge(a0, a1);
}

t_AK_SoundEngine_PostTrigger_168* AK_SoundEngine_PostTrigger_168_orig = nullptr;
t_AK_SoundEngine_PostTrigger_168* AK_SoundEngine_PostTrigger_168_bridge = nullptr;
enum AKRESULT AK_SoundEngine_PostTrigger_168_hook(char const * a0, unsigned __int64 a1)
{
    OnWwiseCall("AK::SoundEngine::PostTrigger");
    return AK_SoundEngine_PostTrigger_168_bridge(a0, a1);
}

t_AK_SoundEngine_PostTrigger_169* AK_SoundEngine_PostTrigger_169_orig = nullptr;
t_AK_SoundEngine_PostTrigger_169* AK_SoundEngine_PostTrigger_169_bridge = nullptr;
enum AKRESULT AK_SoundEngine_PostTrigger_169_hook(wchar_t const * a0, unsigned __int64 a1)
{
    OnWwiseCall("AK::SoundEngine::PostTrigger");
    return AK_SoundEngine_PostTrigger_169_bridge(a0, a1);
}

t_AK_SoundEngine_PrepareBank_170* AK_SoundEngine_PrepareBank_170_orig = nullptr;
t_AK_SoundEngine_PrepareBank_170* AK_SoundEngine_PrepareBank_170_bridge = nullptr;
enum AKRESULT AK_SoundEngine_PrepareBank_170_hook(enum AK::SoundEngine::PreparationType a0, unsigned long a1, void (__cdecl* a2)(unsigned long,void const *,enum AKRESULT,long,void *), void * a3, enum AK::SoundEngine::AkBankContent a4)
{
    OnWwiseCall("AK::SoundEngine::PrepareBank");
    return AK_SoundEngine_PrepareBank_170_bridge(a0, a1, a2, a3, a4);
}

t_AK_SoundEngine_PrepareBank_171* AK_SoundEngine_PrepareBank_171_orig = nullptr;
t_AK_SoundEngine_PrepareBank_171* AK_SoundEngine_PrepareBank_171_bridge = nullptr;
enum AKRESULT AK_SoundEngine_PrepareBank_171_hook(enum AK::SoundEngine::PreparationType a0, unsigned long a1, enum AK::SoundEngine::AkBankContent a2)
{
    OnWwiseCall("AK::SoundEngine::PrepareBank");
    return AK_SoundEngine_PrepareBank_171_bridge(a0, a1, a2);
}

t_AK_SoundEngine_PrepareBank_172* AK_SoundEngine_PrepareBank_172_orig = nullptr;
t_AK_SoundEngine_PrepareBank_172* AK_SoundEngine_PrepareBank_172_bridge = nullptr;
enum AKRESULT AK_SoundEngine_PrepareBank_172_hook(enum AK::SoundEngine::PreparationType a0, char const * a1, void (__cdecl* a2)(unsigned long,void const *,enum AKRESULT,long,void *), void * a3, enum AK::SoundEngine::AkBankContent a4)
{
    OnWwiseCall("AK::SoundEngine::PrepareBank");
    return AK_SoundEngine_PrepareBank_172_bridge(a0, a1, a2, a3, a4);
}

t_AK_SoundEngine_PrepareBank_173* AK_SoundEngine_PrepareBank_173_orig = nullptr;
t_AK_SoundEngine_PrepareBank_173* AK_SoundEngine_PrepareBank_173_bridge = nullptr;
enum AKRESULT AK_SoundEngine_PrepareBank_173_hook(enum AK::SoundEngine::PreparationType a0, char const * a1, enum AK::SoundEngine::AkBankContent a2)
{
    OnWwiseCall("AK::SoundEngine::PrepareBank");
    return AK_SoundEngine_PrepareBank_173_bridge(a0, a1, a2);
}

t_AK_SoundEngine_PrepareBank_174* AK_SoundEngine_PrepareBank_174_orig = nullptr;
t_AK_SoundEngine_PrepareBank_174* AK_SoundEngine_PrepareBank_174_bridge = nullptr;
enum AKRESULT AK_SoundEngine_PrepareBank_174_hook(enum AK::SoundEngine::PreparationType a0, wchar_t const * a1, void (__cdecl* a2)(unsigned long,void const *,enum AKRESULT,long,void *), void * a3, enum AK::SoundEngine::AkBankContent a4)
{
    OnWwiseCall("AK::SoundEngine::PrepareBank");
    return AK_SoundEngine_PrepareBank_174_bridge(a0, a1, a2, a3, a4);
}

t_AK_SoundEngine_PrepareBank_175* AK_SoundEngine_PrepareBank_175_orig = nullptr;
t_AK_SoundEngine_PrepareBank_175* AK_SoundEngine_PrepareBank_175_bridge = nullptr;
enum AKRESULT AK_SoundEngine_PrepareBank_175_hook(enum AK::SoundEngine::PreparationType a0, wchar_t const * a1, enum AK::SoundEngine::AkBankContent a2)
{
    OnWwiseCall("AK::SoundEngine::PrepareBank");
    return AK_SoundEngine_PrepareBank_175_bridge(a0, a1, a2);
}

t_AK_SoundEngine_PrepareEvent_176* AK_SoundEngine_PrepareEvent_176_orig = nullptr;
t_AK_SoundEngine_PrepareEvent_176* AK_SoundEngine_PrepareEvent_176_bridge = nullptr;
enum AKRESULT AK_SoundEngine_PrepareEvent_176_hook(enum AK::SoundEngine::PreparationType a0, unsigned long * a1, unsigned long a2)
{
    OnWwiseCall("AK::SoundEngine::PrepareEvent");
    return AK_SoundEngine_PrepareEvent_176_bridge(a0, a1, a2);
}

t_AK_SoundEngine_PrepareEvent_177* AK_SoundEngine_PrepareEvent_177_orig = nullptr;
t_AK_SoundEngine_PrepareEvent_177* AK_SoundEngine_PrepareEvent_177_bridge = nullptr;
enum AKRESULT AK_SoundEngine_PrepareEvent_177_hook(enum AK::SoundEngine::PreparationType a0, unsigned long * a1, unsigned long a2, void (__cdecl* a3)(unsigned long,void const *,enum AKRESULT,long,void *), void * a4)
{
    OnWwiseCall("AK::SoundEngine::PrepareEvent");
    return AK_SoundEngine_PrepareEvent_177_bridge(a0, a1, a2, a3, a4);
}

t_AK_SoundEngine_PrepareEvent_178* AK_SoundEngine_PrepareEvent_178_orig = nullptr;
t_AK_SoundEngine_PrepareEvent_178* AK_SoundEngine_PrepareEvent_178_bridge = nullptr;
enum AKRESULT AK_SoundEngine_PrepareEvent_178_hook(enum AK::SoundEngine::PreparationType a0, char const * * a1, unsigned long a2)
{
    OnWwiseCall("AK::SoundEngine::PrepareEvent");
    return AK_SoundEngine_PrepareEvent_178_bridge(a0, a1, a2);
}

t_AK_SoundEngine_PrepareEvent_179* AK_SoundEngine_PrepareEvent_179_orig = nullptr;
t_AK_SoundEngine_PrepareEvent_179* AK_SoundEngine_PrepareEvent_179_bridge = nullptr;
enum AKRESULT AK_SoundEngine_PrepareEvent_179_hook(enum AK::SoundEngine::PreparationType a0, char const * * a1, unsigned long a2, void (__cdecl* a3)(unsigned long,void const *,enum AKRESULT,long,void *), void * a4)
{
    OnWwiseCall("AK::SoundEngine::PrepareEvent");
    return AK_SoundEngine_PrepareEvent_179_bridge(a0, a1, a2, a3, a4);
}

t_AK_SoundEngine_PrepareEvent_180* AK_SoundEngine_PrepareEvent_180_orig = nullptr;
t_AK_SoundEngine_PrepareEvent_180* AK_SoundEngine_PrepareEvent_180_bridge = nullptr;
enum AKRESULT AK_SoundEngine_PrepareEvent_180_hook(enum AK::SoundEngine::PreparationType a0, wchar_t const * * a1, unsigned long a2)
{
    OnWwiseCall("AK::SoundEngine::PrepareEvent");
    return AK_SoundEngine_PrepareEvent_180_bridge(a0, a1, a2);
}

t_AK_SoundEngine_PrepareEvent_181* AK_SoundEngine_PrepareEvent_181_orig = nullptr;
t_AK_SoundEngine_PrepareEvent_181* AK_SoundEngine_PrepareEvent_181_bridge = nullptr;
enum AKRESULT AK_SoundEngine_PrepareEvent_181_hook(enum AK::SoundEngine::PreparationType a0, wchar_t const * * a1, unsigned long a2, void (__cdecl* a3)(unsigned long,void const *,enum AKRESULT,long,void *), void * a4)
{
    OnWwiseCall("AK::SoundEngine::PrepareEvent");
    return AK_SoundEngine_PrepareEvent_181_bridge(a0, a1, a2, a3, a4);
}

t_AK_SoundEngine_PrepareGameSyncs_182* AK_SoundEngine_PrepareGameSyncs_182_orig = nullptr;
t_AK_SoundEngine_PrepareGameSyncs_182* AK_SoundEngine_PrepareGameSyncs_182_bridge = nullptr;
enum AKRESULT AK_SoundEngine_PrepareGameSyncs_182_hook(enum AK::SoundEngine::PreparationType a0, enum AkGroupType a1, unsigned long a2, unsigned long * a3, unsigned long a4)
{
    OnWwiseCall("AK::SoundEngine::PrepareGameSyncs");
    return AK_SoundEngine_PrepareGameSyncs_182_bridge(a0, a1, a2, a3, a4);
}

t_AK_SoundEngine_PrepareGameSyncs_183* AK_SoundEngine_PrepareGameSyncs_183_orig = nullptr;
t_AK_SoundEngine_PrepareGameSyncs_183* AK_SoundEngine_PrepareGameSyncs_183_bridge = nullptr;
enum AKRESULT AK_SoundEngine_PrepareGameSyncs_183_hook(enum AK::SoundEngine::PreparationType a0, enum AkGroupType a1, unsigned long a2, unsigned long * a3, unsigned long a4, void (__cdecl* a5)(unsigned long,void const *,enum AKRESULT,long,void *), void * a6)
{
    OnWwiseCall("AK::SoundEngine::PrepareGameSyncs");
    return AK_SoundEngine_PrepareGameSyncs_183_bridge(a0, a1, a2, a3, a4, a5, a6);
}

t_AK_SoundEngine_PrepareGameSyncs_184* AK_SoundEngine_PrepareGameSyncs_184_orig = nullptr;
t_AK_SoundEngine_PrepareGameSyncs_184* AK_SoundEngine_PrepareGameSyncs_184_bridge = nullptr;
enum AKRESULT AK_SoundEngine_PrepareGameSyncs_184_hook(enum AK::SoundEngine::PreparationType a0, enum AkGroupType a1, char const * a2, char const * * a3, unsigned long a4)
{
    OnWwiseCall("AK::SoundEngine::PrepareGameSyncs");
    return AK_SoundEngine_PrepareGameSyncs_184_bridge(a0, a1, a2, a3, a4);
}

t_AK_SoundEngine_PrepareGameSyncs_185* AK_SoundEngine_PrepareGameSyncs_185_orig = nullptr;
t_AK_SoundEngine_PrepareGameSyncs_185* AK_SoundEngine_PrepareGameSyncs_185_bridge = nullptr;
enum AKRESULT AK_SoundEngine_PrepareGameSyncs_185_hook(enum AK::SoundEngine::PreparationType a0, enum AkGroupType a1, char const * a2, char const * * a3, unsigned long a4, void (__cdecl* a5)(unsigned long,void const *,enum AKRESULT,long,void *), void * a6)
{
    OnWwiseCall("AK::SoundEngine::PrepareGameSyncs");
    return AK_SoundEngine_PrepareGameSyncs_185_bridge(a0, a1, a2, a3, a4, a5, a6);
}

t_AK_SoundEngine_PrepareGameSyncs_186* AK_SoundEngine_PrepareGameSyncs_186_orig = nullptr;
t_AK_SoundEngine_PrepareGameSyncs_186* AK_SoundEngine_PrepareGameSyncs_186_bridge = nullptr;
enum AKRESULT AK_SoundEngine_PrepareGameSyncs_186_hook(enum AK::SoundEngine::PreparationType a0, enum AkGroupType a1, wchar_t const * a2, wchar_t const * * a3, unsigned long a4)
{
    OnWwiseCall("AK::SoundEngine::PrepareGameSyncs");
    return AK_SoundEngine_PrepareGameSyncs_186_bridge(a0, a1, a2, a3, a4);
}

t_AK_SoundEngine_PrepareGameSyncs_187* AK_SoundEngine_PrepareGameSyncs_187_orig = nullptr;
t_AK_SoundEngine_PrepareGameSyncs_187* AK_SoundEngine_PrepareGameSyncs_187_bridge = nullptr;
enum AKRESULT AK_SoundEngine_PrepareGameSyncs_187_hook(enum AK::SoundEngine::PreparationType a0, enum AkGroupType a1, wchar_t const * a2, wchar_t const * * a3, unsigned long a4, void (__cdecl* a5)(unsigned long,void const *,enum AKRESULT,long,void *), void * a6)
{
    OnWwiseCall("AK::SoundEngine::PrepareGameSyncs");
    return AK_SoundEngine_PrepareGameSyncs_187_bridge(a0, a1, a2, a3, a4, a5, a6);
}

t_AK_SoundEngine_Query_QueryAudioObjectIDs_188* AK_SoundEngine_Query_QueryAudioObjectIDs_188_orig = nullptr;
t_AK_SoundEngine_Query_QueryAudioObjectIDs_188* AK_SoundEngine_Query_QueryAudioObjectIDs_188_bridge = nullptr;
enum AKRESULT AK_SoundEngine_Query_QueryAudioObjectIDs_188_hook(unsigned long a0, unsigned long & a1, struct AkObjectInfo * a2)
{
    OnWwiseCall("AK::SoundEngine::Query::QueryAudioObjectIDs");
    return AK_SoundEngine_Query_QueryAudioObjectIDs_188_bridge(a0, a1, a2);
}

t_AK_SoundEngine_Query_QueryAudioObjectIDs_189* AK_SoundEngine_Query_QueryAudioObjectIDs_189_orig = nullptr;
t_AK_SoundEngine_Query_QueryAudioObjectIDs_189* AK_SoundEngine_Query_QueryAudioObjectIDs_189_bridge = nullptr;
enum AKRESULT AK_SoundEngine_Query_QueryAudioObjectIDs_189_hook(char const * a0, unsigned long & a1, struct AkObjectInfo * a2)
{
    OnWwiseCall("AK::SoundEngine::Query::QueryAudioObjectIDs");
    return AK_SoundEngine_Query_QueryAudioObjectIDs_189_bridge(a0, a1, a2);
}

t_AK_SoundEngine_Query_QueryAudioObjectIDs_190* AK_SoundEngine_Query_QueryAudioObjectIDs_190_orig = nullptr;
t_AK_SoundEngine_Query_QueryAudioObjectIDs_190* AK_SoundEngine_Query_QueryAudioObjectIDs_190_bridge = nullptr;
enum AKRESULT AK_SoundEngine_Query_QueryAudioObjectIDs_190_hook(wchar_t const * a0, unsigned long & a1, struct AkObjectInfo * a2)
{
    OnWwiseCall("AK::SoundEngine::Query::QueryAudioObjectIDs");
    return AK_SoundEngine_Query_QueryAudioObjectIDs_190_bridge(a0, a1, a2);
}

t_AK_ReadBytesMem_ReadBytes_191* AK_ReadBytesMem_ReadBytes_191_orig = nullptr;
t_AK_ReadBytesMem_ReadBytes_191* AK_ReadBytesMem_ReadBytes_191_bridge = nullptr;
bool AK_ReadBytesMem_ReadBytes_191_hook(AK::ReadBytesMem* a0, void * a1, long a2, long & a3)
{
    OnWwiseCall("AK::ReadBytesMem::ReadBytes");
    return AK_ReadBytesMem_ReadBytes_191_bridge(a0, a1, a2, a3);
}

t_AK_ReadBytesSkip_ReadBytes_192* AK_ReadBytesSkip_ReadBytes_192_orig = nullptr;
t_AK_ReadBytesSkip_ReadBytes_192* AK_ReadBytesSkip_ReadBytes_192_bridge = nullptr;
bool AK_ReadBytesSkip_ReadBytes_192_hook(AK::ReadBytesSkip* a0, void * a1, long a2, long & a3)
{
    OnWwiseCall("AK::ReadBytesSkip::ReadBytes");
    return AK_ReadBytesSkip_ReadBytes_192_bridge(a0, a1, a2, a3);
}

t_AK_MemoryMgr_Realloc_193* AK_MemoryMgr_Realloc_193_orig = nullptr;
t_AK_MemoryMgr_Realloc_193* AK_MemoryMgr_Realloc_193_bridge = nullptr;
void * AK_MemoryMgr_Realloc_193_hook(long a0, void * a1, unsigned __int64 a2)
{
    OnWwiseCall("AK::MemoryMgr::Realloc");
    return AK_MemoryMgr_Realloc_193_bridge(a0, a1, a2);
}

t_AK_SoundEngine_RegisterAudioDeviceStatusCallback_194* AK_SoundEngine_RegisterAudioDeviceStatusCallback_194_orig = nullptr;
t_AK_SoundEngine_RegisterAudioDeviceStatusCallback_194* AK_SoundEngine_RegisterAudioDeviceStatusCallback_194_bridge = nullptr;
enum AKRESULT AK_SoundEngine_RegisterAudioDeviceStatusCallback_194_hook(void (__cdecl* a0)(class AK::IAkGlobalPluginContext *,unsigned long,unsigned long,enum AK::AkAudioDeviceEvent,enum AKRESULT))
{
    OnWwiseCall("AK::SoundEngine::RegisterAudioDeviceStatusCallback");
    return AK_SoundEngine_RegisterAudioDeviceStatusCallback_194_bridge(a0);
}

t_AK_SoundEngine_RegisterBusMeteringCallback_195* AK_SoundEngine_RegisterBusMeteringCallback_195_orig = nullptr;
t_AK_SoundEngine_RegisterBusMeteringCallback_195* AK_SoundEngine_RegisterBusMeteringCallback_195_bridge = nullptr;
enum AKRESULT AK_SoundEngine_RegisterBusMeteringCallback_195_hook(unsigned long a0, void (__cdecl* a1)(class AK::IAkMetering *,struct AkChannelConfig,enum AkMeteringFlags), enum AkMeteringFlags a2)
{
    OnWwiseCall("AK::SoundEngine::RegisterBusMeteringCallback");
    return AK_SoundEngine_RegisterBusMeteringCallback_195_bridge(a0, a1, a2);
}

t_AK_SoundEngine_RegisterBusVolumeCallback_196* AK_SoundEngine_RegisterBusVolumeCallback_196_orig = nullptr;
t_AK_SoundEngine_RegisterBusVolumeCallback_196* AK_SoundEngine_RegisterBusVolumeCallback_196_bridge = nullptr;
enum AKRESULT AK_SoundEngine_RegisterBusVolumeCallback_196_hook(unsigned long a0, void (__cdecl* a1)(struct AkSpeakerVolumeMatrixCallbackInfo *))
{
    OnWwiseCall("AK::SoundEngine::RegisterBusVolumeCallback");
    return AK_SoundEngine_RegisterBusVolumeCallback_196_bridge(a0, a1);
}

t_AK_SoundEngine_RegisterCodec_197* AK_SoundEngine_RegisterCodec_197_orig = nullptr;
t_AK_SoundEngine_RegisterCodec_197* AK_SoundEngine_RegisterCodec_197_bridge = nullptr;
enum AKRESULT AK_SoundEngine_RegisterCodec_197_hook(unsigned long a0, unsigned long a1, class IAkSoftwareCodec * (__cdecl* a2)(void *), class IAkSoftwareCodec * (__cdecl* a3)(void *))
{
    OnWwiseCall("AK::SoundEngine::RegisterCodec");
    return AK_SoundEngine_RegisterCodec_197_bridge(a0, a1, a2, a3);
}

t_AK_SoundEngine_RegisterPlugin_201* AK_SoundEngine_RegisterPlugin_201_orig = nullptr;
t_AK_SoundEngine_RegisterPlugin_201* AK_SoundEngine_RegisterPlugin_201_bridge = nullptr;
enum AKRESULT AK_SoundEngine_RegisterPlugin_201_hook(enum AkPluginType a0, unsigned long a1, unsigned long a2, class AK::IAkPlugin * (__cdecl* a3)(class AK::IAkPluginMemAlloc *), class AK::IAkPluginParam * (__cdecl* a4)(class AK::IAkPluginMemAlloc *))
{
    OnWwiseCall("AK::SoundEngine::RegisterPlugin");
    return AK_SoundEngine_RegisterPlugin_201_bridge(a0, a1, a2, a3, a4);
}

t_AK_SoundEngine_RegisterPluginDLL_202* AK_SoundEngine_RegisterPluginDLL_202_orig = nullptr;
t_AK_SoundEngine_RegisterPluginDLL_202* AK_SoundEngine_RegisterPluginDLL_202_bridge = nullptr;
enum AKRESULT AK_SoundEngine_RegisterPluginDLL_202_hook(wchar_t const * a0, wchar_t const * a1)
{
    OnWwiseCall("AK::SoundEngine::RegisterPluginDLL");
    return AK_SoundEngine_RegisterPluginDLL_202_bridge(a0, a1);
}

t_AK_MemoryMgr_ReleaseBlock_203* AK_MemoryMgr_ReleaseBlock_203_orig = nullptr;
t_AK_MemoryMgr_ReleaseBlock_203* AK_MemoryMgr_ReleaseBlock_203_bridge = nullptr;
enum AKRESULT AK_MemoryMgr_ReleaseBlock_203_hook(long a0, void * a1)
{
    OnWwiseCall("AK::MemoryMgr::ReleaseBlock");
    return AK_MemoryMgr_ReleaseBlock_203_bridge(a0, a1);
}

t_AK_SoundEngine_RemoveDefaultListener_204* AK_SoundEngine_RemoveDefaultListener_204_orig = nullptr;
t_AK_SoundEngine_RemoveDefaultListener_204* AK_SoundEngine_RemoveDefaultListener_204_bridge = nullptr;
enum AKRESULT AK_SoundEngine_RemoveDefaultListener_204_hook(unsigned __int64 a0)
{
    OnWwiseCall("AK::SoundEngine::RemoveDefaultListener");
    return AK_SoundEngine_RemoveDefaultListener_204_bridge(a0);
}

t_AK_StreamMgr_RemoveLanguageChangeObserver_205* AK_StreamMgr_RemoveLanguageChangeObserver_205_orig = nullptr;
t_AK_StreamMgr_RemoveLanguageChangeObserver_205* AK_StreamMgr_RemoveLanguageChangeObserver_205_bridge = nullptr;
void AK_StreamMgr_RemoveLanguageChangeObserver_205_hook(void * a0)
{
    OnWwiseCall("AK::StreamMgr::RemoveLanguageChangeObserver");
    return AK_StreamMgr_RemoveLanguageChangeObserver_205_bridge(a0);
}

t_AK_SoundEngine_RemoveListener_206* AK_SoundEngine_RemoveListener_206_orig = nullptr;
t_AK_SoundEngine_RemoveListener_206* AK_SoundEngine_RemoveListener_206_bridge = nullptr;
enum AKRESULT AK_SoundEngine_RemoveListener_206_hook(unsigned __int64 a0, unsigned __int64 a1)
{
    OnWwiseCall("AK::SoundEngine::RemoveListener");
    return AK_SoundEngine_RemoveListener_206_bridge(a0, a1);
}

t_AK_SoundEngine_RemoveOutput_207* AK_SoundEngine_RemoveOutput_207_orig = nullptr;
t_AK_SoundEngine_RemoveOutput_207* AK_SoundEngine_RemoveOutput_207_bridge = nullptr;
enum AKRESULT AK_SoundEngine_RemoveOutput_207_hook(unsigned __int64 a0)
{
    OnWwiseCall("AK::SoundEngine::RemoveOutput");
    return AK_SoundEngine_RemoveOutput_207_bridge(a0);
}

t_AK_SoundEngine_RenderAudio_208* AK_SoundEngine_RenderAudio_208_orig = nullptr;
t_AK_SoundEngine_RenderAudio_208* AK_SoundEngine_RenderAudio_208_bridge = nullptr;
enum AKRESULT AK_SoundEngine_RenderAudio_208_hook(bool a0)
{
    OnWwiseCall("AK::SoundEngine::RenderAudio");
    return AK_SoundEngine_RenderAudio_208_bridge(a0);
}

t_AK_SoundEngine_ReplaceOutput_209* AK_SoundEngine_ReplaceOutput_209_orig = nullptr;
t_AK_SoundEngine_ReplaceOutput_209* AK_SoundEngine_ReplaceOutput_209_bridge = nullptr;
enum AKRESULT AK_SoundEngine_ReplaceOutput_209_hook(struct AkOutputSettings const & a0, unsigned __int64 a1, unsigned __int64 * a2)
{
    OnWwiseCall("AK::SoundEngine::ReplaceOutput");
    return AK_SoundEngine_ReplaceOutput_209_bridge(a0, a1, a2);
}

t_AK_WriteBytesCount_Reserve_210* AK_WriteBytesCount_Reserve_210_orig = nullptr;
t_AK_WriteBytesCount_Reserve_210* AK_WriteBytesCount_Reserve_210_bridge = nullptr;
bool AK_WriteBytesCount_Reserve_210_hook(AK::WriteBytesCount* a0, long a1)
{
    OnWwiseCall("AK::WriteBytesCount::Reserve");
    return AK_WriteBytesCount_Reserve_210_bridge(a0, a1);
}

t_AK_WriteBytesMem_Reserve_211* AK_WriteBytesMem_Reserve_211_orig = nullptr;
t_AK_WriteBytesMem_Reserve_211* AK_WriteBytesMem_Reserve_211_bridge = nullptr;
bool AK_WriteBytesMem_Reserve_211_hook(AK::WriteBytesMem* a0, long a1)
{
    OnWwiseCall("AK::WriteBytesMem::Reserve");
    return AK_WriteBytesMem_Reserve_211_bridge(a0, a1);
}

t_AK_SoundEngine_ResetListenersToDefault_212* AK_SoundEngine_ResetListenersToDefault_212_orig = nullptr;
t_AK_SoundEngine_ResetListenersToDefault_212* AK_SoundEngine_ResetListenersToDefault_212_bridge = nullptr;
enum AKRESULT AK_SoundEngine_ResetListenersToDefault_212_hook(unsigned __int64 a0)
{
    OnWwiseCall("AK::SoundEngine::ResetListenersToDefault");
    return AK_SoundEngine_ResetListenersToDefault_212_bridge(a0);
}

t_AK_SoundEngine_ResetRTPCValue_213* AK_SoundEngine_ResetRTPCValue_213_orig = nullptr;
t_AK_SoundEngine_ResetRTPCValue_213* AK_SoundEngine_ResetRTPCValue_213_bridge = nullptr;
enum AKRESULT AK_SoundEngine_ResetRTPCValue_213_hook(unsigned long a0, unsigned __int64 a1, long a2, enum AkCurveInterpolation a3, bool a4)
{
    OnWwiseCall("AK::SoundEngine::ResetRTPCValue");
    return AK_SoundEngine_ResetRTPCValue_213_bridge(a0, a1, a2, a3, a4);
}

t_AK_SoundEngine_ResetRTPCValue_214* AK_SoundEngine_ResetRTPCValue_214_orig = nullptr;
t_AK_SoundEngine_ResetRTPCValue_214* AK_SoundEngine_ResetRTPCValue_214_bridge = nullptr;
enum AKRESULT AK_SoundEngine_ResetRTPCValue_214_hook(char const * a0, unsigned __int64 a1, long a2, enum AkCurveInterpolation a3, bool a4)
{
    OnWwiseCall("AK::SoundEngine::ResetRTPCValue");
    return AK_SoundEngine_ResetRTPCValue_214_bridge(a0, a1, a2, a3, a4);
}

t_AK_SoundEngine_ResetRTPCValue_215* AK_SoundEngine_ResetRTPCValue_215_orig = nullptr;
t_AK_SoundEngine_ResetRTPCValue_215* AK_SoundEngine_ResetRTPCValue_215_bridge = nullptr;
enum AKRESULT AK_SoundEngine_ResetRTPCValue_215_hook(wchar_t const * a0, unsigned __int64 a1, long a2, enum AkCurveInterpolation a3, bool a4)
{
    OnWwiseCall("AK::SoundEngine::ResetRTPCValue");
    return AK_SoundEngine_ResetRTPCValue_215_bridge(a0, a1, a2, a3, a4);
}

t_AK_SoundEngine_DynamicDialogue_ResolveDialogueEvent_216* AK_SoundEngine_DynamicDialogue_ResolveDialogueEvent_216_orig = nullptr;
t_AK_SoundEngine_DynamicDialogue_ResolveDialogueEvent_216* AK_SoundEngine_DynamicDialogue_ResolveDialogueEvent_216_bridge = nullptr;
unsigned long AK_SoundEngine_DynamicDialogue_ResolveDialogueEvent_216_hook(unsigned long a0, unsigned long * a1, unsigned long a2, unsigned long a3, bool (__cdecl* a4)(unsigned long,unsigned long,void *), void * a5)
{
    OnWwiseCall("AK::SoundEngine::DynamicDialogue::ResolveDialogueEvent");
    return AK_SoundEngine_DynamicDialogue_ResolveDialogueEvent_216_bridge(a0, a1, a2, a3, a4, a5);
}

t_AK_SoundEngine_DynamicDialogue_ResolveDialogueEvent_217* AK_SoundEngine_DynamicDialogue_ResolveDialogueEvent_217_orig = nullptr;
t_AK_SoundEngine_DynamicDialogue_ResolveDialogueEvent_217* AK_SoundEngine_DynamicDialogue_ResolveDialogueEvent_217_bridge = nullptr;
unsigned long AK_SoundEngine_DynamicDialogue_ResolveDialogueEvent_217_hook(char const * a0, char const * * a1, unsigned long a2, unsigned long a3, bool (__cdecl* a4)(unsigned long,unsigned long,void *), void * a5)
{
    OnWwiseCall("AK::SoundEngine::DynamicDialogue::ResolveDialogueEvent");
    return AK_SoundEngine_DynamicDialogue_ResolveDialogueEvent_217_bridge(a0, a1, a2, a3, a4, a5);
}

t_AK_SoundEngine_DynamicDialogue_ResolveDialogueEvent_218* AK_SoundEngine_DynamicDialogue_ResolveDialogueEvent_218_orig = nullptr;
t_AK_SoundEngine_DynamicDialogue_ResolveDialogueEvent_218* AK_SoundEngine_DynamicDialogue_ResolveDialogueEvent_218_bridge = nullptr;
unsigned long AK_SoundEngine_DynamicDialogue_ResolveDialogueEvent_218_hook(wchar_t const * a0, wchar_t const * * a1, unsigned long a2, unsigned long a3, bool (__cdecl* a4)(unsigned long,unsigned long,void *), void * a5)
{
    OnWwiseCall("AK::SoundEngine::DynamicDialogue::ResolveDialogueEvent");
    return AK_SoundEngine_DynamicDialogue_ResolveDialogueEvent_218_bridge(a0, a1, a2, a3, a4, a5);
}

t_AK_SoundEngine_DynamicSequence_Resume_219* AK_SoundEngine_DynamicSequence_Resume_219_orig = nullptr;
t_AK_SoundEngine_DynamicSequence_Resume_219* AK_SoundEngine_DynamicSequence_Resume_219_bridge = nullptr;
enum AKRESULT AK_SoundEngine_DynamicSequence_Resume_219_hook(unsigned long a0, long a1, enum AkCurveInterpolation a2)
{
    OnWwiseCall("AK::SoundEngine::DynamicSequence::Resume");
    return AK_SoundEngine_DynamicSequence_Resume_219_bridge(a0, a1, a2);
}

t_AK_SoundEngine_SeekOnEvent_220* AK_SoundEngine_SeekOnEvent_220_orig = nullptr;
t_AK_SoundEngine_SeekOnEvent_220* AK_SoundEngine_SeekOnEvent_220_bridge = nullptr;
enum AKRESULT AK_SoundEngine_SeekOnEvent_220_hook(unsigned long a0, unsigned __int64 a1, long a2, bool a3, unsigned long a4)
{
    OnWwiseCall("AK::SoundEngine::SeekOnEvent");
    return AK_SoundEngine_SeekOnEvent_220_bridge(a0, a1, a2, a3, a4);
}

t_AK_SoundEngine_SeekOnEvent_221* AK_SoundEngine_SeekOnEvent_221_orig = nullptr;
t_AK_SoundEngine_SeekOnEvent_221* AK_SoundEngine_SeekOnEvent_221_bridge = nullptr;
enum AKRESULT AK_SoundEngine_SeekOnEvent_221_hook(unsigned long a0, unsigned __int64 a1, float a2, bool a3, unsigned long a4)
{
    OnWwiseCall("AK::SoundEngine::SeekOnEvent");
    return AK_SoundEngine_SeekOnEvent_221_bridge(a0, a1, a2, a3, a4);
}

t_AK_SoundEngine_SeekOnEvent_222* AK_SoundEngine_SeekOnEvent_222_orig = nullptr;
t_AK_SoundEngine_SeekOnEvent_222* AK_SoundEngine_SeekOnEvent_222_bridge = nullptr;
enum AKRESULT AK_SoundEngine_SeekOnEvent_222_hook(char const * a0, unsigned __int64 a1, long a2, bool a3, unsigned long a4)
{
    OnWwiseCall("AK::SoundEngine::SeekOnEvent");
    return AK_SoundEngine_SeekOnEvent_222_bridge(a0, a1, a2, a3, a4);
}

t_AK_SoundEngine_SeekOnEvent_223* AK_SoundEngine_SeekOnEvent_223_orig = nullptr;
t_AK_SoundEngine_SeekOnEvent_223* AK_SoundEngine_SeekOnEvent_223_bridge = nullptr;
enum AKRESULT AK_SoundEngine_SeekOnEvent_223_hook(char const * a0, unsigned __int64 a1, float a2, bool a3, unsigned long a4)
{
    OnWwiseCall("AK::SoundEngine::SeekOnEvent");
    return AK_SoundEngine_SeekOnEvent_223_bridge(a0, a1, a2, a3, a4);
}

t_AK_SoundEngine_SeekOnEvent_224* AK_SoundEngine_SeekOnEvent_224_orig = nullptr;
t_AK_SoundEngine_SeekOnEvent_224* AK_SoundEngine_SeekOnEvent_224_bridge = nullptr;
enum AKRESULT AK_SoundEngine_SeekOnEvent_224_hook(wchar_t const * a0, unsigned __int64 a1, long a2, bool a3, unsigned long a4)
{
    OnWwiseCall("AK::SoundEngine::SeekOnEvent");
    return AK_SoundEngine_SeekOnEvent_224_bridge(a0, a1, a2, a3, a4);
}

t_AK_SoundEngine_SeekOnEvent_225* AK_SoundEngine_SeekOnEvent_225_orig = nullptr;
t_AK_SoundEngine_SeekOnEvent_225* AK_SoundEngine_SeekOnEvent_225_bridge = nullptr;
enum AKRESULT AK_SoundEngine_SeekOnEvent_225_hook(wchar_t const * a0, unsigned __int64 a1, float a2, bool a3, unsigned long a4)
{
    OnWwiseCall("AK::SoundEngine::SeekOnEvent");
    return AK_SoundEngine_SeekOnEvent_225_bridge(a0, a1, a2, a3, a4);
}

t_AK_SoundEngine_SendPluginCustomGameData_226* AK_SoundEngine_SendPluginCustomGameData_226_orig = nullptr;
t_AK_SoundEngine_SendPluginCustomGameData_226* AK_SoundEngine_SendPluginCustomGameData_226_bridge = nullptr;
enum AKRESULT AK_SoundEngine_SendPluginCustomGameData_226_hook(unsigned long a0, unsigned __int64 a1, enum AkPluginType a2, unsigned long a3, unsigned long a4, void const * a5, unsigned long a6)
{
    OnWwiseCall("AK::SoundEngine::SendPluginCustomGameData");
    return AK_SoundEngine_SendPluginCustomGameData_226_bridge(a0, a1, a2, a3, a4, a5, a6);
}

t_AK_SoundEngine_SetActorMixerEffect_227* AK_SoundEngine_SetActorMixerEffect_227_orig = nullptr;
t_AK_SoundEngine_SetActorMixerEffect_227* AK_SoundEngine_SetActorMixerEffect_227_bridge = nullptr;
enum AKRESULT AK_SoundEngine_SetActorMixerEffect_227_hook(unsigned long a0, unsigned long a1, unsigned long a2)
{
    OnWwiseCall("AK::SoundEngine::SetActorMixerEffect");
    return AK_SoundEngine_SetActorMixerEffect_227_bridge(a0, a1, a2);
}

t_AK_SoundEngine_SetBankLoadIOSettings_228* AK_SoundEngine_SetBankLoadIOSettings_228_orig = nullptr;
t_AK_SoundEngine_SetBankLoadIOSettings_228* AK_SoundEngine_SetBankLoadIOSettings_228_bridge = nullptr;
enum AKRESULT AK_SoundEngine_SetBankLoadIOSettings_228_hook(float a0, char a1)
{
    OnWwiseCall("AK::SoundEngine::SetBankLoadIOSettings");
    return AK_SoundEngine_SetBankLoadIOSettings_228_bridge(a0, a1);
}

t_AK_SoundEngine_SetBusConfig_229* AK_SoundEngine_SetBusConfig_229_orig = nullptr;
t_AK_SoundEngine_SetBusConfig_229* AK_SoundEngine_SetBusConfig_229_bridge = nullptr;
enum AKRESULT AK_SoundEngine_SetBusConfig_229_hook(unsigned long a0, struct AkChannelConfig a1)
{
    OnWwiseCall("AK::SoundEngine::SetBusConfig");
    return AK_SoundEngine_SetBusConfig_229_bridge(a0, a1);
}

t_AK_SoundEngine_SetBusConfig_230* AK_SoundEngine_SetBusConfig_230_orig = nullptr;
t_AK_SoundEngine_SetBusConfig_230* AK_SoundEngine_SetBusConfig_230_bridge = nullptr;
enum AKRESULT AK_SoundEngine_SetBusConfig_230_hook(char const * a0, struct AkChannelConfig a1)
{
    OnWwiseCall("AK::SoundEngine::SetBusConfig");
    return AK_SoundEngine_SetBusConfig_230_bridge(a0, a1);
}

t_AK_SoundEngine_SetBusConfig_231* AK_SoundEngine_SetBusConfig_231_orig = nullptr;
t_AK_SoundEngine_SetBusConfig_231* AK_SoundEngine_SetBusConfig_231_bridge = nullptr;
enum AKRESULT AK_SoundEngine_SetBusConfig_231_hook(wchar_t const * a0, struct AkChannelConfig a1)
{
    OnWwiseCall("AK::SoundEngine::SetBusConfig");
    return AK_SoundEngine_SetBusConfig_231_bridge(a0, a1);
}

t_AK_SoundEngine_SetBusDevice_232* AK_SoundEngine_SetBusDevice_232_orig = nullptr;
t_AK_SoundEngine_SetBusDevice_232* AK_SoundEngine_SetBusDevice_232_bridge = nullptr;
enum AKRESULT AK_SoundEngine_SetBusDevice_232_hook(unsigned long a0, unsigned long a1)
{
    OnWwiseCall("AK::SoundEngine::SetBusDevice");
    return AK_SoundEngine_SetBusDevice_232_bridge(a0, a1);
}

t_AK_SoundEngine_SetBusDevice_233* AK_SoundEngine_SetBusDevice_233_orig = nullptr;
t_AK_SoundEngine_SetBusDevice_233* AK_SoundEngine_SetBusDevice_233_bridge = nullptr;
enum AKRESULT AK_SoundEngine_SetBusDevice_233_hook(char const * a0, char const * a1)
{
    OnWwiseCall("AK::SoundEngine::SetBusDevice");
    return AK_SoundEngine_SetBusDevice_233_bridge(a0, a1);
}

t_AK_SoundEngine_SetBusDevice_234* AK_SoundEngine_SetBusDevice_234_orig = nullptr;
t_AK_SoundEngine_SetBusDevice_234* AK_SoundEngine_SetBusDevice_234_bridge = nullptr;
enum AKRESULT AK_SoundEngine_SetBusDevice_234_hook(wchar_t const * a0, wchar_t const * a1)
{
    OnWwiseCall("AK::SoundEngine::SetBusDevice");
    return AK_SoundEngine_SetBusDevice_234_bridge(a0, a1);
}

t_AK_SoundEngine_SetBusEffect_235* AK_SoundEngine_SetBusEffect_235_orig = nullptr;
t_AK_SoundEngine_SetBusEffect_235* AK_SoundEngine_SetBusEffect_235_bridge = nullptr;
enum AKRESULT AK_SoundEngine_SetBusEffect_235_hook(unsigned long a0, unsigned long a1, unsigned long a2)
{
    OnWwiseCall("AK::SoundEngine::SetBusEffect");
    return AK_SoundEngine_SetBusEffect_235_bridge(a0, a1, a2);
}

t_AK_SoundEngine_SetBusEffect_236* AK_SoundEngine_SetBusEffect_236_orig = nullptr;
t_AK_SoundEngine_SetBusEffect_236* AK_SoundEngine_SetBusEffect_236_bridge = nullptr;
enum AKRESULT AK_SoundEngine_SetBusEffect_236_hook(char const * a0, unsigned long a1, unsigned long a2)
{
    OnWwiseCall("AK::SoundEngine::SetBusEffect");
    return AK_SoundEngine_SetBusEffect_236_bridge(a0, a1, a2);
}

t_AK_SoundEngine_SetBusEffect_237* AK_SoundEngine_SetBusEffect_237_orig = nullptr;
t_AK_SoundEngine_SetBusEffect_237* AK_SoundEngine_SetBusEffect_237_bridge = nullptr;
enum AKRESULT AK_SoundEngine_SetBusEffect_237_hook(wchar_t const * a0, unsigned long a1, unsigned long a2)
{
    OnWwiseCall("AK::SoundEngine::SetBusEffect");
    return AK_SoundEngine_SetBusEffect_237_bridge(a0, a1, a2);
}

t_AK_SoundEngine_SetContainerHistory_238* AK_SoundEngine_SetContainerHistory_238_orig = nullptr;
t_AK_SoundEngine_SetContainerHistory_238* AK_SoundEngine_SetContainerHistory_238_bridge = nullptr;
enum AKRESULT AK_SoundEngine_SetContainerHistory_238_hook(class AK::IReadBytes * a0)
{
    OnWwiseCall("AK::SoundEngine::SetContainerHistory");
    return AK_SoundEngine_SetContainerHistory_238_bridge(a0);
}

t_AK_WriteBytesCount_SetCount_239* AK_WriteBytesCount_SetCount_239_orig = nullptr;
t_AK_WriteBytesCount_SetCount_239* AK_WriteBytesCount_SetCount_239_bridge = nullptr;
void AK_WriteBytesCount_SetCount_239_hook(AK::WriteBytesCount* a0, long a1)
{
    OnWwiseCall("AK::WriteBytesCount::SetCount");
    return AK_WriteBytesCount_SetCount_239_bridge(a0, a1);
}

t_AK_WriteBytesMem_SetCount_240* AK_WriteBytesMem_SetCount_240_orig = nullptr;
t_AK_WriteBytesMem_SetCount_240* AK_WriteBytesMem_SetCount_240_bridge = nullptr;
void AK_WriteBytesMem_SetCount_240_hook(AK::WriteBytesMem* a0, long a1)
{
    OnWwiseCall("AK::WriteBytesMem::SetCount");
    return AK_WriteBytesMem_SetCount_240_bridge(a0, a1);
}

t_AK_StreamMgr_SetCurrentLanguage_241* AK_StreamMgr_SetCurrentLanguage_241_orig = nullptr;
t_AK_StreamMgr_SetCurrentLanguage_241* AK_StreamMgr_SetCurrentLanguage_241_bridge = nullptr;
enum AKRESULT AK_StreamMgr_SetCurrentLanguage_241_hook(wchar_t const * a0)
{
    OnWwiseCall("AK::StreamMgr::SetCurrentLanguage");
    return AK_StreamMgr_SetCurrentLanguage_241_bridge(a0);
}

t_AK_StreamMgr_SetFileLocationResolver_243* AK_StreamMgr_SetFileLocationResolver_243_orig = nullptr;
t_AK_StreamMgr_SetFileLocationResolver_243* AK_StreamMgr_SetFileLocationResolver_243_bridge = nullptr;
void AK_StreamMgr_SetFileLocationResolver_243_hook(class AK::StreamMgr::IAkFileLocationResolver * a0)
{
    OnWwiseCall("AK::StreamMgr::SetFileLocationResolver");
    return AK_StreamMgr_SetFileLocationResolver_243_bridge(a0);
}

t_AK_SoundEngine_SetGameObjectAuxSendValues_244* AK_SoundEngine_SetGameObjectAuxSendValues_244_orig = nullptr;
t_AK_SoundEngine_SetGameObjectAuxSendValues_244* AK_SoundEngine_SetGameObjectAuxSendValues_244_bridge = nullptr;
enum AKRESULT AK_SoundEngine_SetGameObjectAuxSendValues_244_hook(unsigned __int64 a0, struct AkAuxSendValue * a1, unsigned long a2)
{
    OnWwiseCall("AK::SoundEngine::SetGameObjectAuxSendValues");
    LogAuxSendValues(a0, a1, a2);
    return AK_SoundEngine_SetGameObjectAuxSendValues_244_bridge(a0, a1, a2);
}

t_AK_SoundEngine_SetGameObjectOutputBusVolume_245* AK_SoundEngine_SetGameObjectOutputBusVolume_245_orig = nullptr;
t_AK_SoundEngine_SetGameObjectOutputBusVolume_245* AK_SoundEngine_SetGameObjectOutputBusVolume_245_bridge = nullptr;
enum AKRESULT AK_SoundEngine_SetGameObjectOutputBusVolume_245_hook(unsigned __int64 a0, unsigned __int64 a1, float a2)
{
    OnWwiseCall("AK::SoundEngine::SetGameObjectOutputBusVolume");
    LEASI_TRACE("  -> emitter={} listener={} controlValue={}", a0, a1, a2);
    return AK_SoundEngine_SetGameObjectOutputBusVolume_245_bridge(a0, a1, a2);
}

t_AK_SoundEngine_SetListenerSpatialization_246* AK_SoundEngine_SetListenerSpatialization_246_orig = nullptr;
t_AK_SoundEngine_SetListenerSpatialization_246* AK_SoundEngine_SetListenerSpatialization_246_bridge = nullptr;
enum AKRESULT AK_SoundEngine_SetListenerSpatialization_246_hook(unsigned __int64 a0, bool a1, struct AkChannelConfig a2, float * a3)
{
    OnWwiseCall("AK::SoundEngine::SetListenerSpatialization");
    return AK_SoundEngine_SetListenerSpatialization_246_bridge(a0, a1, a2, a3);
}

t_AK_SoundEngine_SetListeners_247* AK_SoundEngine_SetListeners_247_orig = nullptr;
t_AK_SoundEngine_SetListeners_247* AK_SoundEngine_SetListeners_247_bridge = nullptr;
enum AKRESULT AK_SoundEngine_SetListeners_247_hook(unsigned __int64 a0, unsigned __int64 const * a1, unsigned long a2)
{
    OnWwiseCall("AK::SoundEngine::SetListeners");
    return AK_SoundEngine_SetListeners_247_bridge(a0, a1, a2);
}

t_AK_SoundEngine_SetMaxNumVoicesLimit_249* AK_SoundEngine_SetMaxNumVoicesLimit_249_orig = nullptr;
t_AK_SoundEngine_SetMaxNumVoicesLimit_249* AK_SoundEngine_SetMaxNumVoicesLimit_249_bridge = nullptr;
enum AKRESULT AK_SoundEngine_SetMaxNumVoicesLimit_249_hook(unsigned short a0)
{
    OnWwiseCall("AK::SoundEngine::SetMaxNumVoicesLimit");
    return AK_SoundEngine_SetMaxNumVoicesLimit_249_bridge(a0);
}

t_AK_SoundEngine_SetMedia_250* AK_SoundEngine_SetMedia_250_orig = nullptr;
t_AK_SoundEngine_SetMedia_250* AK_SoundEngine_SetMedia_250_bridge = nullptr;
enum AKRESULT AK_SoundEngine_SetMedia_250_hook(struct AkSourceSettings * a0, unsigned long a1)
{
    OnWwiseCall("AK::SoundEngine::SetMedia");
    return AK_SoundEngine_SetMedia_250_bridge(a0, a1);
}

t_AK_WriteBytesMem_SetMemPool_252* AK_WriteBytesMem_SetMemPool_252_orig = nullptr;
t_AK_WriteBytesMem_SetMemPool_252* AK_WriteBytesMem_SetMemPool_252_bridge = nullptr;
void AK_WriteBytesMem_SetMemPool_252_hook(AK::WriteBytesMem* a0, long a1)
{
    OnWwiseCall("AK::WriteBytesMem::SetMemPool");
    return AK_WriteBytesMem_SetMemPool_252_bridge(a0, a1);
}

t_AK_SoundEngine_SetMixer_253* AK_SoundEngine_SetMixer_253_orig = nullptr;
t_AK_SoundEngine_SetMixer_253* AK_SoundEngine_SetMixer_253_bridge = nullptr;
enum AKRESULT AK_SoundEngine_SetMixer_253_hook(unsigned long a0, unsigned long a1)
{
    OnWwiseCall("AK::SoundEngine::SetMixer");
    return AK_SoundEngine_SetMixer_253_bridge(a0, a1);
}

t_AK_SoundEngine_SetMixer_254* AK_SoundEngine_SetMixer_254_orig = nullptr;
t_AK_SoundEngine_SetMixer_254* AK_SoundEngine_SetMixer_254_bridge = nullptr;
enum AKRESULT AK_SoundEngine_SetMixer_254_hook(char const * a0, unsigned long a1)
{
    OnWwiseCall("AK::SoundEngine::SetMixer");
    return AK_SoundEngine_SetMixer_254_bridge(a0, a1);
}

t_AK_SoundEngine_SetMixer_255* AK_SoundEngine_SetMixer_255_orig = nullptr;
t_AK_SoundEngine_SetMixer_255* AK_SoundEngine_SetMixer_255_bridge = nullptr;
enum AKRESULT AK_SoundEngine_SetMixer_255_hook(wchar_t const * a0, unsigned long a1)
{
    OnWwiseCall("AK::SoundEngine::SetMixer");
    return AK_SoundEngine_SetMixer_255_bridge(a0, a1);
}

t_AK_SoundEngine_SetMultipleObstructionAndOcclusion_257* AK_SoundEngine_SetMultipleObstructionAndOcclusion_257_orig = nullptr;
t_AK_SoundEngine_SetMultipleObstructionAndOcclusion_257* AK_SoundEngine_SetMultipleObstructionAndOcclusion_257_bridge = nullptr;
enum AKRESULT AK_SoundEngine_SetMultipleObstructionAndOcclusion_257_hook(unsigned __int64 a0, unsigned __int64 a1, struct AkObstructionOcclusionValues * a2, unsigned long a3)
{
    OnWwiseCall("AK::SoundEngine::SetMultipleObstructionAndOcclusion");
    return AK_SoundEngine_SetMultipleObstructionAndOcclusion_257_bridge(a0, a1, a2, a3);
}

t_AK_SoundEngine_SetMultiplePositions_258* AK_SoundEngine_SetMultiplePositions_258_orig = nullptr;
t_AK_SoundEngine_SetMultiplePositions_258* AK_SoundEngine_SetMultiplePositions_258_bridge = nullptr;
enum AKRESULT AK_SoundEngine_SetMultiplePositions_258_hook(unsigned __int64 a0, struct AkChannelEmitter const * a1, unsigned short a2, enum AK::SoundEngine::MultiPositionType a3)
{
    OnWwiseCall("AK::SoundEngine::SetMultiplePositions");
    return AK_SoundEngine_SetMultiplePositions_258_bridge(a0, a1, a2, a3);
}

t_AK_SoundEngine_SetMultiplePositions_259* AK_SoundEngine_SetMultiplePositions_259_orig = nullptr;
t_AK_SoundEngine_SetMultiplePositions_259* AK_SoundEngine_SetMultiplePositions_259_bridge = nullptr;
enum AKRESULT AK_SoundEngine_SetMultiplePositions_259_hook(unsigned __int64 a0, class AkTransform const * a1, unsigned short a2, enum AK::SoundEngine::MultiPositionType a3)
{
    OnWwiseCall("AK::SoundEngine::SetMultiplePositions");
    return AK_SoundEngine_SetMultiplePositions_259_bridge(a0, a1, a2, a3);
}

t_AK_SoundEngine_SetObjectObstructionAndOcclusion_260* AK_SoundEngine_SetObjectObstructionAndOcclusion_260_orig = nullptr;
t_AK_SoundEngine_SetObjectObstructionAndOcclusion_260* AK_SoundEngine_SetObjectObstructionAndOcclusion_260_bridge = nullptr;
enum AKRESULT AK_SoundEngine_SetObjectObstructionAndOcclusion_260_hook(unsigned __int64 a0, unsigned __int64 a1, float a2, float a3)
{
    OnWwiseCall("AK::SoundEngine::SetObjectObstructionAndOcclusion");
    LEASI_TRACE("  -> emitter={} listener={} obstruction={} occlusion={}", a0, a1, a2, a3);
    return AK_SoundEngine_SetObjectObstructionAndOcclusion_260_bridge(a0, a1, a2, a3);
}

t_AK_SoundEngine_SetOutputVolume_261* AK_SoundEngine_SetOutputVolume_261_orig = nullptr;
t_AK_SoundEngine_SetOutputVolume_261* AK_SoundEngine_SetOutputVolume_261_bridge = nullptr;
enum AKRESULT AK_SoundEngine_SetOutputVolume_261_hook(unsigned __int64 a0, float a1)
{
    OnWwiseCall("AK::SoundEngine::SetOutputVolume");
    return AK_SoundEngine_SetOutputVolume_261_bridge(a0, a1);
}

t_AK_SoundEngine_SetPanningRule_262* AK_SoundEngine_SetPanningRule_262_orig = nullptr;
t_AK_SoundEngine_SetPanningRule_262* AK_SoundEngine_SetPanningRule_262_bridge = nullptr;
enum AKRESULT AK_SoundEngine_SetPanningRule_262_hook(enum AkPanningRule a0, unsigned __int64 a1)
{
    OnWwiseCall("AK::SoundEngine::SetPanningRule");
    return AK_SoundEngine_SetPanningRule_262_bridge(a0, a1);
}

t_AK_MemoryMgr_SetPoolName_263* AK_MemoryMgr_SetPoolName_263_orig = nullptr;
t_AK_MemoryMgr_SetPoolName_263* AK_MemoryMgr_SetPoolName_263_bridge = nullptr;
enum AKRESULT AK_MemoryMgr_SetPoolName_263_hook(long a0, char const * a1)
{
    OnWwiseCall("AK::MemoryMgr::SetPoolName");
    return AK_MemoryMgr_SetPoolName_263_bridge(a0, a1);
}

t_AK_SoundEngine_SetPosition_265* AK_SoundEngine_SetPosition_265_orig = nullptr;
t_AK_SoundEngine_SetPosition_265* AK_SoundEngine_SetPosition_265_bridge = nullptr;
enum AKRESULT AK_SoundEngine_SetPosition_265_hook(unsigned __int64 a0, class AkTransform const & a1)
{
    OnWwiseCall("AK::SoundEngine::SetPosition");
    return AK_SoundEngine_SetPosition_265_bridge(a0, a1);
}

t_AK_SoundEngine_SetPositionInternal_266* AK_SoundEngine_SetPositionInternal_266_orig = nullptr;
t_AK_SoundEngine_SetPositionInternal_266* AK_SoundEngine_SetPositionInternal_266_bridge = nullptr;
enum AKRESULT AK_SoundEngine_SetPositionInternal_266_hook(unsigned __int64 a0, class AkTransform const & a1)
{
    OnWwiseCall("AK::SoundEngine::SetPositionInternal");
    return AK_SoundEngine_SetPositionInternal_266_bridge(a0, a1);
}

t_AK_SoundEngine_SetRandomSeed_275* AK_SoundEngine_SetRandomSeed_275_orig = nullptr;
t_AK_SoundEngine_SetRandomSeed_275* AK_SoundEngine_SetRandomSeed_275_bridge = nullptr;
void AK_SoundEngine_SetRandomSeed_275_hook(unsigned long a0)
{
    OnWwiseCall("AK::SoundEngine::SetRandomSeed");
    return AK_SoundEngine_SetRandomSeed_275_bridge(a0);
}

t_AK_SoundEngine_SetScalingFactor_276* AK_SoundEngine_SetScalingFactor_276_orig = nullptr;
t_AK_SoundEngine_SetScalingFactor_276* AK_SoundEngine_SetScalingFactor_276_bridge = nullptr;
enum AKRESULT AK_SoundEngine_SetScalingFactor_276_hook(unsigned __int64 a0, float a1)
{
    OnWwiseCall("AK::SoundEngine::SetScalingFactor");
    return AK_SoundEngine_SetScalingFactor_276_bridge(a0, a1);
}

t_AK_SoundEngine_SetSpeakerAngles_277* AK_SoundEngine_SetSpeakerAngles_277_orig = nullptr;
t_AK_SoundEngine_SetSpeakerAngles_277* AK_SoundEngine_SetSpeakerAngles_277_bridge = nullptr;
enum AKRESULT AK_SoundEngine_SetSpeakerAngles_277_hook(float const * a0, unsigned long a1, float a2, unsigned __int64 a3)
{
    OnWwiseCall("AK::SoundEngine::SetSpeakerAngles");
    return AK_SoundEngine_SetSpeakerAngles_277_bridge(a0, a1, a2, a3);
}

t_AK_SoundEngine_SetVolumeThreshold_284* AK_SoundEngine_SetVolumeThreshold_284_orig = nullptr;
t_AK_SoundEngine_SetVolumeThreshold_284* AK_SoundEngine_SetVolumeThreshold_284_bridge = nullptr;
enum AKRESULT AK_SoundEngine_SetVolumeThreshold_284_hook(float a0)
{
    OnWwiseCall("AK::SoundEngine::SetVolumeThreshold");
    return AK_SoundEngine_SetVolumeThreshold_284_bridge(a0);
}

t_AK_SoundEngine_StartOutputCapture_285* AK_SoundEngine_StartOutputCapture_285_orig = nullptr;
t_AK_SoundEngine_StartOutputCapture_285* AK_SoundEngine_StartOutputCapture_285_bridge = nullptr;
enum AKRESULT AK_SoundEngine_StartOutputCapture_285_hook(wchar_t const * a0)
{
    OnWwiseCall("AK::SoundEngine::StartOutputCapture");
    return AK_SoundEngine_StartOutputCapture_285_bridge(a0);
}

t_AK_SoundEngine_DynamicSequence_Stop_288* AK_SoundEngine_DynamicSequence_Stop_288_orig = nullptr;
t_AK_SoundEngine_DynamicSequence_Stop_288* AK_SoundEngine_DynamicSequence_Stop_288_bridge = nullptr;
enum AKRESULT AK_SoundEngine_DynamicSequence_Stop_288_hook(unsigned long a0, long a1, enum AkCurveInterpolation a2)
{
    OnWwiseCall("AK::SoundEngine::DynamicSequence::Stop");
    return AK_SoundEngine_DynamicSequence_Stop_288_bridge(a0, a1, a2);
}

t_AK_SoundEngine_StopMIDIOnEvent_290* AK_SoundEngine_StopMIDIOnEvent_290_orig = nullptr;
t_AK_SoundEngine_StopMIDIOnEvent_290* AK_SoundEngine_StopMIDIOnEvent_290_bridge = nullptr;
enum AKRESULT AK_SoundEngine_StopMIDIOnEvent_290_hook(unsigned long a0, unsigned __int64 a1)
{
    OnWwiseCall("AK::SoundEngine::StopMIDIOnEvent");
    return AK_SoundEngine_StopMIDIOnEvent_290_bridge(a0, a1);
}

t_AK_SoundEngine_StopOutputCapture_291* AK_SoundEngine_StopOutputCapture_291_orig = nullptr;
t_AK_SoundEngine_StopOutputCapture_291* AK_SoundEngine_StopOutputCapture_291_bridge = nullptr;
enum AKRESULT AK_SoundEngine_StopOutputCapture_291_hook()
{
    OnWwiseCall("AK::SoundEngine::StopOutputCapture");
    return AK_SoundEngine_StopOutputCapture_291_bridge();
}

t_AK_SoundEngine_StopSourcePlugin_295* AK_SoundEngine_StopSourcePlugin_295_orig = nullptr;
t_AK_SoundEngine_StopSourcePlugin_295* AK_SoundEngine_StopSourcePlugin_295_bridge = nullptr;
enum AKRESULT AK_SoundEngine_StopSourcePlugin_295_hook(unsigned long a0, unsigned long a1, unsigned long a2)
{
    OnWwiseCall("AK::SoundEngine::StopSourcePlugin");
    return AK_SoundEngine_StopSourcePlugin_295_bridge(a0, a1, a2);
}

t_AK_SoundEngine_Suspend_296* AK_SoundEngine_Suspend_296_orig = nullptr;
t_AK_SoundEngine_Suspend_296* AK_SoundEngine_Suspend_296_bridge = nullptr;
enum AKRESULT AK_SoundEngine_Suspend_296_hook(bool a0)
{
    OnWwiseCall("AK::SoundEngine::Suspend");
    return AK_SoundEngine_Suspend_296_bridge(a0);
}

t_AK_MusicEngine_Term_298* AK_MusicEngine_Term_298_orig = nullptr;
t_AK_MusicEngine_Term_298* AK_MusicEngine_Term_298_bridge = nullptr;
void AK_MusicEngine_Term_298_hook()
{
    OnWwiseCall("AK::MusicEngine::Term");
    return AK_MusicEngine_Term_298_bridge();
}

t_AK_SoundEngine_DynamicSequence_UnlockPlaylist_306* AK_SoundEngine_DynamicSequence_UnlockPlaylist_306_orig = nullptr;
t_AK_SoundEngine_DynamicSequence_UnlockPlaylist_306* AK_SoundEngine_DynamicSequence_UnlockPlaylist_306_bridge = nullptr;
enum AKRESULT AK_SoundEngine_DynamicSequence_UnlockPlaylist_306_hook(unsigned long a0)
{
    OnWwiseCall("AK::SoundEngine::DynamicSequence::UnlockPlaylist");
    return AK_SoundEngine_DynamicSequence_UnlockPlaylist_306_bridge(a0);
}

t_AK_SoundEngine_UnpinEventInStreamCache_307* AK_SoundEngine_UnpinEventInStreamCache_307_orig = nullptr;
t_AK_SoundEngine_UnpinEventInStreamCache_307* AK_SoundEngine_UnpinEventInStreamCache_307_bridge = nullptr;
enum AKRESULT AK_SoundEngine_UnpinEventInStreamCache_307_hook(unsigned long a0)
{
    OnWwiseCall("AK::SoundEngine::UnpinEventInStreamCache");
    return AK_SoundEngine_UnpinEventInStreamCache_307_bridge(a0);
}

t_AK_SoundEngine_UnpinEventInStreamCache_308* AK_SoundEngine_UnpinEventInStreamCache_308_orig = nullptr;
t_AK_SoundEngine_UnpinEventInStreamCache_308* AK_SoundEngine_UnpinEventInStreamCache_308_bridge = nullptr;
enum AKRESULT AK_SoundEngine_UnpinEventInStreamCache_308_hook(char const * a0)
{
    OnWwiseCall("AK::SoundEngine::UnpinEventInStreamCache");
    return AK_SoundEngine_UnpinEventInStreamCache_308_bridge(a0);
}

t_AK_SoundEngine_UnpinEventInStreamCache_309* AK_SoundEngine_UnpinEventInStreamCache_309_orig = nullptr;
t_AK_SoundEngine_UnpinEventInStreamCache_309* AK_SoundEngine_UnpinEventInStreamCache_309_bridge = nullptr;
enum AKRESULT AK_SoundEngine_UnpinEventInStreamCache_309_hook(wchar_t const * a0)
{
    OnWwiseCall("AK::SoundEngine::UnpinEventInStreamCache");
    return AK_SoundEngine_UnpinEventInStreamCache_309_bridge(a0);
}

t_AK_SoundEngine_UnregisterAllGameObj_310* AK_SoundEngine_UnregisterAllGameObj_310_orig = nullptr;
t_AK_SoundEngine_UnregisterAllGameObj_310* AK_SoundEngine_UnregisterAllGameObj_310_bridge = nullptr;
enum AKRESULT AK_SoundEngine_UnregisterAllGameObj_310_hook()
{
    OnWwiseCall("AK::SoundEngine::UnregisterAllGameObj");
    return AK_SoundEngine_UnregisterAllGameObj_310_bridge();
}

t_AK_SoundEngine_UnregisterAudioDeviceStatusCallback_311* AK_SoundEngine_UnregisterAudioDeviceStatusCallback_311_orig = nullptr;
t_AK_SoundEngine_UnregisterAudioDeviceStatusCallback_311* AK_SoundEngine_UnregisterAudioDeviceStatusCallback_311_bridge = nullptr;
enum AKRESULT AK_SoundEngine_UnregisterAudioDeviceStatusCallback_311_hook()
{
    OnWwiseCall("AK::SoundEngine::UnregisterAudioDeviceStatusCallback");
    return AK_SoundEngine_UnregisterAudioDeviceStatusCallback_311_bridge();
}

t_AK_SoundEngine_UnregisterGlobalCallback_313* AK_SoundEngine_UnregisterGlobalCallback_313_orig = nullptr;
t_AK_SoundEngine_UnregisterGlobalCallback_313* AK_SoundEngine_UnregisterGlobalCallback_313_bridge = nullptr;
enum AKRESULT AK_SoundEngine_UnregisterGlobalCallback_313_hook(void (__cdecl* a0)(class AK::IAkGlobalPluginContext *,enum AkGlobalCallbackLocation,void *), unsigned long a1)
{
    OnWwiseCall("AK::SoundEngine::UnregisterGlobalCallback");
    return AK_SoundEngine_UnregisterGlobalCallback_313_bridge(a0, a1);
}

t_AK_SoundEngine_UnsetMedia_314* AK_SoundEngine_UnsetMedia_314_orig = nullptr;
t_AK_SoundEngine_UnsetMedia_314* AK_SoundEngine_UnsetMedia_314_bridge = nullptr;
enum AKRESULT AK_SoundEngine_UnsetMedia_314_hook(struct AkSourceSettings * a0, unsigned long a1)
{
    OnWwiseCall("AK::SoundEngine::UnsetMedia");
    return AK_SoundEngine_UnsetMedia_314_bridge(a0, a1);
}

t_AK_SoundEngine_WakeupFromSuspend_315* AK_SoundEngine_WakeupFromSuspend_315_orig = nullptr;
t_AK_SoundEngine_WakeupFromSuspend_315* AK_SoundEngine_WakeupFromSuspend_315_bridge = nullptr;
enum AKRESULT AK_SoundEngine_WakeupFromSuspend_315_hook()
{
    OnWwiseCall("AK::SoundEngine::WakeupFromSuspend");
    return AK_SoundEngine_WakeupFromSuspend_315_bridge();
}

t_AK_WriteBytesCount_WriteBytes_316* AK_WriteBytesCount_WriteBytes_316_orig = nullptr;
t_AK_WriteBytesCount_WriteBytes_316* AK_WriteBytesCount_WriteBytes_316_bridge = nullptr;
bool AK_WriteBytesCount_WriteBytes_316_hook(AK::WriteBytesCount* a0, void const * a1, long a2, long & a3)
{
    OnWwiseCall("AK::WriteBytesCount::WriteBytes");
    return AK_WriteBytesCount_WriteBytes_316_bridge(a0, a1, a2, a3);
}

t_AK_WriteBytesMem_WriteBytes_317* AK_WriteBytesMem_WriteBytes_317_orig = nullptr;
t_AK_WriteBytesMem_WriteBytes_317* AK_WriteBytesMem_WriteBytes_317_bridge = nullptr;
bool AK_WriteBytesMem_WriteBytes_317_hook(AK::WriteBytesMem* a0, void const * a1, long a2, long & a3)
{
    OnWwiseCall("AK::WriteBytesMem::WriteBytes");
    return AK_WriteBytesMem_WriteBytes_317_bridge(a0, a1, a2, a3);
}

WwiseHookEntry g_WwiseHooks[kWwiseHookCount] = {
    { "?Init@MemoryMgr@AK@@YA?AW4AKRESULT@@PEAUAkMemSettings@@@Z", (void**)&AK_MemoryMgr_Init_128_orig, (void**)&AK_MemoryMgr_Init_128_bridge, (void*)&AK_MemoryMgr_Init_128_hook },
    { "?Create@StreamMgr@AK@@YAPEAVIAkStreamMgr@2@AEBUAkStreamMgrSettings@@@Z", (void**)&AK_StreamMgr_Create_37_orig, (void**)&AK_StreamMgr_Create_37_bridge, (void*)&AK_StreamMgr_Create_37_hook },
    { "?CreateDevice@StreamMgr@AK@@YAKAEBUAkDeviceSettings@@PEAVIAkLowLevelIOHook@12@@Z", (void**)&AK_StreamMgr_CreateDevice_38_orig, (void**)&AK_StreamMgr_CreateDevice_38_bridge, (void*)&AK_StreamMgr_CreateDevice_38_hook },
    { "?GetDefaultInitSettings@SoundEngine@AK@@YAXAEAUAkInitSettings@@@Z", (void**)&AK_SoundEngine_GetDefaultInitSettings_67_orig, (void**)&AK_SoundEngine_GetDefaultInitSettings_67_bridge, (void*)&AK_SoundEngine_GetDefaultInitSettings_67_hook },
    { "?GetDefaultPlatformInitSettings@SoundEngine@AK@@YAXAEAUAkPlatformInitSettings@@@Z", (void**)&AK_SoundEngine_GetDefaultPlatformInitSettings_68_orig, (void**)&AK_SoundEngine_GetDefaultPlatformInitSettings_68_bridge, (void*)&AK_SoundEngine_GetDefaultPlatformInitSettings_68_hook },
    { "?Init@SoundEngine@AK@@YA?AW4AKRESULT@@PEAUAkInitSettings@@PEAUAkPlatformInitSettings@@@Z", (void**)&AK_SoundEngine_Init_130_orig, (void**)&AK_SoundEngine_Init_130_bridge, (void*)&AK_SoundEngine_Init_130_hook },
    { "?Init@MusicEngine@AK@@YA?AW4AKRESULT@@PEAUAkMusicSettings@@@Z", (void**)&AK_MusicEngine_Init_129_orig, (void**)&AK_MusicEngine_Init_129_bridge, (void*)&AK_MusicEngine_Init_129_hook },
    { "?RegisterGlobalCallback@SoundEngine@AK@@YA?AW4AKRESULT@@P6AXPEAVIAkGlobalPluginContext@2@W4AkGlobalCallbackLocation@@PEAX@ZK2W4AkPluginType@@KK@Z", (void**)&AK_SoundEngine_RegisterGlobalCallback_200_orig, (void**)&AK_SoundEngine_RegisterGlobalCallback_200_bridge, (void*)&AK_SoundEngine_RegisterGlobalCallback_200_hook },
    { "?SetDefaultListeners@SoundEngine@AK@@YA?AW4AKRESULT@@PEB_KK@Z", (void**)&AK_SoundEngine_SetDefaultListeners_242_orig, (void**)&AK_SoundEngine_SetDefaultListeners_242_bridge, (void*)&AK_SoundEngine_SetDefaultListeners_242_hook },
    { "?PostEvent@SoundEngine@AK@@YAKK_KKP6AXW4AkCallbackType@@PEAUAkCallbackInfo@@@ZPEAXKPEAUAkExternalSourceInfo@@K@Z", (void**)&AK_SoundEngine_PostEvent_161_orig, (void**)&AK_SoundEngine_PostEvent_161_bridge, (void*)&AK_SoundEngine_PostEvent_161_hook },
    { "?PostEvent@SoundEngine@AK@@YAKPEBD_KKP6AXW4AkCallbackType@@PEAUAkCallbackInfo@@@ZPEAXKPEAUAkExternalSourceInfo@@K@Z", (void**)&AK_SoundEngine_PostEvent_162_orig, (void**)&AK_SoundEngine_PostEvent_162_bridge, (void*)&AK_SoundEngine_PostEvent_162_hook },
    { "?PostEvent@SoundEngine@AK@@YAKPEB_W_KKP6AXW4AkCallbackType@@PEAUAkCallbackInfo@@@ZPEAXKPEAUAkExternalSourceInfo@@K@Z", (void**)&AK_SoundEngine_PostEvent_163_orig, (void**)&AK_SoundEngine_PostEvent_163_bridge, (void*)&AK_SoundEngine_PostEvent_163_hook },
    { "?LoadBank@SoundEngine@AK@@YA?AW4AKRESULT@@KJ@Z", (void**)&AK_SoundEngine_LoadBank_134_orig, (void**)&AK_SoundEngine_LoadBank_134_bridge, (void*)&AK_SoundEngine_LoadBank_134_hook },
    { "?LoadBank@SoundEngine@AK@@YA?AW4AKRESULT@@KP6AXKPEBXW43@JPEAX@Z2J@Z", (void**)&AK_SoundEngine_LoadBank_135_orig, (void**)&AK_SoundEngine_LoadBank_135_bridge, (void*)&AK_SoundEngine_LoadBank_135_hook },
    { "?LoadBank@SoundEngine@AK@@YA?AW4AKRESULT@@PEBDJAEAK@Z", (void**)&AK_SoundEngine_LoadBank_136_orig, (void**)&AK_SoundEngine_LoadBank_136_bridge, (void*)&AK_SoundEngine_LoadBank_136_hook },
    { "?LoadBank@SoundEngine@AK@@YA?AW4AKRESULT@@PEBDP6AXKPEBXW43@JPEAX@Z3JAEAK@Z", (void**)&AK_SoundEngine_LoadBank_137_orig, (void**)&AK_SoundEngine_LoadBank_137_bridge, (void*)&AK_SoundEngine_LoadBank_137_hook },
    { "?LoadBank@SoundEngine@AK@@YA?AW4AKRESULT@@PEBXKAEAK@Z", (void**)&AK_SoundEngine_LoadBank_138_orig, (void**)&AK_SoundEngine_LoadBank_138_bridge, (void*)&AK_SoundEngine_LoadBank_138_hook },
    { "?LoadBank@SoundEngine@AK@@YA?AW4AKRESULT@@PEBXKJAEAK@Z", (void**)&AK_SoundEngine_LoadBank_139_orig, (void**)&AK_SoundEngine_LoadBank_139_bridge, (void*)&AK_SoundEngine_LoadBank_139_hook },
    { "?LoadBank@SoundEngine@AK@@YA?AW4AKRESULT@@PEBXKP6AXK0W43@JPEAX@Z2AEAK@Z", (void**)&AK_SoundEngine_LoadBank_140_orig, (void**)&AK_SoundEngine_LoadBank_140_bridge, (void*)&AK_SoundEngine_LoadBank_140_hook },
    { "?LoadBank@SoundEngine@AK@@YA?AW4AKRESULT@@PEBXKP6AXK0W43@JPEAX@Z2JAEAK@Z", (void**)&AK_SoundEngine_LoadBank_141_orig, (void**)&AK_SoundEngine_LoadBank_141_bridge, (void*)&AK_SoundEngine_LoadBank_141_hook },
    { "?LoadBank@SoundEngine@AK@@YA?AW4AKRESULT@@PEB_WJAEAK@Z", (void**)&AK_SoundEngine_LoadBank_142_orig, (void**)&AK_SoundEngine_LoadBank_142_bridge, (void*)&AK_SoundEngine_LoadBank_142_hook },
    { "?LoadBank@SoundEngine@AK@@YA?AW4AKRESULT@@PEB_WP6AXKPEBXW43@JPEAX@Z3JAEAK@Z", (void**)&AK_SoundEngine_LoadBank_143_orig, (void**)&AK_SoundEngine_LoadBank_143_bridge, (void*)&AK_SoundEngine_LoadBank_143_hook },
    { "?UnloadBank@SoundEngine@AK@@YA?AW4AKRESULT@@KPEBXP6AXK0W43@JPEAX@Z2@Z", (void**)&AK_SoundEngine_UnloadBank_300_orig, (void**)&AK_SoundEngine_UnloadBank_300_bridge, (void*)&AK_SoundEngine_UnloadBank_300_hook },
    { "?UnloadBank@SoundEngine@AK@@YA?AW4AKRESULT@@KPEBXPEAJ@Z", (void**)&AK_SoundEngine_UnloadBank_301_orig, (void**)&AK_SoundEngine_UnloadBank_301_bridge, (void*)&AK_SoundEngine_UnloadBank_301_hook },
    { "?UnloadBank@SoundEngine@AK@@YA?AW4AKRESULT@@PEBDPEBXP6AXK1W43@JPEAX@Z3@Z", (void**)&AK_SoundEngine_UnloadBank_302_orig, (void**)&AK_SoundEngine_UnloadBank_302_bridge, (void*)&AK_SoundEngine_UnloadBank_302_hook },
    { "?UnloadBank@SoundEngine@AK@@YA?AW4AKRESULT@@PEBDPEBXPEAJ@Z", (void**)&AK_SoundEngine_UnloadBank_303_orig, (void**)&AK_SoundEngine_UnloadBank_303_bridge, (void*)&AK_SoundEngine_UnloadBank_303_hook },
    { "?UnloadBank@SoundEngine@AK@@YA?AW4AKRESULT@@PEB_WPEBXP6AXK1W43@JPEAX@Z3@Z", (void**)&AK_SoundEngine_UnloadBank_304_orig, (void**)&AK_SoundEngine_UnloadBank_304_bridge, (void*)&AK_SoundEngine_UnloadBank_304_hook },
    { "?UnloadBank@SoundEngine@AK@@YA?AW4AKRESULT@@PEB_WPEBXPEAJ@Z", (void**)&AK_SoundEngine_UnloadBank_305_orig, (void**)&AK_SoundEngine_UnloadBank_305_bridge, (void*)&AK_SoundEngine_UnloadBank_305_hook },
    { "?UnregisterGameObj@SoundEngine@AK@@YA?AW4AKRESULT@@_K@Z", (void**)&AK_SoundEngine_UnregisterGameObj_312_orig, (void**)&AK_SoundEngine_UnregisterGameObj_312_bridge, (void*)&AK_SoundEngine_UnregisterGameObj_312_hook },
    { "?SetSwitch@SoundEngine@AK@@YA?AW4AKRESULT@@KK_K@Z", (void**)&AK_SoundEngine_SetSwitch_281_orig, (void**)&AK_SoundEngine_SetSwitch_281_bridge, (void*)&AK_SoundEngine_SetSwitch_281_hook },
    { "?SetSwitch@SoundEngine@AK@@YA?AW4AKRESULT@@PEBD0_K@Z", (void**)&AK_SoundEngine_SetSwitch_282_orig, (void**)&AK_SoundEngine_SetSwitch_282_bridge, (void*)&AK_SoundEngine_SetSwitch_282_hook },
    { "?SetSwitch@SoundEngine@AK@@YA?AW4AKRESULT@@PEB_W0_K@Z", (void**)&AK_SoundEngine_SetSwitch_283_orig, (void**)&AK_SoundEngine_SetSwitch_283_bridge, (void*)&AK_SoundEngine_SetSwitch_283_hook },
    { "?SetState@SoundEngine@AK@@YA?AW4AKRESULT@@KK@Z", (void**)&AK_SoundEngine_SetState_278_orig, (void**)&AK_SoundEngine_SetState_278_bridge, (void*)&AK_SoundEngine_SetState_278_hook },
    { "?SetState@SoundEngine@AK@@YA?AW4AKRESULT@@PEBD0@Z", (void**)&AK_SoundEngine_SetState_279_orig, (void**)&AK_SoundEngine_SetState_279_bridge, (void*)&AK_SoundEngine_SetState_279_hook },
    { "?SetState@SoundEngine@AK@@YA?AW4AKRESULT@@PEB_W0@Z", (void**)&AK_SoundEngine_SetState_280_orig, (void**)&AK_SoundEngine_SetState_280_bridge, (void*)&AK_SoundEngine_SetState_280_hook },
    { "?SetRTPCValue@SoundEngine@AK@@YA?AW4AKRESULT@@KM_KJW4AkCurveInterpolation@@_N@Z", (void**)&AK_SoundEngine_SetRTPCValue_269_orig, (void**)&AK_SoundEngine_SetRTPCValue_269_bridge, (void*)&AK_SoundEngine_SetRTPCValue_269_hook },
    { "?SetRTPCValue@SoundEngine@AK@@YA?AW4AKRESULT@@PEBDM_KJW4AkCurveInterpolation@@_N@Z", (void**)&AK_SoundEngine_SetRTPCValue_270_orig, (void**)&AK_SoundEngine_SetRTPCValue_270_bridge, (void*)&AK_SoundEngine_SetRTPCValue_270_hook },
    { "?SetRTPCValue@SoundEngine@AK@@YA?AW4AKRESULT@@PEB_WM_KJW4AkCurveInterpolation@@_N@Z", (void**)&AK_SoundEngine_SetRTPCValue_271_orig, (void**)&AK_SoundEngine_SetRTPCValue_271_bridge, (void*)&AK_SoundEngine_SetRTPCValue_271_hook },
    { "?SetRTPCValueByPlayingID@SoundEngine@AK@@YA?AW4AKRESULT@@KMKJW4AkCurveInterpolation@@_N@Z", (void**)&AK_SoundEngine_SetRTPCValueByPlayingID_272_orig, (void**)&AK_SoundEngine_SetRTPCValueByPlayingID_272_bridge, (void*)&AK_SoundEngine_SetRTPCValueByPlayingID_272_hook },
    { "?SetRTPCValueByPlayingID@SoundEngine@AK@@YA?AW4AKRESULT@@PEBDMKJW4AkCurveInterpolation@@_N@Z", (void**)&AK_SoundEngine_SetRTPCValueByPlayingID_273_orig, (void**)&AK_SoundEngine_SetRTPCValueByPlayingID_273_bridge, (void*)&AK_SoundEngine_SetRTPCValueByPlayingID_273_hook },
    { "?SetRTPCValueByPlayingID@SoundEngine@AK@@YA?AW4AKRESULT@@PEB_WMKJW4AkCurveInterpolation@@_N@Z", (void**)&AK_SoundEngine_SetRTPCValueByPlayingID_274_orig, (void**)&AK_SoundEngine_SetRTPCValueByPlayingID_274_bridge, (void*)&AK_SoundEngine_SetRTPCValueByPlayingID_274_hook },
    { "?StopAll@SoundEngine@AK@@YAX_K@Z", (void**)&AK_SoundEngine_StopAll_289_orig, (void**)&AK_SoundEngine_StopAll_289_bridge, (void*)&AK_SoundEngine_StopAll_289_hook },
    { "?Term@SoundEngine@AK@@YAXXZ", (void**)&AK_SoundEngine_Term_299_orig, (void**)&AK_SoundEngine_Term_299_bridge, (void*)&AK_SoundEngine_Term_299_hook },
    { "?Term@MemoryMgr@AK@@YAXXZ", (void**)&AK_MemoryMgr_Term_297_orig, (void**)&AK_MemoryMgr_Term_297_bridge, (void*)&AK_MemoryMgr_Term_297_hook },
    { "?AddDefaultListener@SoundEngine@AK@@YA?AW4AKRESULT@@_K@Z", (void**)&AK_SoundEngine_AddDefaultListener_14_orig, (void**)&AK_SoundEngine_AddDefaultListener_14_bridge, (void*)&AK_SoundEngine_AddDefaultListener_14_hook },
    { "?AddLanguageChangeObserver@StreamMgr@AK@@YA?AW4AKRESULT@@P6AXQEB_WPEAX@Z1@Z", (void**)&AK_StreamMgr_AddLanguageChangeObserver_15_orig, (void**)&AK_StreamMgr_AddLanguageChangeObserver_15_bridge, (void*)&AK_StreamMgr_AddLanguageChangeObserver_15_hook },
    { "?AddListener@SoundEngine@AK@@YA?AW4AKRESULT@@_K0@Z", (void**)&AK_SoundEngine_AddListener_16_orig, (void**)&AK_SoundEngine_AddListener_16_bridge, (void*)&AK_SoundEngine_AddListener_16_hook },
    { "?AddOutput@SoundEngine@AK@@YA?AW4AKRESULT@@AEBUAkOutputSettings@@PEA_KPEB_KK@Z", (void**)&AK_SoundEngine_AddOutput_17_orig, (void**)&AK_SoundEngine_AddOutput_17_bridge, (void*)&AK_SoundEngine_AddOutput_17_hook },
    { "?AddOutputCaptureMarker@SoundEngine@AK@@YA?AW4AKRESULT@@PEBD@Z", (void**)&AK_SoundEngine_AddOutputCaptureMarker_18_orig, (void**)&AK_SoundEngine_AddOutputCaptureMarker_18_bridge, (void*)&AK_SoundEngine_AddOutputCaptureMarker_18_hook },
    { "?Break@DynamicSequence@SoundEngine@AK@@YA?AW4AKRESULT@@K@Z", (void**)&AK_SoundEngine_DynamicSequence_Break_21_orig, (void**)&AK_SoundEngine_DynamicSequence_Break_21_bridge, (void*)&AK_SoundEngine_DynamicSequence_Break_21_hook },
    { "?Bytes@WriteBytesMem@AK@@UEBAPEAEXZ", (void**)&AK_WriteBytesMem_Bytes_23_orig, (void**)&AK_WriteBytesMem_Bytes_23_bridge, (void*)&AK_WriteBytesMem_Bytes_23_hook },
    { "?CancelBankCallbackCookie@SoundEngine@AK@@YAXPEAX@Z", (void**)&AK_SoundEngine_CancelBankCallbackCookie_24_orig, (void**)&AK_SoundEngine_CancelBankCallbackCookie_24_bridge, (void*)&AK_SoundEngine_CancelBankCallbackCookie_24_hook },
    { "?CancelEventCallback@SoundEngine@AK@@YAXK@Z", (void**)&AK_SoundEngine_CancelEventCallback_25_orig, (void**)&AK_SoundEngine_CancelEventCallback_25_bridge, (void*)&AK_SoundEngine_CancelEventCallback_25_hook },
    { "?CancelEventCallbackCookie@SoundEngine@AK@@YAXPEAX@Z", (void**)&AK_SoundEngine_CancelEventCallbackCookie_26_orig, (void**)&AK_SoundEngine_CancelEventCallbackCookie_26_bridge, (void*)&AK_SoundEngine_CancelEventCallbackCookie_26_hook },
    { "?CancelEventCallbackGameObject@SoundEngine@AK@@YAX_K@Z", (void**)&AK_SoundEngine_CancelEventCallbackGameObject_27_orig, (void**)&AK_SoundEngine_CancelEventCallbackGameObject_27_bridge, (void*)&AK_SoundEngine_CancelEventCallbackGameObject_27_hook },
    { "?CheckPoolId@MemoryMgr@AK@@YA?AW4AKRESULT@@J@Z", (void**)&AK_MemoryMgr_CheckPoolId_28_orig, (void**)&AK_MemoryMgr_CheckPoolId_28_bridge, (void*)&AK_MemoryMgr_CheckPoolId_28_hook },
    { "?Clear@WriteBytesCount@AK@@UEAAXXZ", (void**)&AK_WriteBytesCount_Clear_29_orig, (void**)&AK_WriteBytesCount_Clear_29_bridge, (void*)&AK_WriteBytesCount_Clear_29_hook },
    { "?Clear@WriteBytesMem@AK@@UEAAXXZ", (void**)&AK_WriteBytesMem_Clear_30_orig, (void**)&AK_WriteBytesMem_Clear_30_bridge, (void*)&AK_WriteBytesMem_Clear_30_hook },
    { "?ClearBanks@SoundEngine@AK@@YA?AW4AKRESULT@@XZ", (void**)&AK_SoundEngine_ClearBanks_31_orig, (void**)&AK_SoundEngine_ClearBanks_31_bridge, (void*)&AK_SoundEngine_ClearBanks_31_hook },
    { "?ClearPreparedEvents@SoundEngine@AK@@YA?AW4AKRESULT@@XZ", (void**)&AK_SoundEngine_ClearPreparedEvents_32_orig, (void**)&AK_SoundEngine_ClearPreparedEvents_32_bridge, (void*)&AK_SoundEngine_ClearPreparedEvents_32_hook },
    { "?Close@DynamicSequence@SoundEngine@AK@@YA?AW4AKRESULT@@K@Z", (void**)&AK_SoundEngine_DynamicSequence_Close_33_orig, (void**)&AK_SoundEngine_DynamicSequence_Close_33_bridge, (void*)&AK_SoundEngine_DynamicSequence_Close_33_hook },
    { "?Count@WriteBytesCount@AK@@UEBAJXZ", (void**)&AK_WriteBytesCount_Count_35_orig, (void**)&AK_WriteBytesCount_Count_35_bridge, (void*)&AK_WriteBytesCount_Count_35_hook },
    { "?CreatePool@MemoryMgr@AK@@YAJPEAXKKKK@Z", (void**)&AK_MemoryMgr_CreatePool_39_orig, (void**)&AK_MemoryMgr_CreatePool_39_bridge, (void*)&AK_MemoryMgr_CreatePool_39_hook },
    { "?DecodeBank@SoundEngine@AK@@YA?AW4AKRESULT@@PEBXKJAEAPEAXAEAK@Z", (void**)&AK_SoundEngine_DecodeBank_40_orig, (void**)&AK_SoundEngine_DecodeBank_40_bridge, (void*)&AK_SoundEngine_DecodeBank_40_hook },
    { "?DestroyDevice@StreamMgr@AK@@YA?AW4AKRESULT@@K@Z", (void**)&AK_StreamMgr_DestroyDevice_41_orig, (void**)&AK_StreamMgr_DestroyDevice_41_bridge, (void*)&AK_StreamMgr_DestroyDevice_41_hook },
    { "?DestroyPool@MemoryMgr@AK@@YA?AW4AKRESULT@@J@Z", (void**)&AK_MemoryMgr_DestroyPool_42_orig, (void**)&AK_MemoryMgr_DestroyPool_42_bridge, (void*)&AK_MemoryMgr_DestroyPool_42_hook },
    { "?Detach@WriteBytesMem@AK@@UEAAPEAEXZ", (void**)&AK_WriteBytesMem_Detach_44_orig, (void**)&AK_WriteBytesMem_Detach_44_bridge, (void*)&AK_WriteBytesMem_Detach_44_hook },
    { "?ExecuteActionOnEvent@SoundEngine@AK@@YA?AW4AKRESULT@@KW4AkActionOnEventType@12@_KJW4AkCurveInterpolation@@K@Z", (void**)&AK_SoundEngine_ExecuteActionOnEvent_45_orig, (void**)&AK_SoundEngine_ExecuteActionOnEvent_45_bridge, (void*)&AK_SoundEngine_ExecuteActionOnEvent_45_hook },
    { "?ExecuteActionOnEvent@SoundEngine@AK@@YA?AW4AKRESULT@@PEBDW4AkActionOnEventType@12@_KJW4AkCurveInterpolation@@K@Z", (void**)&AK_SoundEngine_ExecuteActionOnEvent_46_orig, (void**)&AK_SoundEngine_ExecuteActionOnEvent_46_bridge, (void*)&AK_SoundEngine_ExecuteActionOnEvent_46_hook },
    { "?ExecuteActionOnEvent@SoundEngine@AK@@YA?AW4AKRESULT@@PEB_WW4AkActionOnEventType@12@_KJW4AkCurveInterpolation@@K@Z", (void**)&AK_SoundEngine_ExecuteActionOnEvent_47_orig, (void**)&AK_SoundEngine_ExecuteActionOnEvent_47_bridge, (void*)&AK_SoundEngine_ExecuteActionOnEvent_47_hook },
    { "?Falign@MemoryMgr@AK@@YA?AW4AKRESULT@@JPEAX@Z", (void**)&AK_MemoryMgr_Falign_49_orig, (void**)&AK_MemoryMgr_Falign_49_bridge, (void*)&AK_MemoryMgr_Falign_49_hook },
    { "?FlushAllCaches@StreamMgr@AK@@YAXXZ", (void**)&AK_StreamMgr_FlushAllCaches_50_orig, (void**)&AK_StreamMgr_FlushAllCaches_50_bridge, (void*)&AK_StreamMgr_FlushAllCaches_50_hook },
    { "?Free@MemoryMgr@AK@@YA?AW4AKRESULT@@JPEAX@Z", (void**)&AK_MemoryMgr_Free_51_orig, (void**)&AK_MemoryMgr_Free_51_bridge, (void*)&AK_MemoryMgr_Free_51_hook },
    { "?GetActiveGameObjects@Query@SoundEngine@AK@@YA?AW4AKRESULT@@AEAV?$AkArray@_K_KU?$AkArrayAllocatorNoAlign@U_ArrayPoolDefault@@@@$0CA@U?$AkAssignmentMovePolicy@_K@@@@@Z", (void**)&AK_SoundEngine_Query_GetActiveGameObjects_52_orig, (void**)&AK_SoundEngine_Query_GetActiveGameObjects_52_bridge, (void*)&AK_SoundEngine_Query_GetActiveGameObjects_52_hook },
    { "?GetAudioSettings@SoundEngine@AK@@YA?AW4AKRESULT@@AEAUAkAudioSettings@@@Z", (void**)&AK_SoundEngine_GetAudioSettings_53_orig, (void**)&AK_SoundEngine_GetAudioSettings_53_bridge, (void*)&AK_SoundEngine_GetAudioSettings_53_hook },
    { "?GetBackgroundMusicMute@SoundEngine@AK@@YA_NXZ", (void**)&AK_SoundEngine_GetBackgroundMusicMute_54_orig, (void**)&AK_SoundEngine_GetBackgroundMusicMute_54_bridge, (void*)&AK_SoundEngine_GetBackgroundMusicMute_54_hook },
    { "?GetBlock@MemoryMgr@AK@@YAPEAXJ@Z", (void**)&AK_MemoryMgr_GetBlock_55_orig, (void**)&AK_MemoryMgr_GetBlock_55_bridge, (void*)&AK_MemoryMgr_GetBlock_55_hook },
    { "?GetBlockSize@MemoryMgr@AK@@YAKJ@Z", (void**)&AK_MemoryMgr_GetBlockSize_56_orig, (void**)&AK_MemoryMgr_GetBlockSize_56_bridge, (void*)&AK_MemoryMgr_GetBlockSize_56_hook },
    { "?GetBufferStatusForPinnedEvent@SoundEngine@AK@@YA?AW4AKRESULT@@KAEAMAEA_N@Z", (void**)&AK_SoundEngine_GetBufferStatusForPinnedEvent_57_orig, (void**)&AK_SoundEngine_GetBufferStatusForPinnedEvent_57_bridge, (void*)&AK_SoundEngine_GetBufferStatusForPinnedEvent_57_hook },
    { "?GetBufferStatusForPinnedEvent@SoundEngine@AK@@YA?AW4AKRESULT@@PEBDAEAMAEA_N@Z", (void**)&AK_SoundEngine_GetBufferStatusForPinnedEvent_58_orig, (void**)&AK_SoundEngine_GetBufferStatusForPinnedEvent_58_bridge, (void*)&AK_SoundEngine_GetBufferStatusForPinnedEvent_58_hook },
    { "?GetBufferStatusForPinnedEvent@SoundEngine@AK@@YA?AW4AKRESULT@@PEB_WAEAMAEA_N@Z", (void**)&AK_SoundEngine_GetBufferStatusForPinnedEvent_59_orig, (void**)&AK_SoundEngine_GetBufferStatusForPinnedEvent_59_bridge, (void*)&AK_SoundEngine_GetBufferStatusForPinnedEvent_59_hook },
    { "?GetBufferTick@SoundEngine@AK@@YAKXZ", (void**)&AK_SoundEngine_GetBufferTick_60_orig, (void**)&AK_SoundEngine_GetBufferTick_60_bridge, (void*)&AK_SoundEngine_GetBufferTick_60_hook },
    { "?GetContainerHistory@SoundEngine@AK@@YA?AW4AKRESULT@@PEAVIWriteBytes@2@@Z", (void**)&AK_SoundEngine_GetContainerHistory_61_orig, (void**)&AK_SoundEngine_GetContainerHistory_61_bridge, (void*)&AK_SoundEngine_GetContainerHistory_61_hook },
    { "?GetCurrentLanguage@StreamMgr@AK@@YAPEB_WXZ", (void**)&AK_StreamMgr_GetCurrentLanguage_62_orig, (void**)&AK_StreamMgr_GetCurrentLanguage_62_bridge, (void*)&AK_StreamMgr_GetCurrentLanguage_62_hook },
    { "?GetDefaultDeviceSettings@StreamMgr@AK@@YAXAEAUAkDeviceSettings@@@Z", (void**)&AK_StreamMgr_GetDefaultDeviceSettings_65_orig, (void**)&AK_StreamMgr_GetDefaultDeviceSettings_65_bridge, (void*)&AK_StreamMgr_GetDefaultDeviceSettings_65_hook },
    { "?GetDefaultInitSettings@MusicEngine@AK@@YAXAEAUAkMusicSettings@@@Z", (void**)&AK_MusicEngine_GetDefaultInitSettings_66_orig, (void**)&AK_MusicEngine_GetDefaultInitSettings_66_bridge, (void*)&AK_MusicEngine_GetDefaultInitSettings_66_hook },
    { "?GetDefaultSettings@StreamMgr@AK@@YAXAEAUAkStreamMgrSettings@@@Z", (void**)&AK_StreamMgr_GetDefaultSettings_69_orig, (void**)&AK_StreamMgr_GetDefaultSettings_69_bridge, (void*)&AK_StreamMgr_GetDefaultSettings_69_hook },
    { "?GetDeviceID@AK@@YAKPEAUIMMDevice@@@Z", (void**)&AK_GetDeviceID_70_orig, (void**)&AK_GetDeviceID_70_bridge, (void*)&AK_GetDeviceID_70_hook },
    { "?GetDeviceIDFromName@AK@@YAKPEA_W@Z", (void**)&AK_GetDeviceIDFromName_71_orig, (void**)&AK_GetDeviceIDFromName_71_bridge, (void*)&AK_GetDeviceIDFromName_71_hook },
    { "?GetDeviceSpatialAudioSupport@SoundEngine@AK@@YA?AW4AKRESULT@@K@Z", (void**)&AK_SoundEngine_GetDeviceSpatialAudioSupport_72_orig, (void**)&AK_SoundEngine_GetDeviceSpatialAudioSupport_72_bridge, (void*)&AK_SoundEngine_GetDeviceSpatialAudioSupport_72_hook },
    { "?GetDialogueEventCustomPropertyValue@DynamicDialogue@SoundEngine@AK@@YA?AW4AKRESULT@@KKAEAJ@Z", (void**)&AK_SoundEngine_DynamicDialogue_GetDialogueEventCustomPropertyValue_73_orig, (void**)&AK_SoundEngine_DynamicDialogue_GetDialogueEventCustomPropertyValue_73_bridge, (void*)&AK_SoundEngine_DynamicDialogue_GetDialogueEventCustomPropertyValue_73_hook },
    { "?GetDialogueEventCustomPropertyValue@DynamicDialogue@SoundEngine@AK@@YA?AW4AKRESULT@@KKAEAM@Z", (void**)&AK_SoundEngine_DynamicDialogue_GetDialogueEventCustomPropertyValue_74_orig, (void**)&AK_SoundEngine_DynamicDialogue_GetDialogueEventCustomPropertyValue_74_bridge, (void*)&AK_SoundEngine_DynamicDialogue_GetDialogueEventCustomPropertyValue_74_hook },
    { "?GetDirectSoundInstance@AK@@YAPEAUIDirectSound8@@XZ", (void**)&AK_GetDirectSoundInstance_75_orig, (void**)&AK_GetDirectSoundInstance_75_bridge, (void*)&AK_GetDirectSoundInstance_75_hook },
    { "?GetEventIDFromPlayingID@Query@SoundEngine@AK@@YAKK@Z", (void**)&AK_SoundEngine_Query_GetEventIDFromPlayingID_76_orig, (void**)&AK_SoundEngine_Query_GetEventIDFromPlayingID_76_bridge, (void*)&AK_SoundEngine_Query_GetEventIDFromPlayingID_76_hook },
    { "?GetFileLocationResolver@StreamMgr@AK@@YAPEAVIAkFileLocationResolver@12@XZ", (void**)&AK_StreamMgr_GetFileLocationResolver_77_orig, (void**)&AK_StreamMgr_GetFileLocationResolver_77_bridge, (void*)&AK_StreamMgr_GetFileLocationResolver_77_hook },
    { "?GetGameObjectAuxSendValues@Query@SoundEngine@AK@@YA?AW4AKRESULT@@_KPEAUAkAuxSendValue@@AEAK@Z", (void**)&AK_SoundEngine_Query_GetGameObjectAuxSendValues_78_orig, (void**)&AK_SoundEngine_Query_GetGameObjectAuxSendValues_78_bridge, (void*)&AK_SoundEngine_Query_GetGameObjectAuxSendValues_78_hook },
    { "?GetGameObjectDryLevelValue@Query@SoundEngine@AK@@YA?AW4AKRESULT@@_K0AEAM@Z", (void**)&AK_SoundEngine_Query_GetGameObjectDryLevelValue_79_orig, (void**)&AK_SoundEngine_Query_GetGameObjectDryLevelValue_79_bridge, (void*)&AK_SoundEngine_Query_GetGameObjectDryLevelValue_79_hook },
    { "?GetGameObjectFromPlayingID@Query@SoundEngine@AK@@YA_KK@Z", (void**)&AK_SoundEngine_Query_GetGameObjectFromPlayingID_80_orig, (void**)&AK_SoundEngine_Query_GetGameObjectFromPlayingID_80_bridge, (void*)&AK_SoundEngine_Query_GetGameObjectFromPlayingID_80_hook },
    { "?GetGlobalPluginContext@SoundEngine@AK@@YAPEAVIAkGlobalPluginContext@2@XZ", (void**)&AK_SoundEngine_GetGlobalPluginContext_81_orig, (void**)&AK_SoundEngine_GetGlobalPluginContext_81_bridge, (void*)&AK_SoundEngine_GetGlobalPluginContext_81_hook },
    { "?GetIDFromString@SoundEngine@AK@@YAKPEBD@Z", (void**)&AK_SoundEngine_GetIDFromString_82_orig, (void**)&AK_SoundEngine_GetIDFromString_82_bridge, (void*)&AK_SoundEngine_GetIDFromString_82_hook },
    { "?GetIDFromString@SoundEngine@AK@@YAKPEB_W@Z", (void**)&AK_SoundEngine_GetIDFromString_83_orig, (void**)&AK_SoundEngine_GetIDFromString_83_bridge, (void*)&AK_SoundEngine_GetIDFromString_83_hook },
    { "?GetIsGameObjectActive@Query@SoundEngine@AK@@YA_N_K@Z", (void**)&AK_SoundEngine_Query_GetIsGameObjectActive_84_orig, (void**)&AK_SoundEngine_Query_GetIsGameObjectActive_84_bridge, (void*)&AK_SoundEngine_Query_GetIsGameObjectActive_84_hook },
    { "?GetListenerPosition@Query@SoundEngine@AK@@YA?AW4AKRESULT@@_KAEAVAkTransform@@@Z", (void**)&AK_SoundEngine_Query_GetListenerPosition_85_orig, (void**)&AK_SoundEngine_Query_GetListenerPosition_85_bridge, (void*)&AK_SoundEngine_Query_GetListenerPosition_85_hook },
    { "?GetListenerSpatialization@Query@SoundEngine@AK@@YA?AW4AKRESULT@@KAEA_NAEAPEAMAEAUAkChannelConfig@@@Z", (void**)&AK_SoundEngine_Query_GetListenerSpatialization_86_orig, (void**)&AK_SoundEngine_Query_GetListenerSpatialization_86_bridge, (void*)&AK_SoundEngine_Query_GetListenerSpatialization_86_hook },
    { "?GetListeners@Query@SoundEngine@AK@@YA?AW4AKRESULT@@_KPEA_KAEAK@Z", (void**)&AK_SoundEngine_Query_GetListeners_87_orig, (void**)&AK_SoundEngine_Query_GetListeners_87_bridge, (void*)&AK_SoundEngine_Query_GetListeners_87_hook },
    { "?GetMaxPools@MemoryMgr@AK@@YAJXZ", (void**)&AK_MemoryMgr_GetMaxPools_88_orig, (void**)&AK_MemoryMgr_GetMaxPools_88_bridge, (void*)&AK_MemoryMgr_GetMaxPools_88_hook },
    { "?GetMaxRadius@Query@SoundEngine@AK@@YA?AW4AKRESULT@@AEAV?$AkArray@UGameObjDst@Query@SoundEngine@AK@@AEBU1234@U?$AkArrayAllocatorNoAlign@U_ArrayPoolDefault@@@@$0CA@U?$AkAssignmentMovePolicy@UGameObjDst@Query@SoundEngine@AK@@@@@@@Z", (void**)&AK_SoundEngine_Query_GetMaxRadius_89_orig, (void**)&AK_SoundEngine_Query_GetMaxRadius_89_bridge, (void*)&AK_SoundEngine_Query_GetMaxRadius_89_hook },
    { "?GetMaxRadius@Query@SoundEngine@AK@@YAM_K@Z", (void**)&AK_SoundEngine_Query_GetMaxRadius_90_orig, (void**)&AK_SoundEngine_Query_GetMaxRadius_90_bridge, (void*)&AK_SoundEngine_Query_GetMaxRadius_90_hook },
    { "?GetNumPools@MemoryMgr@AK@@YAJXZ", (void**)&AK_MemoryMgr_GetNumPools_91_orig, (void**)&AK_MemoryMgr_GetNumPools_91_bridge, (void*)&AK_MemoryMgr_GetNumPools_91_hook },
    { "?GetObjectObstructionAndOcclusion@Query@SoundEngine@AK@@YA?AW4AKRESULT@@_K0AEAM1@Z", (void**)&AK_SoundEngine_Query_GetObjectObstructionAndOcclusion_93_orig, (void**)&AK_SoundEngine_Query_GetObjectObstructionAndOcclusion_93_bridge, (void*)&AK_SoundEngine_Query_GetObjectObstructionAndOcclusion_93_hook },
    { "?GetOutputID@SoundEngine@AK@@YA_KKK@Z", (void**)&AK_SoundEngine_GetOutputID_94_orig, (void**)&AK_SoundEngine_GetOutputID_94_bridge, (void*)&AK_SoundEngine_GetOutputID_94_hook },
    { "?GetOutputID@SoundEngine@AK@@YA_KPEBDK@Z", (void**)&AK_SoundEngine_GetOutputID_95_orig, (void**)&AK_SoundEngine_GetOutputID_95_bridge, (void*)&AK_SoundEngine_GetOutputID_95_hook },
    { "?GetOutputID@SoundEngine@AK@@YA_KPEB_WK@Z", (void**)&AK_SoundEngine_GetOutputID_96_orig, (void**)&AK_SoundEngine_GetOutputID_96_bridge, (void*)&AK_SoundEngine_GetOutputID_96_hook },
    { "?GetPanningRule@SoundEngine@AK@@YA?AW4AKRESULT@@AEAW4AkPanningRule@@_K@Z", (void**)&AK_SoundEngine_GetPanningRule_97_orig, (void**)&AK_SoundEngine_GetPanningRule_97_bridge, (void*)&AK_SoundEngine_GetPanningRule_97_hook },
    { "?GetPauseTimes@DynamicSequence@SoundEngine@AK@@YA?AW4AKRESULT@@KAEAK0@Z", (void**)&AK_SoundEngine_DynamicSequence_GetPauseTimes_98_orig, (void**)&AK_SoundEngine_DynamicSequence_GetPauseTimes_98_bridge, (void*)&AK_SoundEngine_DynamicSequence_GetPauseTimes_98_hook },
    { "?GetPlayingIDsFromGameObject@Query@SoundEngine@AK@@YA?AW4AKRESULT@@_KAEAKPEAK@Z", (void**)&AK_SoundEngine_Query_GetPlayingIDsFromGameObject_99_orig, (void**)&AK_SoundEngine_Query_GetPlayingIDsFromGameObject_99_bridge, (void*)&AK_SoundEngine_Query_GetPlayingIDsFromGameObject_99_hook },
    { "?GetPlayingItem@DynamicSequence@SoundEngine@AK@@YA?AW4AKRESULT@@KAEAKAEAPEAX@Z", (void**)&AK_SoundEngine_DynamicSequence_GetPlayingItem_100_orig, (void**)&AK_SoundEngine_DynamicSequence_GetPlayingItem_100_bridge, (void*)&AK_SoundEngine_DynamicSequence_GetPlayingItem_100_hook },
    { "?GetPlayingSegmentInfo@MusicEngine@AK@@YA?AW4AKRESULT@@KAEAUAkSegmentInfo@@_N@Z", (void**)&AK_MusicEngine_GetPlayingSegmentInfo_101_orig, (void**)&AK_MusicEngine_GetPlayingSegmentInfo_101_bridge, (void*)&AK_MusicEngine_GetPlayingSegmentInfo_101_hook },
    { "?GetPoolAttributes@MemoryMgr@AK@@YA?AW4AkMemPoolAttributes@@J@Z", (void**)&AK_MemoryMgr_GetPoolAttributes_102_orig, (void**)&AK_MemoryMgr_GetPoolAttributes_102_bridge, (void*)&AK_MemoryMgr_GetPoolAttributes_102_hook },
    { "?GetPoolID@StreamMgr@AK@@YAJXZ", (void**)&AK_StreamMgr_GetPoolID_103_orig, (void**)&AK_StreamMgr_GetPoolID_103_bridge, (void*)&AK_StreamMgr_GetPoolID_103_hook },
    { "?GetPoolMemoryUsed@MemoryMgr@AK@@YAXJAEAUPoolMemInfo@12@@Z", (void**)&AK_MemoryMgr_GetPoolMemoryUsed_104_orig, (void**)&AK_MemoryMgr_GetPoolMemoryUsed_104_bridge, (void*)&AK_MemoryMgr_GetPoolMemoryUsed_104_hook },
    { "?GetPoolStats@MemoryMgr@AK@@YA?AW4AKRESULT@@JAEAUPoolStats@12@@Z", (void**)&AK_MemoryMgr_GetPoolStats_106_orig, (void**)&AK_MemoryMgr_GetPoolStats_106_bridge, (void*)&AK_MemoryMgr_GetPoolStats_106_hook },
    { "?GetPosition@Query@SoundEngine@AK@@YA?AW4AKRESULT@@_KAEAVAkTransform@@@Z", (void**)&AK_SoundEngine_Query_GetPosition_107_orig, (void**)&AK_SoundEngine_Query_GetPosition_107_bridge, (void*)&AK_SoundEngine_Query_GetPosition_107_hook },
    { "?GetPositioningInfo@Query@SoundEngine@AK@@YA?AW4AKRESULT@@KAEAUAkPositioningInfo@@@Z", (void**)&AK_SoundEngine_Query_GetPositioningInfo_108_orig, (void**)&AK_SoundEngine_Query_GetPositioningInfo_108_bridge, (void*)&AK_SoundEngine_Query_GetPositioningInfo_108_hook },
    { "?GetRTPCValue@Query@SoundEngine@AK@@YA?AW4AKRESULT@@K_KKAEAMAEAW4RTPCValue_type@123@@Z", (void**)&AK_SoundEngine_Query_GetRTPCValue_109_orig, (void**)&AK_SoundEngine_Query_GetRTPCValue_109_bridge, (void*)&AK_SoundEngine_Query_GetRTPCValue_109_hook },
    { "?GetRTPCValue@Query@SoundEngine@AK@@YA?AW4AKRESULT@@PEBD_KKAEAMAEAW4RTPCValue_type@123@@Z", (void**)&AK_SoundEngine_Query_GetRTPCValue_110_orig, (void**)&AK_SoundEngine_Query_GetRTPCValue_110_bridge, (void*)&AK_SoundEngine_Query_GetRTPCValue_110_hook },
    { "?GetRTPCValue@Query@SoundEngine@AK@@YA?AW4AKRESULT@@PEB_W_KKAEAMAEAW4RTPCValue_type@123@@Z", (void**)&AK_SoundEngine_Query_GetRTPCValue_111_orig, (void**)&AK_SoundEngine_Query_GetRTPCValue_111_bridge, (void*)&AK_SoundEngine_Query_GetRTPCValue_111_hook },
    { "?GetSourcePlayPosition@SoundEngine@AK@@YA?AW4AKRESULT@@KPEAJ_N@Z", (void**)&AK_SoundEngine_GetSourcePlayPosition_112_orig, (void**)&AK_SoundEngine_GetSourcePlayPosition_112_bridge, (void*)&AK_SoundEngine_GetSourcePlayPosition_112_hook },
    { "?GetSourcePlayPositions@SoundEngine@AK@@YA?AW4AKRESULT@@KPEAUAkSourcePosition@@PEAK_N@Z", (void**)&AK_SoundEngine_GetSourcePlayPositions_113_orig, (void**)&AK_SoundEngine_GetSourcePlayPositions_113_bridge, (void*)&AK_SoundEngine_GetSourcePlayPositions_113_hook },
    { "?GetSourceStreamBuffering@SoundEngine@AK@@YA?AW4AKRESULT@@KAEAJAEA_N@Z", (void**)&AK_SoundEngine_GetSourceStreamBuffering_114_orig, (void**)&AK_SoundEngine_GetSourceStreamBuffering_114_bridge, (void*)&AK_SoundEngine_GetSourceStreamBuffering_114_hook },
    { "?GetSpeakerAngles@SoundEngine@AK@@YA?AW4AKRESULT@@PEAMAEAKAEAM_K@Z", (void**)&AK_SoundEngine_GetSpeakerAngles_115_orig, (void**)&AK_SoundEngine_GetSpeakerAngles_115_bridge, (void*)&AK_SoundEngine_GetSpeakerAngles_115_hook },
    { "?GetSpeakerConfiguration@SoundEngine@AK@@YA?AUAkChannelConfig@@_K@Z", (void**)&AK_SoundEngine_GetSpeakerConfiguration_116_orig, (void**)&AK_SoundEngine_GetSpeakerConfiguration_116_bridge, (void*)&AK_SoundEngine_GetSpeakerConfiguration_116_hook },
    { "?GetState@Query@SoundEngine@AK@@YA?AW4AKRESULT@@KAEAK@Z", (void**)&AK_SoundEngine_Query_GetState_117_orig, (void**)&AK_SoundEngine_Query_GetState_117_bridge, (void*)&AK_SoundEngine_Query_GetState_117_hook },
    { "?GetState@Query@SoundEngine@AK@@YA?AW4AKRESULT@@PEBDAEAK@Z", (void**)&AK_SoundEngine_Query_GetState_118_orig, (void**)&AK_SoundEngine_Query_GetState_118_bridge, (void*)&AK_SoundEngine_Query_GetState_118_hook },
    { "?GetState@Query@SoundEngine@AK@@YA?AW4AKRESULT@@PEB_WAEAK@Z", (void**)&AK_SoundEngine_Query_GetState_119_orig, (void**)&AK_SoundEngine_Query_GetState_119_bridge, (void*)&AK_SoundEngine_Query_GetState_119_hook },
    { "?GetSwitch@Query@SoundEngine@AK@@YA?AW4AKRESULT@@K_KAEAK@Z", (void**)&AK_SoundEngine_Query_GetSwitch_120_orig, (void**)&AK_SoundEngine_Query_GetSwitch_120_bridge, (void*)&AK_SoundEngine_Query_GetSwitch_120_hook },
    { "?GetSwitch@Query@SoundEngine@AK@@YA?AW4AKRESULT@@PEBD_KAEAK@Z", (void**)&AK_SoundEngine_Query_GetSwitch_121_orig, (void**)&AK_SoundEngine_Query_GetSwitch_121_bridge, (void*)&AK_SoundEngine_Query_GetSwitch_121_hook },
    { "?GetSwitch@Query@SoundEngine@AK@@YA?AW4AKRESULT@@PEB_W_KAEAK@Z", (void**)&AK_SoundEngine_Query_GetSwitch_122_orig, (void**)&AK_SoundEngine_Query_GetSwitch_122_bridge, (void*)&AK_SoundEngine_Query_GetSwitch_122_hook },
    { "?GetWindowsDevice@AK@@YA_NJAEAKPEAPEAUIMMDevice@@W4AkAudioDeviceState@@@Z", (void**)&AK_GetWindowsDevice_124_orig, (void**)&AK_GetWindowsDevice_124_bridge, (void*)&AK_GetWindowsDevice_124_hook },
    { "?GetWindowsDeviceCount@AK@@YAKW4AkAudioDeviceState@@@Z", (void**)&AK_GetWindowsDeviceCount_125_orig, (void**)&AK_GetWindowsDeviceCount_125_bridge, (void*)&AK_GetWindowsDeviceCount_125_hook },
    { "?GetWindowsDeviceName@AK@@YAPEB_WJAEAKW4AkAudioDeviceState@@@Z", (void**)&AK_GetWindowsDeviceName_126_orig, (void**)&AK_GetWindowsDeviceName_126_bridge, (void*)&AK_GetWindowsDeviceName_126_hook },
    { "?GetWwiseXAudio2Interface@AK@@YAPEAUIUnknown@@XZ", (void**)&AK_GetWwiseXAudio2Interface_127_orig, (void**)&AK_GetWwiseXAudio2Interface_127_bridge, (void*)&AK_GetWwiseXAudio2Interface_127_hook },
    { "?IsInitialized@MemoryMgr@AK@@YA_NXZ", (void**)&AK_MemoryMgr_IsInitialized_132_orig, (void**)&AK_MemoryMgr_IsInitialized_132_bridge, (void*)&AK_MemoryMgr_IsInitialized_132_hook },
    { "?IsInitialized@SoundEngine@AK@@YA_NXZ", (void**)&AK_SoundEngine_IsInitialized_133_orig, (void**)&AK_SoundEngine_IsInitialized_133_bridge, (void*)&AK_SoundEngine_IsInitialized_133_hook },
    { "?LockPlaylist@DynamicSequence@SoundEngine@AK@@YAPEAVPlaylist@123@K@Z", (void**)&AK_SoundEngine_DynamicSequence_LockPlaylist_144_orig, (void**)&AK_SoundEngine_DynamicSequence_LockPlaylist_144_bridge, (void*)&AK_SoundEngine_DynamicSequence_LockPlaylist_144_hook },
    { "?Malign@MemoryMgr@AK@@YAPEAXJ_KK@Z", (void**)&AK_MemoryMgr_Malign_145_orig, (void**)&AK_MemoryMgr_Malign_145_bridge, (void*)&AK_MemoryMgr_Malign_145_hook },
    { "?Malloc@MemoryMgr@AK@@YAPEAXJ_K@Z", (void**)&AK_MemoryMgr_Malloc_146_orig, (void**)&AK_MemoryMgr_Malloc_146_bridge, (void*)&AK_MemoryMgr_Malloc_146_hook },
    { "?MuteBackgroundMusic@SoundEngine@AK@@YAX_N@Z", (void**)&AK_SoundEngine_MuteBackgroundMusic_152_orig, (void**)&AK_SoundEngine_MuteBackgroundMusic_152_bridge, (void*)&AK_SoundEngine_MuteBackgroundMusic_152_hook },
    { "?Open@DynamicSequence@SoundEngine@AK@@YAK_KKP6AXW4AkCallbackType@@PEAUAkCallbackInfo@@@ZPEAXW4DynamicSequenceType@123@@Z", (void**)&AK_SoundEngine_DynamicSequence_Open_153_orig, (void**)&AK_SoundEngine_DynamicSequence_Open_153_bridge, (void*)&AK_SoundEngine_DynamicSequence_Open_153_hook },
    { "?Pause@DynamicSequence@SoundEngine@AK@@YA?AW4AKRESULT@@KJW4AkCurveInterpolation@@@Z", (void**)&AK_SoundEngine_DynamicSequence_Pause_154_orig, (void**)&AK_SoundEngine_DynamicSequence_Pause_154_bridge, (void*)&AK_SoundEngine_DynamicSequence_Pause_154_hook },
    { "?PinEventInStreamCache@SoundEngine@AK@@YA?AW4AKRESULT@@KDD@Z", (void**)&AK_SoundEngine_PinEventInStreamCache_155_orig, (void**)&AK_SoundEngine_PinEventInStreamCache_155_bridge, (void*)&AK_SoundEngine_PinEventInStreamCache_155_hook },
    { "?PinEventInStreamCache@SoundEngine@AK@@YA?AW4AKRESULT@@PEBDDD@Z", (void**)&AK_SoundEngine_PinEventInStreamCache_156_orig, (void**)&AK_SoundEngine_PinEventInStreamCache_156_bridge, (void*)&AK_SoundEngine_PinEventInStreamCache_156_hook },
    { "?PinEventInStreamCache@SoundEngine@AK@@YA?AW4AKRESULT@@PEB_WDD@Z", (void**)&AK_SoundEngine_PinEventInStreamCache_157_orig, (void**)&AK_SoundEngine_PinEventInStreamCache_157_bridge, (void*)&AK_SoundEngine_PinEventInStreamCache_157_hook },
    { "?Play@DynamicSequence@SoundEngine@AK@@YA?AW4AKRESULT@@KJW4AkCurveInterpolation@@@Z", (void**)&AK_SoundEngine_DynamicSequence_Play_158_orig, (void**)&AK_SoundEngine_DynamicSequence_Play_158_bridge, (void*)&AK_SoundEngine_DynamicSequence_Play_158_hook },
    { "?PlaySourcePlugin@SoundEngine@AK@@YAKKK_K@Z", (void**)&AK_SoundEngine_PlaySourcePlugin_159_orig, (void**)&AK_SoundEngine_PlaySourcePlugin_159_bridge, (void*)&AK_SoundEngine_PlaySourcePlugin_159_hook },
    { "?PostMIDIOnEvent@SoundEngine@AK@@YA?AW4AKRESULT@@K_KPEAUAkMIDIPost@@G@Z", (void**)&AK_SoundEngine_PostMIDIOnEvent_164_orig, (void**)&AK_SoundEngine_PostMIDIOnEvent_164_bridge, (void*)&AK_SoundEngine_PostMIDIOnEvent_164_hook },
    { "?PostTrigger@SoundEngine@AK@@YA?AW4AKRESULT@@K_K@Z", (void**)&AK_SoundEngine_PostTrigger_167_orig, (void**)&AK_SoundEngine_PostTrigger_167_bridge, (void*)&AK_SoundEngine_PostTrigger_167_hook },
    { "?PostTrigger@SoundEngine@AK@@YA?AW4AKRESULT@@PEBD_K@Z", (void**)&AK_SoundEngine_PostTrigger_168_orig, (void**)&AK_SoundEngine_PostTrigger_168_bridge, (void*)&AK_SoundEngine_PostTrigger_168_hook },
    { "?PostTrigger@SoundEngine@AK@@YA?AW4AKRESULT@@PEB_W_K@Z", (void**)&AK_SoundEngine_PostTrigger_169_orig, (void**)&AK_SoundEngine_PostTrigger_169_bridge, (void*)&AK_SoundEngine_PostTrigger_169_hook },
    { "?PrepareBank@SoundEngine@AK@@YA?AW4AKRESULT@@W4PreparationType@12@KP6AXKPEBXW43@JPEAX@Z3W4AkBankContent@12@@Z", (void**)&AK_SoundEngine_PrepareBank_170_orig, (void**)&AK_SoundEngine_PrepareBank_170_bridge, (void*)&AK_SoundEngine_PrepareBank_170_hook },
    { "?PrepareBank@SoundEngine@AK@@YA?AW4AKRESULT@@W4PreparationType@12@KW4AkBankContent@12@@Z", (void**)&AK_SoundEngine_PrepareBank_171_orig, (void**)&AK_SoundEngine_PrepareBank_171_bridge, (void*)&AK_SoundEngine_PrepareBank_171_hook },
    { "?PrepareBank@SoundEngine@AK@@YA?AW4AKRESULT@@W4PreparationType@12@PEBDP6AXKPEBXW43@JPEAX@Z4W4AkBankContent@12@@Z", (void**)&AK_SoundEngine_PrepareBank_172_orig, (void**)&AK_SoundEngine_PrepareBank_172_bridge, (void*)&AK_SoundEngine_PrepareBank_172_hook },
    { "?PrepareBank@SoundEngine@AK@@YA?AW4AKRESULT@@W4PreparationType@12@PEBDW4AkBankContent@12@@Z", (void**)&AK_SoundEngine_PrepareBank_173_orig, (void**)&AK_SoundEngine_PrepareBank_173_bridge, (void*)&AK_SoundEngine_PrepareBank_173_hook },
    { "?PrepareBank@SoundEngine@AK@@YA?AW4AKRESULT@@W4PreparationType@12@PEB_WP6AXKPEBXW43@JPEAX@Z4W4AkBankContent@12@@Z", (void**)&AK_SoundEngine_PrepareBank_174_orig, (void**)&AK_SoundEngine_PrepareBank_174_bridge, (void*)&AK_SoundEngine_PrepareBank_174_hook },
    { "?PrepareBank@SoundEngine@AK@@YA?AW4AKRESULT@@W4PreparationType@12@PEB_WW4AkBankContent@12@@Z", (void**)&AK_SoundEngine_PrepareBank_175_orig, (void**)&AK_SoundEngine_PrepareBank_175_bridge, (void*)&AK_SoundEngine_PrepareBank_175_hook },
    { "?PrepareEvent@SoundEngine@AK@@YA?AW4AKRESULT@@W4PreparationType@12@PEAKK@Z", (void**)&AK_SoundEngine_PrepareEvent_176_orig, (void**)&AK_SoundEngine_PrepareEvent_176_bridge, (void*)&AK_SoundEngine_PrepareEvent_176_hook },
    { "?PrepareEvent@SoundEngine@AK@@YA?AW4AKRESULT@@W4PreparationType@12@PEAKKP6AXKPEBXW43@JPEAX@Z4@Z", (void**)&AK_SoundEngine_PrepareEvent_177_orig, (void**)&AK_SoundEngine_PrepareEvent_177_bridge, (void*)&AK_SoundEngine_PrepareEvent_177_hook },
    { "?PrepareEvent@SoundEngine@AK@@YA?AW4AKRESULT@@W4PreparationType@12@PEAPEBDK@Z", (void**)&AK_SoundEngine_PrepareEvent_178_orig, (void**)&AK_SoundEngine_PrepareEvent_178_bridge, (void*)&AK_SoundEngine_PrepareEvent_178_hook },
    { "?PrepareEvent@SoundEngine@AK@@YA?AW4AKRESULT@@W4PreparationType@12@PEAPEBDKP6AXKPEBXW43@JPEAX@Z4@Z", (void**)&AK_SoundEngine_PrepareEvent_179_orig, (void**)&AK_SoundEngine_PrepareEvent_179_bridge, (void*)&AK_SoundEngine_PrepareEvent_179_hook },
    { "?PrepareEvent@SoundEngine@AK@@YA?AW4AKRESULT@@W4PreparationType@12@PEAPEB_WK@Z", (void**)&AK_SoundEngine_PrepareEvent_180_orig, (void**)&AK_SoundEngine_PrepareEvent_180_bridge, (void*)&AK_SoundEngine_PrepareEvent_180_hook },
    { "?PrepareEvent@SoundEngine@AK@@YA?AW4AKRESULT@@W4PreparationType@12@PEAPEB_WKP6AXKPEBXW43@JPEAX@Z4@Z", (void**)&AK_SoundEngine_PrepareEvent_181_orig, (void**)&AK_SoundEngine_PrepareEvent_181_bridge, (void*)&AK_SoundEngine_PrepareEvent_181_hook },
    { "?PrepareGameSyncs@SoundEngine@AK@@YA?AW4AKRESULT@@W4PreparationType@12@W4AkGroupType@@KPEAKK@Z", (void**)&AK_SoundEngine_PrepareGameSyncs_182_orig, (void**)&AK_SoundEngine_PrepareGameSyncs_182_bridge, (void*)&AK_SoundEngine_PrepareGameSyncs_182_hook },
    { "?PrepareGameSyncs@SoundEngine@AK@@YA?AW4AKRESULT@@W4PreparationType@12@W4AkGroupType@@KPEAKKP6AXKPEBXW43@JPEAX@Z5@Z", (void**)&AK_SoundEngine_PrepareGameSyncs_183_orig, (void**)&AK_SoundEngine_PrepareGameSyncs_183_bridge, (void*)&AK_SoundEngine_PrepareGameSyncs_183_hook },
    { "?PrepareGameSyncs@SoundEngine@AK@@YA?AW4AKRESULT@@W4PreparationType@12@W4AkGroupType@@PEBDPEAPEBDK@Z", (void**)&AK_SoundEngine_PrepareGameSyncs_184_orig, (void**)&AK_SoundEngine_PrepareGameSyncs_184_bridge, (void*)&AK_SoundEngine_PrepareGameSyncs_184_hook },
    { "?PrepareGameSyncs@SoundEngine@AK@@YA?AW4AKRESULT@@W4PreparationType@12@W4AkGroupType@@PEBDPEAPEBDKP6AXKPEBXW43@JPEAX@Z6@Z", (void**)&AK_SoundEngine_PrepareGameSyncs_185_orig, (void**)&AK_SoundEngine_PrepareGameSyncs_185_bridge, (void*)&AK_SoundEngine_PrepareGameSyncs_185_hook },
    { "?PrepareGameSyncs@SoundEngine@AK@@YA?AW4AKRESULT@@W4PreparationType@12@W4AkGroupType@@PEB_WPEAPEB_WK@Z", (void**)&AK_SoundEngine_PrepareGameSyncs_186_orig, (void**)&AK_SoundEngine_PrepareGameSyncs_186_bridge, (void*)&AK_SoundEngine_PrepareGameSyncs_186_hook },
    { "?PrepareGameSyncs@SoundEngine@AK@@YA?AW4AKRESULT@@W4PreparationType@12@W4AkGroupType@@PEB_WPEAPEB_WKP6AXKPEBXW43@JPEAX@Z6@Z", (void**)&AK_SoundEngine_PrepareGameSyncs_187_orig, (void**)&AK_SoundEngine_PrepareGameSyncs_187_bridge, (void*)&AK_SoundEngine_PrepareGameSyncs_187_hook },
    { "?QueryAudioObjectIDs@Query@SoundEngine@AK@@YA?AW4AKRESULT@@KAEAKPEAUAkObjectInfo@@@Z", (void**)&AK_SoundEngine_Query_QueryAudioObjectIDs_188_orig, (void**)&AK_SoundEngine_Query_QueryAudioObjectIDs_188_bridge, (void*)&AK_SoundEngine_Query_QueryAudioObjectIDs_188_hook },
    { "?QueryAudioObjectIDs@Query@SoundEngine@AK@@YA?AW4AKRESULT@@PEBDAEAKPEAUAkObjectInfo@@@Z", (void**)&AK_SoundEngine_Query_QueryAudioObjectIDs_189_orig, (void**)&AK_SoundEngine_Query_QueryAudioObjectIDs_189_bridge, (void*)&AK_SoundEngine_Query_QueryAudioObjectIDs_189_hook },
    { "?QueryAudioObjectIDs@Query@SoundEngine@AK@@YA?AW4AKRESULT@@PEB_WAEAKPEAUAkObjectInfo@@@Z", (void**)&AK_SoundEngine_Query_QueryAudioObjectIDs_190_orig, (void**)&AK_SoundEngine_Query_QueryAudioObjectIDs_190_bridge, (void*)&AK_SoundEngine_Query_QueryAudioObjectIDs_190_hook },
    { "?ReadBytes@ReadBytesMem@AK@@UEAA_NPEAXJAEAJ@Z", (void**)&AK_ReadBytesMem_ReadBytes_191_orig, (void**)&AK_ReadBytesMem_ReadBytes_191_bridge, (void*)&AK_ReadBytesMem_ReadBytes_191_hook },
    { "?ReadBytes@ReadBytesSkip@AK@@UEAA_NPEAXJAEAJ@Z", (void**)&AK_ReadBytesSkip_ReadBytes_192_orig, (void**)&AK_ReadBytesSkip_ReadBytes_192_bridge, (void*)&AK_ReadBytesSkip_ReadBytes_192_hook },
    { "?Realloc@MemoryMgr@AK@@YAPEAXJPEAX_K@Z", (void**)&AK_MemoryMgr_Realloc_193_orig, (void**)&AK_MemoryMgr_Realloc_193_bridge, (void*)&AK_MemoryMgr_Realloc_193_hook },
    { "?RegisterAudioDeviceStatusCallback@SoundEngine@AK@@YA?AW4AKRESULT@@P6AXPEAVIAkGlobalPluginContext@2@KKW4AkAudioDeviceEvent@2@W43@@Z@Z", (void**)&AK_SoundEngine_RegisterAudioDeviceStatusCallback_194_orig, (void**)&AK_SoundEngine_RegisterAudioDeviceStatusCallback_194_bridge, (void*)&AK_SoundEngine_RegisterAudioDeviceStatusCallback_194_hook },
    { "?RegisterBusMeteringCallback@SoundEngine@AK@@YA?AW4AKRESULT@@KP6AXPEAVIAkMetering@2@UAkChannelConfig@@W4AkMeteringFlags@@@Z2@Z", (void**)&AK_SoundEngine_RegisterBusMeteringCallback_195_orig, (void**)&AK_SoundEngine_RegisterBusMeteringCallback_195_bridge, (void*)&AK_SoundEngine_RegisterBusMeteringCallback_195_hook },
    { "?RegisterBusVolumeCallback@SoundEngine@AK@@YA?AW4AKRESULT@@KP6AXPEAUAkSpeakerVolumeMatrixCallbackInfo@@@Z@Z", (void**)&AK_SoundEngine_RegisterBusVolumeCallback_196_orig, (void**)&AK_SoundEngine_RegisterBusVolumeCallback_196_bridge, (void*)&AK_SoundEngine_RegisterBusVolumeCallback_196_hook },
    { "?RegisterCodec@SoundEngine@AK@@YA?AW4AKRESULT@@KKP6APEAVIAkSoftwareCodec@@PEAX@Z1@Z", (void**)&AK_SoundEngine_RegisterCodec_197_orig, (void**)&AK_SoundEngine_RegisterCodec_197_bridge, (void*)&AK_SoundEngine_RegisterCodec_197_hook },
    { "?RegisterPlugin@SoundEngine@AK@@YA?AW4AKRESULT@@W4AkPluginType@@KKP6APEAVIAkPlugin@2@PEAVIAkPluginMemAlloc@2@@ZP6APEAVIAkPluginParam@2@1@Z@Z", (void**)&AK_SoundEngine_RegisterPlugin_201_orig, (void**)&AK_SoundEngine_RegisterPlugin_201_bridge, (void*)&AK_SoundEngine_RegisterPlugin_201_hook },
    { "?RegisterPluginDLL@SoundEngine@AK@@YA?AW4AKRESULT@@PEB_W0@Z", (void**)&AK_SoundEngine_RegisterPluginDLL_202_orig, (void**)&AK_SoundEngine_RegisterPluginDLL_202_bridge, (void*)&AK_SoundEngine_RegisterPluginDLL_202_hook },
    { "?ReleaseBlock@MemoryMgr@AK@@YA?AW4AKRESULT@@JPEAX@Z", (void**)&AK_MemoryMgr_ReleaseBlock_203_orig, (void**)&AK_MemoryMgr_ReleaseBlock_203_bridge, (void*)&AK_MemoryMgr_ReleaseBlock_203_hook },
    { "?RemoveDefaultListener@SoundEngine@AK@@YA?AW4AKRESULT@@_K@Z", (void**)&AK_SoundEngine_RemoveDefaultListener_204_orig, (void**)&AK_SoundEngine_RemoveDefaultListener_204_bridge, (void*)&AK_SoundEngine_RemoveDefaultListener_204_hook },
    { "?RemoveLanguageChangeObserver@StreamMgr@AK@@YAXPEAX@Z", (void**)&AK_StreamMgr_RemoveLanguageChangeObserver_205_orig, (void**)&AK_StreamMgr_RemoveLanguageChangeObserver_205_bridge, (void*)&AK_StreamMgr_RemoveLanguageChangeObserver_205_hook },
    { "?RemoveListener@SoundEngine@AK@@YA?AW4AKRESULT@@_K0@Z", (void**)&AK_SoundEngine_RemoveListener_206_orig, (void**)&AK_SoundEngine_RemoveListener_206_bridge, (void*)&AK_SoundEngine_RemoveListener_206_hook },
    { "?RemoveOutput@SoundEngine@AK@@YA?AW4AKRESULT@@_K@Z", (void**)&AK_SoundEngine_RemoveOutput_207_orig, (void**)&AK_SoundEngine_RemoveOutput_207_bridge, (void*)&AK_SoundEngine_RemoveOutput_207_hook },
    { "?RenderAudio@SoundEngine@AK@@YA?AW4AKRESULT@@_N@Z", (void**)&AK_SoundEngine_RenderAudio_208_orig, (void**)&AK_SoundEngine_RenderAudio_208_bridge, (void*)&AK_SoundEngine_RenderAudio_208_hook },
    { "?ReplaceOutput@SoundEngine@AK@@YA?AW4AKRESULT@@AEBUAkOutputSettings@@_KPEA_K@Z", (void**)&AK_SoundEngine_ReplaceOutput_209_orig, (void**)&AK_SoundEngine_ReplaceOutput_209_bridge, (void*)&AK_SoundEngine_ReplaceOutput_209_hook },
    { "?Reserve@WriteBytesCount@AK@@UEAA_NJ@Z", (void**)&AK_WriteBytesCount_Reserve_210_orig, (void**)&AK_WriteBytesCount_Reserve_210_bridge, (void*)&AK_WriteBytesCount_Reserve_210_hook },
    { "?Reserve@WriteBytesMem@AK@@UEAA_NJ@Z", (void**)&AK_WriteBytesMem_Reserve_211_orig, (void**)&AK_WriteBytesMem_Reserve_211_bridge, (void*)&AK_WriteBytesMem_Reserve_211_hook },
    { "?ResetListenersToDefault@SoundEngine@AK@@YA?AW4AKRESULT@@_K@Z", (void**)&AK_SoundEngine_ResetListenersToDefault_212_orig, (void**)&AK_SoundEngine_ResetListenersToDefault_212_bridge, (void*)&AK_SoundEngine_ResetListenersToDefault_212_hook },
    { "?ResetRTPCValue@SoundEngine@AK@@YA?AW4AKRESULT@@K_KJW4AkCurveInterpolation@@_N@Z", (void**)&AK_SoundEngine_ResetRTPCValue_213_orig, (void**)&AK_SoundEngine_ResetRTPCValue_213_bridge, (void*)&AK_SoundEngine_ResetRTPCValue_213_hook },
    { "?ResetRTPCValue@SoundEngine@AK@@YA?AW4AKRESULT@@PEBD_KJW4AkCurveInterpolation@@_N@Z", (void**)&AK_SoundEngine_ResetRTPCValue_214_orig, (void**)&AK_SoundEngine_ResetRTPCValue_214_bridge, (void*)&AK_SoundEngine_ResetRTPCValue_214_hook },
    { "?ResetRTPCValue@SoundEngine@AK@@YA?AW4AKRESULT@@PEB_W_KJW4AkCurveInterpolation@@_N@Z", (void**)&AK_SoundEngine_ResetRTPCValue_215_orig, (void**)&AK_SoundEngine_ResetRTPCValue_215_bridge, (void*)&AK_SoundEngine_ResetRTPCValue_215_hook },
    { "?ResolveDialogueEvent@DynamicDialogue@SoundEngine@AK@@YAKKPEAKKKP6A_NKKPEAX@Z1@Z", (void**)&AK_SoundEngine_DynamicDialogue_ResolveDialogueEvent_216_orig, (void**)&AK_SoundEngine_DynamicDialogue_ResolveDialogueEvent_216_bridge, (void*)&AK_SoundEngine_DynamicDialogue_ResolveDialogueEvent_216_hook },
    { "?ResolveDialogueEvent@DynamicDialogue@SoundEngine@AK@@YAKPEBDPEAPEBDKKP6A_NKKPEAX@Z2@Z", (void**)&AK_SoundEngine_DynamicDialogue_ResolveDialogueEvent_217_orig, (void**)&AK_SoundEngine_DynamicDialogue_ResolveDialogueEvent_217_bridge, (void*)&AK_SoundEngine_DynamicDialogue_ResolveDialogueEvent_217_hook },
    { "?ResolveDialogueEvent@DynamicDialogue@SoundEngine@AK@@YAKPEB_WPEAPEB_WKKP6A_NKKPEAX@Z2@Z", (void**)&AK_SoundEngine_DynamicDialogue_ResolveDialogueEvent_218_orig, (void**)&AK_SoundEngine_DynamicDialogue_ResolveDialogueEvent_218_bridge, (void*)&AK_SoundEngine_DynamicDialogue_ResolveDialogueEvent_218_hook },
    { "?Resume@DynamicSequence@SoundEngine@AK@@YA?AW4AKRESULT@@KJW4AkCurveInterpolation@@@Z", (void**)&AK_SoundEngine_DynamicSequence_Resume_219_orig, (void**)&AK_SoundEngine_DynamicSequence_Resume_219_bridge, (void*)&AK_SoundEngine_DynamicSequence_Resume_219_hook },
    { "?SeekOnEvent@SoundEngine@AK@@YA?AW4AKRESULT@@K_KJ_NK@Z", (void**)&AK_SoundEngine_SeekOnEvent_220_orig, (void**)&AK_SoundEngine_SeekOnEvent_220_bridge, (void*)&AK_SoundEngine_SeekOnEvent_220_hook },
    { "?SeekOnEvent@SoundEngine@AK@@YA?AW4AKRESULT@@K_KM_NK@Z", (void**)&AK_SoundEngine_SeekOnEvent_221_orig, (void**)&AK_SoundEngine_SeekOnEvent_221_bridge, (void*)&AK_SoundEngine_SeekOnEvent_221_hook },
    { "?SeekOnEvent@SoundEngine@AK@@YA?AW4AKRESULT@@PEBD_KJ_NK@Z", (void**)&AK_SoundEngine_SeekOnEvent_222_orig, (void**)&AK_SoundEngine_SeekOnEvent_222_bridge, (void*)&AK_SoundEngine_SeekOnEvent_222_hook },
    { "?SeekOnEvent@SoundEngine@AK@@YA?AW4AKRESULT@@PEBD_KM_NK@Z", (void**)&AK_SoundEngine_SeekOnEvent_223_orig, (void**)&AK_SoundEngine_SeekOnEvent_223_bridge, (void*)&AK_SoundEngine_SeekOnEvent_223_hook },
    { "?SeekOnEvent@SoundEngine@AK@@YA?AW4AKRESULT@@PEB_W_KJ_NK@Z", (void**)&AK_SoundEngine_SeekOnEvent_224_orig, (void**)&AK_SoundEngine_SeekOnEvent_224_bridge, (void*)&AK_SoundEngine_SeekOnEvent_224_hook },
    { "?SeekOnEvent@SoundEngine@AK@@YA?AW4AKRESULT@@PEB_W_KM_NK@Z", (void**)&AK_SoundEngine_SeekOnEvent_225_orig, (void**)&AK_SoundEngine_SeekOnEvent_225_bridge, (void*)&AK_SoundEngine_SeekOnEvent_225_hook },
    { "?SendPluginCustomGameData@SoundEngine@AK@@YA?AW4AKRESULT@@K_KW4AkPluginType@@KKPEBXK@Z", (void**)&AK_SoundEngine_SendPluginCustomGameData_226_orig, (void**)&AK_SoundEngine_SendPluginCustomGameData_226_bridge, (void*)&AK_SoundEngine_SendPluginCustomGameData_226_hook },
    { "?SetActorMixerEffect@SoundEngine@AK@@YA?AW4AKRESULT@@KKK@Z", (void**)&AK_SoundEngine_SetActorMixerEffect_227_orig, (void**)&AK_SoundEngine_SetActorMixerEffect_227_bridge, (void*)&AK_SoundEngine_SetActorMixerEffect_227_hook },
    { "?SetBankLoadIOSettings@SoundEngine@AK@@YA?AW4AKRESULT@@MD@Z", (void**)&AK_SoundEngine_SetBankLoadIOSettings_228_orig, (void**)&AK_SoundEngine_SetBankLoadIOSettings_228_bridge, (void*)&AK_SoundEngine_SetBankLoadIOSettings_228_hook },
    { "?SetBusConfig@SoundEngine@AK@@YA?AW4AKRESULT@@KUAkChannelConfig@@@Z", (void**)&AK_SoundEngine_SetBusConfig_229_orig, (void**)&AK_SoundEngine_SetBusConfig_229_bridge, (void*)&AK_SoundEngine_SetBusConfig_229_hook },
    { "?SetBusConfig@SoundEngine@AK@@YA?AW4AKRESULT@@PEBDUAkChannelConfig@@@Z", (void**)&AK_SoundEngine_SetBusConfig_230_orig, (void**)&AK_SoundEngine_SetBusConfig_230_bridge, (void*)&AK_SoundEngine_SetBusConfig_230_hook },
    { "?SetBusConfig@SoundEngine@AK@@YA?AW4AKRESULT@@PEB_WUAkChannelConfig@@@Z", (void**)&AK_SoundEngine_SetBusConfig_231_orig, (void**)&AK_SoundEngine_SetBusConfig_231_bridge, (void*)&AK_SoundEngine_SetBusConfig_231_hook },
    { "?SetBusDevice@SoundEngine@AK@@YA?AW4AKRESULT@@KK@Z", (void**)&AK_SoundEngine_SetBusDevice_232_orig, (void**)&AK_SoundEngine_SetBusDevice_232_bridge, (void*)&AK_SoundEngine_SetBusDevice_232_hook },
    { "?SetBusDevice@SoundEngine@AK@@YA?AW4AKRESULT@@PEBD0@Z", (void**)&AK_SoundEngine_SetBusDevice_233_orig, (void**)&AK_SoundEngine_SetBusDevice_233_bridge, (void*)&AK_SoundEngine_SetBusDevice_233_hook },
    { "?SetBusDevice@SoundEngine@AK@@YA?AW4AKRESULT@@PEB_W0@Z", (void**)&AK_SoundEngine_SetBusDevice_234_orig, (void**)&AK_SoundEngine_SetBusDevice_234_bridge, (void*)&AK_SoundEngine_SetBusDevice_234_hook },
    { "?SetBusEffect@SoundEngine@AK@@YA?AW4AKRESULT@@KKK@Z", (void**)&AK_SoundEngine_SetBusEffect_235_orig, (void**)&AK_SoundEngine_SetBusEffect_235_bridge, (void*)&AK_SoundEngine_SetBusEffect_235_hook },
    { "?SetBusEffect@SoundEngine@AK@@YA?AW4AKRESULT@@PEBDKK@Z", (void**)&AK_SoundEngine_SetBusEffect_236_orig, (void**)&AK_SoundEngine_SetBusEffect_236_bridge, (void*)&AK_SoundEngine_SetBusEffect_236_hook },
    { "?SetBusEffect@SoundEngine@AK@@YA?AW4AKRESULT@@PEB_WKK@Z", (void**)&AK_SoundEngine_SetBusEffect_237_orig, (void**)&AK_SoundEngine_SetBusEffect_237_bridge, (void*)&AK_SoundEngine_SetBusEffect_237_hook },
    { "?SetContainerHistory@SoundEngine@AK@@YA?AW4AKRESULT@@PEAVIReadBytes@2@@Z", (void**)&AK_SoundEngine_SetContainerHistory_238_orig, (void**)&AK_SoundEngine_SetContainerHistory_238_bridge, (void*)&AK_SoundEngine_SetContainerHistory_238_hook },
    { "?SetCount@WriteBytesCount@AK@@UEAAXJ@Z", (void**)&AK_WriteBytesCount_SetCount_239_orig, (void**)&AK_WriteBytesCount_SetCount_239_bridge, (void*)&AK_WriteBytesCount_SetCount_239_hook },
    { "?SetCount@WriteBytesMem@AK@@UEAAXJ@Z", (void**)&AK_WriteBytesMem_SetCount_240_orig, (void**)&AK_WriteBytesMem_SetCount_240_bridge, (void*)&AK_WriteBytesMem_SetCount_240_hook },
    { "?SetCurrentLanguage@StreamMgr@AK@@YA?AW4AKRESULT@@PEB_W@Z", (void**)&AK_StreamMgr_SetCurrentLanguage_241_orig, (void**)&AK_StreamMgr_SetCurrentLanguage_241_bridge, (void*)&AK_StreamMgr_SetCurrentLanguage_241_hook },
    { "?SetFileLocationResolver@StreamMgr@AK@@YAXPEAVIAkFileLocationResolver@12@@Z", (void**)&AK_StreamMgr_SetFileLocationResolver_243_orig, (void**)&AK_StreamMgr_SetFileLocationResolver_243_bridge, (void*)&AK_StreamMgr_SetFileLocationResolver_243_hook },
    { "?SetGameObjectAuxSendValues@SoundEngine@AK@@YA?AW4AKRESULT@@_KPEAUAkAuxSendValue@@K@Z", (void**)&AK_SoundEngine_SetGameObjectAuxSendValues_244_orig, (void**)&AK_SoundEngine_SetGameObjectAuxSendValues_244_bridge, (void*)&AK_SoundEngine_SetGameObjectAuxSendValues_244_hook },
    { "?SetGameObjectOutputBusVolume@SoundEngine@AK@@YA?AW4AKRESULT@@_K0M@Z", (void**)&AK_SoundEngine_SetGameObjectOutputBusVolume_245_orig, (void**)&AK_SoundEngine_SetGameObjectOutputBusVolume_245_bridge, (void*)&AK_SoundEngine_SetGameObjectOutputBusVolume_245_hook },
    { "?SetListenerSpatialization@SoundEngine@AK@@YA?AW4AKRESULT@@_K_NUAkChannelConfig@@PEAM@Z", (void**)&AK_SoundEngine_SetListenerSpatialization_246_orig, (void**)&AK_SoundEngine_SetListenerSpatialization_246_bridge, (void*)&AK_SoundEngine_SetListenerSpatialization_246_hook },
    { "?SetListeners@SoundEngine@AK@@YA?AW4AKRESULT@@_KPEB_KK@Z", (void**)&AK_SoundEngine_SetListeners_247_orig, (void**)&AK_SoundEngine_SetListeners_247_bridge, (void*)&AK_SoundEngine_SetListeners_247_hook },
    { "?SetMaxNumVoicesLimit@SoundEngine@AK@@YA?AW4AKRESULT@@G@Z", (void**)&AK_SoundEngine_SetMaxNumVoicesLimit_249_orig, (void**)&AK_SoundEngine_SetMaxNumVoicesLimit_249_bridge, (void*)&AK_SoundEngine_SetMaxNumVoicesLimit_249_hook },
    { "?SetMedia@SoundEngine@AK@@YA?AW4AKRESULT@@PEAUAkSourceSettings@@K@Z", (void**)&AK_SoundEngine_SetMedia_250_orig, (void**)&AK_SoundEngine_SetMedia_250_bridge, (void*)&AK_SoundEngine_SetMedia_250_hook },
    { "?SetMemPool@WriteBytesMem@AK@@QEAAXJ@Z", (void**)&AK_WriteBytesMem_SetMemPool_252_orig, (void**)&AK_WriteBytesMem_SetMemPool_252_bridge, (void*)&AK_WriteBytesMem_SetMemPool_252_hook },
    { "?SetMixer@SoundEngine@AK@@YA?AW4AKRESULT@@KK@Z", (void**)&AK_SoundEngine_SetMixer_253_orig, (void**)&AK_SoundEngine_SetMixer_253_bridge, (void*)&AK_SoundEngine_SetMixer_253_hook },
    { "?SetMixer@SoundEngine@AK@@YA?AW4AKRESULT@@PEBDK@Z", (void**)&AK_SoundEngine_SetMixer_254_orig, (void**)&AK_SoundEngine_SetMixer_254_bridge, (void*)&AK_SoundEngine_SetMixer_254_hook },
    { "?SetMixer@SoundEngine@AK@@YA?AW4AKRESULT@@PEB_WK@Z", (void**)&AK_SoundEngine_SetMixer_255_orig, (void**)&AK_SoundEngine_SetMixer_255_bridge, (void*)&AK_SoundEngine_SetMixer_255_hook },
    { "?SetMultipleObstructionAndOcclusion@SoundEngine@AK@@YA?AW4AKRESULT@@_K0PEAUAkObstructionOcclusionValues@@K@Z", (void**)&AK_SoundEngine_SetMultipleObstructionAndOcclusion_257_orig, (void**)&AK_SoundEngine_SetMultipleObstructionAndOcclusion_257_bridge, (void*)&AK_SoundEngine_SetMultipleObstructionAndOcclusion_257_hook },
    { "?SetMultiplePositions@SoundEngine@AK@@YA?AW4AKRESULT@@_KPEBUAkChannelEmitter@@GW4MultiPositionType@12@@Z", (void**)&AK_SoundEngine_SetMultiplePositions_258_orig, (void**)&AK_SoundEngine_SetMultiplePositions_258_bridge, (void*)&AK_SoundEngine_SetMultiplePositions_258_hook },
    { "?SetMultiplePositions@SoundEngine@AK@@YA?AW4AKRESULT@@_KPEBVAkTransform@@GW4MultiPositionType@12@@Z", (void**)&AK_SoundEngine_SetMultiplePositions_259_orig, (void**)&AK_SoundEngine_SetMultiplePositions_259_bridge, (void*)&AK_SoundEngine_SetMultiplePositions_259_hook },
    { "?SetObjectObstructionAndOcclusion@SoundEngine@AK@@YA?AW4AKRESULT@@_K0MM@Z", (void**)&AK_SoundEngine_SetObjectObstructionAndOcclusion_260_orig, (void**)&AK_SoundEngine_SetObjectObstructionAndOcclusion_260_bridge, (void*)&AK_SoundEngine_SetObjectObstructionAndOcclusion_260_hook },
    { "?SetOutputVolume@SoundEngine@AK@@YA?AW4AKRESULT@@_KM@Z", (void**)&AK_SoundEngine_SetOutputVolume_261_orig, (void**)&AK_SoundEngine_SetOutputVolume_261_bridge, (void*)&AK_SoundEngine_SetOutputVolume_261_hook },
    { "?SetPanningRule@SoundEngine@AK@@YA?AW4AKRESULT@@W4AkPanningRule@@_K@Z", (void**)&AK_SoundEngine_SetPanningRule_262_orig, (void**)&AK_SoundEngine_SetPanningRule_262_bridge, (void*)&AK_SoundEngine_SetPanningRule_262_hook },
    { "?SetPoolName@MemoryMgr@AK@@YA?AW4AKRESULT@@JPEBD@Z", (void**)&AK_MemoryMgr_SetPoolName_263_orig, (void**)&AK_MemoryMgr_SetPoolName_263_bridge, (void*)&AK_MemoryMgr_SetPoolName_263_hook },
    { "?SetPosition@SoundEngine@AK@@YA?AW4AKRESULT@@_KAEBVAkTransform@@@Z", (void**)&AK_SoundEngine_SetPosition_265_orig, (void**)&AK_SoundEngine_SetPosition_265_bridge, (void*)&AK_SoundEngine_SetPosition_265_hook },
    { "?SetPositionInternal@SoundEngine@AK@@YA?AW4AKRESULT@@_KAEBVAkTransform@@@Z", (void**)&AK_SoundEngine_SetPositionInternal_266_orig, (void**)&AK_SoundEngine_SetPositionInternal_266_bridge, (void*)&AK_SoundEngine_SetPositionInternal_266_hook },
    { "?SetRandomSeed@SoundEngine@AK@@YAXK@Z", (void**)&AK_SoundEngine_SetRandomSeed_275_orig, (void**)&AK_SoundEngine_SetRandomSeed_275_bridge, (void*)&AK_SoundEngine_SetRandomSeed_275_hook },
    { "?SetScalingFactor@SoundEngine@AK@@YA?AW4AKRESULT@@_KM@Z", (void**)&AK_SoundEngine_SetScalingFactor_276_orig, (void**)&AK_SoundEngine_SetScalingFactor_276_bridge, (void*)&AK_SoundEngine_SetScalingFactor_276_hook },
    { "?SetSpeakerAngles@SoundEngine@AK@@YA?AW4AKRESULT@@PEBMKM_K@Z", (void**)&AK_SoundEngine_SetSpeakerAngles_277_orig, (void**)&AK_SoundEngine_SetSpeakerAngles_277_bridge, (void*)&AK_SoundEngine_SetSpeakerAngles_277_hook },
    { "?SetVolumeThreshold@SoundEngine@AK@@YA?AW4AKRESULT@@M@Z", (void**)&AK_SoundEngine_SetVolumeThreshold_284_orig, (void**)&AK_SoundEngine_SetVolumeThreshold_284_bridge, (void*)&AK_SoundEngine_SetVolumeThreshold_284_hook },
    { "?StartOutputCapture@SoundEngine@AK@@YA?AW4AKRESULT@@PEB_W@Z", (void**)&AK_SoundEngine_StartOutputCapture_285_orig, (void**)&AK_SoundEngine_StartOutputCapture_285_bridge, (void*)&AK_SoundEngine_StartOutputCapture_285_hook },
    { "?Stop@DynamicSequence@SoundEngine@AK@@YA?AW4AKRESULT@@KJW4AkCurveInterpolation@@@Z", (void**)&AK_SoundEngine_DynamicSequence_Stop_288_orig, (void**)&AK_SoundEngine_DynamicSequence_Stop_288_bridge, (void*)&AK_SoundEngine_DynamicSequence_Stop_288_hook },
    { "?StopMIDIOnEvent@SoundEngine@AK@@YA?AW4AKRESULT@@K_K@Z", (void**)&AK_SoundEngine_StopMIDIOnEvent_290_orig, (void**)&AK_SoundEngine_StopMIDIOnEvent_290_bridge, (void*)&AK_SoundEngine_StopMIDIOnEvent_290_hook },
    { "?StopOutputCapture@SoundEngine@AK@@YA?AW4AKRESULT@@XZ", (void**)&AK_SoundEngine_StopOutputCapture_291_orig, (void**)&AK_SoundEngine_StopOutputCapture_291_bridge, (void*)&AK_SoundEngine_StopOutputCapture_291_hook },
    { "?StopSourcePlugin@SoundEngine@AK@@YA?AW4AKRESULT@@KKK@Z", (void**)&AK_SoundEngine_StopSourcePlugin_295_orig, (void**)&AK_SoundEngine_StopSourcePlugin_295_bridge, (void*)&AK_SoundEngine_StopSourcePlugin_295_hook },
    { "?Suspend@SoundEngine@AK@@YA?AW4AKRESULT@@_N@Z", (void**)&AK_SoundEngine_Suspend_296_orig, (void**)&AK_SoundEngine_Suspend_296_bridge, (void*)&AK_SoundEngine_Suspend_296_hook },
    { "?Term@MusicEngine@AK@@YAXXZ", (void**)&AK_MusicEngine_Term_298_orig, (void**)&AK_MusicEngine_Term_298_bridge, (void*)&AK_MusicEngine_Term_298_hook },
    { "?UnlockPlaylist@DynamicSequence@SoundEngine@AK@@YA?AW4AKRESULT@@K@Z", (void**)&AK_SoundEngine_DynamicSequence_UnlockPlaylist_306_orig, (void**)&AK_SoundEngine_DynamicSequence_UnlockPlaylist_306_bridge, (void*)&AK_SoundEngine_DynamicSequence_UnlockPlaylist_306_hook },
    { "?UnpinEventInStreamCache@SoundEngine@AK@@YA?AW4AKRESULT@@K@Z", (void**)&AK_SoundEngine_UnpinEventInStreamCache_307_orig, (void**)&AK_SoundEngine_UnpinEventInStreamCache_307_bridge, (void*)&AK_SoundEngine_UnpinEventInStreamCache_307_hook },
    { "?UnpinEventInStreamCache@SoundEngine@AK@@YA?AW4AKRESULT@@PEBD@Z", (void**)&AK_SoundEngine_UnpinEventInStreamCache_308_orig, (void**)&AK_SoundEngine_UnpinEventInStreamCache_308_bridge, (void*)&AK_SoundEngine_UnpinEventInStreamCache_308_hook },
    { "?UnpinEventInStreamCache@SoundEngine@AK@@YA?AW4AKRESULT@@PEB_W@Z", (void**)&AK_SoundEngine_UnpinEventInStreamCache_309_orig, (void**)&AK_SoundEngine_UnpinEventInStreamCache_309_bridge, (void*)&AK_SoundEngine_UnpinEventInStreamCache_309_hook },
    { "?UnregisterAllGameObj@SoundEngine@AK@@YA?AW4AKRESULT@@XZ", (void**)&AK_SoundEngine_UnregisterAllGameObj_310_orig, (void**)&AK_SoundEngine_UnregisterAllGameObj_310_bridge, (void*)&AK_SoundEngine_UnregisterAllGameObj_310_hook },
    { "?UnregisterAudioDeviceStatusCallback@SoundEngine@AK@@YA?AW4AKRESULT@@XZ", (void**)&AK_SoundEngine_UnregisterAudioDeviceStatusCallback_311_orig, (void**)&AK_SoundEngine_UnregisterAudioDeviceStatusCallback_311_bridge, (void*)&AK_SoundEngine_UnregisterAudioDeviceStatusCallback_311_hook },
    { "?UnregisterGlobalCallback@SoundEngine@AK@@YA?AW4AKRESULT@@P6AXPEAVIAkGlobalPluginContext@2@W4AkGlobalCallbackLocation@@PEAX@ZK@Z", (void**)&AK_SoundEngine_UnregisterGlobalCallback_313_orig, (void**)&AK_SoundEngine_UnregisterGlobalCallback_313_bridge, (void*)&AK_SoundEngine_UnregisterGlobalCallback_313_hook },
    { "?UnsetMedia@SoundEngine@AK@@YA?AW4AKRESULT@@PEAUAkSourceSettings@@K@Z", (void**)&AK_SoundEngine_UnsetMedia_314_orig, (void**)&AK_SoundEngine_UnsetMedia_314_bridge, (void*)&AK_SoundEngine_UnsetMedia_314_hook },
    { "?WakeupFromSuspend@SoundEngine@AK@@YA?AW4AKRESULT@@XZ", (void**)&AK_SoundEngine_WakeupFromSuspend_315_orig, (void**)&AK_SoundEngine_WakeupFromSuspend_315_bridge, (void*)&AK_SoundEngine_WakeupFromSuspend_315_hook },
    { "?WriteBytes@WriteBytesCount@AK@@UEAA_NPEBXJAEAJ@Z", (void**)&AK_WriteBytesCount_WriteBytes_316_orig, (void**)&AK_WriteBytesCount_WriteBytes_316_bridge, (void*)&AK_WriteBytesCount_WriteBytes_316_hook },
    { "?WriteBytes@WriteBytesMem@AK@@UEAA_NPEBXJAEAJ@Z", (void**)&AK_WriteBytesMem_WriteBytes_317_orig, (void**)&AK_WriteBytesMem_WriteBytes_317_bridge, (void*)&AK_WriteBytesMem_WriteBytes_317_hook },
};

}