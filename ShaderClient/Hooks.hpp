#pragma once

#include <memory>
#include <LESDK/Headers.hpp>
#include <LESDK/Init.hpp>
#include "Common/Base.hpp"
#include "ShaderClient/MilanShaderCompiler.hpp"

namespace ShaderClient
{
    // 

    using tFGetMaterialFriendlyName = FString* (void* fmaterial, FString* OutStr);
    extern tFGetMaterialFriendlyName* FMaterial_GetFriendlyName; // (tFGetMaterialFriendlyName)0x7ff74ab4dec0; // RVA 2BDEC0
    extern int* GIsSeekFreePCConsole; // Used to trick game into outputting HLSL  @ 0x7ff74c06f724; // RVA 17DF724

    // The platform currently being cooked for
    // Seems to be used by a few serialization methods.
    extern ECookingTarget* GCookingTarget;

    // A package in memory that is not on disk, things hook up to it, I guess.
    extern UPackage** GObjTransientPackage;
    // Output device, can be null I think?
    extern FArchive** GError;

#pragma pack(push, 1)
    struct FHLSLTranslator
    {
        void* VTable; // 0x0

        // From Ghidra Decomp.
        UBOOL bSuccess;
        int ShaderType; // EShaderFrequency
        int MaterialProperty; // This is probably 1 byte but we set size to 4 as that's how it looks in decomp.
        FMaterial* Material; // Material we're translating.

        void* uniformExpressions;
        EShaderPlatform ShaderPlatform;

        // BioWare specific
        UBOOL bUseHalfTypes;
        // BioWare specific
        FString NumericTypeName;

        // Code chunks
        FString NormalCodeChunk;
        FString EmissiveColorCodeChunk;
        FString DiffuseColorCodeChunk;
        FString DiffusePowerCodeChunk;
        FString SpecularColorCodeChunk;
        FString SpecularPowerCodeChunk;
        FString OpacityCodeChunk;
        FString MaskCodeChunk;
        FString DistortionCodeChunk;
        FString TwoSidedLightingMaskCodeChunk;
        FString CustomLightingCodeChunk;
        FString CustomLightingDiffuseCodeChunk;
        FString AnisotropicDirectionCodeChunk;
        FString WorldPositionOffsetCodeChunk;
        // BioWare specific
        FString WrapLightingCoefficientsCodeChunk;

        int TemplateLineNumber; // For outputting things.
        FString InputsString;
        FString Template; // The input template string

        TMap<UMaterialExpression*, int> ExpressionMap[18]; // Division of allocation indicates there are 18 TMaps in a row, they seem to store expression data of some kind
        TArray<FString> CodeChunks[18]; // Division of allocation indicates there are 18 TArrays in a row, they seem to store the chunks of code used during translation

        TArray<void*> UniformExpressions; // Haven't decomped type data
        TArray<UMaterialExpression*> ExpressionStack; // Used during translation to prevent infinite loops?
        INT nextSymbol;
        TArray<FString> CustomExpressionImplementations; // Array of custom expression data
    };
#pragma pack(pop)

    // Used to perform file operations
    struct FFileManager
    {
        virtual void Unknown1() = 0; // Probably destructor
        virtual FArchive* CreateFileReader() = 0;
        virtual FArchive* CreateFileWriter(wchar_t* filePath, int flags, void* outputDevice, unsigned long long maxArchiveSize) = 0;
        virtual void Unknown4() = 0;
        virtual void Unknown5() = 0;
        virtual void Unknown6() = 0;
        virtual void Unknown7() = 0;
        virtual void Unknown8() = 0;
        virtual void Unknown9() = 0;
        virtual void Unknown10() = 0;
        virtual void Unknown11() = 0;
        virtual void Unknown12() = 0;
        virtual void Unknown13() = 0;
        virtual void Unknown14() = 0;
        virtual void Unknown15() = 0;
        virtual void Unknown16() = 0;
        virtual void Unknown17() = 0;
        virtual void Unknown18() = 0;
        virtual void Unknown19() = 0;
        virtual void Unknown20() = 0;
        virtual void Unknown21() = 0;
        virtual void Unknown22() = 0;
        virtual void Unknown23() = 0;
        virtual void Unknown24() = 0;
        virtual void Unknown25() = 0;
        virtual void Unknown26() = 0;
        virtual void Unknown27() = 0;
        virtual void Unknown28() = 0;
        virtual void Unknown29() = 0;

        virtual WCHAR** GetLoadingFileName(void* something) = 0;
    };

    extern FFileManager** GFileManager;

    using tFMaterialCompile = UBOOL(FMaterial* fmaterial, FStaticParameterSet* parameterSet, EShaderPlatform shaderplatform, FMaterialShaderMap** shaderMap, UBOOL forceCompile, UBOOL debugDump);
    extern tFMaterialCompile* FMaterialCompile;
    extern tFMaterialCompile* FMaterialCompile_orig;


    using tCompileShader = UBOOL(void* map, FMaterial* material, void* InParameters, wchar_t* materialCode, void* InUniformExpressionSet, EShaderPlatform platform, TArray<FString>* Errors, BOOL bSilent, BOOL bDebugDump);
    extern tCompileShader* FMaterialShaderMap_Compile;
    extern tCompileShader* FMaterialShaderMap_Compile_orig;

    using tInitShaderMap = UBOOL(FMaterial* fmaterial, void* StaticParameters, EShaderPlatform Platform, int Quality);
    extern tInitShaderMap InitShaderMap_hook;
    extern tInitShaderMap* InitShaderMap_orig;

