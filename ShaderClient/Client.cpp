#include "d3dcompiler.h"
#include "LESDK/Common/FArchive.hpp"
#include "ShaderClient/Client.hpp"
#include "ShaderClient/Hooks.hpp"
#include "ShaderClient/Helpers.hpp"
#include "ShaderClient/PipeHandler.hpp"
#include "ShaderClient/MilanShaderCompiler.hpp"
#include "ShaderClient/MilanShaderCompilerShared.h"

#define PRINT_DEBUG_INFO false

namespace ShaderClient
{
	BYTE shaderBuffer[4096 * 1024]; // 4MB buffer for shader bytecode
	char errorMessage[512]; // 512 bytes for error message.
	NamedParameterAllocation parameterAllocationBuffer[256]; // There better not be more than this...!
	std::wstring currentAction;

	void UpdateLoadScreenMessage() {
		if (showLoadMessage_parm1 != nullptr && ShowLoadMessage_orig != nullptr) {
			ShowLoadMessage_orig(showLoadMessage_parm1, 1, 170447, 0);
		}
	}

	void UpdateCurrentAction(std::wstring_view newAction) {
		currentAction = newAction;
		UpdateLoadScreenMessage();
	}

	/// <summary>
	/// Saves a ShaderCache export to a string-based archive.
	/// </summary>
	/// <param name="shaderCache"></param>
	void SaveShaderCache(UShaderCache* shaderCache) {
		UpdateCurrentAction(L"Saving Shader Cache");

		auto pipeHandle = CreateHostPipe();
		DWORD numBytesWritten = 0;

		// Send the command.
		int cmd_sc = 1;
		LEASI_DEBUG("Send ShaderCacheCommand");
		WriteFile(pipeHandle, &cmd_sc, 1, &numBytesWritten, NULL);

		// Serialize the shader cache
		// 1. Make memory writer to write shader cache to
		TArray<BYTE> OutputData;
		FMemoryWriter Writer(OutputData);
		SetVTable((void**)&Writer, MemoryArchiveWriterVTableStart);

		// Wrap writer in names-only serializer
		FArchiveProxy ProxyArchive;
		CreateArchiveProxy((FArchiveProxy*)&ProxyArchive, &Writer);
		auto obj = &ProxyArchive;
		auto vtable1 = *((long*)GetVTable((void***)&ProxyArchive));
		SetVTable((void**)&ProxyArchive, StringArchiveProxyVTableStart);
		auto vtable2 = GetVTable((void***)&ProxyArchive);
		LEASI_UNUSED(obj);
		LEASI_UNUSED_2(vtable1, vtable2);
		SerializeUShaderCache(shaderCache, (FArchive*)&ProxyArchive);

		// Send shader cache size to follow
		LEASI_DEBUG("Send ShaderCache Size: {}", Writer.Data.Count());
		auto count = Writer.Data.Count();
		WriteFile(pipeHandle, &count, 4, &numBytesWritten, NULL);

		// Write Shader Data to pipe.
		// std::cout << "Send ShaderCache\n";
		LEASI_DEBUG("Send ShaderCache");
		WriteFile(pipeHandle, Writer.Data.GetData(), Writer.Data.Count(), &numBytesWritten, NULL);

		// Destroy Writer
		Writer.VTable->Destructor(&Writer);
	}

	void SaveMaterialHLSL(FMaterialResource* material, wchar_t* materialCode) {
		auto materialObjName = material->Material->GetFullPath();

		FString outStr;
		auto name = FMaterial_GetFriendlyName(material, &outStr);
		UpdateCurrentAction(std::format(L"Sending {}", name->Chars()));
		LEASI_INFO(L"Send HLSL for {}", materialObjName);
		auto pipeHandle = CreateHostPipe();
		DWORD numBytesWritten = 0;

		// Send the command.
		int cmd_sc = 5;
		WriteFile(pipeHandle, &cmd_sc, 1, &numBytesWritten, NULL);

		// Send material name string size to follow
		auto matNameStrSize = (materialObjName.Length()) * 2;
		WriteFile(pipeHandle, &matNameStrSize, 4, &numBytesWritten, NULL);

		// Write material name string to follow
		WriteFile(pipeHandle, materialObjName.Chars(), (DWORD)matNameStrSize, &numBytesWritten, NULL);


		// Send HLSL string size to follow
		std::wstring hlslStr(materialCode);
		auto hlslStrSize = (hlslStr.length() + 1) * 2;
		WriteFile(pipeHandle, &hlslStrSize, 4, &numBytesWritten, NULL);

		// Write HLSL string to pipe.
		WriteFile(pipeHandle, hlslStr.data(), (DWORD)hlslStrSize, &numBytesWritten, NULL);
		UpdateCurrentAction(L"");
	}

