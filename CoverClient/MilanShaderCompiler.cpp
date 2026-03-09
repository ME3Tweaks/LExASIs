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
		D3D_SHADER_MACRO macro;
		macro.Name = name;
		macro.Definition = definition;
		Macros.push_back(macro);
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

	void AddCustomMacros(FMaterialResource* material, ShaderCompileCombo& scc, std::vector<D3D_SHADER_MACRO>& macros) {
		auto shaderType = scc.shaderType;
		auto shaderTypeName = std::wstring(shaderType->Name);

#if defined(SDK_TARGET_LE3)
		// Gonna have to find a better way to do this
		// Maybe we can override the shader type ModifyCompilationEnvironment() calls?
		// Note: Too many of these to be feasible.

		// Unsure of the value this is supposed to be.
		// But it seems to only be used to initialize arrays	
		AddMacro(macros, "NUM_LIGHTMAP_COEFFICIENTS", "2");
		//}

		if (shaderTypeName.find(L"TBasePassPixelShader") != std::string::npos // Ensure this type of shader...
			&& shaderTypeName.find(L"PolicySkyLight") != std::string::npos) { // and it is marked with 'SkyLight' (we check prev text as 'NoSkyLight' is also used 
			AddMacro(macros, "ENABLE_SKY_LIGHT", "1");
		}

		if (shaderTypeName.find(L"FCustomVectorVertexLightMapPolicy") != std::string::npos) { 
			AddMacro(macros, "VERTEX_LIGHTMAP", "1");
			if (shaderTypeName.find(L"TBasePassVertexShader") != std::string::npos) {
				// Not sure how wide this one is set
				AddMacro(macros, "USING_VECTORLIGHTMAPS", "1");
			}
		}


		// BF =================
			
		// Shader Types
		// Not sure on this one
		if (shaderTypeName.find(L"TDepthOnlyScreenDoorPixelShader") != std::string::npos) { // Ensure this type of shader...
			AddMacro(macros, "MATERIAL_USE_SCREEN_DOOR_FADE", "1");
		}


		if (shaderTypeName.find(L"TAmbientOcclusionPixelShader") != std::string::npos) { // Ensure this type of shader...

			AddMacro(macros, "AO_QUALITY", "1");

			// last 2 bools seem to be arbitrary projection, ssao2, probably.
			if (shaderTypeName.find(L"FALSEFALSE") != std::string::npos) {

			}
			else if (shaderTypeName.find(L"FALSETRUE") != std::string::npos) {
				AddMacro(macros, "SSAO2", "1");

			}
			else if (shaderTypeName.find(L"TRUEFALSE") != std::string::npos) {
				AddMacro(macros, "ARBITRARY_PROJECTION", "1");

			}
			else if (shaderTypeName.find(L"TRUETRUE") != std::string::npos) {
				AddMacro(macros, "ARBITRARY_PROJECTION", "1");
				AddMacro(macros, "SSAO2", "1");
			}
		}

		if (shaderTypeName.find(L"TShadowDepthPixelShader") != std::string::npos) {

			// last 2 bools seem to be perspective correct, screen door fade
			if (shaderTypeName.find(L"FALSEFALSE") != std::string::npos) {
				// nothing
			}
			else if (shaderTypeName.find(L"FALSETRUE") != std::string::npos) {
				AddMacro(macros, "MATERIAL_USE_SCREEN_DOOR_FADE", "1");

			}
			else if (shaderTypeName.find(L"TRUEFALSE") != std::string::npos) {
				AddMacro(macros, "PERSPECTIVE_CORRECT_DEPTH", "1");

			}
			else if (shaderTypeName.find(L"TRUETRUE") != std::string::npos) {
				AddMacro(macros, "PERSPECTIVE_CORRECT_DEPTH", "1");
				AddMacro(macros, "MATERIAL_USE_SCREEN_DOOR_FADE", "1");

			}
		}

		if (shaderTypeName.find(L"TShadowDepthVertexShader<ShadowDepth_PerspectiveCorrect>") != std::string::npos) {
			AddMacro(macros, "PERSPECTIVE_CORRECT_DEPTH", "1");
		}

		/*
		* Template:
		if (shaderTypeName.find(L"") != std::string::npos) {
			AddMacro(macros, "", "1");
		}
		*/

		if (shaderTypeName.find(L"FSignedDistanceField") != std::string::npos) {
			AddMacro(macros, "STATICLIGHTING_SIGNEDDISTANCEFIELD", "1");
		}

		if (shaderTypeName.find(L"FDirectionalLightPolicy") != std::string::npos ||
			shaderTypeName.find(L"FDirectionalLightPolicy") != std::string::npos) {
			AddMacro(macros, "ENABLE_DISTANCE_SHADOW_FADING", "1");
		}

		if (shaderTypeName.find(L"ShadowTexturePolicy") != std::string::npos) {
			AddMacro(macros, "STATICLIGHTING_TEXTUREMASK", "1");
		}

		if (shaderTypeName.find(L"FShadowVertexBufferPolicy") != std::string::npos) {
			AddMacro(macros, "STATICLIGHTING_VERTEXMASK", "1");
		}

		if (shaderTypeName.find(L"TAOApplyPixelShader<AOApply_ReadFromAOHistory>") != std::string::npos) {
			AddMacro(macros, "APPLY_FROM_AOHISTORY", "1");
		}

		if (shaderTypeName.find(L"FTranslucencyPostRenderDepthPixelShader") != std::string::npos) {
			AddMacro(macros, "OUTPUT_DEPTH_TO_ALPHA", "1");
		}

		if (shaderTypeName.find(L"FFXAA3BlendPixelShader") != std::string::npos) {
			AddMacro(macros, "FXAA_PC", "1");
			AddMacro(macros, "FXAA_HLSL_3", "1");
		}

		// ========== BEGIN JSON-BASED POLICY MACRO DEFINITIONS ==========
		// Generated from ModifyCompilationEnvironmentDefinitions.json

		if (shaderTypeName.find(L"FLandscapeVertexFactory") != std::string::npos) {
			AddMacro(macros, "PER_PIXEL_TANGENT_BASIS", "1");
		}

		if (shaderTypeName.find(L"FLandscapeDecalVertexFactory") != std::string::npos) {
			AddMacro(macros, "DECAL_FACTORY", "1");
			AddMacro(macros, "WORLD_COORDS", "1");
		}

		if (shaderTypeName.find(L"FLandscapeVertexFactoryMobile") != std::string::npos) {
			AddMacro(macros, "IS_LANDSCAPE_SIMMOBILE", "1");
		}

		if (shaderTypeName.find(L"FNoLightMapPolicy") != std::string::npos) {
			AddMacro(macros, "NUM_LIGHTMAP_COEFFICIENTS", "2");
		}

		if (shaderTypeName.find(L"FDirectionalVertexLightMapPolicy") != std::string::npos) {
			AddMacro(macros, "VERTEX_LIGHTMAP", "1");
			AddMacro(macros, "NUM_LIGHTMAP_COEFFICIENTS", "2");
		}

		if (shaderTypeName.find(L"FSimpleVertexLightMapPolicy") != std::string::npos) {
			AddMacro(macros, "SIMPLE_VERTEX_LIGHTMAP", "1");
			AddMacro(macros, "NUM_LIGHTMAP_COEFFICIENTS", "1");
		}

		if (shaderTypeName.find(L"FDirectionalLightMapTexturePolicy") != std::string::npos) {
			AddMacro(macros, "TEXTURE_LIGHTMAP", "1");
			AddMacro(macros, "NUM_LIGHTMAP_COEFFICIENTS", "2");
		}
		
		if (shaderTypeName.find(L"FCustomVectorLightMapTexturePolicy") != std::string::npos) {
			// Seems to set either TEXTURE_LIGHTMAP || SIMPLE_TEXTURE_LIGHTMAP
			// both yield same hlsl though...
			AddMacro(macros, "TEXTURE_LIGHTMAP", "1");
			AddMacro(macros, "NUM_LIGHTMAP_COEFFICIENTS", "2");
		}

		if (shaderTypeName.find(L"FSimpleLightMapTexturePolicy") != std::string::npos) {
			AddMacro(macros, "SIMPLE_TEXTURE_LIGHTMAP", "1");
			AddMacro(macros, "NUM_LIGHTMAP_COEFFICIENTS", "1");
		}

		if (shaderTypeName.find(L"FDummyLightMapTexturePolicy") != std::string::npos) {
			AddMacro(macros, "TEXTURE_LIGHTMAP", "1");
		}

		if (shaderTypeName.find(L"FDirectionalLightLightMapPolicy") != std::string::npos) {
			AddMacro(macros, "NUM_LIGHTMAP_COEFFICIENTS", "2");
			AddMacro(macros, "ENABLE_DIRECTIONAL_LIGHT", "1");
			AddMacro(macros, "TRANSLUCENCY_ONEPASS_DYNAMICALLY_SHADOWED", "1");
		}

		if (shaderTypeName.find(L"FSHLightLightMapPolicy") != std::string::npos) {
			// This is as subclass of FDirectionalLightLightMapPolicy, so it has the same macros as that, plus the SH light macro.
			AddMacro(macros, "ENABLE_SH_LIGHT", "1");
			AddMacro(macros, "NUM_LIGHTMAP_COEFFICIENTS", "2");
			AddMacro(macros, "ENABLE_DIRECTIONAL_LIGHT", "1");
			AddMacro(macros, "TRANSLUCENCY_ONEPASS_DYNAMICALLY_SHADOWED", "1");
		}

		if (shaderTypeName.find(L"FDynamicallyShadowedMultiTypeLightLightMapPolicy") != std::string::npos) {
			AddMacro(macros, "DYNAMICALLY_SHADOWED_BASEPASS_LIGHT", "1");
			AddMacro(macros, "ENABLE_MULTITYPE_BASEPASS_LIGHT", "1");
			AddMacro(macros, "NUM_LIGHTMAP_COEFFICIENTS", "2");
		}

		if (shaderTypeName.find(L"FSHLightAndMultiTypeLightMapPolicy") != std::string::npos) {
			AddMacro(macros, "ENABLE_SH_LIGHT", "1");
		}

		if (shaderTypeName.find(L"FTextureShadowedDynamicLightLightMapPolicy") != std::string::npos) {
			AddMacro(macros, "STATICLIGHTING_TEXTUREMASK", "1");
		}

		if (shaderTypeName.find(L"FTextureDistanceFieldShadowedDynamicLightLightMapPolicy") != std::string::npos) {
			AddMacro(macros, "STATICLIGHTING_SIGNEDDISTANCEFIELD", "1");
		}

		if (shaderTypeName.find(L"FVertexShadowedDynamicLightLightMapPolicy") != std::string::npos) {
			AddMacro(macros, "STATICLIGHTING_VERTEXMASK", "1");
		}

		if (shaderTypeName.find(L"FLocalDecalVertexFactory") != std::string::npos) {
			AddMacro(macros, "WORLD_COORDS", "1");
			AddMacro(macros, "DECAL_FACTORY", "1");
		}

		if (shaderTypeName.find(L"FTerrainDecalVertexFactory") != std::string::npos) {
			AddMacro(macros, "DECAL_FACTORY", "1");
			AddMacro(macros, "WORLD_COORDS", "1");
		}

		if (shaderTypeName.find(L"FTerrainMorphVertexFactory") != std::string::npos) {
			AddMacro(macros, "TERRAIN_MORPHING_ENABLED", "1");
		}

		if (shaderTypeName.find(L"FTerrainMorphDecalVertexFactory") != std::string::npos) {
			AddMacro(macros, "DECAL_FACTORY", "1");
			AddMacro(macros, "WORLD_COORDS", "1");
		}

		if (shaderTypeName.find(L"FTerrainFullMorphVertexFactory") != std::string::npos) {
			AddMacro(macros, "TERRAIN_MORPHING_ENABLED", "1");
			AddMacro(macros, "TERRAIN_MORPHING_GRADIENTS", "1");
		}

		if (shaderTypeName.find(L"FTerrainFullMorphDecalVertexFactory") != std::string::npos) {
			AddMacro(macros, "DECAL_FACTORY", "1");
			AddMacro(macros, "WORLD_COORDS", "1");
		}

		if (shaderTypeName.find(L"FTessellationMaterialPolicy") != std::string::npos) {
			// Not sure game uses this.
			// AddMacro(macros, "USING_SM5_TESSELLATION", "0");
			// AddMacro(macros, "USING_SM5_TESSELLATION", "1");
			// AddMacro(macros, "TESSELLATION_TYPE_FLAT", "1");
			// AddMacro(macros, "TESSELLATION_TYPE_PNTRIANGLES", "1");
			// AddMacro(macros, "DISPLACEMENT_ANTICRACK", "1");
			// AddMacro(macros, "DISPLACEMENT_ANTICRACK", "0");
		}

		// ========== END JSON-BASED POLICY MACRO DEFINITIONS ==========
		// Add more macros here after checking above

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