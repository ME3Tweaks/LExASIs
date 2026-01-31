#include "Common/Base.hpp"
#include "LEXInterop/Entry.hpp"
#include "LEXInterop/Hooks.hpp"
#include "LEXInterop/Pipe.hpp"
#include "LEXInterop/FileLoader.hpp"
#include "LEXInterop/AdditionalFunctions.hpp"
#include "LEXInterop/ScriptDebugger/ScriptDebugger.hpp"
#include "LEXInterop/SharedVersion.h"

SPI_PLUGINSIDE_SUPPORT(SDK_TARGET_NAME_W ASI_NAME_NO_SPACE_W, DEVELOPER_W, L"" VERSION_STRING_W, SPI_GAME_SDK_TARGET, SPI_VERSION_ANY);
SPI_PLUGINSIDE_POSTLOAD;
SPI_PLUGINSIDE_ASYNCATTACH;

SPI_IMPLEMENT_ATTACH
{
#ifdef DEBUG
    ::LESDK::InitializeConsole();
#endif
    ::LESDK::Initializer Init{ InterfacePtr, SDK_TARGET_NAME_A ASI_NAME_NO_SPACE_A };

    ::LEXInterop::InitializeGlobals(Init);
    ::LEXInterop::FileLoader::InitializePackagePrecacheMap(Init);
    ::LEXInterop::InitializeHooks(Init);
    ::LEXInterop::InitializeAdditionalFunctions(Init);
    ::LEXInterop::ScriptDebugger::Initialize(Init);
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
        GError = Init.ResolveTyped<void*>(BUILTIN_GERROR_RIP);
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
        auto const FindPackageFile_target = Init.ResolveTyped<t_FindPackageFile>(BUILTIN_FINDPACKAGEFILE_RVA);
        CHECK_RESOLVED(FindPackageFile_target);
        FindPackageFile_orig = reinterpret_cast<t_FindPackageFile*>(Init.InstallHook("FindPackageFile", FindPackageFile_target, FindPackageFile_hook));
        CHECK_RESOLVED(FindPackageFile_orig);

        // SetLinker hook 
        auto const SetLinker_target = Init.ResolveTyped<t_SetLinker>(BUILTIN_SETLINKER_RVA);
        CHECK_RESOLVED(SetLinker_target);
        SetLinker_orig = reinterpret_cast<t_SetLinker*>(Init.InstallHook("SetLinker", SetLinker_target, SetLinker_hook));
        CHECK_RESOLVED(SetLinker_orig);

        // GameEngineTick hook 
        auto const GameEngineTick_target = Init.ResolveTyped<t_GameEngineTick>(BUILTIN_GAMEENGINETICK_RVA);
        CHECK_RESOLVED(GameEngineTick_target);
        GameEngineTick_orig = reinterpret_cast<t_GameEngineTick*>(Init.InstallHook("GameEngineTick", GameEngineTick_target, GameEngineTick_hook));
        CHECK_RESOLVED(GameEngineTick_orig);

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