	void SaveMaterialBinary(FMaterialResource* material) {
		auto materialObjName = material->Material->GetFullName();

		FString outStr;
		auto name = FMaterial_GetFriendlyName(material, &outStr);

		UpdateCurrentAction(std::format(L"Saving {}", name->Chars()));

		auto pipeHandle = CreateHostPipe();
		DWORD numBytesWritten = 0;

		// Send the command.
		int cmd_sc = 4;
		WriteFile(pipeHandle, &cmd_sc, 1, &numBytesWritten, NULL);

		// Serialize name of outgoing material.		
		int len = materialObjName.Length() * 2;
		//std::cout << "Send Mat Name Size: " << len << std::endl;
		WriteFile(pipeHandle, &len, 4, &numBytesWritten, NULL);
		WriteFile(pipeHandle, materialObjName.Chars(), len, &numBytesWritten, NULL);


		// Serialize the material binary.
		// 1. Make memory writer to write binary to
		TArray<BYTE> OutputData;
		FMemoryWriter Writer(OutputData);
		SetVTable((void**)&Writer, MemoryArchiveWriterVTableStart);

		// Wrap writer in names-only serializer
		FArchiveProxy ProxyArchive = GetStringWithObjectWriterProxy(Writer);
		*GCookingTarget = CT_WindowsConsole;
		SerializeMaterialResource(material, (FArchive*)&ProxyArchive);
		*GCookingTarget = CT_Unknown;

		// Send material resource size to follow
		// std::cout << "Send MaterialBinary Size: " << Writer.Data.Count << "\n";
		auto count = Writer.Data.Count();
		WriteFile(pipeHandle, &count, 4, &numBytesWritten, NULL);

		// Write material to pipe.
		//std::cout << "Send Cache\n";
		WriteFile(pipeHandle, Writer.Data.GetData(), Writer.Data.Count(), &numBytesWritten, NULL);


		// Destroy Writer
		Writer.VTable->Destructor(&Writer);
	}