    using tHLSLTranslate = UBOOL(FHLSLTranslator* translator);
    extern tHLSLTranslate* FHLSLTranslator_Translate;
    extern tHLSLTranslate* FHLSLTranslator_Translate_orig;

    using tCompileWrapper = UBOOL(void* material, void* param_2, int shaderPlatform, void* param_4, UBOOL bForceCompile, UBOOL bDumpDebug);
    extern tCompileWrapper* FMaterial_Compile; // (tCompileWrapper)0x7ff74ab4a320; // RVA 2BA320

    using tGetMaterialCode = FString*(FHLSLTranslator* translator, FString* inStr, UBOOL somethingMaybeChar);
    extern tGetMaterialCode* FHLSL_GetMaterialCode; // (tGetMaterialCode)0x7ff74ab4e1f0; // RVA 

    using tGetShaderTypeList = TLinkedList<FShaderType*>** ();
    extern tGetShaderTypeList* GetShaderTypeList; // @ 0x7ff74ac17540
    using tGetVertexFactoryTypeList = TLinkedList<FVertexFactoryType*>** ();
    extern tGetVertexFactoryTypeList* GetVertexFactoryTypeList; // @ 0x7ff74b164830

    // platform should always be passed as 5 (SM5);
    // Not sure this is the same for all shader types...
    using tShouldCache = bool(int platform, void* material, void* vertexFactoryClass);
    extern tShouldCache* ShouldCache;

    using tByteOrderSerialize = FArchive* (FArchive* self, void* data, int size);
    extern tByteOrderSerialize* ByteOrderSerialize;  //@ 0x7ff74a914930 LE3

    using tCreateArchiveProxy = FArchiveProxy* (FArchiveProxy* self, FArchive* arcToProxy);
    extern tCreateArchiveProxy* CreateArchiveProxy;
    // Set to FArchiveProxy vtable
    extern void* StringArchiveProxyVTableStart;

    // Set to FMemoryWriter
    extern void* MemoryArchiveWriterVTableStart;

    using tSerializeUShaderCache = void(UShaderCache*, FArchive*);
    extern tSerializeUShaderCache* SerializeUShaderCache; // @ 0x7ff74ac06840

    using tSerializeMaterialResource = void(FMaterialResource*, FArchive*);
    extern tSerializeMaterialResource* SerializeMaterialResource; // @ 7ff74ab556a0

    using tSerializeFUniformExpressionSet = void(FUniformExpressionSet*, FArchive*);
    extern tSerializeFUniformExpressionSet* SerializeFUniformExpressionSet;

    struct FArchiveProxy_NamesAndObjectsAsStrings_VTable {
        char Blank[0x30];
        FArchiveProxy* (*SerializeObject)(FArchiveProxy*, UObject**); // This method is replaced.
        char Blank2[0xA8];
    };

    // This allocates a memory block for the VTable. Assign this address as the VTable to make it work once you memcpy the original and replace the SerializeObject func.
    extern FArchiveProxy_NamesAndObjectsAsStrings_VTable NamesAndObjectsAsStrings_VTable;

    using tGetTlkString = FString* (void* gTalkTable, int id, FString* outStr, int, int);
    extern tGetTlkString* GetTlkString; // @ 7ff74a927400 LE3
    extern tGetTlkString* GetTlkString_orig; // @ 7ff74a927400 LE3

    using tShowLoadMessage = void(void* unknown, int param_2, UINT stringId, int somebool);
    extern tShowLoadMessage* ShowLoadMessage_orig;
    extern void* showLoadMessage_parm1;

    // For directing to the shader compiler levels.
    using t_UObject_ProcessInternal = void(UObject* Context, FFrame* Stack, void* Result);
    extern t_UObject_ProcessInternal* UObject_ProcessInternal_orig;
    void UObject_ProcessInternal_hook(UObject* Context, FFrame* Stack, void* Result);

    // For overriding the loading screen a shader uses
    using tPlayLoadingMovie = void(USFXLoadMovieManager* param_1, FString* levelName, int param3);
	extern tPlayLoadingMovie* PlayLoadingMovie_orig;
    void PlayLoadingMovie_hook(USFXLoadMovieManager* param_1, FString* levelName, int param3);

    // Functions

    UBOOL FMaterialCompile_hook(FMaterialResource* fmaterial, FStaticParameterSet* parameterSet, EShaderPlatform shaderplatform, FMaterialShaderMap** shaderMap, UBOOL forceCompile, UBOOL debugDump);
    UBOOL InitShaderMap_hook(FMaterial* fmaterial, void* StaticParameters, EShaderPlatform Platform, int Quality);
    FArchiveProxy* PackagelessSerializeObjectRef(FArchiveProxy* Ar, UObject** obj);
    FArchiveProxy GetStringWithObjectWriterProxy(FArchive& inArchive);
    UBOOL FHLSLTranslator_Translate_hook(FHLSLTranslator* translator);
    bool CompileSingleShader(FMaterialResource* material, ShaderCompileCombo* scc, FShaderCompilerEnvironment& env, std::vector<FShader*>* shaderList);
    UBOOL FMaterialShaderMap_Compile_hook(FMaterialShaderMap* map, FMaterialResource* material, FStaticParameterSet* InParameters, wchar_t* materialCode, FUniformExpressionSet* InUniformExpressionSet, EShaderPlatform platform, TArray<FString>* outErrors, BOOL bSilent, BOOL bDebugDump);
    FString* GetTlkString_hook(void* gTalkTable, int id, FString* outStr, int something, int somethingElse);
    void ShowLoadMessage_hook(void* unknown, int param_2, UINT stringId, int someBool);
}
