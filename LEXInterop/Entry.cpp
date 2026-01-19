#include "Common/Base.hpp"
#include "LEXInterop/Entry.hpp"
#include "LEXInterop/Hooks.hpp"
#include "LEXInterop/Pipe.hpp"
#include "LEXInterop/FileLoader.hpp"
#include "LEXInterop/AdditionalFunctions.hpp"

SPI_PLUGINSIDE_SUPPORT(SDK_TARGET_NAME_W L"LEXInterop", L"ME3Tweaks", L"9.0.0", SPI_GAME_SDK_TARGET, SPI_VERSION_ANY);
SPI_PLUGINSIDE_POSTLOAD;
SPI_PLUGINSIDE_ASYNCATTACH;

SPI_IMPLEMENT_ATTACH
{
#ifdef DEBUG
    ::LESDK::InitializeConsole();
#endif
    ::LESDK::Initializer Init{ InterfacePtr, SDK_TARGET_NAME_A "LEXInterop" };

    ::LEXInterop::InitializeGlobals(Init);
    ::LEXInterop::FileLoader::InitializePackagePrecacheMap(Init);
    ::LEXInterop::InitializeHooks(Init);
	::LEXInterop::InitializeAdditionalFunctions(Init);
    ::LEXInterop::StartPipeThread();

    LEASI_INFO("LEXInterop initialized");
    return true;
}

SPI_IMPLEMENT_DETACH
{
    LEASI_UNUSED(InterfacePtr);

    ::LEXInterop::StopPipeThread();
#ifdef DEBUG
    ::LESDK::TerminateConsole();
#endif

    return true;
}

namespace LEXInterop
{
    void InitializeGlobals(::LESDK::Initializer& Init)
    {
        Common::InitializeRequiredGlobals(Init);
        GEngine = Init.ResolveTyped<UEngine*>(BUILTIN_GENGINE_RIP);
        CHECK_RESOLVED(GEngine);
        GNatives = Init.ResolveTyped<tNative*>(BUILTIN_GNATIVES_RIP);
        CHECK_RESOLVED(GNatives);
        GSys = Init.ResolveTyped<USystem*>(BUILTIN_GSYS_RIP);
        CHECK_RESOLVED(GSys);
        GWorld = Init.ResolveTyped<UWorld*>(BUILTIN_GWORLD_RIP);
        CHECK_RESOLVED(GWorld);
        GError = Init.ResolveTyped<void*>(BUILTIN_GWORLD_RIP);
        CHECK_RESOLVED(GError);
        LEASI_INFO("Globals initialized");
    }

    void InitializeHooks(::LESDK::Initializer& Init)
    {
        // ProcessEvent hook
        auto const ProcessEvent_target = Init.ResolveTyped<t_UObject_ProcessEvent>(BUILTIN_PROCESSEVENT_PHOOK);
        CHECK_RESOLVED(ProcessEvent_target);
        UObject_ProcessEvent_orig = reinterpret_cast<t_UObject_ProcessEvent*>(Init.InstallHook("UObject::ProcessEvent", ProcessEvent_target, UObject_ProcessEvent_hook));
        CHECK_RESOLVED(UObject_ProcessEvent_orig);

        // FindPackageFile hook
#if defined(SDK_TARGET_LE1)
        constexpr auto findPackageFilePattern = "54 41 55 41 56 41 57 48 8d 6c 24 e1 48 81 ec e0 00 00 00 48 c7 45 b7 fe ff ff ff 48 89 9c 24 20 01 00 00";
#elif defined(SDK_TARGET_LE2)
        constexpr auto findPackageFilePattern = "54 41 55 41 56 41 57 48 8d 6c 24 e1 48 81 ec e0 00 00 00 48 c7 45 b7 fe ff ff ff 48 89 9c 24 20 01 00 00 48 8b 05 d9 1b b9 00";
#elif defined(SDK_TARGET_LE3)
        constexpr auto findPackageFilePattern = "48 89 4c 24 08 55 56 57 41 54 41 55 41 56 41 57 48 8b ec 48 81 ec 80 00 00 00 48 c7 45 b8 fe ff ff ff";
#endif
        auto const FindPackageFile_target = Init.ResolveTyped<t_FindPackageFile>(::LESDK::Address::FromPostHook(findPackageFilePattern));
        CHECK_RESOLVED(FindPackageFile_target);
        FindPackageFile_orig = reinterpret_cast<t_FindPackageFile*>(Init.InstallHook("FindPackageFile", FindPackageFile_target, FindPackageFile_hook));
        CHECK_RESOLVED(FindPackageFile_orig);

        // SetLinker hook 
        constexpr auto setLinkerPattern = "8b c9 4d 85 d2 74 39 48 85 d2 74 1c 48 8b c1";
        auto const SetLinker_target = Init.ResolveTyped<t_SetLinker>(::LESDK::Address::FromPostHook(setLinkerPattern));
        CHECK_RESOLVED(SetLinker_target);
        SetLinker_orig = reinterpret_cast<t_SetLinker*>(Init.InstallHook("SetLinker", SetLinker_target, SetLinker_hook));
        CHECK_RESOLVED(SetLinker_target);

        LEASI_INFO("Hooks initialized");
    }

    void StartPipeThread()
    {
        PipeServer::Start();
    }

    void StopPipeThread()
    {
        PipeServer::Stop();
    }
}
