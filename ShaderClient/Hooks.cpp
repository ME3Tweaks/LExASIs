
#include "ShaderClient/Hooks.hpp"
#include "ShaderClient/Helpers.hpp"
#include "ShaderClient/Client.hpp"
#include <map>

namespace ShaderClient
{
	tFGetMaterialFriendlyName* FMaterial_GetFriendlyName = nullptr;
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

	tSerializeFUniformExpressionSet* SerializeFUniformExpressionSet = nullptr;

	FArchiveProxy_NamesAndObjectsAsStrings_VTable NamesAndObjectsAsStrings_VTable;

	tGetTlkString* GetTlkString_orig = nullptr;

	tShowLoadMessage* ShowLoadMessage_orig = nullptr;

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
			auto memy = (FMemoryArchiveBase*)Ar->InnerArchive;
			auto memyOff = memy->Offset;
			DWORD len = (DWORD)val.length();
			Ar->VTable->Serialize(Ar, &len, 4);
			auto memyOff2 = memy->Offset;
			DWORD memyDiff = memyOff2 - memyOff;
			if (memyDiff != 4) {
				LEASI_ERROR("Error - diff not 4");
			}

			// Serialize the string.
			memyOff = memy->Offset;
			Ar->VTable->Serialize(Ar, val.data(), len);
			memyOff2 = memy->Offset;
			memyDiff = memyOff2 - memyOff;
			if (memyDiff != len) {
				LEASI_ERROR("Error ");
			}
		}

		return Ar;
	}

	UBOOL InitShaderMap_hook(FMaterial* fmaterial, void* StaticParameters, EShaderPlatform Platform, int Quality)
	{
		if (MilanCompileShader == nullptr) {
			return InitShaderMap_orig(fmaterial, StaticParameters, Platform, Quality);
		}
		if (GIsSeekFreePCConsole != nullptr) {
			*GIsSeekFreePCConsole = 0; // Tricks game into thinking it can attempt to compile shader
		}
		auto result = InitShaderMap_orig(fmaterial, StaticParameters, Platform, Quality);
		if (result == FALSE)
		{
			FString name;
			FMaterial_GetFriendlyName(fmaterial, &name);
			LEASI_WARN(L"InitShaderMap failed for material {}", name);
		}
		else
		{
			// Breakpoint for debug
			auto sm = fmaterial->ShaderMap;
		}
		if (GIsSeekFreePCConsole != nullptr) {
			*GIsSeekFreePCConsole = 1; // Restore this cause all sorts of other stuff will probably break if we don't.
		}
		return result;
	}

	UBOOL FMaterialCompile_hook(FMaterialResource* fmaterial, FStaticParameterSet* parameterSet, EShaderPlatform shaderplatform, FMaterialShaderMap** shaderMap, UBOOL forceCompile, UBOOL debugDump)
	{
		auto result = FMaterialCompile_orig(fmaterial, parameterSet, shaderplatform, shaderMap, forceCompile, debugDump);
		if (result) {
			// Successfully generated material shader
			LEASI_INFO("Material compiled successfully: {}", fmaterial->Material->Name.GetName());

			SFXName scName(L"TestShaderCache", 0);
			auto transient = (*GObjTransientPackage)->Class->GetName();
			auto shaderCache = StaticConstructObject<UShaderCache>(*GObjTransientPackage, scName);
			shaderCache->ShaderCache.Platform = SP_SM5;

			// Add to shader cache shader map
			shaderCache->MaterialShaderMap.Set((*shaderMap)->StaticParameters, *shaderMap);
			SaveShaderCache(shaderCache);

			// Send the material binary to ShaderHost
			SaveMaterialBinary(fmaterial);
		}
		else {
			LEASI_ERROR("Material failed to compile: {}", fmaterial->Material->Name.GetName());
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

		FString name;
		FMaterial_GetFriendlyName(material, &name);
		LEASI_INFO(L"Trying to compile shader {}", name);
		UpdateCurrentAction(std::format(L"Compiling {}", name.Chars()));

		std::vector<ShaderCompileCombo> comboList{};
		GetShaderTypesToCompile(material, comboList);

		// This becomes the Mesh Shader Map
		std::map<FVertexFactoryType*, std::vector<FShader*>> shadersByVF{};

		LEASI_INFO(L"  Compiling shaders:");
		bool compiledSuccessfully = true;
		for (auto& var : comboList)
		{
			std::vector<FShader*>& shaderList = shadersByVF[var.vertexFactoryType];
			var.name = name;
			if (var.vertexFactoryType != nullptr) {
				LEASI_INFO(L"    {}->{}", var.vertexFactoryType->Name, var.shaderType->Name);
			}
			else {
				LEASI_INFO(L"    {}", var.shaderType->Name);
			}

			FShaderCompilerEnvironment env{};
			env.materialHlslString = materialCode;
			env.UniformExpressions = InUniformExpressionSet;
			var.shaderType->ModifyCompilationEnvironmentRef(SP_SM5, &env);
			if (var.vertexFactoryType != nullptr) {
				var.vertexFactoryType->ModifyCompilationEnvironmentRef(SP_SM5, &env);
			}

			compiledSuccessfully &= CompileSingleShader(material, &var, env, &shaderList);
		}

		// Build the shader map
		for (auto& [type, list] : shadersByVF) {
			// Access key: it->first
				// Access value: it->second
			FMeshMaterialShaderMap* fmmsm = FMeshMaterialShaderMap::Allocate();
			fmmsm->InitCount = 0;
			fmmsm->VertexFactoryType = type;
			for (auto&& shader : list) {
				// Add the shader to the map
				fmmsm->Shaders.Set(shader->Type, shader);
				// Need to do FShader::BeginInit() here.
			}
			map->MeshShaderMaps.Add(fmmsm);
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
	void* showLoadMessage_parm1 = nullptr;
	void ShowLoadMessage_hook(void* unknown, int param_2, UINT stringId, int someBool) {
		if (showLoadMessage_parm1 == nullptr)
		{
			showLoadMessage_parm1 = unknown;
		}
		ShowLoadMessage_orig(unknown, param_2, stringId, someBool);
	}
}