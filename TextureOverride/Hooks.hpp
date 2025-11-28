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
#elif defined(SDK_TARGET_LE3)
    #define UGAMEENGINE_EXEC_RVA        ::LESDK::Address::FromOffset(0x541920)
    #define UTEXTURE2D_SERIALIZE_RVA    ::LESDK::Address::FromOffset(0x3C1FB0)
    #define OODLE_DECOMPRESS_RVA        ::LESDK::Address::FromOffset(0x11fd10)
    // MultiTFC:
    #define REGISTER_TFC_RVA            ::LESDK::Address::FromOffset(0x3B8470)
    #define GET_DLC_NAME_RVA            ::LESDK::Address::FromOffset(0xaf75f0)
    #define REGISTER_DLC_TFC_RVA        ::LESDK::Address::FromOffset(0xaf6e20)
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

    // ! LE3 - MultiTFC
    // ========================================
#if defined(SDK_TARGET_LE3)

#pragma pack(push, 4)
    // Contains information about a DLC module - TFC, TOC, config, etc
    struct SFXAdditionalContent
    {
        unsigned char           Unknown1[0x64];
        FString                 RelativeDLCPath;        // 0x64
        TArray<BYTE>            PCConsoleTOC;           // 0x74
        TArray<BYTE>            ConfigFile;             // 0x84
        unsigned char           Unknown2[0x1C];         // 0x94
        FGuid                   TFCGuid;                // 0xB0 [0xB4] - Textures_DLC_....tfc
        int                     UnknownInt3;
        int                     Flags;                  // 0xC4 [0xC8] - Seems to indicate state of various things on DLC
        int                     UnknownInt4;
        int                     UnknownInt5;
        void*                   PointerToGameContent;   // 0xD0 [0xD4]
    };
#pragma pack(pop)

    static_assert(offsetof(SFXAdditionalContent, RelativeDLCPath) == 0x64);
    static_assert(offsetof(SFXAdditionalContent, PCConsoleTOC) == 0x74);
    static_assert(offsetof(SFXAdditionalContent, ConfigFile) == 0x84);
    static_assert(offsetof(SFXAdditionalContent, Unknown2) == 0x94);
    static_assert(offsetof(SFXAdditionalContent, TFCGuid) == 0xB0);
    static_assert(offsetof(SFXAdditionalContent, Flags) == 0xC4);
    static_assert(offsetof(SFXAdditionalContent, PointerToGameContent) == 0xD0);

    // Utility method for getting DLC name from SFXAdditionalContent
    using tGetDLCName = wchar_t* (SFXAdditionalContent* Content, FString* OutDlcName);
    extern tGetDLCName* GetDLCName;

    // TFC Registration
    using tRegisterTFC = void (FString* Name);
    extern tRegisterTFC* RegisterTFC;

    // DLC TFC Registration Hook
    using tRegisterDLCTFC = unsigned long long (SFXAdditionalContent* Content);
    extern tRegisterDLCTFC* RegisterDLCTFC_orig;
    unsigned long long RegisterDLCTFC_hook(SFXAdditionalContent* Content);
#endif
}
