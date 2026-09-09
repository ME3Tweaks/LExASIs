
#include "ShaderClient/Hooks.hpp"
#include "ShaderClient/Helpers.hpp"
#include "ShaderClient/Client.hpp"
#include "Common/Objects.hpp"
#include "LESDK/Headers.hpp"
#include <map>

namespace ShaderClient
{
	tFGetMaterialFriendlyName* FMaterial_GetFriendlyName = nullptr;
	void* TerrainMaterialResourceVTable = nullptr;
	int* GIsSeekFreePCConsole = nullptr;

	ECookingTarget* GCookingTarget = nullptr;

	UPackage** GObjTransientPackage = nullptr;
	FArchive** GError = nullptr;

	FFileManager** GFileManager = nullptr;

	tFMaterialCompile* FMaterialCompile = nullptr;
	tFMaterialCompile* FMaterialCompile_orig = nullptr;

	tCompileShader* FMaterialShaderMap_Compile = nullptr;
	tCompileShader* FMaterialShaderMap_Compile_orig = nullptr;

	tInitShaderMap* InitShaderMap_orig = nullptr;
	tHLSLTranslate* FHLSLTranslator_Translate = nullptr;
	tHLSLTranslate* FHLSLTranslator_Translate_orig = nullptr;
	bool AllowLegacyOpaqueSceneDepthTest = false;

	tCompileWrapper* FMaterial_Compile = nullptr;

	tGetMaterialCode* FHLSL_GetMaterialCode = nullptr;

	tGetShaderTypeList* GetShaderTypeList = nullptr;
	tGetVertexFactoryTypeList* GetVertexFactoryTypeList = nullptr;

	tShouldCache* ShouldCache = nullptr;

	tByteOrderSerialize* ByteOrderSerialize = nullptr;

	tCreateArchiveProxy* CreateArchiveProxy = nullptr;
	void* StringArchiveProxyVTableStart = nullptr;

	void* MemoryArchiveWriterVTableStart = nullptr;

	tSerializeUShaderCache* SerializeUShaderCache = nullptr;

	tSerializeMaterialResource* SerializeMaterialResource = nullptr;
	tSerializeTerrainMaterialResource* SerializeTerrainMaterialResource = nullptr;

	tSerializeFUniformExpressionSet* SerializeFUniformExpressionSet = nullptr;

	FArchiveProxy_NamesAndObjectsAsStrings_VTable NamesAndObjectsAsStrings_VTable;

	tGetTlkString* GetTlkString_orig = nullptr;

	FArchiveProxy GetStringWithObjectWriterProxy(FArchive& inArchive) {
		// Copy strings VTable
		memcpy((void*)&NamesAndObjectsAsStrings_VTable, StringArchiveProxyVTableStart, 0xE0);

		// Replace SerializeObject()
		NamesAndObjectsAsStrings_VTable.SerializeObject = PackagelessSerializeObjectRef;

		// Wrap writer in our new special serializer
		FArchiveProxy ProxyArchive;
		CreateArchiveProxy((FArchiveProxy*)&ProxyArchive, &inArchive);
		SetVTable((void**)&ProxyArchive, (void*)&NamesAndObjectsAsStrings_VTable);
		return ProxyArchive;
	}

	FArchiveProxy* PackagelessSerializeObjectRef(FArchiveProxy* Ar, UObject** obj) {
		if (Ar->ArIsLoading) {
			// This isn't really supported...
			/*
			DWORD strLen;
			Ar->VTable->SerializeInt(Ar, strLen, 512); // Read string length.
			if (strLen < 0) {
				// Unicode
			}
			else if (strLen > 0){
				// ANSI
			}
			*/
		}
		else
		{
			// auto vtable = GetVTable((void***)&obj);
			// auto specialVtable = &NamesAndObjectsAsStrings_VTable;

			std::wstring val;
			if (obj == nullptr) {
				val = L"[NULL]";
			}
			else
			{
				val = (*obj)->GetFullPath().Chars();
			}

			// std::cout << "SerializeObject: " << val << std::endl;

			// Serialize string length.
			DWORD len = (DWORD)val.length();
			Ar->VTable->Serialize(Ar, &len, 4);

			// Serialize the string.
			Ar->VTable->Serialize(Ar, val.data(), len * sizeof(wchar_t)); // len * 2 cause this is an array of wchar_t
		}

		return Ar;
	}

