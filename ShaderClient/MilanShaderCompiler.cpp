#pragma once
#include "Common/Base.hpp"
#include "LESDK/Headers.hpp"
#include "ShaderClient/MilanShaderCompiler.hpp"
#include "ShaderClient/MilanShaderCompilerShared.h"
#include "ShaderClient/Hooks.hpp"
/*
 * Contains methods for use with MilanShaderCompilerLib
 */

namespace ShaderClient {
	/* Invoke to compile a shader and get the bytecode and binding info from D3D */
	tMilanCompileShader* MilanCompileShader = 0;

	/* Invoke to get instruction count of a shader and other info */
	tMilanGetShaderInfo* MilanGetShaderInfo = 0;

	void LoadCompiler() {
		auto compilerDLL = LoadLibraryW(L"MilanShaderCompilerLib.dll");
		if (compilerDLL == nullptr) {
			LEASI_ERROR("Unable to load MilanShaderCompilerLib.dll");
		}
		MilanCompileShader = (tMilanCompileShader*)GetProcAddress(compilerDLL, "CompileShader");
		if (MilanCompileShader == nullptr) {
			LEASI_WARN("Unable to locate 'CompileShader' in MilanShaderCompilerLib.dll");
		}
		else {
			LEASI_INFO("Loaded 'CompileShader' from MilanShaderCompilerLib.dll");
		}

		MilanGetShaderInfo = (tMilanGetShaderInfo*)GetProcAddress(compilerDLL, "GetShaderInfo");
		if (MilanGetShaderInfo == nullptr) {
			LEASI_WARN("Unable to locate 'GetShaderInfo' in MilanShaderCompilerLib.dll");
		}
		else {
			LEASI_INFO("Loaded 'GetShaderInfo' from MilanShaderCompilerLib.dll");
		}
	}

	void AddMacro(std::vector<D3D_SHADER_MACRO>& Macros, const char* name, const char* definition) {
		for (auto& macro : Macros) {
			if (macro.Name && name && strcmp(macro.Name, name) == 0) {
				macro.Definition = definition;
				return;
			}
		}

		D3D_SHADER_MACRO macro;
		macro.Name = name;
		macro.Definition = definition;
		Macros.push_back(macro);
	}

	// Removes an added macro. This is used to make the generic adding code a bit cleaner when we only have to clean up a few.
	void RemoveMacro(std::vector<D3D_SHADER_MACRO>& Macros, const char* name) {
		Macros.erase(std::remove_if(Macros.begin(), Macros.end(),
			[name](const D3D_SHADER_MACRO& m) { return m.Name && name && strcmp(m.Name, name) == 0; }),
			Macros.end());
	}

	// Allocates a new char* from a wchar_t*
	// Returned object MUST be deleted!
	char* wchar_to_char(const wchar_t* wstr) {
		if (!wstr) return nullptr;
		size_t len = wcslen(wstr);
		char* cstr = new char[len + 1];
		for (size_t i = 0; i < len; ++i) {
			cstr[i] = static_cast<char>(wstr[i]);
		}
		cstr[len] = '\0';
		return cstr;
	}


	/// <summary>
	/// Adds a macro coming from the engine's ModifyCompilationEnvironment, skipping it when one of
	/// our own rules already set that name.
	///
	/// This used to append unconditionally, so a name set by both sides landed in the array twice
	/// and which value applied was down to D3DCompile's handling of duplicate defines rather than
	/// any decision here. Ours now wins deterministically.
	///
	/// A skipped entry whose value differs from ours is recorded in outConflicts so the disagreement
	/// stays visible in debug packages instead of disappearing with the duplicate.
	/// </summary>
	void AddMacroW(std::vector<D3D_SHADER_MACRO>& Macros, const wchar_t* name, const wchar_t* definition,
		std::vector<MacroConflict>* outConflicts) {

		char* nameC = wchar_to_char(name);
		char* defC = wchar_to_char(definition);

		for (const auto& macro : Macros) {
			if (macro.Name && nameC && strcmp(macro.Name, nameC) == 0) {
				if (outConflicts != nullptr && macro.Definition && defC && strcmp(macro.Definition, defC) != 0) {
					outConflicts->push_back(MacroConflict{ std::string(nameC), std::string(macro.Definition), std::string(defC) });
				}
				delete[] nameC;
				delete[] defC;
				return;
			}
		}

		D3D_SHADER_MACRO macro;
		macro.Name = nameC;
		macro.Definition = defC;
		Macros.push_back(macro);
	}

