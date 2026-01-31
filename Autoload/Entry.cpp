#include "Common/Base.hpp"
#include "Common/DefaultLogger.hpp"
#include "Autoload/Entry.hpp"
#include "Autoload/Hooks.hpp"
#include "Autoload/DLCPackage.hpp"
#include "Autoload/SharedVersion.h"

SPI_PLUGINSIDE_SUPPORT(SDK_TARGET_NAME_W ASI_NAME_NO_SPACE_W, DEVELOPER_W, L"" VERSION_STRING_W, SPI_GAME_SDK_TARGET, SPI_VERSION_ANY);
SPI_PLUGINSIDE_POSTLOAD;
SPI_PLUGINSIDE_ASYNCATTACH;


SPI_IMPLEMENT_ATTACH
{
    ::LESDK::Initializer Init{ InterfacePtr, SDK_TARGET_NAME_A ASI_NAME_NO_SPACE_A };
    ::Common::SetupDefaultLogger(SDK_TARGET_NAME_A, ASI_NAME_NO_SPACE_A);
    ::Autoload::InitializeGlobals(Init);
    ::Autoload::InitializeHooks(Init);
    return true;
}

SPI_IMPLEMENT_DETACH
{
    LEASI_UNUSED(InterfacePtr);
    ::Common::ShutdownLogger();
    return true;
}


namespace Autoload
{
    void InitializeGlobals(::LESDK::Initializer& Init)
    {
        Common::InitializeRequiredGlobals(Init);
        LEASI_TRACE("globals initialized");
    }

    void InitializeHooks(::LESDK::Initializer& Init)
    {
        // TFC registration
        RegisterTFC = Init.ResolveTyped<tRegisterTFC>(REGISTER_TFC_RVA);
        CHECK_RESOLVED(RegisterTFC);

        // ISB registration
		auto const CacheContentWrapper_target = Init.ResolveTyped<tCacheContentWrapper>(CACHECONTENT_WRAPPER_RVA);
		CHECK_RESOLVED(CacheContentWrapper_target);
		CacheContentWrapper_orig = (tCacheContentWrapper*)Init.InstallHook("CacheContentWrapper", CacheContentWrapper_target, CacheContentWrapper_hook);
		CHECK_RESOLVED(CacheContentWrapper_orig);

		// OpenFileRead hook for content scan wait and install
		auto const OpenFileRead_target = Init.ResolveTyped<tOpenFileRead>(OPENFILE_READ_RVA);
		CHECK_RESOLVED(OpenFileRead_target);
		OpenFileRead_orig = (tOpenFileRead*)Init.InstallHook("OpenFileRead", OpenFileRead_target, OpenFileRead_hook);
		CHECK_RESOLVED(OpenFileRead_orig);

        // We scan now to get this done as soon as possible
        DLCPackage::ScanForDLCContent();

        // Now we stuff that won't be needed until like 10-15 seconds into the game

		// InstallDownloadableContent hook for rooting startup packages
		auto const InstallDownloadableContent_target = Init.ResolveTyped<tInstallDownloadableContent>(INSTALL_DLC_RVA);
		CHECK_RESOLVED(InstallDownloadableContent_target);
		InstallDownloadableContent_orig = (tInstallDownloadableContent*)Init.InstallHook("InstallDownloadableContent", InstallDownloadableContent_target, InstallDownloadableContent_hook);
		CHECK_RESOLVED(InstallDownloadableContent_orig);

        // Autoload.ini mount
		auto const ProcessIni_target = Init.ResolveTyped<tProcessIni>(PROCESSINI_RVA);
		CHECK_RESOLVED(ProcessIni_target);
		ProcessIni_orig = (tProcessIni*)Init.InstallHook("ExtraContent::ProcessIni", ProcessIni_target, ProcessIni_hook);
		CHECK_RESOLVED(ProcessIni_orig);

        // For console commands profile none and profile autoload
        auto const UGameEngine_Exec_target = Init.ResolveTyped<t_UGameEngine_Exec>(UGAMEENGINE_EXEC_RVA);
        CHECK_RESOLVED(UGameEngine_Exec_target);
        UGameEngine_Exec_orig = (t_UGameEngine_Exec*)Init.InstallHook("UGameEngine::Exec", UGameEngine_Exec_target, UGameEngine_Exec_hook);
        CHECK_RESOLVED(UGameEngine_Exec_orig);

		// For rendering the profiler
		auto const UObject_ProcessEvent_target = Init.ResolveTyped<tProcessEvent>(BUILTIN_PROCESSEVENT_RVA);
		CHECK_RESOLVED(UObject_ProcessEvent_target);
		ProcessEvent_orig = (tProcessEvent*)Init.InstallHook("UObject::ProcessEvent", UObject_ProcessEvent_target, ProcessEvent_hook);
		CHECK_RESOLVED(ProcessEvent_orig);

        LEASI_INFO("Hooks initialized");
    }
}