	bool CompileSingleShader(FMaterialResource* material, ShaderCompileCombo* scc, FShaderCompilerEnvironment& env, std::vector<FShader*>* shaderList) {		
		auto pipeHandle = CreateHostPipe();

		int outShaderSize = 0;
		int outErrorSize = 0;

		char shaderType[3];
		shaderType[0] = scc->shaderType->Frequency == SF_VERTEX ? 'v' : 'p'; // Vertex or Pixel shader
		shaderType[1] = 's';
		shaderType[2] = '\0';

		std::vector<D3D_SHADER_MACRO> Macros;
		// Setup extra required macros here
		AddMacro(Macros, "SM5_PROFILE", "1");
		if (scc->shaderType->Frequency == SF_VERTEX) {
			AddMacro(Macros, "VERTEXSHADER", "1");
		}
		else {
			AddMacro(Macros, "PIXELSHADER", "1");
		}

		AddCustomMacros(material, *scc, Macros);

		for (auto&& pair : env.macroDefinitions)
		{
			auto key = pair.Key;
			auto value = pair.Value;

			AddMacroW(Macros, key.Chars(), value.Chars());

			//char name[255];
			//char def[10];

			//D3D_SHADER_MACRO macro;
			//wcstombs(name, key.Data, 255);
			//wcstombs(def, value.Data, 10);
			//macro.Name = name;
			//macro.Definition = def;

			//Macros.push_back(macro);
		}

		// Add Terminator
		D3D_SHADER_MACRO finalMacro;
		finalMacro.Name = NULL;
		finalMacro.Definition = NULL;
		Macros.push_back(finalMacro);

		if (MilanCompileShader != nullptr) {
			MilanCompileShader(
				ws2s(scc->shaderType->SourceFilename).c_str(),
				ws2s(env.materialHlslString.c_str()).c_str(),
				scc->vertexFactoryType != nullptr ? ws2s(scc->vertexFactoryType->ShaderFilename).c_str() : NULL,
				L"../../Engine/Shaders",
				&Macros.front(),
				ws2s(scc->shaderType->FunctionName).c_str(),
				shaderType,
				D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY | D3DCOMPILE_OPTIMIZATION_LEVEL3, // compile flags
				outShaderSize,
				shaderBuffer[0],
				outErrorSize,
				errorMessage[0]
			);
		}
		else {
			LEASI_ERROR("Cannot compile shader: MilanCompileShader() is null. Is the dll present?");
		}

		if (outShaderSize > 0) {
			// std::cout << "     Shader compiled successfully; size: " << outShaderSize << " bytes" << std::endl;
		}
		else if (outErrorSize > 0) {
			errorMessage[outErrorSize] = '\0';

			// ShaderCompileFailed command
			DWORD numBytesWritten = 0;
			int cmd_sc = 3;

			// std::cout << "Writing error cmd\n";
			WriteFile(pipeHandle, &cmd_sc, 1, &numBytesWritten, NULL);

			// The actual error
			char formattedMessage[768]; // might include path which could be long.
			sprintf(formattedMessage, "Material '%s' shader '%s' failed to compile: %s",
				ws2s(scc->name.Chars()).c_str(),
				ws2s(scc->shaderType->Name).c_str(),
				errorMessage);
			size_t len = strlen(formattedMessage) + 1;
			// Inform ShaderHost of the error.
			// std::cout << "Writing error message: " << formattedMessage << " \n";
			WriteFile(pipeHandle, formattedMessage, (DWORD)len, &numBytesWritten, NULL);
			LEASI_INFO("      {}", formattedMessage);
		}

		if (outShaderSize <= 0)
			return false; // Can't do anything more

		// Get information about shader.

		int numInstructions = 0;
		int outNumParameters = 0;
		MilanGetShaderInfo(outShaderSize, shaderBuffer, numInstructions, outNumParameters, parameterAllocationBuffer);
		if (PRINT_DEBUG_INFO) {
			LEASI_INFO("      Number of instructions: {}", numInstructions);
			LEASI_INFO("      Parameter bindings: ");

			// Need to figure out why binding func can't find the parameters...
			for each(auto var in parameterAllocationBuffer)
			{
				if (var.Name != nullptr && var.Name[0] != '\0') {
					LEASI_INFO(L"        {}", var.Name);
				}
			}
		}

		auto subType = (FShaderTypeSub*)scc->shaderType;
		FGuid guid = FGuid::MakeRandomGuid();

		// I am not sure what happens if you pass a larger struct than expected in; it is a pointer... so it should be fine?
		VertexCompiledShaderData compiledInfo;

		TArray<BYTE> shaderData(shaderBuffer, outShaderSize);
		compiledInfo.Code = &shaderData;
		compiledInfo.ShaderType = scc->shaderType;
		compiledInfo.Target.Frequency = scc->shaderType->Frequency;
		compiledInfo.Target.Platform = SP_SM5;

		// Mesh materials are Vertex shaders, so they need VertexFactoryType on the struct
		auto isMeshMaterial = scc->shaderType->vtable->GetMeshMaterialShaderType(scc->shaderType) != nullptr;
		if (isMeshMaterial) {
			compiledInfo.VertexFactoryType = scc->vertexFactoryType;
		}

		FShaderParameterMap parameterMap{};
		parameterMap.UniformExpressionSet = env.UniformExpressions; // Copy the environment's expressions

		// Pass in the parameter allocations we got from D3D
		for (int i = 0; i < outNumParameters; i++) {
			FString key = parameterAllocationBuffer[i].Name;
			FParameterAllocation value = parameterAllocationBuffer[i].Allocation;
			/*if (wcscmp(key.Data, L"SceneColorTexture") == 0) {
				int k = 0;
			}*/
			parameterMap.ParameterMap.Set(key, value);
			/*auto found = parameterMap.ParameterMap.Find(key);
			if (found == nullptr) {
				std::wcout << L"IN LOOP TMap.Set(" << i << L") isn't working! Didn't find " << key.Data << L" in the map. Hash: " << GetTypeHash(key) << L"\n";
			}
			else {
				std::wcout << L"IN LOOP OK - Found " << key.Data << L", Hash: " << GetTypeHash(key) << std::endl;
			}*/
		}

		/*for (int i = 0; i < outNumParameters; i++) {
			FString key = parameterAllocationBuffer[i].Name;
			auto found = parameterMap.ParameterMap.Find(key);
			if (found == nullptr) {
				std::wcout << L"TMap.Set(" << i << L") isn't working! Didn't find " << key.Data << L" in the map. Hash: " << GetTypeHash(key) << L"\n";
			}
			else {
				std::wcout << L"OK - Found " << key.Data << L", Hash: " << GetTypeHash(key) << std::endl;
			}
		}*/

		if (PRINT_DEBUG_INFO) {
			for (auto&& kvp : parameterMap.ParameterMap)
			{
				LEASI_INFO("        {}", kvp.Key);
			}
		}

		compiledInfo.ParameterMap = &parameterMap;
		compiledInfo.NumInstructions = numInstructions;

		int unknown1 = 0;
		int unknown2 = 0;
		int unknown3 = 0;
		auto shader = subType->ConstructCompiledRef(&compiledInfo, &unknown1, &unknown2, &unknown3);

		if (shader == nullptr) {
			LEASI_ERROR("      Failed to construct shader from compiled data.");
			return false;
		}

		// std::cout << "      Successfully generated FShader!" << std::endl;

		// Add shader to the map
		shaderList->emplace_back(shader);
		return true;
	}

