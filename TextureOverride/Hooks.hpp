#pragma once

#include <LESDK/Headers.hpp>
#include <LESDK/Init.hpp>
#include "Common/Base.hpp"
#include <map>
#include <set>
#include <filesystem>

namespace TextureOverride
{

    // ! UGameEngine::Exec
    // ========================================

    using t_UGameEngine_Exec = DWORD(UGameEngine* Context, WCHAR const* Command, void* Archive);
    extern t_UGameEngine_Exec* UGameEngine_Exec_orig;
    DWORD UGameEngine_Exec_hook(UGameEngine* Context, WCHAR const* Command, void* Archive);

    // ! UTexture2D::Serialize
    // ========================================

    using t_UTexture2D_Serialize = void(UTexture2D* Context, void* Archive);
    extern t_UTexture2D_Serialize* UTexture2D_Serialize_orig;
    void UTexture2D_Serialize_hook(UTexture2D* Context, void* Archive);

    // ! OodleDecompress
    // ========================================

    using t_OodleDecompress = void* (unsigned int decompressionFlags, void* outPtr, int uncompressedSize, void* inPtr, int compressedSize);
    extern t_OodleDecompress* OodleDecompress;


    // ! LE2/LE3 - Earlier TFC registration / LE3 Multi TFC
	// ========================================
#if defined(SDK_TARGET_LE2) || defined(SDK_TARGET_LE3)
    /// <summary>
    /// Flag to indicate that we have performed DLC registration already.
    /// </summary>
    extern bool bHasPerformedDLCTFCRegistration;
    
    /// <summary>
    /// Pointer to the file manager
    /// </summary>
    extern void** GFileManager;

    // TFC Registration
    using tRegisterTFC = void(FString* Name);
    extern tRegisterTFC* RegisterTFC;

    /// <summary>
    /// Function to register TFCs in DLC folders
    /// </summary>
    void RegisterDLCTFCs();

    // InternalFindFiles
    using tInternalFindFiles = void(void* self, TArray<FString>* result, const wchar_t* searchPattern, bool files, bool folders, unsigned int param_6);
    extern tInternalFindFiles* InternalFindFiles_orig;
    void InternalFindFiles_hook(void* self, TArray<FString>* result, const wchar_t* searchPattern, bool files, bool folders, unsigned int param_6);
    extern tInternalFindFiles* InternalFindFiles;
#endif
}
