#include "d3dcompiler.h"
#include "Common/Objects.hpp"
#include "Common/json.hpp"
#include "LESDK/Common/FArchive.hpp"
#include "ShaderClient/Client.hpp"
#include "ShaderClient/Hooks.hpp"
#include "ShaderClient/Helpers.hpp"
#include "ShaderClient/PipeHandler.hpp"
#include "ShaderClient/MilanShaderCompiler.hpp"
#include "ShaderClient/MilanShaderCompilerShared.h"

namespace ShaderClient
{
#define ERROR_MESSAGE_BUFFER_SIZE 2048
	BYTE shaderBuffer[4096 * 1024]; // 4MB buffer for shader bytecode
	char errorMessage[ERROR_MESSAGE_BUFFER_SIZE]; // 2KiB bytes for error message.
	NamedParameterAllocation parameterAllocationBuffer[256]; // There better not be more than this...!
	std::wstring currentAction;
	bool SendDebugPackages;
	bool CaptureOnly;

	void UpdateLoadScreenMessage() {
		auto interaction = Common::FindFirstObject<USFXGUIInteraction>();
		if (interaction)
		{
			auto saveLoadWidget = interaction->eventGetSaveLoadWidget();
			if (saveLoadWidget)
			{
				saveLoadWidget->ShowLoadingMessage(FALSE, 170447);
			}
		}
	}

	void UpdateCurrentAction(std::wstring_view newAction) {
		currentAction = newAction;
		UpdateLoadScreenMessage();
	}

	/// <summary>
	/// Returns the wire-protocol identifying name for a material resource: the UMaterial's full
	/// package path for regular materials, or the friendly name for terrain resources, which have
	/// no backing UMaterial (->Material is not a valid field on FTerrainMaterialResource -- see
	/// IsTerrainMaterialResource() in Hooks.hpp for why that dereference is unsafe).
	///
	/// MilanShaderHost currently only knows how to match this name back to a "Material"-class
	/// export (see OnMaterialCompiled in MilanShaderBuilderFlow.cs), so terrain results identified
	/// this way still need matching handling added on the C# side before a round trip works end to
	/// end -- this just keeps the native side from crashing while compiling one.
	/// </summary>
	FString GetMaterialWireName(FMaterialResource* material) {
		if (IsTerrainMaterialResource(material)) {
			FString name;
			GetFriendlyNameVirtual(material, &name);
			return name;
		}
		return material->Material->GetFullPath();
	}

	/// <summary>
	/// Writes sized data to a pipe by first writing the data size, then the data itself.
	/// </summary>
	/// <param name="pipeHandle">The handle to the pipe to write to.</param>
	/// <param name="size">The size in bytes of the data to write.</param>
	/// <param name="data">A pointer to the data buffer to write.</param>
	void WriteSizedData(HANDLE pipeHandle, DWORD size, const void* data) {
		DWORD numBytesWritten = 0;
		WriteFile(pipeHandle, &size, 4, &numBytesWritten, NULL);
		WriteFile(pipeHandle, data, size, &numBytesWritten, NULL);
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
		BYTE cmd_sc = static_cast<BYTE>(ShaderHostCommand::SaveShaderCache);
		// LEASI_DEBUG("Send ShaderCacheCommand");
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

		// Send the shader cache
		WriteSizedData(pipeHandle, Writer.Data.Count(), Writer.Data.GetData());

		/*
			LEASI_DEBUG("Send ShaderCache Size: {}", Writer.Data.Count());
		auto count = Writer.Data.Count();
		WriteFile(pipeHandle, &count, 4, &numBytesWritten, NULL);

		// Write Shader Data to pipe.
		// std::cout << "Send ShaderCache\n";
		LEASI_DEBUG("Send ShaderCache");
		WriteFile(pipeHandle, Writer.Data.GetData(), Writer.Data.Count(), &numBytesWritten, NULL);*/

		// Destroy Writer
		Writer.VTable->Destructor(&Writer);
	}

