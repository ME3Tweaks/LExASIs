#pragma once

#include <LESDK/Headers.hpp>
#include <LESDK/Init.hpp>
#include <vector>

namespace PNGScreenshots::TransparentRender
{
    enum class CaptureStage
    {
        Idle,
        AwaitingWhite,
        AwaitingBlack,
    };

    extern CaptureStage stage;

    // Number of BioHUD.PostRender ticks to wait before the next screenshot is fired, so the
    // material swap has a chance to actually re-render. -1 means "not armed".
    extern int rendersUntilCapture;

    // Whether the game was launched with -transparentrender. This gates whether
    // InitializeHooks is called at all, per PNGScreenshots::TransparentRender not
    // installing its hooks unless the feature is explicitly requested.
    bool IsTransparentRenderEnabled();

    void InitializeHooks(::LESDK::Initializer& Init);

    // Called from PNGScreenshots::appCreateBitmap_hook with the already-converted RGB buffer
    // for every screenshot the engine takes. Returns true if this frame was consumed as part of
    // a transparent-render capture sequence (the caller should skip its normal save/clipboard logic).
    bool HandleCapture(wchar_t const* inputBaseName, int width, int height, const std::vector<unsigned char>& rgb);

    // ! UGameEngine::Exec hook, for the "transparentrender" console command
    // ========================================
    using t_UGameEngine_Exec = DWORD(UGameEngine* Context, WCHAR const* Command, void* Archive);
    extern t_UGameEngine_Exec* UGameEngine_Exec_orig;
    DWORD UGameEngine_Exec_hook(UGameEngine* Context, WCHAR const* Command, void* Archive);

    // ! UObject::ProcessEvent hook, used to wait a frame after each material swap before capturing
    // ========================================
    using t_UObject_ProcessEvent = void(UObject* Context, UFunction* Function, void* Parms, void* Result);
    extern t_UObject_ProcessEvent* UObject_ProcessEvent_orig;
    void UObject_ProcessEvent_hook(UObject* Context, UFunction* Function, void* Parms, void* Result);
}