	bool IsTerrainMaterialResource(FMaterial* fmaterial)
	{
		// The object's own vtable pointer is its first 8 bytes. FTerrainMaterialResource is the
		// only class using TerrainMaterialResourceVTable, so this is an exact identity check --
		// see the comment on TerrainMaterialResourceVTable in Hooks.hpp for why GetFriendlyName()
		// can't be used for this instead.
		return *(void**)fmaterial == TerrainMaterialResourceVTable;
	}

	FString* GetFriendlyNameVirtual(FMaterial* fmaterial, FString* OutStr)
	{
		auto vtable = *(void***)fmaterial;
		auto func = (tFGetMaterialFriendlyName*)vtable[55];
		return func(fmaterial, OutStr);
	}

	UBOOL FHLSLTranslator_Translate_hook(FHLSLTranslator* translator)
	{
		const auto originalResult = FHLSLTranslator_Translate_orig(translator);

		if (!AllowLegacyOpaqueSceneDepthTest || translator == nullptr || translator->Material == nullptr) {
			return originalResult;
		}

		// LE2/LE3 emit this after property compilation but continue generating the HLSL. LE1's
		// equivalent translator has no such error path. Remove precisely that compatibility
		// diagnostic for any opted-in legacy material; all unrelated errors, including scene-color
		// usage, remain fatal.
		static constexpr const wchar_t* LegacyOpaqueSceneDepthError =
			L"Only transparent materials can read from scene depth.";
		auto& compileErrors = translator->Material->CompileErrors;
		bool onlyCompatibilityErrors = compileErrors.Any();
		for (UINT index = 0; index < compileErrors.Count(); ++index) {
			if (!compileErrors(index).Equals(LegacyOpaqueSceneDepthError, false)) {
				onlyCompatibilityErrors = false;
				break;
			}
		}

		if (!onlyCompatibilityErrors) {
			return originalResult;
		}

		compileErrors.Clear();
		translator->bSuccess = TRUE;
		LEASI_WARN(L"Permitting legacy opaque SceneDepth compatibility diagnostic");
		return TRUE;
	}

