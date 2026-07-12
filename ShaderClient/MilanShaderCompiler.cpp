#pragma once
#include "Common/Base.hpp"
#include "LESDK/Headers.hpp"
#include "ShaderClient/MilanShaderCompiler.hpp"
#include "ShaderClient/MilanShaderCompilerShared.h"
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
			LEASI_INFO("Unable to load MilanShaderCompilerLib.dll");
		}
		MilanCompileShader = (tMilanCompileShader*)GetProcAddress(compilerDLL, "CompileShader");
		if (MilanCompileShader == nullptr) {
			LEASI_INFO("Unable to locate 'CompileShader' in MilanShaderCompilerLib.dll");
		}
		else {
			LEASI_INFO("Loaded 'CompileShader' from MilanShaderCompilerLib.dll");
		}

		MilanGetShaderInfo = (tMilanGetShaderInfo*)GetProcAddress(compilerDLL, "GetShaderInfo");
		if (MilanGetShaderInfo == nullptr) {
			LEASI_INFO("Unable to locate 'GetShaderInfo' in MilanShaderCompilerLib.dll");
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


	void AddMacroW(std::vector<D3D_SHADER_MACRO>& Macros, const wchar_t* name, const wchar_t* definition) {
		D3D_SHADER_MACRO macro;
		macro.Name = wchar_to_char(name);
		macro.Definition = wchar_to_char(definition);
		Macros.push_back(macro);
	}

	std::unordered_map<std::wstring, std::vector<D3D_SHADER_MACRO>> VertexFactoryMacrosMap{};
	void InitVertexFactoryMacroMap()
	{
		if (VertexFactoryMacrosMap.size() != 0) {
			// Already initialized
			return;
		}
		VertexFactoryMacrosMap[L"FFluidTessellationVertexFactory"] = {
			{ "XBOXTESSELLATION", "1" }
		};
		VertexFactoryMacrosMap[L"FFluidVertexFactory"] = {
			{ "XBOXTESSELLATION", "1" }
		};
		VertexFactoryMacrosMap[L"FGPUSkinDecalVertexFactory"] = {
			{ "GPUSKIN_DECAL", "1" },
			{ "QUAT_SKINNING", "1" }
		};
		VertexFactoryMacrosMap[L"FGPUSkinMorphDecalVertexFactory"] = {
			{ "GPUSKIN_DECAL", "1" },
			{ "GPUSKIN_MORPH_BLEND", "1" },
			{ "QUAT_SKINNING", "1" }
		};
		VertexFactoryMacrosMap[L"FGPUSkinMorphVertexFactory"] = {
			{ "GPUSKIN_MORPH_BLEND", "1" },
			{ "QUAT_SKINNING", "1" }
		};
		VertexFactoryMacrosMap[L"FGPUSkinVertexFactory"] = {
			{ "QUAT_SKINNING", "1" }
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
			{ "DECAL_FACTORY", "1" }
		};
		VertexFactoryMacrosMap[L"FParticleBeamTrailDynamicParameterVertexFactory"] = {
			{ "NUM_AXIS_ROTATION_VECTORS", "1" },
			{ "PARTICLES_ALLOW_AXIS_ROTATION", "1" },
			{ "SIMPLE_VERTEX_LIGHTMAP", "1" },
			{ "USE_DYNAMIC_PARAMETERS", "1" },
			{ "USE_OCCLUSION_PERCENTAGE", "1" }
		};
		VertexFactoryMacrosMap[L"FParticleBeamTrailVertexFactory"] = {
			{ "NUM_AXIS_ROTATION_VECTORS", "1" },
			{ "PARTICLES_ALLOW_AXIS_ROTATION", "1" },
			{ "USE_OCCLUSION_PERCENTAGE", "1" }
		};
		VertexFactoryMacrosMap[L"FParticleDynamicParameterVertexFactory"] = {
			{ "NUM_AXIS_ROTATION_VECTORS", "1" },
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
			{ "NUM_AXIS_ROTATION_VECTORS", "1" },
			{ "PARTICLES_ALLOW_AXIS_ROTATION", "1" },
			{ "SIMPLE_VERTEX_LIGHTMAP", "1" },
			{ "SUBUV_PARTICLES", "1" },
			{ "USE_DYNAMIC_PARAMETERS", "1" },
			{ "USE_OCCLUSION_PERCENTAGE", "1" }
		};
		VertexFactoryMacrosMap[L"FParticleSubUVVertexFactory"] = {
			{ "NUM_AXIS_ROTATION_VECTORS", "1" },
			{ "PARTICLES_ALLOW_AXIS_ROTATION", "1" },
			{ "SUBUV_PARTICLES", "1" },
			{ "USE_OCCLUSION_PERCENTAGE", "1" }
		};
		VertexFactoryMacrosMap[L"FParticleVertexFactory"] = {
			{ "NUM_AXIS_ROTATION_VECTORS", "1" },
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
			{ "SIMPLE_VERTEX_LIGHTMAP", "1" }
		};
		ShaderTypeMacroMap[L"FVelocityVertexShader"] = {
			{ "SIMPLE_VERTEX_LIGHTMAP", "1" }
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
			{ "SIMPLE_VERTEX_LIGHTMAP", "1" }
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

	}

	void AddCustomMacros(FMaterialResource* material, ShaderCompileCombo& scc, std::vector<D3D_SHADER_MACRO>& macros) {
		auto shaderType = scc.shaderType;
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
			LEASI_TRACE(L"Shader type '{}' not found in ShaderTypeMacroMap", shaderTypeName);
		}

		std::wstring vertexFactoryNameS(vertexFactoryName.begin(), vertexFactoryName.end());
		if (VertexFactoryMacrosMap.count(vertexFactoryNameS)) {
			for (const auto& macro : VertexFactoryMacrosMap[vertexFactoryNameS]) {
				AddMacro(macros, macro.Name, macro.Definition);
			}
		}

		// ========== POST-PROCESSING: REMOVE MACROS ==========
		// Some shader types should NOT have SIMPLE_VERTEX_LIGHTMAP even when their
		// vertex factory adds it (e.g. translucency/dynamic lighting policies)
		if (shaderTypeNameS.find(L"FNoLightMapPolicy") != std::wstring::npos ||
			shaderTypeNameS.find(L"FDirectionalLightLightMapPolicy") != std::wstring::npos ||
			shaderTypeNameS.find(L"FSHLightLightMapPolicy") != std::wstring::npos) {
			RemoveMacro(macros, "SIMPLE_VERTEX_LIGHTMAP");
		}

		// ========== PER-MATERIAL MACROS ==========
		if (material->bUsesVertexColor != 0) {
			AddMacro(macros, "MATERIAL_USES_VERTEX_COLOR", "1");
		}

		// COORDINATE SPACE
		if ((material->UsingTransforms & UsedCoord_Local) || (material->UsingTransforms & UsedCoord_View) || (material->UsingTransforms & UsedCoord_World)) {
			// Unlock world coordinate commands in usfs
			AddMacro(macros, "WORLD_COORDS", "1");
		}

		if (material->UsingTransforms & UsedCoord_WorldPos) {
			// Unlock view coordinate commands in usfs
			AddMacro(macros, "WORLD_POS", "1");
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
		}

		// MATERIAL TWO SIDED
		if (material->VTable->IsTwoSided(material)) {
			AddMacro(macros, "MATERIAL_TWOSIDED", "1");
		}
#endif
	}
}