	void SaveMaterialHLSL(FMaterialResource* material, wchar_t* materialCode) {
		auto materialObjName = GetMaterialWireName(material);

		FString outStr;
		auto name = GetFriendlyNameVirtual(material, &outStr);
		UpdateCurrentAction(std::format(L"Sending {}", name->Chars()));
		LEASI_INFO(L"Send HLSL for {}", materialObjName);
		auto pipeHandle = CreateHostPipe();
		DWORD numBytesWritten = 0;

		// Send the command.
		BYTE cmd_sc = static_cast<BYTE>(ShaderHostCommand::SaveMaterialHLSL);
		WriteFile(pipeHandle, &cmd_sc, 1, &numBytesWritten, NULL);

		auto matNameStrSize = (materialObjName.Length()) * 2;
		WriteSizedData(pipeHandle, matNameStrSize, materialObjName.Chars());

		// Send material name string size to follow
		//WriteFile(pipeHandle, &matNameStrSize, 4, &numBytesWritten, NULL);

		// Write material name string to follow
		//WriteFile(pipeHandle, materialObjName.Chars(), (DWORD)matNameStrSize, &numBytesWritten, NULL);


		// Send HLSL string size to follow
		std::wstring hlslStr(materialCode);
		auto hlslStrSize = (hlslStr.length() + 1) * 2;
		WriteSizedData(pipeHandle, (DWORD)hlslStrSize, hlslStr.data());

		/*
		WriteFile(pipeHandle, &hlslStrSize, 4, &numBytesWritten, NULL);

		// Write HLSL string to pipe.
		WriteFile(pipeHandle, hlslStr.data(), (DWORD)hlslStrSize, &numBytesWritten, NULL);*/
		UpdateCurrentAction(L"");
	}

