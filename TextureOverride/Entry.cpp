#include "Common/Base.hpp"
#include "Common/DefaultLogger.hpp"
#include "TextureOverride/Entry.hpp"
#include "TextureOverride/Hooks.hpp"
#include "TextureOverride/Loading.hpp"
#include "TextureOverride/SharedVersion.h"

SPI_PLUGINSIDE_SUPPORT(SDK_TARGET_NAME_W ASI_NAME_NO_SPACE_W, DEVELOPER_W, L"" VERSION_STRING_W, SPI_GAME_SDK_TARGET, SPI_VERSION_ANY);
SPI_PLUGINSIDE_POSTLOAD;
SPI_PLUGINSIDE_ASYNCATTACH;


SPI_IMPLEMENT_ATTACH
{
    ::LESDK::Initializer Init{ InterfacePtr, SDK_TARGET_NAME_A ASI_NAME_NO_SPACE_A };
    Common::SetupDefaultLogger(SDK_TARGET_NAME_A, ASI_NAME_NO_SPACE_A);
    ::TextureOverride::InitializeGlobals(Init);
    ::TextureOverride::InitializeHooks(Init);
    ::TextureOverride::InitializeArgs();
    ::TextureOverride::LoadDlcManifests();
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


namespace TextureOverride
{
    void InitializeGlobals(::LESDK::Initializer& Init)
    {
		Common::InitializeRequiredGlobals(Init);

        //GEngine = Init.ResolveTyped<UEngine*>(BUILTIN_GENGINE_RIP);
        //CHECK_RESOLVED(GEngine);
        //GNatives = Init.ResolveTyped<tNative*>(BUILTIN_GNATIVES_RIP);
        //CHECK_RESOLVED(GNatives);
        //GSys = Init.ResolveTyped<USystem*>(BUILTIN_GSYS_RIP);
        //CHECK_RESOLVED(GSys);
        //GWorld = Init.ResolveTyped<UWorld*>(BUILTIN_GWORLD_RIP);
        //CHECK_RESOLVED(GWorld);

        LEASI_INFO("globals initialized");
    }

    void InitializeHooks(::LESDK::Initializer& Init)
    {
#if defined(SDK_TARGET_LE2) || defined(SDK_TARGET_LE3)
        // GFileManager so we can use InternalFindFiles
        GFileManager = Init.ResolveTyped<void*>(GFILEMANAGER_RVA);
        CHECK_RESOLVED(GFileManager);

        // InternalFindFiles
        InternalFindFiles = Init.ResolveTyped<tInternalFindFiles>(INTERNAL_FIND_FILES_RVA);
        CHECK_RESOLVED(InternalFindFiles);

        // TFC registration
        RegisterTFC = Init.ResolveTyped<tRegisterTFC>(REGISTER_TFC_RVA);
        CHECK_RESOLVED(RegisterTFC);
#endif

        // Find oodle decompression function.
        OodleDecompress = Init.ResolveTyped<t_OodleDecompress>(OODLE_DECOMPRESS_RVA);
        CHECK_RESOLVED(OodleDecompress);

        // For replacing mip data
        auto const UTexture2D_Serialize_target = Init.ResolveTyped<t_UTexture2D_Serialize>(UTEXTURE2D_SERIALIZE_RVA);
        CHECK_RESOLVED(UTexture2D_Serialize_target);
        UTexture2D_Serialize_orig = (t_UTexture2D_Serialize*)Init.InstallHook("UTexture2D::Serialize", UTexture2D_Serialize_target, UTexture2D_Serialize_hook);
        CHECK_RESOLVED(UTexture2D_Serialize_orig);

        // For console commands
        auto const UGameEngine_Exec_target = Init.ResolveTyped<t_UGameEngine_Exec>(UGAMEENGINE_EXEC_RVA);
        CHECK_RESOLVED(UGameEngine_Exec_target);
        UGameEngine_Exec_orig = (t_UGameEngine_Exec*)Init.InstallHook("UGameEngine::Exec", UGameEngine_Exec_target, UGameEngine_Exec_hook);
        CHECK_RESOLVED(UGameEngine_Exec_orig);

        LEASI_INFO("hooks initialized");
    }

    void InitializeArgs()
    {
        FString CmdArgs{ GetCommandLineW() };
        CmdArgs.Append(L" ");

        if (CmdArgs.Contains(L" -disabletextureoverride ", true))
        {
            g_enableLoadingManifest = false;
            LEASI_WARN(L"texture override disabled via cmd args");
            LEASI_WARN(L"manifests will still be processed");
        }
    }
}
