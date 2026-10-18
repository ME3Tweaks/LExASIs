#include <thread>
#include <chrono>
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
		while (!g_manifestsFinishedLoading && g_remainingAllowedStalls >= 0) {
			if (g_remainingAllowedStalls == 0) {
				LEASI_WARN(L"UTexture2D::Serialize: Gave up waiting for manifests to finish loading");
				// Prevent this loop from being able to be accessed again
				g_remainingAllowedStalls = -1;
				g_manifestsFinishedLoading = true;
				break;
			}

			LEASI_INFO(L"UTexture2D::Serialize: waiting for manifests to finish loading... ({} stalls remaining)", g_remainingAllowedStalls);
			std::this_thread::sleep_for(std::chrono::milliseconds(200));
			g_remainingAllowedStalls--;
		}

		FString const& TextureFullName = GetTextureFullName(Context);
		// LEASI_DEBUG(L"UTexture2D::Serialize: {}", *TextureFullName);

		(*UTexture2D_Serialize_orig)(Context, Archive);

#if defined(SDK_TARGET_LE2) || defined(SDK_TARGET_LE3)
		if (!bHasPerformedDLCTFCRegistration && !(Context->TFCFileGuid.A == Context->TFCFileGuid.B == Context->TFCFileGuid.C == Context->TFCFileGuid.D == 0))
		{
			// Register DLC TFCs on first texture serialize to ensure they are available
			// when UpdateResource() is called as DLC mount comes too late.	
			// LE1 will always have the TFCs registered by autoload so this is not necessary for that.
			if (!bHasPerformedDLCTFCRegistration) {
				bHasPerformedDLCTFCRegistration = true;
				RegisterDLCTFCs();
			}
		}
