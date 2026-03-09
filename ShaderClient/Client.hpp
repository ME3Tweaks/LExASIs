#pragma once
#include <LESDK/Headers.hpp>
#include "ShaderClient/MilanShaderCompiler.hpp"

#define PRINT_DEBUG_INFO false

namespace ShaderClient
{
	enum class ShaderHostCommand : BYTE
	{
		SaveShaderCache = 1,
		TerminateGame = 2, // Unused
		ShaderCompileFailed = 3,
		SaveMaterialBinary = 4,
		SaveMaterialHLSL = 5,
		SaveDebugInfo = 6
	};
	extern std::wstring currentAction;
	extern bool SendDebugPackages;
	void UpdateCurrentAction(std::wstring_view newAction);

	void SaveShaderCache(UShaderCache* shaderCache);
	void SaveMaterialBinary(FMaterialResource* material);
	void SaveMaterialHLSL(FMaterialResource* material, wchar_t* materialCode);

	bool CompileSingleShader(FMaterialResource* material, ShaderCompileCombo* scc, FShaderCompilerEnvironment& env, std::vector<FShader*>* shaderList);
	void GetShaderTypesToCompile(FMaterial* Material, std::vector<ShaderCompileCombo>& outToCompile);
}