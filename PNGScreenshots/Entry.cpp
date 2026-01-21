#include "Entry.hpp"
#include "Hooks.hpp"

#include <SPI.h>
#include "Common/Base.hpp"

#include <spdlog/spdlog.h>

#include "PNGScreenshots/SharedVersion.h"

SPI_PLUGINSIDE_SUPPORT(SDK_TARGET_NAME_W ASI_NAME_NO_SPACE_W, DEVELOPER_W, L"" VERSION_STRING_W, SPI_GAME_SDK_TARGET, SPI_VERSION_ANY);
SPI_PLUGINSIDE_POSTLOAD;
SPI_PLUGINSIDE_ASYNCATTACH;


SPI_IMPLEMENT_ATTACH
{
    ::LESDK::Initializer Init{ InterfacePtr, SDK_TARGET_NAME_A ASI_NAME_NO_SPACE_A };
    ::PNGScreenshots::InitializeHooks(Init);
    return true;
}

SPI_IMPLEMENT_DETACH
{
    LEASI_UNUSED(InterfacePtr);
#ifdef _DEBUG
    ::LESDK::TerminateConsole();
#endif
    return true;
}


namespace PNGScreenshots
{
    void InitializeHooks(::LESDK::Initializer& Init)
    {
        // For console commands
        auto const appCreateBitmap_target = Init.ResolveTyped<t_AppCreateBitmap>(APP_MAKE_BITMAP_RVA);
        CHECK_RESOLVED(appCreateBitmap_target);
        appCreateBitmap_orig = (t_AppCreateBitmap*)Init.InstallHook("AppCreateBitmap", appCreateBitmap_target, appCreateBitmap_hook);
        CHECK_RESOLVED(appCreateBitmap_orig);
    }
}