	UBOOL InitShaderMap_hook(FMaterial* fmaterial, void* StaticParameters, EShaderPlatform Platform, int Quality)
	{
		const bool isTerrain = IsTerrainMaterialResource(fmaterial);
		FString terrainName;
		if (isTerrain)
		{
			GetFriendlyNameVirtual(fmaterial, &terrainName);
		}

		// Capture-only runs never call into the compiler, so a missing MilanShaderCompilerLib must
		// not stop us from tricking the game into requesting compiles — that request is the capture.
		if ((MilanCompileShader == nullptr && !CaptureOnly) || !EnableShaderCompile) {
			auto res = InitShaderMap_orig(fmaterial, StaticParameters, Platform, Quality);
			if (isTerrain)
			{
				auto* shaderMap = fmaterial->ShaderMap.Object;
				// InitShaderMap only establishes that the individual resource can be used. Dump the
				// owning terrain's live cache at this exact point as well: it lets a no-compile run
				// distinguish an accepted resource from one that was saved to the wrong cache array,
				// or that is no longer present in the runtime cache that components will query.
				constexpr size_t TerrainOwnerOffset = 0x1EC;
				constexpr size_t TerrainMaskOffset = 0x1F4;
				constexpr size_t TerrainMaskBitsOffset = 0x1FC;
				auto* terrain = *reinterpret_cast<ATerrain**>(reinterpret_cast<BYTE*>(fmaterial) + TerrainOwnerOffset);
				auto mask = *reinterpret_cast<UINT64*>(reinterpret_cast<BYTE*>(fmaterial) + TerrainMaskOffset);
				auto maskBits = *reinterpret_cast<INT*>(reinterpret_cast<BYTE*>(fmaterial) + TerrainMaskBitsOffset);
				LEASI_INFO(L"Terrain InitShaderMap: resource={}, result={}, platform={}, quality={}, shader-map={}, pixel2D={}",
					terrainName.Chars(),
					res != FALSE ? L"ready" : L"not-ready",
					static_cast<int>(Platform),
					Quality,
					static_cast<void*>(shaderMap),
					shaderMap == nullptr ? 0 : shaderMap->UniformExpressionSet.Uniform2DTextureExpressions.Count());
				if (shaderMap != nullptr)
				{
					// A shader-map completeness result does not show which mesh vertex-factory
					// submaps were deserialized. Terrain's draw path can only use one of the
					// FTerrain* factories, so enumerate those exact lookup keys and their
					// material shader-map entries. This is read-only and runs only for terrain.
					auto vertexFactoryList = *GetVertexFactoryTypeList();
					while (vertexFactoryList != nullptr)
					{
						auto* vertexFactory = vertexFactoryList->CurrentItem;
						if (vertexFactory != nullptr && vertexFactory->Name != nullptr && wcsstr(vertexFactory->Name, L"Terrain") != nullptr)
						{
							auto lookupKey = vertexFactory;
							auto* meshShaderMap = shaderMap->VertexFactoryMap.Find(std::move(lookupKey));
							LEASI_INFO(L"  Terrain vertex factory: name={}, shader-file={}, map-entry={}, mesh-map={}",
								vertexFactory->Name,
								vertexFactory->ShaderFilename == nullptr ? L"<none>" : vertexFactory->ShaderFilename,
								meshShaderMap != nullptr,
								meshShaderMap == nullptr ? nullptr : static_cast<void*>(*meshShaderMap));
						}
						vertexFactoryList = vertexFactoryList->NextItem;
					}
				}
				LEASI_INFO(L"Terrain cache ownership: terrain={}, resource={}, mask={:X}/{} bits, primary={}, secondary={}",
					static_cast<void*>(terrain),
					static_cast<void*>(fmaterial),
					mask,
					maskBits,
					terrain == nullptr ? 0 : terrain->CachedTerrainMaterials[0].CachedMaterials.Count(),
					terrain == nullptr ? 0 : terrain->CachedTerrainMaterials[1].CachedMaterials.Count());
				if (terrain != nullptr)
				{
					for (INT cacheIndex = 0; cacheIndex < 2; cacheIndex++)
					{
						auto& cachedMaterials = terrain->CachedTerrainMaterials[cacheIndex].CachedMaterials;
						for (UINT entryIndex = 0; entryIndex < cachedMaterials.Count(); entryIndex++)
						{
							auto* cachedResource = reinterpret_cast<FMaterial*>(cachedMaterials(entryIndex));
							auto cachedMask = *reinterpret_cast<UINT64*>(reinterpret_cast<BYTE*>(cachedResource) + TerrainMaskOffset);
							auto cachedMaskBits = *reinterpret_cast<INT*>(reinterpret_cast<BYTE*>(cachedResource) + TerrainMaskBitsOffset);
							LEASI_INFO(L"  Terrain cache[{}][{}]: resource={}, current={}, mask={:X}/{} bits, shader-map={}",
								cacheIndex,
								entryIndex,
								static_cast<void*>(cachedResource),
								cachedResource == fmaterial,
								cachedMask,
								cachedMaskBits,
								static_cast<void*>(cachedResource->ShaderMap.Object));
						}
					}
				}
			}
			if (!res) {
				FString name;
				GetFriendlyNameVirtual(fmaterial, &name);
				LEASI_WARN(L"Material requires compile: {}", name.Chars());
			}
			return res;
		}
		int originalSeekFreeValue = 0;
		if (GIsSeekFreePCConsole != nullptr) {
			originalSeekFreeValue = *GIsSeekFreePCConsole;
			*GIsSeekFreePCConsole = 0; // Tricks game into thinking it can attempt to compile shader
		}
		auto result = InitShaderMap_orig(fmaterial, StaticParameters, Platform, Quality);
		if (result == FALSE)
		{
			FString name;
			GetFriendlyNameVirtual(fmaterial, &name);
			LEASI_WARN(L"InitShaderMap failed for material {}", name);
		}
		else
		{
			// Breakpoint for debug
			auto sm = fmaterial->ShaderMap;
		}
		if (GIsSeekFreePCConsole != nullptr) {
			*GIsSeekFreePCConsole = originalSeekFreeValue;
		}
		return result;
	}

