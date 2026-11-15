#include "Entry.hpp"
#include "Hooks.hpp"

#include <SPI.h>
#include "Common/Base.hpp"
#include "Common/DefaultLogger.hpp"

#include "ImageScanner/SharedVersion.h"

SPI_PLUGINSIDE_SUPPORT(SDK_TARGET_NAME_W ASI_NAME_NO_SPACE_W, DEVELOPER_W, L"" VERSION_STRING_W, SPI_GAME_SDK_TARGET, SPI_VERSION_V4);
SPI_PLUGINSIDE_POSTLOAD;
SPI_PLUGINSIDE_ASYNCATTACH;


SPI_IMPLEMENT_ATTACH
{
    ::LESDK::Initializer Init{ InterfacePtr, SDK_TARGET_NAME_A ASI_NAME_NO_SPACE_A };
    Common::SetupDefaultLogger(SDK_TARGET_NAME_A, ASI_NAME_NO_SPACE_A, true, Init);
    LEASI_INFO("Welcome to Image Scanner");
    ::ImageScanner::InitializeHooks(Init);
    return true;
}

SPI_IMPLEMENT_DETACH
{
    LEASI_UNUSED(InterfacePtr);
    Common::ShutdownLogger();
    return true;
}


namespace ImageScanner
{
    void InitializeHooks(::LESDK::Initializer& Init)
    {
        Common::InitializeRequiredGlobals(Init);

        // For console commands
        auto const UGameEngine_Exec_target = Init.ResolveTyped<t_UGameEngine_Exec>(BUILTIN_UGAMEENGINE_EXEC_RVA);
        CHECK_RESOLVED(UGameEngine_Exec_target);
        UGameEngine_Exec_orig = (t_UGameEngine_Exec*)Init.InstallHook("UGameEngine::Exec", UGameEngine_Exec_target, UGameEngine_Exec_hook);
        CHECK_RESOLVED(UGameEngine_Exec_orig);

        // Hook the appCreateBitMap
        auto const appCreateBitmap_target = Init.ResolveTyped<t_AppCreateBitmap>(APP_MAKE_BITMAP_RVA);
        CHECK_RESOLVED(appCreateBitmap_target);
        appCreateBitmap_orig = (t_AppCreateBitmap*)Init.InstallHook("AppCreateBitmap", appCreateBitmap_target, appCreateBitmap_hook);
        CHECK_RESOLVED(appCreateBitmap_orig);

        // For timing the captures
        auto const UObject_ProcessEvent_target = Init.ResolveTyped<t_UObject_ProcessEvent>(BUILTIN_PROCESSEVENT_RVA);
        CHECK_RESOLVED(UObject_ProcessEvent_target);
        UObject_ProcessEvent_orig = (t_UObject_ProcessEvent*)Init.InstallHook("UObject::ProcessEvent", UObject_ProcessEvent_target, UObject_ProcessEvent_hook);
        CHECK_RESOLVED(UObject_ProcessEvent_orig);
    }
}
