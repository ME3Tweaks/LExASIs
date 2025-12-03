#pragma once

#include <LESDK/Headers.hpp>
#include <LESDK/Init.hpp>
#include "Common/Base.hpp"
#include <map>
#include <set>
#include <filesystem>

namespace TextureOverride
{

#if defined(SDK_TARGET_LE1)
    #define UGAMEENGINE_EXEC_RVA        ::LESDK::Address::FromOffset(0x3BD5D0)
    #define UTEXTURE2D_SERIALIZE_RVA    ::LESDK::Address::FromOffset(0x2742b0)
    #define OODLE_DECOMPRESS_RVA        ::LESDK::Address::FromOffset(0x15adb0)
#elif defined(SDK_TARGET_LE2)
    #define UGAMEENGINE_EXEC_RVA        ::LESDK::Address::FromOffset(0x5383C0)
    #define UTEXTURE2D_SERIALIZE_RVA    ::LESDK::Address::FromOffset(0x39ec80)
    #define OODLE_DECOMPRESS_RVA        ::LESDK::Address::FromOffset(0x103ac0)
    // Support for pre-dlc mount streaming textures
    #define REGISTER_TFC_RVA            ::LESDK::Address::FromOffset(0x390d30)
    #define INTERNAL_FIND_FILES_RVA      ::LESDK::Address::FromOffset(0xa4bcc0)
    #define GFILEMANAGER_RVA             ::LESDK::Address::FromOffset(0x16823c0)
    #define FINDPACKAGEFILE_RVA		::LESDK::Address::FromOffset(0xaee720)
    #define FTEXTURE2DRESOURCE_RVA ::LESDK::Address::FromOffset(0x38fc50)
#elif defined(SDK_TARGET_LE3)
    #define UGAMEENGINE_EXEC_RVA        ::LESDK::Address::FromOffset(0x541920)
    #define UTEXTURE2D_SERIALIZE_RVA    ::LESDK::Address::FromOffset(0x3C1FB0)
    #define OODLE_DECOMPRESS_RVA        ::LESDK::Address::FromOffset(0x11fd10)
    // Support for pre-dlc mount streaming textures and multi tfc
    #define REGISTER_TFC_RVA            ::LESDK::Address::FromOffset(0x3B8470)
    #define INTERNAL_FIND_FILES_RVA      ::LESDK::Address::FromOffset(0xa45ad0)
    #define GFILEMANAGER_RVA             ::LESDK::Address::FromOffset(0x17cd5d0)
#endif

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


    // ! LE2/LE3 - FindPackageFile
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
    extern tRegisterTFC* RegisterTFC_orig;
	void RegisterTFC_hook(FString* Name);

    // Research
    using tCreateT2DResource = void*(void* resource, UTexture2D* owner, int initalMipCount, FString* inF1, FString inF2);
	extern tCreateT2DResource* CreateT2DResource_orig;
	void* CreateT2DResource_hook(void* resource, UTexture2D* owner, int initalMipCount, FString* inF1, FString inF2);

    // Research
	using tFindPackageFile = bool(void* self, wchar_t* packageName, FGuid* guid, FString* outFilename, wchar_t* language);
	extern tFindPackageFile* FindPackageFile_orig;
	bool FindPackageFile_hook(void* self, wchar_t* packageName, FGuid* guid, FString* outFilename, wchar_t* language);

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
