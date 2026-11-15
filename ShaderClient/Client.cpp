#include <atomic>
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
    namespace
    {
#pragma pack(push, 1)
        /// <summary>
        /// The native-only tail of LE3 11.3.2's FTerrainMaterialResource. FMaterial ends directly
        /// before Terrain at 0x1EC; this deliberately models only the serialized fields needed to
        /// reproduce FTerrainMaterialResource::PreSave. Pack it because Terrain itself is not
        /// naturally aligned in the shipping layout.
        /// </summary>
        struct FTerrainMaterialResourceSaveState
        {
            BYTE FMaterialState[0x1EC];
            ATerrain* Terrain;
            UINT64 MaskBitMask;
            INT MaskNumBits;
            TArray<FGuid> MaterialIds;
        };
#pragma pack(pop)

        static_assert(offsetof(FTerrainMaterialResourceSaveState, Terrain) == 0x1EC);
        static_assert(offsetof(FTerrainMaterialResourceSaveState, MaskBitMask) == 0x1F4);
        static_assert(offsetof(FTerrainMaterialResourceSaveState, MaskNumBits) == 0x1FC);
        static_assert(offsetof(FTerrainMaterialResourceSaveState, MaterialIds) == 0x200);

        constexpr FGuid ZeroGuid{ 0, 0, 0, 0 };

        /// <summary>
        /// Mirrors FTerrainMaterialResource::PreSave for the already-compiled runtime resource.
        /// The in-game compile callback happens before package pre-save, so without this ShaderHost
        /// receives an empty MaterialIds array and LE3 invalidates the terrain shader cache on load.
        /// </summary>
        void PopulateTerrainMaterialIds(FMaterial* material, ATerrain* terrain)
        {
            auto& terrainResource = *reinterpret_cast<FTerrainMaterialResourceSaveState*>(material);
            if (terrainResource.Terrain != terrain)
            {
                LEASI_ERROR("Terrain resource owner changed while saving shader output.");
                return;
            }

            if (terrainResource.MaskNumBits < 0 || terrainResource.MaskNumBits > 64)
            {
                LEASI_ERROR("Cannot populate terrain MaterialIds: invalid {}-bit mask.", terrainResource.MaskNumBits);
                return;
            }

            LEASI_INFO(L"Terrain dependency input: mask={:X}/{} bits, weighted-materials={}",
                terrainResource.MaskBitMask, terrainResource.MaskNumBits, terrain->WeightedMaterials.Count());
            terrainResource.MaterialIds.Clear();
            for (INT materialIndex = 0; materialIndex < terrainResource.MaskNumBits; materialIndex++)
            {
                if ((terrainResource.MaskBitMask & (UINT64(1) << materialIndex)) == 0)
                {
                    continue;
                }

                FGuid materialId = ZeroGuid;
                if (materialIndex < static_cast<INT>(terrain->WeightedMaterials.Count()))
                {
                    const auto& weightedMaterial = terrain->WeightedMaterials(materialIndex);
                    if (weightedMaterial.Material != nullptr && weightedMaterial.Material->Material != nullptr)
                    {
                        // GetMaterial resolves a material instance to its concrete UMaterial. Terrain
                        // uses the high-quality resource (MSQ_TERRAIN == MSQ_HIGH == slot 0).
                        auto baseMaterial = weightedMaterial.Material->Material->GetMaterial();
                        auto materialResource = baseMaterial == nullptr
                            ? nullptr
                            : reinterpret_cast<FMaterial*>(baseMaterial->MaterialResources[0]);

                        if (baseMaterial != nullptr)
                        {
                            auto terrainMaterialPath = weightedMaterial.Material->GetFullPath();
                            auto materialInterfacePath = weightedMaterial.Material->Material->GetFullPath();
                            auto baseMaterialPath = baseMaterial->GetFullPath();
                            auto resourceOwner = materialResource == nullptr
                                ? nullptr
                                : reinterpret_cast<FMaterialResource*>(materialResource)->Material;
                            // This is the exact selected material input for the current terrain mask.
                            // Keep full paths: terrain dependencies are often authored in other packages,
                            // and the short terrain-material name alone is not enough to identify a missing graph.
                            LEASI_INFO(L"  Terrain input [{}]: terrain-material={}, interface={}, base={}, expressions={}, terrain-resource={}, owner-matches={}",
                                materialIndex,
                                terrainMaterialPath.Chars(),
                                materialInterfacePath.Chars(),
                                baseMaterialPath.Chars(),
                                baseMaterial->Expressions.Count(),
                                materialResource != nullptr,
                                resourceOwner == baseMaterial);
                        }
                        if (materialResource != nullptr)
                        {
                            materialId = materialResource->MaterialGuid;
                        }
                    }
                }
                LEASI_INFO(L"  Terrain dependency {}: {:08X}-{:08X}-{:08X}-{:08X}", materialIndex,
                    materialId.A, materialId.B, materialId.C, materialId.D);
                terrainResource.MaterialIds.Add(materialId);
            }

            LEASI_INFO(L"Terrain resource has {} dependency IDs for mask {:X}.",
                terrainResource.MaterialIds.Count(), terrainResource.MaskBitMask);
        }
    }

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
	/// Returns an identifier for diagnostics and legacy material/HLSL messages: the UMaterial's full
	/// package path for regular materials, or a virtual friendly name for terrain resources, which
	/// have no backing UMaterial (->Material is not a valid field on FTerrainMaterialResource -- see
	/// IsTerrainMaterialResource() in Hooks.hpp for why that dereference is unsafe). Persisted terrain
	/// resources use SaveTerrainMaterialResource's owner-terrain path instead.
	/// </summary>
	FStaticParameterSet const* GCurrentStaticParameters = nullptr;

	int CompileIdleExitSeconds = 120;

	namespace
	{
		// Wall-clock tick of the last compile activity, and whether any has happened at all. Both are
		// atomic: the watchdog runs on its own thread while compiling happens on the game's.
		std::atomic<ULONGLONG> GLastCompileActivityTick{ 0 };
		std::atomic<bool> GAnyCompileSeen{ false };
		std::atomic<bool> GCompileWatchdogStop{ false };
		HANDLE GCompileWatchdogHandle = nullptr;

		DWORD WINAPI CompileIdleWatchdogThread(LPVOID)
		{
			constexpr DWORD PollIntervalMs = 250;
			const ULONGLONG idleLimitMs = static_cast<ULONGLONG>(CompileIdleExitSeconds) * 1000ULL;

			while (!GCompileWatchdogStop.load())
			{
				Sleep(PollIntervalMs);

				// Never exit before the first compile: level load and streaming can take a while, and
				// leaving during it would look exactly like a successful empty capture.
				if (!GAnyCompileSeen.load())
				{
					continue;
				}

				const ULONGLONG last = GLastCompileActivityTick.load();
				if (last == 0 || GetTickCount64() - last < idleLimitMs)
				{
					continue;
				}

				LEASI_INFO(L"Shader compiling idle for {}s; exiting.", CompileIdleExitSeconds);

				// TerminateProcess rather than a graceful shutdown: this process exists only to compile,
				// and the engine's own teardown is what produced the 0xC0000005 exit codes the host
				// reported as crashes. Everything the host needs has already been written to the pipe.
				// Flush first, or the kill discards the info lines buffered since the last periodic flush.
				LEASI_FLUSH();
				TerminateProcess(GetCurrentProcess(), k_shaderClientIdleExitCode);
				return 0;
			}
			return 0;
		}
	}

	void NotifyCompileActivity()
	{
		GAnyCompileSeen.store(true);
		GLastCompileActivityTick.store(GetTickCount64());
	}

	void StartCompileIdleWatchdog()
	{
		if (CompileIdleExitSeconds <= 0)
		{
			LEASI_INFO(L"Auto-exit watchdog disabled.");
			return;
		}
		GCompileWatchdogStop.store(false);
		GCompileWatchdogHandle = CreateThread(nullptr, 0, CompileIdleWatchdogThread, nullptr, 0, nullptr);
		LEASI_INFO(L"Auto-exit watchdog started: will exit after {}s idle once compiling begins.", CompileIdleExitSeconds);
	}

	void StopCompileIdleWatchdog()
	{
		GCompileWatchdogStop.store(true);
		if (GCompileWatchdogHandle != nullptr)
		{
			WaitForSingleObject(GCompileWatchdogHandle, 1000);
			CloseHandle(GCompileWatchdogHandle);
			GCompileWatchdogHandle = nullptr;
		}
	}

	namespace
	{
		// FNV-1a 64. Deliberately hand-rolled: the host has to reproduce this byte for byte in C#,
		// and anything from a library would make the two implementations drift.
		constexpr QWORD k_fnvOffsetBasis = 0xcbf29ce484222325ULL;
		constexpr QWORD k_fnvPrime = 0x100000001b3ULL;

		inline void HashByte(QWORD& hash, BYTE value)
		{
			hash ^= value;
			hash *= k_fnvPrime;
		}

		/// Hashes a name as its UTF-16 code units, low byte first, then a NUL separator. Not UTF-8:
		/// this has to match C# exactly without either side depending on an encoder. SFXName's
		/// k_formatInstanced matches LEC's NameReference.Instanced, so the two agree on the number
		/// suffix as well.
		void HashName(QWORD& hash, SFXName const& name)
		{
			FString const text = name.ToString(SFXName::k_formatInstanced);
			auto const* chars = text.Chars();
			for (decltype(text.Length()) i = 0; i < text.Length(); ++i)
			{
				HashByte(hash, static_cast<BYTE>(chars[i] & 0xFF));
				HashByte(hash, static_cast<BYTE>((chars[i] >> 8) & 0xFF));
			}
			HashByte(hash, 0);
		}

		/// True when the instance actually diverges from its parent. The parameter set carries the
		/// parent's whole parameter list, and only the entries flagged Override are the instance's
		/// own; a set with none of them resolves to the parent's shader map, so it must stay on the
		/// parent's path -- that is also what keeps plain UMaterial captures at their existing paths.
		bool HasAnyOverride(FStaticParameterSet const* parameters)
		{
			if (parameters == nullptr) return false;
			for (auto const& p : parameters->StaticSwitchParameters) { if (p.Override) return true; }
			for (auto const& p : parameters->StaticComponentMaskParameters) { if (p.Override) return true; }
			for (auto const& p : parameters->NormalParameters) { if (p.Override) return true; }
			return false;
		}
	}

	std::wstring StaticPermutationSuffix(FStaticParameterSet const* parameters)
	{
		if (!HasAnyOverride(parameters)) return {};

		// BaseMaterialId and the per-parameter ExpressionGUIDs are deliberately left out: the parent
		// material's path is already in the directory name, and FGuid's byte order across the C++/C#
		// boundary is one more thing that could silently disagree. Names plus values identify the
		// permutation within a parent, which is all the key has to do. Override flags are excluded
		// too -- two instances with identical values share one shader map however they are flagged.
		QWORD hash = k_fnvOffsetBasis;
		for (auto const& p : parameters->StaticSwitchParameters)
		{
			HashByte(hash, 'S');
			HashName(hash, p.Name);
			HashByte(hash, p.Value ? 1 : 0);
		}
		for (auto const& p : parameters->StaticComponentMaskParameters)
		{
			HashByte(hash, 'M');
			HashName(hash, p.Name);
			HashByte(hash, p.R ? 1 : 0);
			HashByte(hash, p.G ? 1 : 0);
			HashByte(hash, p.B ? 1 : 0);
			HashByte(hash, p.A ? 1 : 0);
		}
		for (auto const& p : parameters->NormalParameters)
		{
			HashByte(hash, 'N');
			HashName(hash, p.Name);
			HashByte(hash, p.CompressionSettings);
		}

		return std::format(L"#{:016x}", hash);
	}

	FString GetMaterialWireName(FMaterialResource* material) {
		if (IsTerrainMaterialResource(material)) {
			FString name;
			GetFriendlyNameVirtual(material, &name);
			return name;
		}
		// Instance permutations reach here with ->Material pointing at the parent UMaterial, so the
		// suffix is what keeps them from landing on -- and overwriting -- the parent's own capture.
		return FString(std::format(L"{}{}", material->Material->GetFullPath().Chars(),
			StaticPermutationSuffix(GCurrentStaticParameters)).c_str());
	}

	/// <summary>
	/// Writes sized data to a pipe by first writing the data size, then the data itself.
	/// </summary>
	/// <param name="pipeHandle">The handle to the pipe to write to.</param>
	/// <param name="size">The size in bytes of the data to write.</param>
	/// <param name="data">A pointer to the data buffer to write.</param>
	void SendShaderCompileFailure(const std::string& message) {
		auto pipeHandle = CreateHostPipe();
		std::string payload(1, static_cast<char>(ShaderHostCommand::ShaderCompileFailed));
		payload += message;
		payload.push_back('\0');
		DWORD written = 0;
		if (!WriteFile(pipeHandle, payload.data(), static_cast<DWORD>(payload.size()), &written, nullptr) || written != payload.size()) {
			LEASI_ERROR("Could not forward shader failure to host (Win32 error {})", GetLastError());
		}
	}

	void SendMaterialCompileFailure(FMaterialResource* material) {
		std::string message = "Material '" + ws2s(GetMaterialWireName(material).Chars()) + "' failed to compile";
		if (material->CompileErrors.Count() == 0) {
			message += ": native compiler returned failure without diagnostics.";
		}
		for (UINT i = 0; i < material->CompileErrors.Count(); ++i) {
			message += "\n  " + ws2s(material->CompileErrors.GetData()[i].Chars());
		}
		SendShaderCompileFailure(message);
	}

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

	/// <summary>
	/// Serializes a complete LE3 FTerrainMaterialResource. The resource is created by ATerrain and
	/// has no backing UMaterial, so the ordinary FMaterialResource serializer is insufficient: it
	/// omits the terrain owner, layer mask, dependency material IDs and lighting GUID that make the
	/// cache entry valid on a later load.
	/// </summary>
	void SaveTerrainMaterialResource(FMaterialResource* material) {
		// Verified against LE3 11.3.2's FTerrainMaterialResource::GetFriendlyName and the exact
		// serializer called by ATerrain::Serialize: Terrain is the UObject* at offset 0x1EC. Keep
		// this build-specific layout fact isolated here; every other part of the protocol uses
		// serialized data rather than reaching into the native resource.
		constexpr size_t TerrainOwnerOffset = 0x1EC;
		auto terrain = *reinterpret_cast<ATerrain**>(reinterpret_cast<BYTE*>(material) + TerrainOwnerOffset);
		if (terrain == nullptr) {
			LEASI_ERROR("Cannot send terrain material resource: owner terrain is null.");
			return;
		}

        PopulateTerrainMaterialIds(material, terrain);

		auto terrainPath = terrain->GetFullPath();
		UpdateCurrentAction(std::format(L"Saving terrain resource for {}", terrainPath.Chars()));

		auto pipeHandle = CreateHostPipe();
		DWORD numBytesWritten = 0;
		BYTE command = static_cast<BYTE>(ShaderHostCommand::SaveTerrainMaterialResource);
		WriteFile(pipeHandle, &command, 1, &numBytesWritten, NULL);

		const int terrainPathSize = terrainPath.Length() * sizeof(wchar_t);
		WriteFile(pipeHandle, &terrainPathSize, sizeof(terrainPathSize), &numBytesWritten, NULL);
		WriteFile(pipeHandle, terrainPath.Chars(), terrainPathSize, &numBytesWritten, NULL);

		TArray<BYTE> outputData;
		FMemoryWriter writer(outputData);
		SetVTable((void**)&writer, MemoryArchiveWriterVTableStart);
		FArchiveProxy proxyArchive = GetStringWithObjectWriterProxy(writer);

		*GCookingTarget = CT_WindowsConsole;
		SerializeTerrainMaterialResource((FArchive*)&proxyArchive, material);
		*GCookingTarget = CT_Unknown;

		const auto byteCount = writer.Data.Count();
		WriteFile(pipeHandle, &byteCount, sizeof(byteCount), &numBytesWritten, NULL);
		WriteFile(pipeHandle, writer.Data.GetData(), byteCount, &numBytesWritten, NULL);
		writer.VTable->Destructor(&writer);
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
		// ConstructCompiledRef reads this payload synchronously. Zero every field first so
		// non-mesh shaders never inherit a stale VertexFactoryType/Unknown pointer from the stack.
		VertexCompiledShaderData compiledInfo{};

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
		const bool isTerrainMaterial = IsTerrainMaterialResource(Material);
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
			auto* currentVertexFactory = vertexFactoryList->CurrentItem;
			// Terrain's morph/decal factories are not consistently marked bUsedWithMaterials in
			// LE3's global registry, despite being valid FTerrainMaterialResource::ShouldCache
			// candidates. Filtering them here produced a map containing only
			// FTerrainVertexFactory; terrain components that select a morph factory subsequently
			// render the rainbow fallback. Keep the generic gate for normal materials, but submit
			// every FTerrain* factory to LE3's own three ShouldCache predicates for terrain.
			const bool isTerrainVertexFactory = isTerrainMaterial
				&& currentVertexFactory != nullptr
				&& currentVertexFactory->Name != nullptr
				&& wcsstr(currentVertexFactory->Name, L"FTerrain") == currentVertexFactory->Name;
			if (currentVertexFactory->bUsedWithMaterials || isTerrainVertexFactory) {
				vertexFactoryType = currentVertexFactory;
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
