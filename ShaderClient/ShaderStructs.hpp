#pragma once

#pragma pack(push, 1)
// Struct for in-game HLSL generator class
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

#pragma pop