#endif

		if (g_enableLoadingManifest)
		{
#ifdef _DEBUG
			ScopedTimer Timer{};
#endif

			for (ManifestLoaderPointer const& Manifest : g_loadedManifests)
			{
				CTextureEntry const* const Entry = Manifest->FindEntry(TextureFullName);
				if (Entry == nullptr) continue;
                LEASI_INFO(L"UTexture2D::Serialize: replacing {} via {}", *TextureFullName, Manifest->GetDlcName().data());
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

#if defined(SDK_TARGET_LE2) || defined(SDK_TARGET_LE3)
	tRegisterTFC* RegisterTFC = nullptr;

	tInternalFindFiles* InternalFindFiles_orig = nullptr;
	tInternalFindFiles* InternalFindFiles = nullptr;
	void InternalFindFiles_hook(void* self, TArray<FString>* result, const wchar_t* searchPath, bool findFiles, bool findFolders, unsigned int flags) {

	// Left here in case you ever need to turn it on to inspect things.
#if _DEBUG && FALSE
		if (findFiles && findFolders) 
			LEASI_INFO(L"InternalFindFiles [FILES AND FOLDERS] {}: {}", flags, searchPath);
		}
		else if (findFiles) {
			LEASI_INFO(L"InternalFindFiles: [FILES] {}: {}", flags, searchPath);
		}
		else if (findFolders) {
			LEASI_INFO(L"InternalFindFiles: [FOLDERS] {}: {}", flags, searchPath);
		}
		else {
			LEASI_INFO(L"InternalFindFiles: [???] {}: {}", flags, searchPath);
		}
#endif
		InternalFindFiles_orig(self, result, searchPath, findFiles, findFolders, flags);
	// Left here if you ever need to find things later.
#if _DEBUG && FALSE
		if (findFolders) {
			for (auto& entry : *result) {
				fs::path const FilePath{ entry.Chars() };
				LEASI_INFO(L"  {}", FilePath.c_str());
			}
		}
#endif
	}

	bool bHasPerformedDLCTFCRegistration = false;
	void** GFileManager = nullptr;

#if defined(LEASI_GAME_LMP)
	// LMP TFCs use TFC Compactor's BAeName#_DLC_MOD_NAME.tfc format, which the older TO ASI didn't follow
	// so right now this only will properly work with LMP
	static bool IsSupportedLmpTfcName(std::wstring_view const FileName, std::wstring_view const DlcName)
	{
		std::wstring const Suffix = std::format(L"_{}.tfc", DlcName);
		if (FileName.size() <= Suffix.size() || wcsicmp(FileName.substr(FileName.size() - Suffix.size()).data(), Suffix.c_str()) != 0)
		{
			// Name too short or doesn't end with the DLC name suffix
			return false;
		}
		std::wstring_view Stem = FileName.substr(0, FileName.size() - Suffix.size());
		for (std::wstring_view const Prefix : { L"Textures", L"Lighting", L"CharTextures" })
		{
			// Check stem is at least as long as the prefix and starts with it (case-insensitive)
			if (Stem.size() < Prefix.size() || _wcsnicmp(Stem.data(), Prefix.data(), Prefix.size()) != 0)
				continue;

			std::wstring_view const Index = Stem.substr(Prefix.size());
			// Indexes 2-9 are allowed, 1 is the original, we don't use 0.
			if (std::all_of(Index.begin(), Index.end(), [](wchar_t const c) { return c >= L'2' && c <= L'9'; }))
			{
				return true;
			}
		}
		return false;
	}
#endif

	void RegisterDLCTFCs() {
		// List DLC_MOD_ folders
		const wchar_t* searchPath = L"..\\..\\BIOGame\\DLC\\DLC_MOD_*";
		std::filesystem::path dlcPath = L"..\\..\\BIOGame\\DLC";

		TArray<FString> dlcFolders{};
		InternalFindFiles(*GFileManager, &dlcFolders, searchPath, false, true, 2);
		for (unsigned int i = 0; i < dlcFolders.Count(); i++) {
			// Now enumerate *.tfc files in the DLC folders.
			auto dlcName = dlcFolders.GetData()[i];

			// We check existence to prevent crash if subfolder is not set up properly
			auto dlcCookedPath = dlcPath / dlcName.Chars() / L"CookedPCConsole";
			if (std::filesystem::exists(dlcCookedPath)) {
				// Find *.tfc files in the cookedpcconsole folder
#if defined(LEASI_GAME_LMP)
				// LMP: <Prefix>[N]_DLC_MOD_XXX.tfc, validated per file below
				std::wstring tfcNamePattern = std::format(L"*_{}.tfc", dlcName.Chars());
				auto tfcSearchPattern = dlcCookedPath / tfcNamePattern;
#elif defined(SDK_TARGET_LE3)
				// LE3 DLC requires Textures_DLC_MOD_XXX so we keep that same requirement here as a prefix
				// 02/22/2026 - Also support Lighting_, CharTextures_
				std::wstring tfcNamePattern = std::format(L"*_{}*.tfc", dlcName.Chars());
				auto tfcSearchPattern = dlcCookedPath / tfcNamePattern;
#else
				// LE2 DLC can be named anything
				auto tfcSearchPattern = dlcCookedPath / L"*.tfc";
#endif
				LEASI_TRACE(L"Searching in {}", dlcCookedPath.c_str());
				TArray<FString> tfcFiles{};
				InternalFindFiles(*GFileManager, &tfcFiles, tfcSearchPattern.c_str(), true, false, 2);
				for (unsigned int k = 0; k < tfcFiles.Count(); k++) {

					// Register each TFC using full path
#if defined(LEASI_GAME_LMP)
					auto tfcName = tfcFiles.GetData()[k];
					if (!IsSupportedLmpTfcName(tfcName.Chars(), dlcName.Chars())) {
						LEASI_WARN(L"Skipping registration of unsupported DLC mod TFC name: {}", tfcName.Chars());
						continue;
					}
					auto fullTfcPath = dlcCookedPath / tfcName.Chars();
#elif defined (SDK_TARGET_LE3)
					auto tfcName = tfcFiles.GetData()[k];
					if (!tfcName.StartsWith(L"Textures_DLC_") && !tfcName.StartsWith(L"Lighting_DLC_") && !tfcName.StartsWith(L"CharTextures_DLC_")) {
						// Not a supported TFC name
						LEASI_WARN(L"Skipping registration of unsupported DLC mod TFC name: {}", tfcName.Chars());
						continue;
					}
					auto fullTfcPath = dlcCookedPath / tfcName.Chars();
#else 
					auto fullTfcPath = dlcCookedPath / tfcFiles.GetData()[k].Chars();
#endif
					LEASI_INFO(L"Registering DLC mod TFC: {}", fullTfcPath.c_str());
					FString tfcPath(fullTfcPath.c_str());
					RegisterTFC(&tfcPath);
				}
			}
		}
	}

#endif
}