	std::unordered_map<std::wstring, std::vector<D3D_SHADER_MACRO>> VertexFactoryMacrosMap{};
	void InitVertexFactoryMacroMap()
	{
		if (VertexFactoryMacrosMap.size() != 0) {
			// Already initialized
			return;
		}
		// XBOXTESSELLATION and NUM_AXIS_ROTATION_VECTORS are left to the engine for the same
		// reason as QUAT_SKINNING below: its ModifyCompilationEnvironment still emits them, and it
		// disagreed with these hardcoded values on every shader that set both.
		VertexFactoryMacrosMap[L"FFluidTessellationVertexFactory"] = {
		};
		VertexFactoryMacrosMap[L"FFluidVertexFactory"] = {
		};
		// QUAT_SKINNING is deliberately not set here. The game's own ModifyCompilationEnvironment
		// still emits it, and it emits 0 — LE3 ships matrix skinning (BoneMatrices), not the
		// dual-quaternion path (BoneQuats/DQ0/BoneScales) that hardcoding 1 produced. Letting the
		// engine supply it keeps the authoritative value instead of guessing per vertex factory.
		VertexFactoryMacrosMap[L"FGPUSkinDecalVertexFactory"] = {
			{ "GPUSKIN_DECAL", "1" }
		};
		VertexFactoryMacrosMap[L"FGPUSkinMorphDecalVertexFactory"] = {
			{ "GPUSKIN_DECAL", "1" },
			{ "GPUSKIN_MORPH_BLEND", "1" }
		};
		VertexFactoryMacrosMap[L"FGPUSkinMorphVertexFactory"] = {
			{ "GPUSKIN_MORPH_BLEND", "1" }
		};
		VertexFactoryMacrosMap[L"FGPUSkinVertexFactory"] = {
		};
		VertexFactoryMacrosMap[L"FInstancedStaticMeshVertexFactory"] = {
			{ "USE_INSTANCING", "1" }
		};
		VertexFactoryMacrosMap[L"FLensFlareVertexFactory"] = {
			{ "SIMPLE_VERTEX_LIGHTMAP", "1" },
			{ "USE_LENSFLARE", "1" },
			{ "USE_OCCLUSION_PERCENTAGE", "1" }
		};
		VertexFactoryMacrosMap[L"FLocalDecalVertexFactory"] = {
			{ "DECAL_FACTORY", "1" },
			{ "WORLD_COORDS", "1" }
		};
		VertexFactoryMacrosMap[L"FParticleBeamTrailDynamicParameterVertexFactory"] = {
			{ "PARTICLES_ALLOW_AXIS_ROTATION", "1" },
			{ "SIMPLE_VERTEX_LIGHTMAP", "1" },
			{ "USE_DYNAMIC_PARAMETERS", "1" },
			{ "USE_OCCLUSION_PERCENTAGE", "1" }
		};
		VertexFactoryMacrosMap[L"FParticleBeamTrailVertexFactory"] = {
			{ "PARTICLES_ALLOW_AXIS_ROTATION", "1" },
			{ "USE_OCCLUSION_PERCENTAGE", "1" }
		};
		VertexFactoryMacrosMap[L"FParticleDynamicParameterVertexFactory"] = {
			{ "PARTICLES_ALLOW_AXIS_ROTATION", "1" },
			{ "SIMPLE_VERTEX_LIGHTMAP", "1" },
			{ "USE_DYNAMIC_PARAMETERS", "1" },
			{ "USE_OCCLUSION_PERCENTAGE", "1" }
		};
		VertexFactoryMacrosMap[L"FParticleInstancedMeshVertexFactory"] = {
			{ "COLOR_OVER_LIFE", "1" },
			{ "SIMPLE_VERTEX_LIGHTMAP", "1" }
		};
		VertexFactoryMacrosMap[L"FParticleSubUVDynamicParameterVertexFactory"] = {
			{ "PARTICLES_ALLOW_AXIS_ROTATION", "1" },
			{ "SIMPLE_VERTEX_LIGHTMAP", "1" },
			{ "SUBUV_PARTICLES", "1" },
			{ "USE_DYNAMIC_PARAMETERS", "1" },
			{ "USE_OCCLUSION_PERCENTAGE", "1" }
		};
		VertexFactoryMacrosMap[L"FParticleSubUVVertexFactory"] = {
			{ "PARTICLES_ALLOW_AXIS_ROTATION", "1" },
			{ "SUBUV_PARTICLES", "1" },
			{ "USE_OCCLUSION_PERCENTAGE", "1" }
		};
		VertexFactoryMacrosMap[L"FParticleVertexFactory"] = {
			{ "PARTICLES_ALLOW_AXIS_ROTATION", "1" },
			{ "USE_OCCLUSION_PERCENTAGE", "1" }
		};
		VertexFactoryMacrosMap[L"FSplineMeshVertexFactory"] = {
			{ "USE_SPLINEDEFORM", "1" }
		};
		VertexFactoryMacrosMap[L"FTerrainDecalVertexFactory"] = {
			{ "DECAL_FACTORY", "1" }
		};
		VertexFactoryMacrosMap[L"FTerrainFullMorphDecalVertexFactory"] = {
			{ "DECAL_FACTORY", "1" },
			{ "TERRAIN_MORPHING_ENABLED", "1" },
			{ "TERRAIN_MORPHING_GRADIENTS", "1" }
		};
		VertexFactoryMacrosMap[L"FTerrainFullMorphVertexFactory"] = {
			{ "TERRAIN_MORPHING_ENABLED", "1" },
			{ "TERRAIN_MORPHING_GRADIENTS", "1" }
		};
		VertexFactoryMacrosMap[L"FTerrainMorphDecalVertexFactory"] = {
			{ "DECAL_FACTORY", "1" },
			{ "TERRAIN_MORPHING_ENABLED", "1" }
		};
		VertexFactoryMacrosMap[L"FTerrainMorphVertexFactory"] = {
			{ "TERRAIN_MORPHING_ENABLED", "1" }
		};
		VertexFactoryMacrosMap[L"None"] = {
			{ "SIMPLE_VERTEX_LIGHTMAP", "1" }
		};
	}


