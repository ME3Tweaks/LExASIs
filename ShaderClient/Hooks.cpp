
#include "ShaderClient/Hooks.hpp"
#include "ShaderClient/Helpers.hpp"
#include "ShaderClient/Client.hpp"
#include "Common/Objects.hpp"
#include "LESDK/Headers.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <format>
#include <intrin.h>
#include <map>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_set>

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
	bool TraceRvrMaterialResolution = false;
	bool TraceMaterialTranslation = false;
	std::wstring MaterialTraceFilter = L"HMF_HED_PROMiranda_SCALP_MAT_USER.CombinedEffect_";
	int ShaderProgressDone = -1;
	int ShaderProgressTotal = -1;
	std::wstring CurrentPackageName;

	// Built once by InitializeHooks and then only read. GetTlkString is a very hot path -- id 1 is a
	// sentinel the game asks for constantly, not just when the loading screen draws -- so this must
	// not format or allocate per call. Doing so is enough to stall the game into a black screen while
	// it still makes progress underneath, which is exactly what happened when this was built inline.
	std::wstring ShaderProgressMessage = L"Shader compile in progress...";

	void RebuildShaderProgressMessage()
	{
		ShaderProgressMessage = BuildShaderProgressMessage();
	}

	std::wstring BuildShaderProgressMessage()
	{
		std::wstring message = L"Shader compile in progress";

		// Both counts are required for the counter: one on its own says nothing useful, and printing
		// "[11/-1]" on the loading screen would be worse than printing no counter at all.
		if (ShaderProgressDone >= 0 && ShaderProgressTotal > 0)
		{
			// Parentheses, not square brackets. A bracketed token in a string returned from
			// GetTlkString sends the game's own string handling into an infinite loop on the UI
			// thread: the loading screen appears and then freezes, while shader compilation keeps
			// running on its own thread and the process never exits cleanly. Verified by bisection --
			// "Shader compile in progress [1/1]..." hangs, "(1/1)" does not, with nothing else
			// changed. Do not reintroduce [ ] here.
			message += std::format(L" ({}/{})", ShaderProgressDone, ShaderProgressTotal);
		}

		message += L"...";

		if (!CurrentPackageName.empty())
		{
			message += std::format(L"\nCurrently compiling {}", CurrentPackageName);
		}

		return message;
	}

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

	tRvrGetMaterial* RvrEffectsMaterialUser_GetMaterial_orig = nullptr;
	tRvrAllocateResource* RvrEffectsMaterialUser_AllocateResource_orig = nullptr;
	tRvrGetMaterial* RvrMaterialMultiplexor_GetMaterial_orig = nullptr;
	tRvrAllocateResource* RvrMaterialMultiplexor_AllocateResource_orig = nullptr;
	tUMaterialSelectResource* UMaterial_SelectResource_orig = nullptr;
	tUnpackTexture* UnpackTexture_orig = nullptr;
	tNormalTextureSampleCompile* NormalTextureSampleCompile_orig = nullptr;

	namespace
	{
		enum class ERvrTraceEventKind : BYTE
		{
			EffectsGetMaterial,
			EffectsAllocateResource,
			MultiplexorGetMaterial,
			MultiplexorAllocateResource,
			UMaterialSelectResource,
		};

		struct FRvrTraceEvent
		{
			ERvrTraceEventKind Kind{};
			void* Self = nullptr;
			void* Context = nullptr;
			INT Selector = 0;
			void* Parent = nullptr;
			void* Base = nullptr;
			void* Multiplexor = nullptr;
			void* DefaultMaterial = nullptr;
			void* Returned = nullptr;
			void* ResourceOwner = nullptr;
			INT ParentCount = -1;
			DWORD ParentNameIndex = 0;
			DWORD ParentNameNumber = 0;
			DWORD ResourceIdA = 0;
			DWORD ResourceIdB = 0;
			DWORD ResourceIdC = 0;
			DWORD ResourceIdD = 0;
		};

		// Rvr allocation and FMaterialShaderMap::Compile are on different game threads. Keep a
		// bounded global history rather than a thread-local one, then correlate by the exact
		// returned FMaterialResource pointer at the compiler boundary. No allocation occurs here.
		constexpr size_t RvrTraceCapacity = 4096;
		std::array<FRvrTraceEvent, RvrTraceCapacity> GRvrTraceEvents{};
		size_t GRvrTraceNext = 0;
		size_t GRvrTraceCount = 0;
		SRWLOCK GRvrTraceLock = SRWLOCK_INIT;

		// FMaterial::Compile invokes material translation synchronously. Keeping this thread-local
		// prevents the UnpackTexture hook from turning a package capture into a game-wide texture
		// trace, while avoiding a cross-thread global state race.
		thread_local UINT GMaterialTranslationTraceDepth = 0;

		bool IsReadableRange(const void* address, size_t size)
		{
			if (address == nullptr || size == 0)
			{
				return false;
			}

			MEMORY_BASIC_INFORMATION memoryInfo{};
			if (VirtualQuery(address, &memoryInfo, sizeof(memoryInfo)) == 0 || memoryInfo.State != MEM_COMMIT)
			{
				return false;
			}

			const auto addressValue = reinterpret_cast<uintptr_t>(address);
			const auto regionStart = reinterpret_cast<uintptr_t>(memoryInfo.BaseAddress);
			const auto regionEnd = regionStart + memoryInfo.RegionSize;
			if (addressValue < regionStart || addressValue > regionEnd || size > regionEnd - addressValue)
			{
				return false;
			}

			return (memoryInfo.Protect & (PAGE_NOACCESS | PAGE_GUARD)) == 0;
		}

		template <typename T>
		bool TryReadMember(const void* object, size_t offset, T& value)
		{
			if (object == nullptr || reinterpret_cast<uintptr_t>(object) > UINTPTR_MAX - offset)
			{
				return false;
			}

			const auto address = reinterpret_cast<const void*>(reinterpret_cast<uintptr_t>(object) + offset);
			if (!IsReadableRange(address, sizeof(T)))
			{
				return false;
			}

			std::memcpy(&value, address, sizeof(T));
			return true;
		}

		void RecordRvrTraceEvent(const FRvrTraceEvent& event)
		{
			AcquireSRWLockExclusive(&GRvrTraceLock);
			GRvrTraceEvents[GRvrTraceNext] = event;
			GRvrTraceNext = (GRvrTraceNext + 1) % GRvrTraceEvents.size();
			GRvrTraceCount = std::min(GRvrTraceCount + 1, GRvrTraceEvents.size());
			ReleaseSRWLockExclusive(&GRvrTraceLock);
		}

		void CaptureEffectsRouting(UMaterialInterface* self, FRvrTraceEvent& event)
		{
			// URvrEffectsMaterialUser, LE3 11.3.2: base +0xBC, multiplexor +0xCC,
			// parent +0xD4. GetMaterial selects parent, then base, then Engine default.
			TryReadMember(self, 0xBC, event.Base);
			TryReadMember(self, 0xCC, event.Multiplexor);
			TryReadMember(self, 0xD4, event.Parent);
		}

		void CaptureMultiplexorRouting(UMaterialInterface* self, FRvrTraceEvent& event)
		{
			// URvrMaterialMultiplexor, LE3 11.3.2: tagged-parent array count +0xBC,
			// parent-name FName +0xC4, default material +0xCC.
			TryReadMember(self, 0xBC, event.ParentCount);
			TryReadMember(self, 0xC4, event.ParentNameIndex);
			TryReadMember(self, 0xC8, event.ParentNameNumber);
			TryReadMember(self, 0xCC, event.DefaultMaterial);
		}

		void CaptureResourceOwner(FMaterialResource* resource, FRvrTraceEvent& event)
		{
			// Ordinary FMaterialResource's source UMaterial owner and shader-map ID are at
			// +0xB8 and +0x7C respectively in LE3 11.3.2. Do not call this for terrain.
			TryReadMember(resource, 0xB8, event.ResourceOwner);
			TryReadMember(resource, 0x7C, event.ResourceIdA);
			TryReadMember(resource, 0x80, event.ResourceIdB);
			TryReadMember(resource, 0x84, event.ResourceIdC);
			TryReadMember(resource, 0x88, event.ResourceIdD);
		}

		const wchar_t* RvrTraceKindName(ERvrTraceEventKind kind)
		{
			switch (kind)
			{
			case ERvrTraceEventKind::EffectsGetMaterial: return L"Effects.GetMaterial";
			case ERvrTraceEventKind::EffectsAllocateResource: return L"Effects.AllocateResource";
			case ERvrTraceEventKind::MultiplexorGetMaterial: return L"Multiplexor.GetMaterial";
			case ERvrTraceEventKind::MultiplexorAllocateResource: return L"Multiplexor.AllocateResource";
			case ERvrTraceEventKind::UMaterialSelectResource: return L"UMaterial::VirtFunc_102";
			default: return L"unknown";
			}
		}

		// The normal spdlog file sink is intentionally not relied on for this capture-only
		// diagnostic: automated launches can exit before that shared sink refreshes. This runs only
		// after a material reaches the compiler hook, never in an Rvr detour, so a tiny direct append
		// is safe and gives the experiment a stable, independent output file.
		void AppendRvrTraceLine(const std::wstring& line)
		{
			const auto withNewline = line + L"\r\n";
			OutputDebugStringW(withNewline.c_str());

			CreateDirectoryW(L"Logs", nullptr);
			const auto file = CreateFileW(L"Logs\\RvrMaterialResolution.log", FILE_APPEND_DATA,
				FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
			if (file == INVALID_HANDLE_VALUE)
			{
				return;
			}

			const auto byteCount = static_cast<DWORD>(withNewline.size() * sizeof(wchar_t));
			DWORD bytesWritten = 0;
			WriteFile(file, withNewline.data(), byteCount, &bytesWritten, nullptr);
			CloseHandle(file);
		}

		void AppendMaterialTranslationTraceLine(const std::wstring& line)
		{
			const auto withNewline = line + L"\r\n";
			OutputDebugStringW(withNewline.c_str());

			CreateDirectoryW(L"Logs", nullptr);
			const auto file = CreateFileW(L"Logs\\MaterialTranslationTrace.log", FILE_APPEND_DATA,
				FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
			if (file == INVALID_HANDLE_VALUE)
			{
				return;
			}

			const auto byteCount = static_cast<DWORD>(withNewline.size() * sizeof(wchar_t));
			DWORD bytesWritten = 0;
			WriteFile(file, withNewline.data(), byteCount, &bytesWritten, nullptr);
			CloseHandle(file);
		}
	}

	void WriteRvrMaterialResolutionTraceStatus(const wchar_t* status)
	{
		if (TraceRvrMaterialResolution && status != nullptr)
		{
			AppendRvrTraceLine(std::format(L"Rvr trace status: {}", status));
		}
	}

	void WriteMaterialTranslationTraceStatus(const wchar_t* status)
	{
		if (TraceMaterialTranslation && status != nullptr)
		{
			AppendMaterialTranslationTraceLine(std::format(L"Material-translation trace status: {}", status));
		}
	}

    INT UnpackTexture_hook(FHLSLTranslator* translator, INT sampleCodeIndex,
        void* texture, UINT textureCompressionSettings)
	{
		if (TraceMaterialTranslation && GMaterialTranslationTraceDepth != 0 && texture != nullptr)
		{
			// These are raw guarded reads at the exact addresses native UnpackTexture consumes. Do
			// not call UTexture accessors here: this hook runs in the material translator and needs
			// to remain observational even when an asset is half initialized.
			float unpackMin[4]{};
			float unpackMax[4]{};
			BYTE runtimeCompression = 0;
			void* textureClass = nullptr;
			const bool readMin = TryReadMember(texture, 0x68, unpackMin);
			const bool readMax = TryReadMember(texture, 0x78, unpackMax);
			const bool readRuntimeCompression = TryReadMember(texture, 0xBC, runtimeCompression);
			const bool readClass = TryReadMember(texture, 0x50, textureClass);

			AppendMaterialTranslationTraceLine(std::format(
				L"  UnpackTexture: translator={}, sample-code={}, texture={}, texture-class={}, passed-compression={}, runtime-compression={}, unpack-min=({}, {}, {}, {}), unpack-max=({}, {}, {}, {}), readable(class/min/max/compression)={}/{}/{}/{}",
				static_cast<void*>(translator), sampleCodeIndex, texture, readClass ? textureClass : nullptr,
				textureCompressionSettings,
				readRuntimeCompression ? runtimeCompression : 0xFF,
				unpackMin[0], unpackMin[1], unpackMin[2], unpackMin[3],
				unpackMax[0], unpackMax[1], unpackMax[2], unpackMax[3],
				readClass, readMin, readMax, readRuntimeCompression));
		}

        // UnpackTexture's return is a translator code index consumed immediately by its callers.
        // Keep it explicit rather than tail-calling through a void declaration: losing EAX here
        // corrupts the material expression graph and produces a later access violation.
        return UnpackTexture_orig(translator, sampleCodeIndex, texture, textureCompressionSettings);
    }

	INT __fastcall NormalTextureSampleCompile_hook(void* expression, FHLSLTranslator* translator)
	{
		if (TraceMaterialTranslation && GMaterialTranslationTraceDepth != 0 && expression != nullptr)
		{
			// This is deliberately a raw, pre-call snapshot of the values the normal expression uses
			// at its 0x288F70 compression branch. It distinguishes the InstanceOverride value from the
			// expression texture's own CompressionSettings without changing either path.
			void* expressionVTable = nullptr;
			void* expressionTexture = nullptr;
			void* instanceOverride = nullptr;
			UINT parameterName[2]{};
			BYTE overrideCompression = 0;
			BYTE textureCompression = 0;

			const bool readVTable = TryReadMember(expression, 0x0, expressionVTable);
			const bool readTexture = TryReadMember(expression, 0x80, expressionTexture);
			const bool readParameterName = TryReadMember(expression, 0x9C, parameterName);
			const bool readInstanceOverride = TryReadMember(expression, 0xA4, instanceOverride);
			const bool readOverrideCompression = instanceOverride != nullptr &&
				TryReadMember(instanceOverride, 0x8, overrideCompression);
			const bool readTextureCompression = expressionTexture != nullptr &&
				TryReadMember(expressionTexture, 0xBC, textureCompression);

			AppendMaterialTranslationTraceLine(std::format(
				L"  NormalTextureSample.Compile: expression={}, vtable={}, translator={}, expression-texture={}, parameter-name={}:{} , instance-override={}, override-compression={}, texture-compression={}, readable(vtable/texture/name/override/override-compression/texture-compression)={}/{}/{}/{}/{}/{}",
				expression, readVTable ? expressionVTable : nullptr, static_cast<void*>(translator),
				readTexture ? expressionTexture : nullptr, parameterName[0], parameterName[1],
				readInstanceOverride ? instanceOverride : nullptr,
				readOverrideCompression ? overrideCompression : 0xFF,
				readTextureCompression ? textureCompression : 0xFF,
				readVTable, readTexture, readParameterName, readInstanceOverride,
				readOverrideCompression, readTextureCompression));
		}

		return NormalTextureSampleCompile_orig(expression, translator);
	}


	UMaterial* RvrEffectsMaterialUser_GetMaterial_hook(UMaterialInterface* self, UMaterialInterface* context, INT selector)
	{
		const auto result = RvrEffectsMaterialUser_GetMaterial_orig(self, context, selector);
		if (!TraceRvrMaterialResolution)
		{
			return result;
		}

		FRvrTraceEvent event{ ERvrTraceEventKind::EffectsGetMaterial, self, context, selector };
		CaptureEffectsRouting(self, event);
		event.Returned = result;
		RecordRvrTraceEvent(event);
		return result;
	}

	FMaterialResource* RvrEffectsMaterialUser_AllocateResource_hook(UMaterialInterface* self)
	{
		const auto result = RvrEffectsMaterialUser_AllocateResource_orig(self);
		if (!TraceRvrMaterialResolution)
		{
			return result;
		}

		FRvrTraceEvent event{ ERvrTraceEventKind::EffectsAllocateResource, self };
		CaptureEffectsRouting(self, event);
		event.Returned = result;
		CaptureResourceOwner(result, event);
		RecordRvrTraceEvent(event);
		return result;
	}

	UMaterial* RvrMaterialMultiplexor_GetMaterial_hook(UMaterialInterface* self, UMaterialInterface* context, INT selector)
	{
		const auto result = RvrMaterialMultiplexor_GetMaterial_orig(self, context, selector);
		if (!TraceRvrMaterialResolution)
		{
			return result;
		}

		FRvrTraceEvent event{ ERvrTraceEventKind::MultiplexorGetMaterial, self, context, selector };
		CaptureMultiplexorRouting(self, event);
		event.Returned = result;
		RecordRvrTraceEvent(event);
		return result;
	}

	FMaterialResource* RvrMaterialMultiplexor_AllocateResource_hook(UMaterialInterface* self)
	{
		const auto result = RvrMaterialMultiplexor_AllocateResource_orig(self);
		if (!TraceRvrMaterialResolution)
		{
			return result;
		}

		FRvrTraceEvent event{ ERvrTraceEventKind::MultiplexorAllocateResource, self };
		CaptureMultiplexorRouting(self, event);
		event.Returned = result;
		CaptureResourceOwner(result, event);
		RecordRvrTraceEvent(event);
		return result;
	}

	UINT64 UMaterial_SelectResource_hook(UMaterial* self, FStaticParameterSet* parameters,
		FMaterial* candidateResource, EShaderPlatform platform, INT resourceIndexOrQuality,
		UINT arg6, UINT arg7)
	{
		if (TraceRvrMaterialResolution)
		{
			// VirtFunc_102 invokes FMaterial::Compile before it returns, so this needs to be in
			// the bounded history first for FMaterialCompile_hook to correlate the same resource.
			// Recording scalar values here is observational and cannot affect selection.
			FRvrTraceEvent event{ ERvrTraceEventKind::UMaterialSelectResource, self, parameters, resourceIndexOrQuality };
			TryReadMember(self, 0x40, event.Parent); // child's Outer, normally URvrEffectsMaterialUser
			TryReadMember(self, 0x3A0, event.Base);  // engine fallback resource
			if (resourceIndexOrQuality >= 0 && resourceIndexOrQuality < 64)
			{
				TryReadMember(self, 0xA4 + static_cast<size_t>(resourceIndexOrQuality) * sizeof(void*), event.DefaultMaterial);
			}
			event.Returned = candidateResource;
			CaptureResourceOwner(reinterpret_cast<FMaterialResource*>(candidateResource), event);
			RecordRvrTraceEvent(event);
		}

		return UMaterial_SelectResource_orig(self, parameters, candidateResource, platform,
			resourceIndexOrQuality, arg6, arg7);
	}

	void FlushRvrMaterialResolutionTrace(FMaterialResource* material, const FString& materialPath)
	{
		if (!TraceRvrMaterialResolution || material == nullptr ||
			std::wstring_view(materialPath.Chars()).find(L"HMF_HED_PROMiranda_SCALP_MAT_USER.CombinedEffect_") == std::wstring_view::npos)
		{
			return;
		}

		std::array<FRvrTraceEvent, RvrTraceCapacity> snapshot{};
		size_t snapshotCount = 0;
		AcquireSRWLockShared(&GRvrTraceLock);
		snapshotCount = GRvrTraceCount;
		const auto start = (GRvrTraceNext + RvrTraceCapacity - snapshotCount) % RvrTraceCapacity;
		for (size_t index = 0; index < snapshotCount; ++index)
		{
			snapshot[index] = GRvrTraceEvents[(start + index) % RvrTraceCapacity];
		}
		ReleaseSRWLockShared(&GRvrTraceLock);

		void* matchedOwner = nullptr;
		size_t resourceMatchCount = 0;
		for (size_t index = 0; index < snapshotCount; ++index)
		{
			const auto& event = snapshot[index];
			if (event.Returned == material || event.DefaultMaterial == material)
			{
				matchedOwner = event.ResourceOwner;
				++resourceMatchCount;
			}
		}

		const auto header = std::format(L"Transient material trace for {}: {} retained events, {} resource matches; compiled resource={}, owner={}",
			materialPath.Chars(), snapshotCount, resourceMatchCount, static_cast<void*>(material), matchedOwner);
		LEASI_INFO(L"{}", header);
		AppendRvrTraceLine(header);
		for (size_t index = 0; index < snapshotCount; ++index)
		{
			const auto& event = snapshot[index];
			const bool allocatedThisResource = event.Returned == material || event.DefaultMaterial == material;
			const bool resolvedMatchedOwner = matchedOwner != nullptr && event.Returned == matchedOwner;
			if (!allocatedThisResource && !resolvedMatchedOwner)
			{
				continue;
			}
			const auto line = event.Kind == ERvrTraceEventKind::UMaterialSelectResource
				? std::format(L"  {}: child={}, static-parameters={}, requested-index={}, child-outer={}, fallback={}, selected-resource={}, candidate-resource={}, owner={}, resource-id={:08X}-{:08X}-{:08X}-{:08X}, matches-compile-resource={}",
					RvrTraceKindName(event.Kind), event.Self, event.Context, event.Selector, event.Parent,
					event.Base, event.DefaultMaterial, event.Returned, event.ResourceOwner, event.ResourceIdA,
					event.ResourceIdB, event.ResourceIdC, event.ResourceIdD, allocatedThisResource)
				: std::format(L"  Rvr {}: self={}, context={}, selector={}, parent={}, base={}, mux={}, default={}, return={}, owner={}, matches-compile-resource={}, tag-parent-count={}, tag-name={}:{}",
					RvrTraceKindName(event.Kind), event.Self, event.Context, event.Selector, event.Parent,
					event.Base, event.Multiplexor, event.DefaultMaterial, event.Returned, event.ResourceOwner,
					allocatedThisResource, event.ParentCount, event.ParentNameIndex, event.ParentNameNumber);
			LEASI_INFO(L"{}", line);
			AppendRvrTraceLine(line);
		}
		GRvrTraceCount = 0;
	}

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
	tUMaterialCheckMaterialUsage* UMaterial_CheckMaterialUsage_orig = nullptr;

	UBOOL UMaterial_CheckMaterialUsage_hook(UMaterial* material, int usage)
	{
		const auto result = UMaterial_CheckMaterialUsage_orig(material, usage);

		// These are the return addresses of the two calls in LE3 11.3.2's
		// FTerrainSceneProxy::FTerrainSceneProxy (0x7ff74b08dc60). Filtering here keeps
		// general material-usage traffic out of ShaderClient's log while reporting the exact
		// native decision that leads to the DefaultMaterial foliage substitution.
		const auto returnAddress = reinterpret_cast<uintptr_t>(_ReturnAddress());
		constexpr uintptr_t TerrainFoliageUsageReturnAddress = 0x7ff74b08df8e;
		constexpr uintptr_t TerrainStaticLightingUsageReturnAddress = 0x7ff74b08dfa3;
		if (returnAddress == TerrainFoliageUsageReturnAddress || returnAddress == TerrainStaticLightingUsageReturnAddress)
		{
			const wchar_t* usageName = usage == 5 ? L"foliage" : usage == 7 ? L"static-lighting" : L"unexpected";
			const std::wstring materialName = material == nullptr ? L"<null>" : material->GetFullPath().Chars();
			if (result)
			{
				LEASI_INFO(L"Terrain foliage material usage accepted: material={}, usage={}", materialName, usageName);
			}
			else
			{
				LEASI_WARN(L"Terrain foliage fell back to Engine.DefaultMaterial: material={} rejected usage={}", materialName, usageName);
			}
		}

		return result;
	}

	namespace
	{
		// UMaterialInterface vtable slot 98 (+0x310). UMaterial and UMaterialInstance both route it
		// into the shared usage worker at 0x7ff74ab2e260. On this shipping build that worker is pure:
		// it clears its out flag and never sets it. Asking again here therefore reproduces the
		// constructor's decision without side effects.
		UBOOL CheckMaterialUsageVirtual(UMaterialInterface* material, int usage)
		{
			using tCheckUsage = UBOOL(UMaterialInterface*, int);
			auto* const vtable = *reinterpret_cast<tCheckUsage* const* const*>(material);
			return vtable[0x310 / sizeof(void*)](material, usage);
		}

		// UStaticMeshComponent vtable +0x5D8: GetMaterial(MaterialIndex, LODIndex), the same call
		// FLODInfo makes for each element.
		UMaterialInterface* GetComponentMaterial(UStaticMeshComponent* component, INT materialIndex, INT lodIndex)
		{
			using tGetMaterial = UMaterialInterface* (UStaticMeshComponent*, INT, INT);
			auto* const vtable = *reinterpret_cast<tGetMaterial* const* const*>(component);
			return vtable[0x5D8 / sizeof(void*)](component, materialIndex, lodIndex);
		}

		// Native-only UStaticMesh render data, as FLODInfo reads it:
		//   StaticMesh+0x60    FStaticMeshRenderData** (TIndirectArray LODModels data)
		//   RenderData+0x1B0   FStaticMeshElement* Elements, +0x1B8 INT count, stride 0x50
		//   Element+0x34       INT MaterialIndex
		constexpr size_t StaticMeshLODModelsOffset = 0x60;
		constexpr size_t RenderDataElementsOffset = 0x1B0;
		constexpr size_t RenderDataElementCountOffset = 0x1B8;
		constexpr size_t StaticMeshElementStride = 0x50;
		constexpr size_t StaticMeshElementMaterialIndexOffset = 0x34;

		// FLODInfo layout: TArray<FElementInfo> Elements at +0x8. FElementInfo is 12 bytes, with a
		// UBOOL at +0 and the UMaterialInterface* at +4 (unaligned; see the MOVSD store at 0x7ff74b06d867).
		constexpr size_t LODInfoElementsOffset = 0x8;
		constexpr size_t LODInfoElementStride = 0xC;
		constexpr size_t LODInfoElementMaterialOffset = 0x4;

		// Material instances answer usage checks with their base UMaterial's flags. Naming that
		// material is what makes the log actionable.
		UMaterial* FindBaseMaterial(UMaterialInterface* material)
		{
			for (int depth = 0; material != nullptr && depth < 32; depth++)
			{
				if (material->IsA(UMaterial::StaticClass()))
				{
					return static_cast<UMaterial*>(material);
				}
				if (!material->IsA(UMaterialInstance::StaticClass()))
				{
					return nullptr;
				}
				material = static_cast<UMaterialInstance*>(material)->Parent;
			}
			return nullptr;
		}

		// Proxies are rebuilt every time a component reattaches, so the same element would otherwise
		// be reported over and over.
		std::mutex StaticMeshFallbackReportedLock;
		std::unordered_set<std::wstring> StaticMeshFallbackReported;

		void ReportStaticMeshDefaultMaterialFallbacks(void* lodInfo, UStaticMeshComponent* component, INT lodIndex)
		{
			if (lodInfo == nullptr || component == nullptr || GEngine == nullptr || *GEngine == nullptr)
			{
				return;
			}

			UMaterialInterface* const defaultMaterial = (*GEngine)->DefaultMaterial;
			auto* const elements = *reinterpret_cast<BYTE**>(static_cast<BYTE*>(lodInfo) + LODInfoElementsOffset);
			const INT elementCount = *reinterpret_cast<INT*>(static_cast<BYTE*>(lodInfo) + LODInfoElementsOffset + 0x8);
			if (defaultMaterial == nullptr || elements == nullptr || component->StaticMesh == nullptr)
			{
				return;
			}

			auto* const lodModels = *reinterpret_cast<BYTE***>(reinterpret_cast<BYTE*>(component->StaticMesh) + StaticMeshLODModelsOffset);
			auto* const renderData = lodModels[lodIndex];
			auto* const meshElements = *reinterpret_cast<BYTE**>(renderData + RenderDataElementsOffset);
			const INT meshElementCount = *reinterpret_cast<INT*>(renderData + RenderDataElementCountOffset);

			// Mirrors the constructor's bHasStaticLighting and encoding tests exactly.
			bool hasStaticLighting = false;
			if (lodIndex < static_cast<INT>(component->LODData.Count()))
			{
				const auto& lodData = component->LODData(lodIndex);
				hasStaticLighting = lodData.LightMap != nullptr || lodData.ShadowMaps.Count() != 0 || lodData.ShadowVertexBuffers.Count() != 0;
			}
			const INT lightMapEncoding = *reinterpret_cast<INT*>(reinterpret_cast<BYTE*>(component) + 0x27C);

			for (INT elementIndex = 0; elementIndex < elementCount && elementIndex < meshElementCount; elementIndex++)
			{
				UMaterialInterface* used = nullptr;
				std::memcpy(&used, elements + elementIndex * LODInfoElementStride + LODInfoElementMaterialOffset, sizeof(used));
				if (used != defaultMaterial)
				{
					continue;
				}

				const INT materialIndex = *reinterpret_cast<INT*>(meshElements + elementIndex * StaticMeshElementStride + StaticMeshElementMaterialIndexOffset);
				UMaterialInterface* const requested = GetComponentMaterial(component, materialIndex, lodIndex);
				if (requested == defaultMaterial)
				{
					// Asked for DefaultMaterial by name; nothing substituted.
					continue;
				}

				std::wstring reason;
				if (requested == nullptr)
				{
					reason = L"no material assigned";
				}
				else if (hasStaticLighting && !CheckMaterialUsageVirtual(requested, 7))
				{
					reason = L"rejected MATUSAGE_StaticLighting (bUsedWithStaticLighting)";
				}
				else if (hasStaticLighting && lightMapEncoding == 1 && !CheckMaterialUsageVirtual(requested, 22))
				{
					reason = L"rejected MATUSAGE_VectorLightMaps (bUsedWithVectorLightMaps; LightMapEncoding=LMET_Vector)";
				}
				else if (hasStaticLighting && lightMapEncoding == 2 && !CheckMaterialUsageVirtual(requested, 23))
				{
					reason = L"rejected MATUSAGE_SimpleLightMaps (bUsedWithSimpleLightMaps; LightMapEncoding=LMET_Simple)";
				}
				else
				{
					reason = std::format(L"unknown (static-lighting={}, encoding={})", hasStaticLighting, lightMapEncoding);
				}

				const std::wstring componentPath = component->GetFullPath().Chars();
				{
					std::scoped_lock lock{ StaticMeshFallbackReportedLock };
					if (!StaticMeshFallbackReported.insert(std::format(L"{}|{}|{}", componentPath, lodIndex, elementIndex)).second)
					{
						continue;
					}
				}

				const std::wstring requestedPath = requested == nullptr ? L"<null>" : requested->GetFullPath().Chars();
				UMaterial* const baseMaterial = FindBaseMaterial(requested);
				const std::wstring basePath = baseMaterial == nullptr ? L"<none>" : baseMaterial->GetFullPath().Chars();
				LEASI_WARN(L"Static mesh fell back to Engine.DefaultMaterial: component={}, mesh={}, LOD={}, element={}, material={}, base={}, reason={}",
					componentPath,
					component->StaticMesh->GetFullPath().Chars(),
					lodIndex,
					elementIndex,
					requestedPath,
					basePath,
					reason);
			}
		}
	}

	tStaticMeshLODInfoCtor* StaticMeshLODInfoCtor_orig = nullptr;

	void* StaticMeshLODInfoCtor_hook(void* lodInfo, UStaticMeshComponent* component, INT lodIndex)
	{
		auto* const result = StaticMeshLODInfoCtor_orig(lodInfo, component, lodIndex);
		ReportStaticMeshDefaultMaterialFallbacks(lodInfo, component, lodIndex);
		return result;
	}

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
		// Scope the set across the whole compile, not just the inner shader map compiles.
		//
		// SaveMaterialBinary() runs near the end of this function, after every
		// FMaterialShaderMap::Compile inside it has already returned and dropped its own scope. Without
		// this the global was back to nullptr by then, GetMaterialWireName() produced no key, and an
		// instance permutation's material resource went out under the parent UMaterial's bare path --
		// where ShaderHost resolves it to the parent and overwrites the parent's compiled resource.
		// Base materials are unaffected either way: their set overrides nothing, so it yields no key.
		StaticParameterScope compileScope{ parameterSet };

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
		// This is the actual HLSL-translation boundary. Correlate the UMaterial selection event
		// here rather than at FMaterialShaderMap::Compile, which can be reached multiple times for
		// a resource and made the earlier Rvr-only experiment look like a hanging/no-op path.
		const bool matchesTraceFilter = fmaterial->Material != nullptr && !MaterialTraceFilter.empty() &&
			std::wstring_view(fmaterial->Material->GetFullPath().Chars()).find(MaterialTraceFilter) != std::wstring_view::npos;
		const bool traceTarget = TraceRvrMaterialResolution && !isTerrain && matchesTraceFilter;
		const bool translationTraceTarget = TraceMaterialTranslation && !isTerrain && matchesTraceFilter;
		if (traceTarget)
		{
			const auto materialPath = fmaterial->Material->GetFullPath();
			FlushRvrMaterialResolutionTrace(fmaterial, materialPath);

			// The selected-resource hook above may run during startup rather than immediately before
			// this compile. Record the compiler boundary's own resource owner and the owner's Outer as
			// the authoritative fallback correlation, irrespective of that timing.
			FRvrTraceEvent compileEvent{};
			CaptureResourceOwner(fmaterial, compileEvent);
			void* ownerOuter = nullptr;
			TryReadMember(compileEvent.ResourceOwner, 0x40, ownerOuter);
			AppendRvrTraceLine(std::format(L"  FMaterial::Compile input: resource={}, owner={}, owner-outer={}, material={}, force={}, fallback={}, debug={}, current-map={}, out-map={}",
				static_cast<void*>(fmaterial), compileEvent.ResourceOwner, ownerOuter, materialPath.Chars(),
				forceCompile, bCompileFallback, debugDump, static_cast<void*>(fmaterial->ShaderMap.Object),
				shaderMap == nullptr ? nullptr : static_cast<void*>(*shaderMap)));
		}

		if (translationTraceTarget)
		{
			AppendMaterialTranslationTraceLine(std::format(L"FMaterial::Compile translation window entered: material={}, resource={}",
				fmaterial->Material->GetFullPath().Chars(), static_cast<void*>(fmaterial)));
			++GMaterialTranslationTraceDepth;
		}
		auto result = FMaterialCompile_orig(fmaterial, parameterSet, shaderplatform, shaderMap, forceCompile, bCompileFallback, debugDump);
		if (translationTraceTarget)
		{
			--GMaterialTranslationTraceDepth;
			AppendMaterialTranslationTraceLine(std::format(L"FMaterial::Compile translation window exited: material={}, returned={}",
				fmaterial->Material->GetFullPath().Chars(), result));
		}
		if (traceTarget)
		{
			AppendRvrTraceLine(std::format(L"  FMaterial::Compile result: material={}, returned={}, current-map={}, out-map={}",
				friendlyName.Chars(), result, static_cast<void*>(fmaterial->ShaderMap.Object),
				shaderMap == nullptr ? nullptr : static_cast<void*>(*shaderMap)));
			if (!result)
			{
				// Capture-only normally suppresses compile errors to keep corpus output concise. This
				// opt-in, one-family trace needs the reason a resource did not reach SaveShaderCache.
				for (UINT errorIndex = 0; errorIndex < fmaterial->CompileErrors.Count(); ++errorIndex)
				{
					AppendRvrTraceLine(std::format(L"    compile error: {}", fmaterial->CompileErrors.GetData()[errorIndex].Chars()));
				}
			}
		}
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
			SendMaterialCompileFailure(fmaterial);
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

		// One FMaterialShaderMap::Compile is one shader map, which is one static parameter set. Scope
		// the set to this call so GetMaterialWireName can key the capture by it; an instance
		// permutation is compiled through a resource whose ->Material is the parent UMaterial, and
		// without this every permutation would overwrite the parent's own captured jobs.
		//
		// The scope now lives in Client.hpp and restores the previous value on exit, so finishing here
		// hands the enclosing FMaterial::Compile its set back rather than clearing it.
		StaticParameterScope staticParameterScope{ InParameters };

		// Feeds the auto-exit watchdog. Signalled on entry and again on exit so a single long compile
		// cannot be mistaken for the level having gone idle.
		NotifyCompileActivity();

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
				NotifyCompileActivity();
				continue;
			}

			NotifyCompileActivity();
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
			// Pre-built; see ShaderProgressMessage. Do not format here.
			*outStr = ShaderProgressMessage.c_str();
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
	bool ExitWhenLoadScreenDrops = true;
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

	tStopLoadingMovie* StopLoadingMovie_orig = nullptr;
	UBOOL StopLoadingMovie_hook(USFXLoadMovieManager* self, int param2) {
		auto const result = StopLoadingMovie_orig(self, param2);

		// Only in shader-compile mode: everywhere else the load screen coming down is just the game
		// starting to play.
		if (BootShaderCompilerLevel && ExitWhenLoadScreenDrops) {
			LEASI_INFO(L"Load screen came down; shader compiling is finished. Exiting.");

			// Deliberately abrupt. This process exists only to compile shaders, everything the host
			// needs is already in the pipe, and the engine's own shutdown path is slow and has been a
			// source of spurious 0xC0000005 exit codes.
			//
			// Flush first. The logger only flushes info lines every 5s, so without this the kill takes
			// everything logged since the last flush with it -- a whole short compile, "Trying to compile"
			// and "compiled successfully" lines included, leaving a log that looks like the game never
			// reached the compiler level.
			LEASI_FLUSH();
			TerminateProcess(GetCurrentProcess(), k_shaderClientExitCode);
		}

		return result;
	}

	tReturnBool* IsWindowFocused_orig = nullptr;
	BOOL IsWindowFocused_hook() {
		// This is only hooked if with -ShaderCompile flag.
		return TRUE;
	}
}