	tShaderMapIsComplete* FShaderMap_IsComplete_orig = nullptr;
	UBOOL FShaderMap_IsComplete_hook(void* map, FMaterial* material)
	{
		FString name;
		GetFriendlyNameVirtual(material, &name);

		auto result = FShaderMap_IsComplete_orig(map, material);

		if (IsTerrainMaterialResource(material))
		{
			LEASI_INFO(L"Terrain shader-map completeness: resource={}, map={}, result={}",
				name.Chars(),
				map,
				result != FALSE ? L"complete" : L"incomplete");
		}
		if (result == 0) {
			LEASI_WARN(L"\tShader map incomplete for material {}", name);
		}
		else {
			// LEASI_INFO(L"\tShader map complete for material {}", name);
		}

		return result;
	}

	tTerrainGetCachedMaterial* ATerrain_GetCachedMaterial_orig = nullptr;
	void* ATerrain_GetCachedMaterial_hook(ATerrain* terrain, const void* mask, UBOOL* outIsTerrainResource)
	{
		// Confirmed against LE3 11.3.2 at 0x7ff74b097c70: FTerrainMaterialMask is
		// { UINT64 BitMask; INT NumBits; }, and a successful return is the terrain
		// resource's FMaterialRenderProxy base at +0xB4.
		struct FTerrainMaterialMaskRuntime
		{
			UINT64 BitMask;
			INT NumBits;
		};

		auto* result = ATerrain_GetCachedMaterial_orig(terrain, mask, outIsTerrainResource);
		const auto* requestedMask = reinterpret_cast<const FTerrainMaterialMaskRuntime*>(mask);
		const bool selectedTerrainResource = outIsTerrainResource != nullptr && *outIsTerrainResource != FALSE;
		auto* selectedResource = selectedTerrainResource
			? reinterpret_cast<FMaterial*>(reinterpret_cast<BYTE*>(result) - 0xB4)
			: nullptr;
		FString terrainPath = terrain == nullptr ? FString(L"<null terrain>") : terrain->GetFullPath();

		LEASI_INFO(L"Terrain material selection: terrain={}, requested-mask={:X}/{} bits, result={}, terrain-resource={}, resource={}, shader-map={}",
			terrainPath.Chars(),
			requestedMask == nullptr ? 0 : requestedMask->BitMask,
			requestedMask == nullptr ? 0 : requestedMask->NumBits,
			result,
			selectedTerrainResource,
			static_cast<void*>(selectedResource),
			selectedResource == nullptr ? nullptr : static_cast<void*>(selectedResource->ShaderMap.Object));

		if (!selectedTerrainResource)
		{
			LEASI_WARN(L"Terrain selected EngineMaterials.TooManyMaterials fallback: no cached resource with this mask had a usable shader map.");
		}
		return result;
	}