	void GetShaderTypesToCompile(FMaterial* Material, std::vector<ShaderCompileCombo>& outToCompile) {
		auto shaderAddr = GetShaderTypeList();
		// MESH MATERIALS =====================
		{
			// Determine if we should cache this shader or not.
			// Iterate over vertex factories.
			// In that loop, iterate over shader types.
			// In that loop, check if material wants to compile.
			auto vertexAddr = GetVertexFactoryTypeList();
			auto vertexFactoryList = *vertexAddr;
			FVertexFactoryType* vertexFactoryType = nullptr;
			while (vertexFactoryList != nullptr) {
				// Loop over all vertex factories.
				if (vertexFactoryList->CurrentItem->bUsedWithMaterials) {
					vertexFactoryType = vertexFactoryList->CurrentItem;
					auto shaderList = *shaderAddr;
					while (shaderList != nullptr) {
						// Loop over shader types

						auto shaderType = (FShaderTypeSub*)shaderList->CurrentItem;

						// MeshShaderType
						auto vtable1 = shaderType->vtable;
						auto meshShaderType = vtable1->GetMeshMaterialShaderType(shaderType);
						if (meshShaderType == nullptr) {
							// If mesh shader type is null, we skip this shader type.
							shaderList = shaderList->NextItem;
							continue;
						}

						// Shader Type
						bool shouldCache = shaderType->ShouldCacheRef(SP_SM5, Material, vertexFactoryType);

						// Material
						shouldCache &= Material->VTable->ShouldCache(Material, SP_SM5, shaderType, vertexFactoryType);

						// Vertex Factory
						shouldCache &= vertexFactoryType->ShouldCacheRef(SP_SM5, Material, shaderType);

						if (shouldCache)
						{
							// We should cache this shader type for this material and vertex factory.
							ShaderCompileCombo scc{};
							scc.shaderType = shaderType;
							scc.vertexFactoryType = vertexFactoryType;
							outToCompile.push_back(scc);
							//std::wcout << L"Caching shader with VF " << vertexFactoryType->Name << ", ST: " << shaderList->CurrentItem->Name << std::endl;
						}
						else {
							// We should not cache this shader type for this material and vertex factory.
							//std::wcout << L"  Not caching shader type: " << shaderList->CurrentItem->Name << std::endl;
						}


						// Go to next item.
						shaderList = shaderList->NextItem;
					}
				}
				vertexFactoryList = vertexFactoryList->NextItem;
			}
		}

		// NON-MESH MATERIALS =====================
		{
			auto shaderList = *shaderAddr;
			while (shaderList != nullptr) {
				// Loop over shader types
				auto shaderType = (FShaderTypeSub*)shaderList->CurrentItem;

				// MaterialShaderType
				auto vtable1 = shaderType->vtable;
				// Since we aren't using cpp virtual table we have to emulate it by sending the
				// first parameter
				auto mst2 = vtable1->GetMaterialShaderType(shaderType);

				if (mst2 == nullptr)
				{
					shaderList = shaderList->NextItem;
					continue;
				}

				// Shader Type
				bool shouldCache = shaderType->ShouldCacheRef(SP_SM5, Material, NULL);

				// Material
				shouldCache &= Material->VTable->ShouldCache(Material, SP_SM5, shaderType, NULL);

				if (shouldCache)
				{
					// We should cache this shader type for this material and vertex factory.
					//std::wcout << L"Caching shader: " << shaderType->Name << std::endl;
					//CompileShader(shaderList->CurrentItem, NULL, MaterialName, outStr);
					ShaderCompileCombo scc{};
					scc.shaderType = shaderType;
					outToCompile.push_back(scc);
				}
				else {
					// We should not cache this shader type for this material and vertex factory.
					//std::wcout << L"  Not caching shader type: " << shaderType->Name << std::endl;
				}

				shaderList = shaderList->NextItem;
			}
		}
	}
}