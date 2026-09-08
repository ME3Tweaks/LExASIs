#pragma once
#include "ShaderClient/MilanShaderCompilerShared.h"
#include "LESDK/Headers.hpp"
#include "LESDK/Init.hpp"

namespace ShaderClient {
    enum EShaderFrequency {
        SF_VERTEX = 0,
        SF_PIXEL = 3,
    };

    enum EUsedCoords {
        UsedCoord_None = 0x0,
        UsedCoord_World = 0x1,
        UsedCoord_View = 0x2,
        UsedCoord_Local = 0x4,
        UsedCoord_WorldPos = 0x8,
    };

    typedef struct _D3D_SHADER_MACRO
    {
        LPCSTR Name;
        LPCSTR Definition;
    } 	D3D_SHADER_MACRO;

    class ShaderCompileCombo {
    public:
        FString name; // Name of material
        FShaderType* shaderType;
        FVertexFactoryType* vertexFactoryType; // null if not a mesh type
    };

    using tMilanCompileShader = int(const char* baseShaderName, const char* materialHlsl, const char* vertexFactoryName, const wchar_t* includesDirectory, D3D_SHADER_MACRO* macros, const char* entryPoint, const char* shaderType,
        int compileFlags,
        int& outShaderBytesSize,
        BYTE& outShaderBytes,
        int& outErrorMsgSize,
        char& outErrorMsg);

    using tMilanGetShaderInfo = void(int shaderBufferSize, void* shaderBuffer,
        int& outInstructionCount,
        int& outNumParameters,
        NamedParameterAllocation* allocationArray //In/Out
        );


    /* Invoke to compile a shader */
    extern tMilanCompileShader* MilanCompileShader;

    /* Invoke to get instruction count of a shader and other info */
    extern tMilanGetShaderInfo* MilanGetShaderInfo;

    extern std::unordered_map<std::wstring, std::vector<D3D_SHADER_MACRO>> ShaderTypeMacroMap;
    extern std::unordered_map<std::wstring, std::vector<D3D_SHADER_MACRO>> VertexFactoryMacrosMap;


    void LoadCompiler();

    /// <summary>
    /// A macro the engine tried to set to a different value than one of our own rules already had.
    /// Ours wins; this records the disagreement so it stays visible in debug packages.
    /// </summary>
    struct MacroConflict
    {
        std::string Name;
        std::string MilanValue;
        std::string EngineValue;
    };

    void AddMacro(std::vector<D3D_SHADER_MACRO>& Macros, const char* name, const char* definition);
    void AddMacroW(std::vector<D3D_SHADER_MACRO>& Macros, const wchar_t* name, const wchar_t* definition,
        std::vector<MacroConflict>* outConflicts = nullptr);
    void AddCustomMacros(FMaterialResource* material, ShaderCompileCombo& scc, std::vector<D3D_SHADER_MACRO>& macros);
}