	UBOOL FMaterialCompile_hook(FMaterialResource* fmaterial, FStaticParameterSet* parameterSet, EShaderPlatform shaderplatform, FMaterialShaderMap** shaderMap, UBOOL forceCompile, UBOOL bCompileFallback, UBOOL debugDump)
	{
		// FTerrainMaterialResource has no `Material` (UMaterial*) field -- ->Material on one of these
		// is garbage. Every generic material below the FMaterial interface still works on it, but the
		// three usage flags below are UMaterial-only fields, and the logging needs a name that doesn't
		// dereference ->Material. See IsTerrainMaterialResource() in Hooks.hpp for why.
		bool isTerrain = IsTerrainMaterialResource(fmaterial);

		if (!isTerrain) {
			// Add these as they don't appear to be reliably set from our decooked stuff.
			fmaterial->Material->bUsedWithStaticLighting = true;
			fmaterial->Material->bUsedWithVectorLightMaps = true;
			fmaterial->Material->bUsedWithSimpleLightMaps = true;
		}
		// Terrain doesn't need the equivalent nudge: FTerrainMaterialResource::IsUsedWithStaticLighting()
		// is hardcoded to always return TRUE already, and there's no single UMaterial to poke for the
		// lightmap flags -- if terrain-specific lightmap shader variants turn out to be missing, that's
		// the next thing to look at (see the matching TODO in MilanShaderCompiler.cpp's AddCustomMacros).

		FString friendlyName;
		GetFriendlyNameVirtual(fmaterial, &friendlyName);

		auto result = FMaterialCompile_orig(fmaterial, parameterSet, shaderplatform, shaderMap, forceCompile, bCompileFallback, debugDump);
		if (result) {
			// Successfully generated material shader
			LEASI_INFO(L"Material compiled successfully: {}", friendlyName.Chars());
			if (isTerrain && shaderMap != nullptr && *shaderMap != nullptr)
			{
				const auto& savedExpressions = (*shaderMap)->UniformExpressionSet;
				LEASI_INFO(L"Terrain shader-map expressions at save: pixel2D={}, pixelCube={}, vertexVector={}, vertexScalar={}",
					savedExpressions.Uniform2DTextureExpressions.Count(),
					savedExpressions.UniformCubeTextureExpressions.Count(),
					savedExpressions.UniformVertexVectorExpressions.Count(),
					savedExpressions.UniformVertexScalarExpressions.Count());
			}

			SFXName scName(L"TestShaderCache", 0);
			auto transient = (*GObjTransientPackage)->Class->GetName();
			auto shaderCache = StaticConstructObject<UShaderCache>(*GObjTransientPackage, scName);
			shaderCache->ShaderCache.Platform = SP_SM5;

			// Add to shader cache shader map
			shaderCache->MaterialShaderMap.Set((*shaderMap)->StaticParameters, *shaderMap);
			SaveShaderCache(shaderCache);

			// Terrain resources have an extension beyond the common FMaterial base. Serialize it
			// through ATerrain's exact LE3 serializer so ShaderHost can restore the owning Terrain
			// cache by mask; the ordinary material serializer would silently drop that extension.
			if (isTerrain) {
				SaveTerrainMaterialResource(fmaterial);
			}
			else {
				SaveMaterialBinary(fmaterial);
			}
		}
		else if (!CaptureOnly) {
			LEASI_ERROR(L"Material failed to compile: {}", friendlyName.Chars());
			for (UINT i = 0; i < fmaterial->CompileErrors.Count(); i++) {
				LEASI_WARN("  {}", fmaterial->CompileErrors.GetData()[i]);
			}
		}

		UpdateCurrentAction(L"");
		return result;
	}

