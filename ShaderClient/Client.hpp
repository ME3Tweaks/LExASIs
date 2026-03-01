#pragma once
#include <LESDK/Headers.hpp>
#include "ShaderClient/MilanShaderCompiler.hpp"
namespace ShaderClient
{
	extern std::wstring currentAction;
	void UpdateCurrentAction(std::wstring_view newAction);

	void SaveShaderCache(UShaderCache* shaderCache);
	void SaveMaterialBinary(FMaterialResource* material);
	void SaveMaterialHLSL(FMaterialResource* material, wchar_t* materialCode);

	bool CompileSingleShader(FMaterialResource* material, ShaderCompileCombo* scc, FShaderCompilerEnvironment& env, std::vector<FShader*>* shaderList);
	void GetShaderTypesToCompile(FMaterial* Material, std::vector<ShaderCompileCombo>& outToCompile);
}