	std::unordered_map<std::wstring, std::vector<D3D_SHADER_MACRO>> ShaderTypeMacroMap;
	void InitShaderTypeMacroMap()
	{
		if (ShaderTypeMacroMap.size() != 0) {
			return; // Already initialized
		}

		ShaderTypeMacroMap[L"FHitMaskPixelShader"] = {
			{ "SIMPLE_VERTEX_LIGHTMAP", "1" }
		};
		ShaderTypeMacroMap[L"FHitMaskVertexShader"] = {
			{ "SIMPLE_VERTEX_LIGHTMAP", "1" }
		};
		ShaderTypeMacroMap[L"FHitProxyPixelShader"] = {
			{ "SIMPLE_VERTEX_LIGHTMAP", "1" }
		};
		ShaderTypeMacroMap[L"FHitProxyVertexShader"] = {
			{ "SIMPLE_VERTEX_LIGHTMAP", "1" }
		};
		ShaderTypeMacroMap[L"FModShadowMeshPixelShader"] = {
			{ "SIMPLE_VERTEX_LIGHTMAP", "1" }
		};
		ShaderTypeMacroMap[L"FModShadowMeshVertexShader"] = {
			{ "SIMPLE_VERTEX_LIGHTMAP", "1" }
		};
		ShaderTypeMacroMap[L"FSFXWorldNormalPixelShader"] = {
			{ "SIMPLE_VERTEX_LIGHTMAP", "1" }
		};
		ShaderTypeMacroMap[L"FSFXWorldNormalVertexShader"] = {
			{ "SIMPLE_VERTEX_LIGHTMAP", "1" }
		};
		ShaderTypeMacroMap[L"FVelocityPixelShader"] = {
			{ "SIMPLE_VERTEX_LIGHTMAP", "1" },
		};
		ShaderTypeMacroMap[L"FVelocityVertexShader"] = {
			{ "SIMPLE_VERTEX_LIGHTMAP", "1" },
		};
		ShaderTypeMacroMap[L"TAOMeshVertexShader<0>"] = {
			{ "SIMPLE_VERTEX_LIGHTMAP", "1" }
		};
		ShaderTypeMacroMap[L"TAOMeshVertexShader<1>"] = {
			{ "SIMPLE_VERTEX_LIGHTMAP", "1" }
		};
		ShaderTypeMacroMap[L"TBasePassPixelShaderFCustomSimpleLightMapTexturePolicyNoSkyLight"] = {
			{ "USING_SIMPLELIGHTMAPS", "1" },
			{ "TEXTURE_LIGHTMAP", "1" },
			{ "NUM_LIGHTMAP_COEFFICIENTS", "1" }
		};
		ShaderTypeMacroMap[L"TBasePassPixelShaderFCustomSimpleLightMapTexturePolicySkyLight"] = {
			{ "USING_SIMPLELIGHTMAPS", "1" },
			{ "TEXTURE_LIGHTMAP", "1" },
			{ "ENABLE_SKY_LIGHT", "1" },
			{ "NUM_LIGHTMAP_COEFFICIENTS", "1" }
		};
		ShaderTypeMacroMap[L"TBasePassPixelShaderFCustomSimpleVertexLightMapPolicyNoSkyLight"] = {
			{ "SIMPLE_VERTEX_LIGHTMAP", "1" }
		};
		ShaderTypeMacroMap[L"TBasePassPixelShaderFCustomSimpleVertexLightMapPolicySkyLight"] = {
			{ "ENABLE_SKY_LIGHT", "1" },
			{ "SIMPLE_VERTEX_LIGHTMAP", "1" },
			{ "NUM_LIGHTMAP_COEFFICIENTS", "1" } // Redefined as 1
		};
		ShaderTypeMacroMap[L"TBasePassPixelShaderFCustomVectorLightMapTexturePolicyNoSkyLight"] = {
			{ "USING_VECTORLIGHTMAPS", "1" },
			{ "TEXTURE_LIGHTMAP", "1" }
		};
		ShaderTypeMacroMap[L"TBasePassPixelShaderFCustomVectorLightMapTexturePolicySkyLight"] = {
			{ "USING_VECTORLIGHTMAPS", "1" },
			{ "TEXTURE_LIGHTMAP", "1" },
			{ "ENABLE_SKY_LIGHT", "1" }
		};
		ShaderTypeMacroMap[L"TBasePassPixelShaderFCustomVectorVertexLightMapPolicyNoSkyLight"] = {
			{ "USING_VECTORLIGHTMAPS", "1" },
			{ "VERTEX_LIGHTMAP", "1" }
		};
		ShaderTypeMacroMap[L"TBasePassPixelShaderFCustomVectorVertexLightMapPolicySkyLight"] = {
			{ "USING_VECTORLIGHTMAPS", "1" },
			{ "VERTEX_LIGHTMAP", "1" },
			{ "ENABLE_SKY_LIGHT", "1" }
		};
		ShaderTypeMacroMap[L"TBasePassPixelShaderFDirectionalLightLightMapPolicyNoSkyLight"] = {
			{ "TRANSLUCENCY_ONEPASS_DYNAMICALLY_SHADOWED", "1" },
			{ "ENABLE_DIRECTIONAL_LIGHT", "1" }
		};
		ShaderTypeMacroMap[L"TBasePassPixelShaderFDirectionalLightLightMapPolicySkyLight"] = {
			{ "ENABLE_DIRECTIONAL_LIGHT", "1" },
			{ "TRANSLUCENCY_ONEPASS_DYNAMICALLY_SHADOWED", "1" },
			{ "ENABLE_SKY_LIGHT", "1" }
		};
		ShaderTypeMacroMap[L"TBasePassPixelShaderFDirectionalLightMapTexturePolicyNoSkyLight"] = {
			{ "TEXTURE_LIGHTMAP", "1" }
		};
		ShaderTypeMacroMap[L"TBasePassPixelShaderFDirectionalLightMapTexturePolicySkyLight"] = {
			{ "ENABLE_SKY_LIGHT", "1" },
			{ "TEXTURE_LIGHTMAP", "1" }
		};
		ShaderTypeMacroMap[L"TBasePassPixelShaderFDirectionalVertexLightMapPolicyNoSkyLight"] = {
			{ "VERTEX_LIGHTMAP", "1" }
		};
		ShaderTypeMacroMap[L"TBasePassPixelShaderFDirectionalVertexLightMapPolicySkyLight"] = {
			{ "ENABLE_SKY_LIGHT", "1" },
			{ "VERTEX_LIGHTMAP", "1" }
		};
		ShaderTypeMacroMap[L"TBasePassPixelShaderFNoLightMapPolicyNoSkyLight"] = {
		};
		ShaderTypeMacroMap[L"TBasePassPixelShaderFNoLightMapPolicySkyLight"] = {
			{ "ENABLE_SKY_LIGHT", "1" }
		};
		ShaderTypeMacroMap[L"TBasePassPixelShaderFSHLightLightMapPolicyNoSkyLight"] = {
			{ "ENABLE_DIRECTIONAL_LIGHT", "1" },
			{ "ENABLE_SH_LIGHT", "1" },
			{ "TRANSLUCENCY_ONEPASS_DYNAMICALLY_SHADOWED", "1" }
		};
		ShaderTypeMacroMap[L"TBasePassPixelShaderFSHLightLightMapPolicySkyLight"] = {
			{ "ENABLE_SH_LIGHT", "1" },
			{ "TRANSLUCENCY_ONEPASS_DYNAMICALLY_SHADOWED", "1" },
			{ "ENABLE_DIRECTIONAL_LIGHT", "1" },
			{ "ENABLE_SKY_LIGHT", "1" }
		};
		ShaderTypeMacroMap[L"TBasePassPixelShaderFSimpleLightMapTexturePolicyNoSkyLight"] = {
			{ "SIMPLE_TEXTURE_LIGHTMAP", "1" },
			{ "NUM_LIGHTMAP_COEFFICIENTS", "1" }
		};
		ShaderTypeMacroMap[L"TBasePassPixelShaderFSimpleLightMapTexturePolicySkyLight"] = {
			{ "ENABLE_SKY_LIGHT", "1" },
			{ "SIMPLE_TEXTURE_LIGHTMAP", "1" },
			{ "NUM_LIGHTMAP_COEFFICIENTS", "1" }
		};
		ShaderTypeMacroMap[L"TBasePassPixelShaderFSimpleVertexLightMapPolicyNoSkyLight"] = {
			{ "SIMPLE_VERTEX_LIGHTMAP", "1" }
		};
		ShaderTypeMacroMap[L"TBasePassPixelShaderFSimpleVertexLightMapPolicySkyLight"] = {
			{ "ENABLE_SKY_LIGHT", "1" },
			{ "SIMPLE_VERTEX_LIGHTMAP", "1" }
		};
		ShaderTypeMacroMap[L"TBasePassVertexShaderFCustomSimpleLightMapTexturePolicyFNoDensityPolicy"] = {
			{ "FOGVOLUMEDENSITY_NONE", "1" },
			{ "TEXTURE_LIGHTMAP", "1" },
			{ "NUM_LIGHTMAP_COEFFICIENTS", "1" }
		};
		ShaderTypeMacroMap[L"TBasePassVertexShaderFCustomSimpleVertexLightMapPolicyFNoDensityPolicy"] = {
			{ "FOGVOLUMEDENSITY_NONE", "1" },
			{ "SIMPLE_VERTEX_LIGHTMAP", "1" },
			{ "NUM_LIGHTMAP_COEFFICIENTS", "1" }
		};
		ShaderTypeMacroMap[L"TBasePassVertexShaderFCustomSimpleVertexLightMapPolicyFConstantDensityPolicy"] = {
			{ "SIMPLE_VERTEX_LIGHTMAP", "1" },
			{ "NUM_LIGHTMAP_COEFFICIENTS", "1" }
		};
		ShaderTypeMacroMap[L"TBasePassVertexShaderFCustomVectorLightMapTexturePolicyFNoDensityPolicy"] = {
			{ "USING_VECTORLIGHTMAPS", "1" },
			{ "FOGVOLUMEDENSITY_NONE", "1" },
			{ "TEXTURE_LIGHTMAP", "1" }
		};
		ShaderTypeMacroMap[L"TBasePassVertexShaderFCustomVectorVertexLightMapPolicyFNoDensityPolicy"] = {
			{ "USING_VECTORLIGHTMAPS", "1" },
			{ "SIMPLE_VERTEX_LIGHTMAP", "1" },
			{ "FOGVOLUMEDENSITY_NONE", "1" },
			{ "VERTEX_LIGHTMAP", "1" }
		};
		ShaderTypeMacroMap[L"TBasePassVertexShaderFDirectionalLightLightMapPolicyFNoDensityPolicy"] = {
			{ "ENABLE_DIRECTIONAL_LIGHT", "1" },
			{ "FOGVOLUMEDENSITY_NONE", "1" },
			{ "TRANSLUCENCY_ONEPASS_DYNAMICALLY_SHADOWED", "1" }
		};
		ShaderTypeMacroMap[L"TBasePassVertexShaderFDirectionalLightMapTexturePolicyFNoDensityPolicy"] = {
			{ "FOGVOLUMEDENSITY_NONE", "1" },
			{ "TEXTURE_LIGHTMAP", "1" }
		};
		ShaderTypeMacroMap[L"TBasePassVertexShaderFDirectionalVertexLightMapPolicyFNoDensityPolicy"] = {
			{ "SIMPLE_VERTEX_LIGHTMAP", "1" },
			{ "FOGVOLUMEDENSITY_NONE", "1" },
			{ "VERTEX_LIGHTMAP", "1" }
		};
		ShaderTypeMacroMap[L"TBasePassVertexShaderFNoLightMapPolicyFNoDensityPolicy"] = {
			{ "FOGVOLUMEDENSITY_NONE", "1" }
		};
		ShaderTypeMacroMap[L"TBasePassVertexShaderFSHLightLightMapPolicyFNoDensityPolicy"] = {
			{ "ENABLE_SH_LIGHT", "1" },
			{ "FOGVOLUMEDENSITY_NONE", "1" },
			{ "ENABLE_DIRECTIONAL_LIGHT", "1" },
			{ "TRANSLUCENCY_ONEPASS_DYNAMICALLY_SHADOWED", "1" }
		};
		ShaderTypeMacroMap[L"TBasePassVertexShaderFSimpleLightMapTexturePolicyFNoDensityPolicy"] = {
			{ "SIMPLE_TEXTURE_LIGHTMAP", "1" },
			{ "FOGVOLUMEDENSITY_NONE", "1" },
			{ "NUM_LIGHTMAP_COEFFICIENTS", "1" }
		};
		ShaderTypeMacroMap[L"TBasePassVertexShaderFSimpleVertexLightMapPolicyFNoDensityPolicy"] = {
			{ "FOGVOLUMEDENSITY_NONE", "1" },
			{ "SIMPLE_VERTEX_LIGHTMAP", "1" },
			{ "NUM_LIGHTMAP_COEFFICIENTS", "1" }
		};
		ShaderTypeMacroMap[L"TDepthOnlyScreenDoorPixelShader"] = {
			{ "MATERIAL_USE_SCREEN_DOOR_FADE", "1" },
			{ "SIMPLE_VERTEX_LIGHTMAP", "1" }
		};
		ShaderTypeMacroMap[L"TDepthOnlyVertexShader<0>"] = {
			{ "SIMPLE_VERTEX_LIGHTMAP", "1" }
		};
		ShaderTypeMacroMap[L"TDepthOnlyVertexShader<1>"] = {
			{ "SIMPLE_VERTEX_LIGHTMAP", "1" }
		};
		ShaderTypeMacroMap[L"TLightMapDensityPixelShader<FDirectionalLightMapTexturePolicy>"] = {
			{ "SIMPLE_VERTEX_LIGHTMAP", "1" },
			{ "TEXTURE_LIGHTMAP", "1" },
			{ "SIMPLE_TEXTURE_LIGHTMAP", "1" }
		};
		ShaderTypeMacroMap[L"TLightMapDensityPixelShader<FDummyLightMapTexturePolicy>"] = {
			{ "SIMPLE_VERTEX_LIGHTMAP", "1" },
			{ "TEXTURE_LIGHTMAP", "1" },
			{ "SIMPLE_TEXTURE_LIGHTMAP", "1" }
		};
		ShaderTypeMacroMap[L"TLightMapDensityPixelShader<FNoLightMapPolicy>"] = {
			{ "SIMPLE_VERTEX_LIGHTMAP", "1" }
		};
		ShaderTypeMacroMap[L"TLightMapDensityPixelShader<FSimpleLightMapTexturePolicy>"] = {
			{ "SIMPLE_TEXTURE_LIGHTMAP", "1" },
			{ "SIMPLE_VERTEX_LIGHTMAP", "1" }
		};
		ShaderTypeMacroMap[L"TLightMapDensityVertexShader<FDirectionalLightMapTexturePolicy>"] = {
			{ "SIMPLE_VERTEX_LIGHTMAP", "1" },
			{ "TEXTURE_LIGHTMAP", "1" },
			{ "SIMPLE_TEXTURE_LIGHTMAP", "1" }
		};
		ShaderTypeMacroMap[L"TLightMapDensityVertexShader<FDummyLightMapTexturePolicy>"] = {
			{ "SIMPLE_VERTEX_LIGHTMAP", "1" },
			{ "TEXTURE_LIGHTMAP", "1" },
			{ "SIMPLE_TEXTURE_LIGHTMAP", "1" }
		};
		ShaderTypeMacroMap[L"TLightMapDensityVertexShader<FNoLightMapPolicy>"] = {
			{ "SIMPLE_VERTEX_LIGHTMAP", "1" }
		};
		ShaderTypeMacroMap[L"TLightMapDensityVertexShader<FSimpleLightMapTexturePolicy>"] = {
			{ "SIMPLE_TEXTURE_LIGHTMAP", "1" },
			{ "SIMPLE_VERTEX_LIGHTMAP", "1" }
		};
		ShaderTypeMacroMap[L"TLightPixelShaderFDirectionalLightPolicyFNoStaticShadowingPolicy"] = {
			{ "SIMPLE_VERTEX_LIGHTMAP", "1" },
			{ "ENABLE_DISTANCE_SHADOW_FADING", "1" }
		};
		ShaderTypeMacroMap[L"TLightPixelShaderFDirectionalLightPolicyFShadowTexturePolicy"] = {
			{ "SIMPLE_VERTEX_LIGHTMAP", "1" },
			{ "ENABLE_DISTANCE_SHADOW_FADING", "1" },
			{ "STATICLIGHTING_TEXTUREMASK", "1" }
		};
		ShaderTypeMacroMap[L"TLightPixelShaderFDirectionalLightPolicyFShadowVertexBufferPolicy"] = {
			{ "SIMPLE_VERTEX_LIGHTMAP", "1" },
			{ "STATICLIGHTING_VERTEXMASK", "1" },
			{ "ENABLE_DISTANCE_SHADOW_FADING", "1" }
		};
		ShaderTypeMacroMap[L"TLightPixelShaderFDirectionalLightPolicyFSignedDistanceFieldShadowTexturePolicy"] = {
			{ "SIMPLE_VERTEX_LIGHTMAP", "1" },
			{ "STATICLIGHTING_SIGNEDDISTANCEFIELD", "1" },
			{ "ENABLE_DISTANCE_SHADOW_FADING", "1" },
			{ "STATICLIGHTING_TEXTUREMASK", "1" }
		};
		ShaderTypeMacroMap[L"TLightPixelShaderFPointLightPolicyFNoStaticShadowingPolicy"] = {
			{ "SIMPLE_VERTEX_LIGHTMAP", "1" }
		};
		ShaderTypeMacroMap[L"TLightPixelShaderFPointLightPolicyFShadowTexturePolicy"] = {
			{ "SIMPLE_VERTEX_LIGHTMAP", "1" },
			{ "STATICLIGHTING_TEXTUREMASK", "1" }
		};
		ShaderTypeMacroMap[L"TLightPixelShaderFPointLightPolicyFShadowVertexBufferPolicy"] = {
			{ "STATICLIGHTING_VERTEXMASK", "1" },
			{ "SIMPLE_VERTEX_LIGHTMAP", "1" }
		};
		ShaderTypeMacroMap[L"TLightPixelShaderFPointLightPolicyFSignedDistanceFieldShadowTexturePolicy"] = {
			{ "SIMPLE_VERTEX_LIGHTMAP", "1" },
			{ "STATICLIGHTING_SIGNEDDISTANCEFIELD", "1" },
			{ "STATICLIGHTING_TEXTUREMASK", "1" }
		};
		ShaderTypeMacroMap[L"TLightPixelShaderFSFXPointLightPolicyFNoStaticShadowingPolicy"] = {
			{ "SIMPLE_VERTEX_LIGHTMAP", "1" }
		};
		ShaderTypeMacroMap[L"TLightPixelShaderFSphericalHarmonicLightPolicyFNoStaticShadowingPolicy"] = {
			{ "SIMPLE_VERTEX_LIGHTMAP", "1" }
		};
		ShaderTypeMacroMap[L"TLightPixelShaderFSpotLightPolicyFNoStaticShadowingPolicy"] = {
			{ "SIMPLE_VERTEX_LIGHTMAP", "1" }
		};
		ShaderTypeMacroMap[L"TLightPixelShaderFSpotLightPolicyFShadowTexturePolicy"] = {
			{ "SIMPLE_VERTEX_LIGHTMAP", "1" },
			{ "STATICLIGHTING_TEXTUREMASK", "1" }
		};
		ShaderTypeMacroMap[L"TLightPixelShaderFSpotLightPolicyFShadowVertexBufferPolicy"] = {
			{ "STATICLIGHTING_VERTEXMASK", "1" },
			{ "SIMPLE_VERTEX_LIGHTMAP", "1" }
		};
		ShaderTypeMacroMap[L"TLightPixelShaderFSpotLightPolicyFSignedDistanceFieldShadowTexturePolicy"] = {
			{ "SIMPLE_VERTEX_LIGHTMAP", "1" },
			{ "STATICLIGHTING_SIGNEDDISTANCEFIELD", "1" },
			{ "STATICLIGHTING_TEXTUREMASK", "1" }
		};
		ShaderTypeMacroMap[L"TLightVertexShaderFDirectionalLightPolicyFNoStaticShadowingPolicy"] = {
			{ "SIMPLE_VERTEX_LIGHTMAP", "1" },
			{ "ENABLE_DISTANCE_SHADOW_FADING", "1" }
		};
		ShaderTypeMacroMap[L"TLightVertexShaderFDirectionalLightPolicyFShadowTexturePolicy"] = {
			{ "SIMPLE_VERTEX_LIGHTMAP", "1" },
			{ "ENABLE_DISTANCE_SHADOW_FADING", "1" },
			{ "STATICLIGHTING_TEXTUREMASK", "1" }
		};
		ShaderTypeMacroMap[L"TLightVertexShaderFDirectionalLightPolicyFShadowVertexBufferPolicy"] = {
			{ "SIMPLE_VERTEX_LIGHTMAP", "1" },
			{ "STATICLIGHTING_VERTEXMASK", "1" },
			{ "ENABLE_DISTANCE_SHADOW_FADING", "1" }
		};
		ShaderTypeMacroMap[L"TLightVertexShaderFDirectionalLightPolicyFSignedDistanceFieldShadowTexturePolicy"] = {
			{ "SIMPLE_VERTEX_LIGHTMAP", "1" },
			{ "STATICLIGHTING_SIGNEDDISTANCEFIELD", "1" },
			{ "ENABLE_DISTANCE_SHADOW_FADING", "1" },
			{ "STATICLIGHTING_TEXTUREMASK", "1" }
		};
		ShaderTypeMacroMap[L"TLightVertexShaderFPointLightPolicyFNoStaticShadowingPolicy"] = {
			{ "SIMPLE_VERTEX_LIGHTMAP", "1" }
		};
		ShaderTypeMacroMap[L"TLightVertexShaderFPointLightPolicyFShadowTexturePolicy"] = {
			{ "SIMPLE_VERTEX_LIGHTMAP", "1" },
			{ "STATICLIGHTING_TEXTUREMASK", "1" }
		};
		ShaderTypeMacroMap[L"TLightVertexShaderFPointLightPolicyFShadowVertexBufferPolicy"] = {
			{ "STATICLIGHTING_VERTEXMASK", "1" },
			{ "SIMPLE_VERTEX_LIGHTMAP", "1" }
		};
		ShaderTypeMacroMap[L"TLightVertexShaderFPointLightPolicyFSignedDistanceFieldShadowTexturePolicy"] = {
			{ "SIMPLE_VERTEX_LIGHTMAP", "1" },
			{ "STATICLIGHTING_SIGNEDDISTANCEFIELD", "1" },
			{ "STATICLIGHTING_TEXTUREMASK", "1" }
		};
		ShaderTypeMacroMap[L"TLightVertexShaderFSFXPointLightPolicyFNoStaticShadowingPolicy"] = {
			{ "SIMPLE_VERTEX_LIGHTMAP", "1" }
		};
		ShaderTypeMacroMap[L"TLightVertexShaderFSphericalHarmonicLightPolicyFNoStaticShadowingPolicy"] = {
			{ "SIMPLE_VERTEX_LIGHTMAP", "1" }
		};
		ShaderTypeMacroMap[L"TLightVertexShaderFSpotLightPolicyFNoStaticShadowingPolicy"] = {
			{ "SIMPLE_VERTEX_LIGHTMAP", "1" }
		};
		ShaderTypeMacroMap[L"TLightVertexShaderFSpotLightPolicyFShadowTexturePolicy"] = {
			{ "SIMPLE_VERTEX_LIGHTMAP", "1" },
			{ "STATICLIGHTING_TEXTUREMASK", "1" }
		};
		ShaderTypeMacroMap[L"TLightVertexShaderFSpotLightPolicyFShadowVertexBufferPolicy"] = {
			{ "STATICLIGHTING_VERTEXMASK", "1" },
			{ "SIMPLE_VERTEX_LIGHTMAP", "1" }
		};
		ShaderTypeMacroMap[L"TLightVertexShaderFSpotLightPolicyFSignedDistanceFieldShadowTexturePolicy"] = {
			{ "SIMPLE_VERTEX_LIGHTMAP", "1" },
			{ "STATICLIGHTING_SIGNEDDISTANCEFIELD", "1" },
			{ "STATICLIGHTING_TEXTUREMASK", "1" }
		};
		ShaderTypeMacroMap[L"TShadowDepthPixelShaderFALSEFALSE"] = {
			{ "SIMPLE_VERTEX_LIGHTMAP", "1" }
		};
		ShaderTypeMacroMap[L"TShadowDepthPixelShaderFALSETRUE"] = {
			{ "MATERIAL_USE_SCREEN_DOOR_FADE", "1" },
			{ "SIMPLE_VERTEX_LIGHTMAP", "1" }
		};
		ShaderTypeMacroMap[L"TShadowDepthPixelShaderTRUEFALSE"] = {
			{ "SIMPLE_VERTEX_LIGHTMAP", "1" },
			{ "PERSPECTIVE_CORRECT_DEPTH", "1" }
		};
		ShaderTypeMacroMap[L"TShadowDepthPixelShaderTRUETRUE"] = {
			{ "PERSPECTIVE_CORRECT_DEPTH", "1" },
			{ "SIMPLE_VERTEX_LIGHTMAP", "1" },
			{ "MATERIAL_USE_SCREEN_DOOR_FADE", "1" }
		};
		ShaderTypeMacroMap[L"TShadowDepthVertexShader<ShadowDepth_OutputDepth>"] = {
			{ "SIMPLE_VERTEX_LIGHTMAP", "1" }
		};
		ShaderTypeMacroMap[L"TShadowDepthVertexShader<ShadowDepth_OutputDepthToColor>"] = {
			{ "SIMPLE_VERTEX_LIGHTMAP", "1" }
		};
		ShaderTypeMacroMap[L"TShadowDepthVertexShader<ShadowDepth_PerspectiveCorrect>"] = {
			{ "SIMPLE_VERTEX_LIGHTMAP", "1" },
			{ "PERSPECTIVE_CORRECT_DEPTH", "1" }
		};

		// Additions since EngineDebugMaterials
		ShaderTypeMacroMap[L"TBasePassVertexShaderFCustomSimpleVertexLightMapPolicyFLinearHalfspaceDensityPolicy"] = {
			{ "SIMPLE_VERTEX_LIGHTMAP", "1" },
			{ "NUM_LIGHTMAP_COEFFICIENTS", "1" },
		};

		ShaderTypeMacroMap[L"TBasePassVertexShaderFCustomSimpleVertexLightMapPolicyFSphereDensityPolicy"] = {
			{ "SIMPLE_VERTEX_LIGHTMAP", "1" },
			{ "NUM_LIGHTMAP_COEFFICIENTS", "1" },
		};

	}

