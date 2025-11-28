#include "TextureOverride/Hooks.hpp"
#include "TextureOverride/Loading.hpp"

namespace fs = std::filesystem;


namespace TextureOverride
{

    t_UGameEngine_Exec* UGameEngine_Exec_orig = nullptr;
    DWORD UGameEngine_Exec_hook(UGameEngine* const Context, WCHAR const* const Command, void* const Archive)
    {
        if (std::wstring_view const CommandView{ Command };
            CommandView.starts_with(L"to.") || CommandView.starts_with(L"TO."))
        {
            FString const CommandCopy{ Command };
            if (CommandCopy.Contains(L"to.disable"))
            {
                g_enableLoadingManifest = false;
                LEASI_WARN(L"texture override disabled via console command");
                LEASI_WARN(L"manifests will still be processed");
            }
            else if (CommandCopy.Contains(L"to.enable"))
            {
                g_enableLoadingManifest = true;
                LEASI_WARN(L"texture override re-enabled via console command");
            }
#ifdef _DEBUG
            else if (CommandCopy.Contains(L"to.stats"))
            {
                g_statTextureSerializeCount = std::max<int>(g_statTextureSerializeCount, 1);

                LEASI_INFO(L"(stats) UTexture2D::Serialize time added:     {:.4f} ms", g_statTextureSerializeSeconds * 1000);
                LEASI_INFO(L"(stats) UTexture2D::Serialize active count:   {}", g_statTextureSerializeCount);
                LEASI_INFO(L"(stats) ==================================================");
                LEASI_INFO(L"(stats) AVERAGE TIME ADDED:                   {:.4f} ms", g_statTextureSerializeSeconds * 1000 / g_statTextureSerializeCount);
            }
#endif
        }

        return UGameEngine_Exec_orig(Context, Command, Archive);
    }

    t_UTexture2D_Serialize* UTexture2D_Serialize_orig = nullptr;
    void UTexture2D_Serialize_hook(UTexture2D* const Context, void* const Archive)
    {
        FString const& TextureFullName = GetTextureFullName(Context);
        //LEASI_TRACE(L"UTexture2D::Serialize: {}", *TextureFullName);

        (*UTexture2D_Serialize_orig)(Context, Archive);

        if (g_enableLoadingManifest)
        {
#ifdef _DEBUG
            ScopedTimer Timer{};
#endif

            for (ManifestLoaderPointer const& Manifest : g_loadedManifests)
            {
                CTextureEntry const* const Entry = Manifest->FindEntry(TextureFullName);
                if (Entry == nullptr) continue;
                LEASI_INFO(L"UTexture2D::Serialize: replacing {}", *TextureFullName);
                UpdateTextureFromManifest(Context, *Manifest, *Entry);
                return;  // apply only the highest-priority manifest mount
            }

#ifdef _DEBUG
            g_statTextureSerializeCount++;
            g_statTextureSerializeSeconds += Timer.GetSeconds();
#endif
        }

        // Clown mode
        // Context->InternalFormatLODBias = 12;
    }

    t_OodleDecompress* OodleDecompress = nullptr;


#if defined(SDK_TARGET_LE3)

    tRegisterTFC* RegisterTFC = nullptr;
    tGetDLCName* GetDLCName = nullptr;

    tRegisterDLCTFC* RegisterDLCTFC_orig = nullptr;
    unsigned long long RegisterDLCTFC_hook(SFXAdditionalContent* const Content)
    {
        // Record already mounted DLC names, so we don't mount twice.
        static std::set<std::wstring> MountedDLCNames{};

        unsigned long long Result = RegisterDLCTFC_orig(Content);

        FString DlcName;
        GetDLCName(Content, &DlcName);

        if (DlcName.Length() <= 8 || !DlcName.StartsWith(L"DLC_MOD_"))
        {
            // Not a DLC mod.
            return Result;
        }

        std::wstring DlcNameStr{ DlcName.Chars() };
        if (MountedDLCNames.contains(DlcNameStr))
        {
            // The DLC TFCs were already mounted. Game seems to call this twice for some reason.
            return Result;
        }
        MountedDLCNames.insert(DlcNameStr);

        fs::path const CookedPath{ Content->RelativeDLCPath.Chars() };
        fs::path const DlcCookedPath = CookedPath / L"CookedPCConsole";

        if (fs::exists(DlcCookedPath))
        {
            FString const TfcNameStem = FString::Printf(L"Textures_%s.tfc", *DlcName);
            fs::path const TfcDefaultPath = DlcCookedPath / *TfcNameStem;

            // Find TFCs in CookedPCConsole folder
            for (fs::directory_entry const& p : fs::directory_iterator(DlcCookedPath))
            {
                if (p.path().extension() == L".tfc")
                {
                    if (p.path().compare(TfcDefaultPath) != 0)  // Not the default TFC
                    {
                        LEASI_INFO(L"Registering additional mod TFC: {}", p.path().c_str());
                        FString TfcName{ p.path().c_str() };
                        RegisterTFC(&TfcName);
                    }
                }
            }
        }

        return Result;
    }

#endif
}
