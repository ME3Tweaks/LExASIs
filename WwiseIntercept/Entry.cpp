#include "Entry.hpp"
#include "Hooks.hpp"

#include <SPI.h>
#include "Common/Base.hpp"
#include "Common/DefaultLogger.hpp"

#include "WwiseIntercept/SharedVersion.h"

SPI_PLUGINSIDE_SUPPORT(SDK_TARGET_NAME_W ASI_NAME_NO_SPACE_W, DEVELOPER_W, L"" VERSION_STRING_W, SPI_GAME_SDK_TARGET, SPI_VERSION_V4);
SPI_PLUGINSIDE_POSTLOAD;

// Deliberately SEQATTACH, not ASYNCATTACH like the other ASIs in this repo.
// Wwise initializes very early (AK::MemoryMgr::Init, called from a BioWare
// wrapper function reached almost immediately during engine startup) - all
// ~300 InstallHook calls below must complete before the game can possibly
// reach that call. SEQATTACH runs SpiOnAttach synchronously on the thread
// driving startup and blocks further game startup until it returns, which
// guarantees that ordering without any manual thread suspension. See the
// WwiseIntercept plan for the full reasoning.
SPI_PLUGINSIDE_SEQATTACH;


SPI_IMPLEMENT_ATTACH
{
    ::LESDK::Initializer Init{ InterfacePtr, SDK_TARGET_NAME_A ASI_NAME_NO_SPACE_A };
    Common::SetupDefaultLogger(SDK_TARGET_NAME_A, ASI_NAME_NO_SPACE_A, true, Init);
    LEASI_INFO("Welcome to Wwise Intercept");
    ::WwiseIntercept::InitializeHooks(Init);

    return true;
}

SPI_IMPLEMENT_DETACH
{
    LEASI_UNUSED(InterfacePtr);
    Common::ShutdownLogger();
    return true;
}