	void AddCustomMacros(FMaterialResource* material, ShaderCompileCombo& scc, std::vector<D3D_SHADER_MACRO>& macros) {
		auto shaderType = scc.shaderType;
		auto isVertexShader = scc.shaderType->Frequency == SF_VERTEX;
		auto shaderTypeName = std::wstring(shaderType->Name);

		auto vertexFactoryType = scc.vertexFactoryType;
		auto vertexFactoryName = vertexFactoryType != nullptr ? std::wstring(vertexFactoryType->Name) : std::wstring(L"None");

		InitVertexFactoryMacroMap();
		InitShaderTypeMacroMap();

#if defined(SDK_TARGET_LE3)

		AddMacro(macros, "NUM_LIGHTMAP_COEFFICIENTS", "2");

		// ========== PER-SHADER TYPE MACROS ==========
		// These were found using a mix of BF, MilanShaderResearch, ShaderComparison,
		// and MacroSetFinder tools with some manual work.

		std::wstring shaderTypeNameS(shaderTypeName.begin(), shaderTypeName.end());
		if (ShaderTypeMacroMap.count(shaderTypeNameS)) {
			for (const auto& macro : ShaderTypeMacroMap[shaderTypeNameS]) {
				AddMacro(macros, macro.Name, macro.Definition);
			}
		}
		else {
			// No longer relevant
			//LEASI_TRACE(L"Shader type '{}' not found in ShaderTypeMacroMap", shaderTypeName);
		}

		std::wstring vertexFactoryNameS(vertexFactoryName.begin(), vertexFactoryName.end());
		if (VertexFactoryMacrosMap.count(vertexFactoryNameS)) {
			for (const auto& macro : VertexFactoryMacrosMap[vertexFactoryNameS]) {
				AddMacro(macros, macro.Name, macro.Definition);
			}
		}

		// ========== PER-TYPE MACROS ============
		if (shaderTypeNameS.find(L"FSimpleLightMapTexturePolicy") != std::wstring::npos) {
			AddMacro(macros, "TEXTURE_LIGHTMAP", "1");
			AddMacro(macros, "NUM_LIGHTMAP_COEFFICIENTS", "1"); // Redefines it as 1.
		}

		if (shaderTypeNameS.find(L"FSimpleVertexLightMapPolicy") != std::wstring::npos) {
			AddMacro(macros, "SIMPLE_VERTEX_LIGHTMAP", "1");
			AddMacro(macros, "NUM_LIGHTMAP_COEFFICIENTS", "1"); // Redefines it as 1.
		}

		if (shaderTypeNameS.find(L"FSHLightLightMapPolicy") != std::wstring::npos) {
			AddMacro(macros, "ENABLE_DIRECTIONAL_LIGHT", "1");
		}

		if (shaderTypeNameS.find(L"FDirectionalVertexLightMapPolicy") != std::wstring::npos) {
			AddMacro(macros, "VERTEX_LIGHTMAP", "1");
		}

		if (shaderTypeNameS.find(L"FDirectionalLightLightMapPolicy") != std::wstring::npos) {
			AddMacro(macros, "ENABLE_DIRECTIONAL_LIGHT", "1");
		}

		if (vertexFactoryNameS.starts_with(L"FTerrain")) {
			AddMacro(macros, "WORLD_COORDS", "1");
		}


		// ========== PER-POLICY MACROS ==========
		if (shaderTypeNameS.find(L"FDirectionalLightLightMapPolicy") != std::wstring::npos) {
			AddMacro(macros, "ENABLE_DIRECTIONAL_LIGHT", "1");
		}

		if (shaderTypeNameS.starts_with(L"TBasePassVertexShaderFCustomVectorLightMapTexturePolicy") ||
			shaderTypeNameS.starts_with(L"TBasePassVertexShaderFDirectionalLightMapTexturePolicy"))
		{
			if (shaderTypeNameS.ends_with(L"FConstantDensityPolicy") ||
				shaderTypeNameS.ends_with(L"FLinearHalfspaceDensityPolicy") || 
				shaderTypeNameS.ends_with(L"FSphereDensityPolicy")) {
				AddMacro(macros, "TEXTURE_LIGHTMAP", "1");
			}
		}


		// ========== PER-MATERIAL MACROS ==========
		// FTerrainMaterialResource has no `Material` (UMaterial*) field -- ->Material on one of these
		// is garbage (see IsTerrainMaterialResource() in Hooks.hpp). FTerrainMaterialResource hardcodes
		// the equivalent decision to FALSE for every flag below, either explicitly (IsDecalMaterial(),
		// CastLitTranslucencyShadowAsMasked(), RenderTwoSidedSeparatePass() all return FALSE in
		// UnTerrain.h) or by inherited FMaterial default (IsUsedWithGammaCorrection()), so skipping
		// these macros for terrain reproduces the same effective behavior instead of guessing at a
		// vtable slot mapping.
		bool isTerrain = IsTerrainMaterialResource(material);

		if (!isTerrain && material->Material->bUsedWithDecals) {
			AddMacro(macros, "MATERIAL_DECAL", "1");
		}

		// These four are referenced as #if gates by the generated Material.usf but never defined in
		// it, so they have to come from the compilation environment. UE3 set them in
		// FMaterial::SetupMaterialEnvironment, which LE3's shipping binary does not contain at all,
		// so nothing was supplying them. Without MATERIAL_LIT_TRANSLUCENCY_DEPTH_POSTPASS the
		// translucency depth pass compiled to a bare "write 0" stub instead of clipping on opacity.
		if (!isTerrain && material->Material->bUseLitTranslucencyDepthPass) {
			AddMacro(macros, "MATERIAL_LIT_TRANSLUCENCY_PREPASS", "1");
		}

		if (!isTerrain && material->Material->bUseLitTranslucencyPostRenderDepthPass) {
			AddMacro(macros, "MATERIAL_LIT_TRANSLUCENCY_DEPTH_POSTPASS", "1");
		}

		if (!isTerrain && material->Material->bCastLitTranslucencyShadowAsMasked) {
			AddMacro(macros, "MATERIAL_CAST_LIT_TRANSLUCENCY_SHADOW_AS_MASKED", "1");
		}

		if (!isTerrain && material->Material->bUsedWithGammaCorrection) {
			AddMacro(macros, "MATERIAL_USE_GAMMA_CORRECTION", "1");
		}

		if (material->bUsesVertexColor != 0) {
			AddMacro(macros, "MATERIAL_USES_VERTEX_COLOR", "1");
		}

		// COORDINATE SPACE
		if ((material->UsingTransforms & UsedCoord_Local) != 0 || (material->UsingTransforms & UsedCoord_View) != 0 || (material->UsingTransforms & UsedCoord_World) != 0) {
			// Unlock world coordinate commands in usfs
			AddMacro(macros, "WORLD_COORDS", "1");
		}


		
		if (material->UsingTransforms & UsedCoord_WorldPos) {
			// Unlock view coordinate commands in usfs
			AddMacro(macros, "WORLD_POS", "1");
		}

		// bUsedWithVectorLightMaps/bUsedWithSimpleLightMaps are plain UMaterial fields with no FMaterial
		// virtual equivalent, so there's nothing safe to substitute for terrain here. Terrain does use
		// static lighting (FTerrainMaterialResource::IsUsedWithStaticLighting() is hardcoded TRUE), so
		// if terrain output is missing one of these lightmap-policy shader variants, this is the first
		// place to look -- for now this just avoids the ->Material crash by skipping the macro.
		if (!isTerrain && material->Material->bUsedWithVectorLightMaps && shaderTypeNameS.find(L"FCustomVectorVertexLightMapPolicy") != std::wstring::npos) {
			AddMacro(macros, "USING_VECTORLIGHTMAPS", "1");
			AddMacro(macros, "VERTEX_LIGHTMAP", "1");
		}

		if (!isTerrain && material->Material->bUsedWithSimpleLightMaps && shaderTypeNameS.find(L"FCustomSimpleLightMapTexturePolicy") != std::wstring::npos) {
			AddMacro(macros, "SIMPLE_TEXTURE_LIGHTMAP", "1");
			AddMacro(macros, "NUM_LIGHTMAP_COEFFICIENTS", "1"); // Redefines it as 1.
		}

		// MATERIAL BLENDING
		auto blendMode = material->VTable->GetBlendMode(material);

		// Blend mode
		switch (blendMode)
		{
			case BLEND_Opaque:
				AddMacro(macros, "MATERIALBLENDING_SOLID", "1");
				break;
			case BLEND_Masked:
				AddMacro(macros, "MATERIALBLENDING_MASKED", "1");
				break;
			case BLEND_SoftMasked:
				AddMacro(macros, "MATERIALBLENDING_SOFTMASKED", "1");
				break;
			case BLEND_AlphaComposite:
			case BLEND_Translucent:
				AddMacro(macros, "MATERIALBLENDING_TRANSLUCENT", "1");
				break;
			case BLEND_Additive:
				AddMacro(macros, "MATERIALBLENDING_ADDITIVE", "1");
				break;
			case BLEND_Modulate:
				AddMacro(macros, "MATERIALBLENDING_MODULATE", "1");
				break;
			default:
				LEASI_ERROR("Unknown blend mode: {}", (int)blendMode);
				break;
		}

		// MATERIAL LIGHTING
		auto lightingMode = material->VTable->GetLightingModel(material);
		switch (lightingMode)
		{
			case MLM_SHPRT:
			case MLM_Phong:
				AddMacro(macros, "MATERIAL_LIGHTINGMODEL_PHONG", "1");
				break;
			case MLM_NonDirectional:
				AddMacro(macros, "MATERIAL_LIGHTINGMODEL_NONDIRECTIONAL", "1");
				break;
			case MLM_Unlit:
				AddMacro(macros, "MATERIAL_LIGHTINGMODEL_UNLIT", "1");
				break;
			case MLM_Custom:
				AddMacro(macros, "MATERIAL_LIGHTINGMODEL_CUSTOM", "1");
				break;
			case MLM_Anisotropic:
				AddMacro(macros, "MATERIAL_LIGHTINGMODEL_ANISOTROPIC", "1");
				break;
			default:
				LEASI_ERROR("Unknown lighting mode: {}", (int)lightingMode);
				break;
		}

		// MATERIAL TWO SIDED
		if (material->VTable->IsTwoSided(material)) {
			AddMacro(macros, "MATERIAL_TWOSIDED", "1");
		}

		// MaterialTemplate.usf keys OPTIONAL_FacingSign off this: when set, FacingSign is a compile
		// time constant, and when clear the pixel shader takes an extra VFACE input. Nothing was
		// supplying it, so every material with the flag got one spurious input signature entry --
		// bytecode otherwise identical, RDEF/SHEX/STAT byte for byte, only ISGN longer. It is a
		// material property in its own right, not derivable from IsTwoSided: of the shaders missing
		// it, most non-two-sided materials were already correct without it.
		if (!isTerrain && material->Material->TwoSidedSeparatePass) {
			AddMacro(macros, "MATERIAL_TWOSIDED_SEPARATE_PASS", "1");
		}


		// ========== POST-PROCESSING: REMOVE MACROS ==========
		// Some shader types should NOT have SIMPLE_VERTEX_LIGHTMAP even when their
		// vertex factory adds it (e.g. translucency/dynamic lighting policies)
		if (shaderTypeNameS.find(L"FNoLightMapPolicy") != std::wstring::npos ||
			shaderTypeNameS.find(L"FDirectionalLightLightMapPolicy") != std::wstring::npos ||
			shaderTypeNameS.find(L"FSHLightLightMapPolicy") != std::wstring::npos) {
			RemoveMacro(macros, "SIMPLE_VERTEX_LIGHTMAP");
			RemoveMacro(macros, "TEXTURE_LIGHTMAP");
		}

		//if (shaderTypeNameS.starts_with(L"TBassPassVertexShaderFCustomSimpleLightMapTexturePolicy") ||
		//	shaderTypeNameS.starts_with(L"TBassPassVertexShaderFSimpleLightMapTexturePolicy")) {
		//	// These are auto generated for lightmapped shader types but these two are light policy suffixes
		//	// that never set this. It's in the macro hell that is shader types
		//	// This is a pretty common one but I don't understand how to apply it
		//	// if you turn this on it breaks a ton of stuff
		//	if (shaderTypeNameS.ends_with(L"SkyLight") && !isVertexShader) {
		//		RemoveMacro(macros, "TEXTURE_LIGHTMAP");
		//	}
		//}

		bool isSkyLight = shaderTypeNameS.ends_with(L"SkyLight") && !shaderTypeNameS.ends_with(L"NoSkyLight");

		// SimpleLightMap uses SIMPLE_TEXTURE_LIGHTMAP and must not also have TEXTURE_LIGHTMAP.
		// This applies to both the SkyLight and NoSkyLight variants; it was previously restricted to
		// SkyLight, which left the NoSkyLight ones sampling a lightmap texture the shipping shader
		// does not sample. Verified bit-exact against shipping bytecode for both variants.
		if (shaderTypeNameS.starts_with(L"TBasePassPixelShaderFSimpleLightMapTexturePolicy")) {
			if (!isVertexShader) {
				AddMacro(macros, "SIMPLE_TEXTURE_LIGHTMAP", "1");
				RemoveMacro(macros, "TEXTURE_LIGHTMAP");
			}
		}

		// CustomSimpleLightMap is the mirror image: TEXTURE_LIGHTMAP, never SIMPLE_TEXTURE_LIGHTMAP.
		// Same correction as above — it holds for NoSkyLight too.
		if (shaderTypeNameS.starts_with(L"TBasePassPixelShaderFCustomSimpleLightMapTexturePolicy")) {
			if (!isVertexShader) {
				RemoveMacro(macros, "SIMPLE_TEXTURE_LIGHTMAP");
				AddMacro(macros, "TEXTURE_LIGHTMAP", "1");
			}
		}

		if (shaderTypeNameS.find(L"FCustomSimpleVertexLightMapPolicy") != std::wstring::npos ||
			shaderTypeNameS.find(L"FCustomVectorVertexLightMapPolicy") != std::wstring::npos ||
			shaderTypeNameS.find(L"FDirectionalVertexLightMapPolicy") != std::wstring::npos) {

			// These are auto generated for lightmapped shader types but these two are light policy suffixes
			// that never set this. It's in the macro hell that is shader types
			// This is a pretty common one but I don't understand how to apply it
			// if you turn this on it breaks a ton of stuff
			if (isSkyLight && !isVertexShader) {
				RemoveMacro(macros, "TEXTURE_LIGHTMAP");
				RemoveMacro(macros, "SIMPLE_TEXTURE_LIGHTMAP");
			}
		}

#endif
	}
}