	void SaveMaterialBinary(FMaterialResource* material) {
		auto materialObjName = GetMaterialWireName(material);

		FString outStr;
		auto name = GetFriendlyNameVirtual(material, &outStr);

		UpdateCurrentAction(std::format(L"Saving {}", name->Chars()));

		auto pipeHandle = CreateHostPipe();
		DWORD numBytesWritten = 0;

		// Send the command.
		BYTE cmd_sc = static_cast<BYTE>(ShaderHostCommand::SaveMaterialBinary);
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

	DWORD GetCompileFlags(ShaderCompileCombo* scc) {
		DWORD flags = D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY | D3DCOMPILE_OPTIMIZATION_LEVEL3;

		auto name = std::wstring(scc->shaderType->Name);

		// Recovered from the RDEF chunk of LE3's own shipping bytecode, which records the flags each
		// shader was built with. Across 14596 shipping shaders and 114 shader types the value is a
		// function of shader type alone, with only two values in the whole game: 0x9000, and 0x9400
		// for every TLightPixelShader* (all 14 of them, uniformly) and nothing else.
		//
		// The previous rules were a substring match on FShadowVertexBufferPolicy, which also caught
		// the TLightVertexShader*FShadowVertexBufferPolicy types that ship as 0x9000, plus a match
		// on TLightPixelShaderFDirectionalLightPolicy that missed the Point, Spot, SFXPoint and
		// SphericalHarmonic light pixel shaders.
		if (name.starts_with(L"TLightPixelShader")) {
			flags |= D3DCOMPILE_PREFER_FLOW_CONTROL;
		}

		// Not present in any material shader map, so we can neither confirm nor refute this
		// one
		if (name.find(L"FBranchingPCFProjectionPixelShader") != std::wstring::npos) {
			flags |= D3DCOMPILE_PREFER_FLOW_CONTROL;
		}

		return flags;
	}

	/// <summary>
	/// Assembles the macro list for a shader exactly as the compile path needs it, and reports how
	/// many entries came from our own rules rather than from the game.
	///
	/// Entries [0, outInjectedCount) are ours (the fixed profile macros plus AddCustomMacros); the
	/// remainder come from the game's own ModifyCompilationEnvironment. Recording that boundary is
	/// what lets the offline harness tell the two apart, which the merged list alone cannot express.
	/// Merge semantics are deliberately unchanged: our rules dedup against each other, and the
	/// engine's macros are appended as-is.
	/// </summary>
	void BuildShaderMacros(FMaterialResource* material, ShaderCompileCombo* scc, const FShaderCompilerEnvironment& env,
		std::vector<D3D_SHADER_MACRO>& Macros, size_t& outInjectedCount, std::vector<MacroConflict>& outConflicts) {

		AddMacro(Macros, "SM5_PROFILE", "1");
		AddMacro(Macros, "SUPPORTS_DEPTH_TEXTURES", "1"); // Always set for SM5
		if (scc->shaderType->Frequency == SF_VERTEX) {
			AddMacro(Macros, "VERTEXSHADER", "1");
		}
		else {
			AddMacro(Macros, "PIXELSHADER", "1");
		}

		if (scc->vertexFactoryType != nullptr) {
			AddMacro(Macros, "NUM_SAMPLES", "1");
		}

		AddCustomMacros(material, *scc, Macros);

		outInjectedCount = Macros.size();

		// Add Macros set by the engine itself. Names we already set win; disagreements are recorded.
		for (auto&& pair : env.macroDefinitions)
		{
			AddMacroW(Macros, pair.Key.Chars(), pair.Value.Chars(), &outConflicts);
		}
	}

	void SendDebugInfoToMilanHost(HANDLE pipeHandle, FMaterialResource* material, ShaderCompileCombo* scc, const FShaderCompilerEnvironment& env, int outShaderSize, const std::vector<D3D_SHADER_MACRO>& Macros, DWORD compileFlags, size_t injectedCount, const std::vector<MacroConflict>& conflicts) {
		// Generate a debug package to ship to ShaderHost
		DWORD numBytesWritten = 0;
		BYTE cmd_sc = static_cast<BYTE>(ShaderHostCommand::SaveDebugInfo);
		WriteFile(pipeHandle, &cmd_sc, 1, &numBytesWritten, NULL);

		// Metadata of package for ShaderHost
		// Material Name
		auto fullPath = GetMaterialWireName(material);
		WriteSizedData(pipeHandle, fullPath.Length() * 2, fullPath.Chars());

		// Build package
		nlohmann::json debugInfo;
		debugInfo["shaderType"] = ws2s(scc->shaderType->Name);
		debugInfo["vertexFactoryType"] = scc->vertexFactoryType != nullptr
			? nlohmann::json(ws2s(scc->vertexFactoryType->Name))
			: nlohmann::json(nullptr);
		debugInfo["vertexFactoryShaderFilename"] = scc->vertexFactoryType != nullptr
			? nlohmann::json(ws2s(scc->vertexFactoryType->ShaderFilename))
			: nlohmann::json(nullptr);
		debugInfo["sourceFilename"] = ws2s(scc->shaderType->SourceFilename);
		debugInfo["functionName"] = ws2s(scc->shaderType->FunctionName);
		debugInfo["materialHlsl"] = ws2s(env.materialHlslString);
		// outShaderSize is -1 when the job was captured without compiling, which must not be
		// confused with a compile that ran and failed.
		debugInfo["compileAttempted"] = outShaderSize >= 0;
		debugInfo["compileSucceeded"] = outShaderSize > 0;
		debugInfo["profile"] = scc->shaderType->Frequency == SF_VERTEX ? "vs" : "ps";

		nlohmann::json macrosArray = nlohmann::json::array();
		size_t index = 0;
		for (const auto& macro : Macros) {
			if (macro.Name == nullptr) break;
			nlohmann::json macroObj;
			macroObj["name"] = macro.Name;
			macroObj["definition"] = macro.Definition;
			// "milan" = one of our rules, "engine" = the game's ModifyCompilationEnvironment.
			macroObj["source"] = index < injectedCount ? "milan" : "engine";
			macrosArray.push_back(macroObj);
			index++;
		}
		debugInfo["macros"] = macrosArray;

		// Needed for the offline harness to reproduce this compile exactly.
		debugInfo["compileFlags"] = (uint32_t)compileFlags;

		// Macros where the engine disagreed with one of our rules. Ours was used.
		nlohmann::json conflictsArray = nlohmann::json::array();
		for (const auto& c : conflicts) {
			nlohmann::json obj;
			obj["name"] = c.Name;
			obj["milanValue"] = c.MilanValue;
			obj["engineValue"] = c.EngineValue;
			conflictsArray.push_back(obj);
		}
		debugInfo["macroConflicts"] = conflictsArray;

		std::string debugJson = debugInfo.dump();

		WriteSizedData(pipeHandle, (DWORD)(debugJson.size()), debugJson.c_str());
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
		size_t injectedCount = 0;
		std::vector<MacroConflict> macroConflicts;
		BuildShaderMacros(material, scc, env, Macros, injectedCount, macroConflicts);

		// Add Terminator
		D3D_SHADER_MACRO finalMacro;
		finalMacro.Name = NULL;
		finalMacro.Definition = NULL;
		Macros.push_back(finalMacro);

		// Compile flags
		DWORD compileFlags = GetCompileFlags(scc);
		// LE3 seems to have ifdef'd this out
		//for (UINT i = 0; i < env.compileFlags.Count(); i++) {
		//	auto flag = env.compileFlags(i);
		//	switch (flag) {
		//	case 0:
		//		// Prefer Flow Control
		//		compileFlags |= D3DCOMPILE_PREFER_FLOW_CONTROL;
		//		break;
		//	case 1:
		//		// Debug
		//		compileFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
		//		break;
		//	case 2:
		//		// Avoid Flow Control
		//		compileFlags |= D3DCOMPILE_AVOID_FLOW_CONTROL;
		//		break;
		//	case 3:
		//		// Skip Validation
		//		// Not implemented
		//		break;
		//	}
		//	compileFlags |= flag;
		//}



		if (MilanCompileShader != nullptr) {
			MilanCompileShader(
				ws2s(scc->shaderType->SourceFilename).c_str(),
				ws2s(env.materialHlslString.c_str()).c_str(),
				scc->vertexFactoryType != nullptr ? ws2s(scc->vertexFactoryType->ShaderFilename).c_str() : NULL, // Filename not name.
				L"../../Engine/Shaders",
				&Macros.front(),
				ws2s(scc->shaderType->FunctionName).c_str(),
				shaderType,
				compileFlags, // compile flags
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
			BYTE cmd_sc = static_cast<BYTE>(ShaderHostCommand::ShaderCompileFailed);
			WriteFile(pipeHandle, &cmd_sc, 1, &numBytesWritten, NULL);

			// The actual error
			char formattedMessage[768 + ERROR_MESSAGE_BUFFER_SIZE]; // might include path which could be long.
			sprintf(formattedMessage, "Material '%s' shader '%s' (VF '%s') failed to compile: %s",
				ws2s(scc->name.Chars()).c_str(),
				ws2s(scc->shaderType->Name).c_str(),
				scc->vertexFactoryType != nullptr ? ws2s(scc->vertexFactoryType->Name).c_str() : "None",
				errorMessage);
			size_t len = strlen(formattedMessage) + 1;

			// Inform ShaderHost of the error.
			WriteFile(pipeHandle, formattedMessage, (DWORD)len, &numBytesWritten, NULL);
			LEASI_INFO("      {}", formattedMessage);
		}

		if (outShaderSize <= 0) {
			if (SendDebugPackages) {
				SendDebugInfoToMilanHost(pipeHandle, material, scc, env, outShaderSize, Macros, compileFlags, injectedCount, macroConflicts);
			}
			return false; // Can't do anything more
		}


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
			parameterMap.ParameterMap.Set(key, value);
		}

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

		// Enable to make debugging much easier
		if (SendDebugPackages)
		{
			SendDebugInfoToMilanHost(pipeHandle, material, scc, env, outShaderSize, Macros, compileFlags, injectedCount, macroConflicts);
		}

		// Add shader to the map
		shaderList->emplace_back(shader);
		return true;
	}

	/// <summary>
	/// Records everything the offline harness needs to reproduce this compile, without compiling.
	///
	/// Capture runs exist to build a calibration corpus, and the compile is both the slow part and
	/// the part whose result we are trying to second-guess. Skipping it also avoids allocating the
	/// FShaders the game cannot free, which is what makes long capture runs survivable.
	/// </summary>
	void CaptureShaderJob(FMaterialResource* material, ShaderCompileCombo* scc, FShaderCompilerEnvironment& env) {
		auto pipeHandle = CreateHostPipe();

		std::vector<D3D_SHADER_MACRO> Macros;
		size_t injectedCount = 0;
		std::vector<MacroConflict> macroConflicts;
		BuildShaderMacros(material, scc, env, Macros, injectedCount, macroConflicts);

		DWORD compileFlags = GetCompileFlags(scc);

		// outShaderSize of 0 would read as a compile failure, so report -1 to mark "not attempted".
		SendDebugInfoToMilanHost(pipeHandle, material, scc, env, -1, Macros, compileFlags, injectedCount, macroConflicts);
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
		// Terrain materials have no non-mesh shader variants -- they're only ever rendered
		// through terrain vertex factories. FTerrainMaterialResource::ShouldCache (Ghidra
		// 0x7ff74b09cb90) unconditionally dereferences the VertexFactoryType* passed to it
		// (to compare its Name against "FTerrainVertexFactory" etc.), so calling it here with
		// NULL -- which is correct for ordinary materials' non-mesh shader types -- crashes.
		if (!IsTerrainMaterialResource(Material))
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