	UBOOL FMaterialShaderMap_Compile_hook(FMaterialShaderMap* map, FMaterialResource* material, FStaticParameterSet* InParameters, wchar_t* materialCode, FUniformExpressionSet* InUniformExpressionSet, EShaderPlatform platform, TArray<FString>* outErrors, BOOL bSilent, BOOL bDebugDump)
	{
		LEASI_UNUSED_4(bDebugDump, bSilent, outErrors, platform);

		// Use to dump HLSL for compiled T3D materials
		//SaveMaterialHLSL(material, materialCode);
		//return false;

		// material->Material->GetFullPath() used to be logged here, but ->Material doesn't exist on
		// FTerrainMaterialResource (see IsTerrainMaterialResource() in Hooks.hpp) -- the friendly name
		// is available for both and already computed right below, so just use that for logging too.
		FString name;
		GetFriendlyNameVirtual(material, &name);
		LEASI_INFO(L"Trying to compile material shaders for {}", name.Chars());
		if (IsTerrainMaterialResource(material) && InUniformExpressionSet != nullptr)
		{
			LEASI_INFO(L"Terrain translator expressions: pixel2D={}, pixelCube={}, vertexVector={}, vertexScalar={}",
				InUniformExpressionSet->Uniform2DTextureExpressions.Count(),
				InUniformExpressionSet->UniformCubeTextureExpressions.Count(),
				InUniformExpressionSet->UniformVertexVectorExpressions.Count(),
				InUniformExpressionSet->UniformVertexScalarExpressions.Count());
		}
		UpdateCurrentAction(std::format(L"Compiling {}", name.Chars())); // Use shortname

		std::vector<ShaderCompileCombo> comboList{};
		GetShaderTypesToCompile(material, comboList);

		// Material shaders and mesh-material shaders live in different lookup tables at runtime.
		// Keeping the nullptr-VF group in a mesh map makes Parent.Shaders empty, while omitting
		// VertexFactoryMap leaves mesh shaders unreachable through their factory lookup.
		std::vector<FShader*> materialShaders{};
		std::map<FVertexFactoryType*, std::vector<FShader*>> shadersByVF{};

		if (PRINT_DEBUG_INFO) {
			LEASI_INFO(L"  Compiling shaders:");
		}
		bool compiledSuccessfully = true;
		for (auto& var : comboList)
		{
			std::vector<FShader*>& shaderList = var.vertexFactoryType != nullptr
				? shadersByVF[var.vertexFactoryType]
				: materialShaders;
			var.name = name;
			if (PRINT_DEBUG_INFO) {
				if (var.vertexFactoryType != nullptr) {
					LEASI_INFO(L"    {}->{}", var.vertexFactoryType->Name, var.shaderType->Name);
				}
				else {
					LEASI_INFO(L"    {}", var.shaderType->Name);
				}
			}

			FShaderCompilerEnvironment env{};
			env.materialHlslString = materialCode;
			env.UniformExpressions = InUniformExpressionSet;
			var.shaderType->ModifyCompilationEnvironmentRef(SP_SM5, &env);
			if (var.vertexFactoryType != nullptr) {
				var.vertexFactoryType->ModifyCompilationEnvironmentRef(SP_SM5, &env);
			}

			if (CaptureOnly) {
				// Record the job and move on. No shader is built, so no shader map is either.
				CaptureShaderJob(material, &var, env);
				continue;
			}

			auto compileOK = CompileSingleShader(material, &var, env, &shaderList);
			if (!compileOK) {
				if (var.vertexFactoryType != nullptr) {
					LEASI_ERROR(L"    Compile failed: {}->{}", var.vertexFactoryType->Name, var.shaderType->Name);
				}
				else {
					LEASI_ERROR(L"    Compile failed: {}", var.shaderType->Name);
				}
			}

			compiledSuccessfully &= compileOK;
		}

		if (CaptureOnly) {
			// Nothing was compiled, so there is no shader map to build. Reporting failure keeps
			// the caller from saving a shader cache full of nothing.
			LEASI_INFO(L"  Captured {} shader jobs for {}", (int)comboList.size(), name.Chars());
			return FALSE;
		}

		// Non-mesh shader types are resolved from FMaterialShaderMap's parent FShaderMap.
		for (auto&& shader : materialShaders) {
			map->Parent.Shaders.Set(shader->Type, shader);
		}

		// Mesh shader types are resolved through both the owned map array and the vertex-factory index.
		for (auto& [type, list] : shadersByVF) {
			FMeshMaterialShaderMap* fmmsm = FMeshMaterialShaderMap::Allocate();
			fmmsm->InitCount = 0;
			fmmsm->VertexFactoryType = type;
			for (auto&& shader : list) {
				// Add the shader to the map
				fmmsm->Shaders.Set(shader->Type, shader);
			}
			map->MeshShaderMaps.Add(fmmsm);
			auto vertexFactoryType = type; // TMap::Set takes its key by mutable reference.
			map->VertexFactoryMap.Set(vertexFactoryType, fmmsm);
		}


		// Set some parameters
		map->FriendlyName = name;
		map->Platform = SP_SM5;
		map->StaticParameters = *InParameters;
		map->MaterialId = material->MaterialGuid;
		// Setting parameters causes a crash
		// Decomp seems to indicate that it's checking if it exists
		// and handling it differently
		// not setting it makes it pass it in by itself
		//map->UniformExpressionSet = *InUniformExpressionSet;

		return compiledSuccessfully;
	}

