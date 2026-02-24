#pragma once
#include <LESDK/Headers.hpp>
#include "ShaderClient/MilanShaderCompiler.hpp"
namespace ShaderClient
{
	void SaveShaderCache(UShaderCache* shaderCache);
	void SaveMaterialBinary(FMaterialResource* material);

	bool CompileSingleShader(FMaterialResource* material, ShaderCompileCombo* scc, FShaderCompilerEnvironment& env, std::vector<FShader*>* shaderList);
	void GetShaderTypesToCompile(FMaterial* Material, std::vector<ShaderCompileCombo>& outToCompile);
}