	FString* GetTlkString_hook(void* gTalkTable, int id, FString* outStr, int something, int somethingElse) {
		// This causes breakpoint in debug mode,
		// and seems to crash game if no debugger,
		// is it cause our wchar_t* is not GMallocd?
		if (id == 347396) { // Window Title
			*outStr = L"Milan Shader Compiler";
			return outStr;
		}
		if (id == 1) {
			*outStr = L"Shader compile in progress...";
			return outStr;
		}
		else if (id == 170447) { // Loading widget Loading
			if (currentAction.empty()) {
				return GetTlkString_orig(gTalkTable, id, outStr, something, somethingElse);
			}

			FString message = currentAction.c_str();
			*outStr = message;
			return outStr;
		}
		else {
			return GetTlkString_orig(gTalkTable, id, outStr, something, somethingElse);
		}
	}

	// We cache this on first hook
	/*void* showLoadMessage_parm1 = nullptr;
	void ShowLoadMessage_hook(void* unknown, int param_2, UINT stringId, int someBool) {
		if (showLoadMessage_parm1 == nullptr)
		{
			showLoadMessage_parm1 = unknown;
		}
		ShowLoadMessage_orig(unknown, param_2, stringId, someBool);
	}*/

	void RunConsoleCommand(const FString& command) {
		auto playerController = ::Common::FindFirstObject<APlayerController>();
		if (playerController) {
			playerController->ConsoleCommand(command, 0);
		}
	}

	bool BootShaderCompilerLevel = false;
	bool EnableShaderCompile = true;
	t_UObject_ProcessInternal* UObject_ProcessInternal_orig = nullptr;
	void UObject_ProcessInternal_hook(UObject* const Context, FFrame* const Stack, void* const Result)
	{
		static bool hasRunSplashCheck = false;

		if (!hasRunSplashCheck) {
			FString const FunctionName = Stack->Node->GetFullPath();
			if (FunctionName.Contains(L"BioSeqAct_ShouldShowSplashSequence")) {
				LEASI_INFO(L"ProcessInternal: {}", *FunctionName);
				hasRunSplashCheck = true;

				auto cli = std::wstring(GetCommandLineW());
				if (BootShaderCompilerLevel) {
					LEASI_INFO(L"Loading shader compiler level");
					RunConsoleCommand(L"at shadercompiler");
				}
				else if (cli.find(L"-ShaderPreview") != std::wstring::npos) {
					EnableShaderCompile = false;
					LEASI_INFO(L"Loading shader viewer level");
					RunConsoleCommand(L"at shaderpreview");
				}
			}
		}
		UObject_ProcessInternal_orig(Context, Stack, Result);
	}

	tPlayLoadingMovie* PlayLoadingMovie_orig = nullptr;
	void PlayLoadingMovie_hook(USFXLoadMovieManager* param_1, FString* levelName, int param3) {
		static bool addedTip = false;
		if (levelName != nullptr) {
			if (levelName->Equals(L"shadercompiler", true)) {
				if (!addedTip) {
					param_1->AreaLoadTips.Clear();
					// Doesn't work right now
					FLoadingLevelTip newLoadingTip{};
					newLoadingTip.Tip.GenericTip = 1;
					newLoadingTip.LevelName = FString(L"shadercompiler");
					newLoadingTip.UseRandomTip = false;
					//param_1->AreaLoadTips.Reserve(param_1->AreaLoadTips.Count() + 1); // dies
					param_1->AreaLoadTips.Add(newLoadingTip);
					addedTip = true;
				}
			}
		}

		PlayLoadingMovie_orig(param_1, levelName, param3);
	}

	tReturnBool* IsWindowFocused_orig = nullptr;
	BOOL IsWindowFocused_hook() {
		// This is only hooked if with -ShaderCompile flag.
		return TRUE;
	}
}
