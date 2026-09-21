#pragma once

#include <memory>
#include <LESDK/Headers.hpp>
#include <LESDK/Init.hpp>
#include "Common/Base.hpp"
#include "Common/ME3TweaksLogger.hpp"

namespace DebugLogger
{
    // Struct definitions
	
	// Parameter passed to package loading methods
	// From old LE1 SDK
	struct UnLinker
	{
		virtual void Unknown0() = 0;
		wchar_t* PackageName;
		//virtual void Unknown3() = 0;
		//virtual void Unknown4() = 0;
		//virtual void Unknown5() = 0;
		//virtual void Unknown6() = 0;
		//virtual void Unknown7() = 0;
		//virtual void Unknown8() = 0;
		//virtual void Unknown9() = 0;
		//virtual void Unknown10() = 0;
		//virtual void Unknown11() = 0;
		//virtual void Unknown12() = 0;
		//virtual void Unknown13() = 0;
		INT NameCount1; // IDK
		INT NameCount2; // IDK
		FGuid PackageGuid;
		ULinkerLoad* Linker;
		TArray<void*> CompletionCallbacks;
		INT ImportIndex;
		INT ExportIndex;
		INT PreLoadIndex;
		INT PostLoadIndex;
		FLOAT TimeLimit;
		BOOL bUseTimeLimit;
		BOOL bTimeLimitExceeded;
		DOUBLE TickStartTime;
		//UObject* LastObjectWorkWasPeformedOn;
		//TCHAR* LastTypeOfWorkPerformed;
		//DOUBLE LoadStartTime;
		//FLOAT LoadPercentage;
		//BOOL bHasFinishedExportGuids;

		FLOAT Load1;
		FLOAT Load2;
		FLOAT Load3;
		FLOAT Load4;
		FLOAT Load5;
		FLOAT Load6;
		FLOAT Load7;
		FLOAT EstimatedLoadPercentage;
	};


    void InstallSharedHooks(::LESDK::Initializer& Init);
    void InstallGameSpecificHooks(::LESDK::Initializer& Init);

    // Hook prototypes and definitions.

    // ! OutputDebugStringW
    // For logging debug console calls.
    // ===================================================
    using tOutputDebugStringW = void(WINAPI)(LPCWSTR);
    extern tOutputDebugStringW* OutputDebugStringW_orig;
    void WINAPI OutputDebugStringW_hook(LPCWSTR lpcszString);

    // ! CreateImport
    // For logging when import resolution fails.
    // ===================================================
    using tCreateEntry = UObject*(ULinkerLoad* Context, int i);
    extern tCreateEntry* CreateImport_orig;
    UObject* CreateImport_hook(ULinkerLoad* Context, int i);

    // ! CreateExport
    // For logging when export creation fails.
    // ===================================================
	extern bool bLogExportCreation;
    extern tCreateEntry* CreateExport_orig;
    UObject* CreateExport_hook(ULinkerLoad* Context, int i);

	// ! CreateImport
	// For logging when import resolution fails.
	// ===================================================
	using tCreateEntry = UObject * (ULinkerLoad* Context, int i);
	extern tCreateEntry* CreateImport_orig;
	UObject* CreateImport_hook(ULinkerLoad* Context, int i);

	using tLoadPackage = UPackage * (UPackage* outer, wchar_t* packageName, ELoadFlags loadFlags);
	extern tLoadPackage* LoadPackage_orig;
	UPackage* LoadPackage_hook(UPackage* outer, wchar_t* packageName, ELoadFlags loadFlags);

	using tLoadPackageAsyncTick = UINT(UnLinker* linker, int a2, float a3);
	extern tLoadPackageAsyncTick* LoadPackageAsyncTick_orig;
	UINT LoadPackageAsyncTick_hook(UnLinker* linker, int a2, float a3);

	using tStaticAllocateObject = UObject*(
		UClass* instancingClass,
		UObject* outer,
		SFXName objClassName,
		long long loadFlags,
		UObject* archetype,
		void* errorDev, // FOutputDevice
		const wchar_t* a7, // Ghidra shows this is pretty commonly 0
		void* instancePtr, // Ghidra shows this is pretty commonly 0
		void* a9); // Often 0
	extern tStaticAllocateObject* StaticAllocateObject_orig;
	UObject* StaticAllocateObject_hook(
		UClass* instancingClass,
		UObject* outer,
		SFXName objClassName,
		long long loadFlags,
		UObject* archetype,
		void* errorDev, // FOutputDevice
		const wchar_t* a7, // Ghidra shows this is pretty commonly 0
		void* instancePtr, // Ghidra shows this is pretty commonly 0
		void* a9) ;

	// LogInternal
	using tLogInternal = void(UObject* callingObject, FFrame* param2);
	extern tLogInternal* LogInternal_orig;
	void LogInternal_hook(UObject* callingObject, FFrame* param2);

	// LogF
	using tFOutputDeviceLogf = void(void* outputDevice, void* serializationFuncPtr, wchar_t* formatStr, void* param1, void* param2, void* param3, void* param4);
	extern tFOutputDeviceLogf* FOutputDeviceLogf_orig;
	void FOutputDeviceLogf_hook(void* outputDevice, void* serializationFuncPtr, wchar_t* format_str, void* param1, void* param2, void* param3, void* param4);

	// LogErrorF
	using tFOutputDeviceErrorLogf = void(void* outputDevice, wchar_t* formatStr, void* param1, void* param2, void* param3, void* param4, void* param5, void* param6, void* param7, void* param8, void* param9);
	extern tFOutputDeviceErrorLogf* FErrorOutputDeviceLogf_orig;
	void FErrorOutputDeviceLogf_hook(void* outputDevice, wchar_t* formatStr, void* param1, void* param2, void* param3, void* param4, void* param5, void* param6, void* param7, void* param8, void* param9);

	// ! appThrowf
	// Formats the exception message and logs it before letting the game's own
	// appThrowf format (again) and throw it. We can't forward "..." to the real
	// variadic function directly, so extra args are relayed as generic slots -
	// this mirrors FErrorOutputDeviceLogf_hook above and is safe on the x64
	// Windows ABI since varargs are always passed as raw register/stack slots.
	// ===================================================
	using tAppThrowF = void(wchar_t* formatStr, void* param1, void* param2, void* param3, void* param4, void* param5, void* param6);
	extern tAppThrowF* AppThrowF_orig;
	void AppThrowF_hook(wchar_t* formatStr, void* param1, void* param2, void* param3, void* param4, void* param5, void* param6);


    // ! UObject::ProcessEvent hook for logging UnrealScript Activated() calls.
    // ========================================

    using t_UObject_ProcessEvent = void(UObject* Context, UFunction* Function, void* Parms, void* Result);
    extern t_UObject_ProcessEvent* UObject_ProcessEvent_orig;
    void UObject_ProcessEvent_hook(UObject* Context, UFunction* Function, void* Parms, void* Result);

    // ! UObject::ProcessInternal hook for logging native Activated() calls.
    // ========================================

    using t_UObject_ProcessInternal = void(UObject* Context, FFrame* Stack, void* Result);
    extern t_UObject_ProcessInternal* UObject_ProcessInternal_orig;
    void UObject_ProcessInternal_hook(UObject* Context, FFrame* Stack, void* Result);

	// ! FindPackageFile
	// =========================================
	using t_FindPackageFile = unsigned(void* This, wchar_t* InName, FGuid* Guid, FString* OutFileName, const wchar_t* Language, unsigned Unk);
	extern t_FindPackageFile* FindPackageFile_orig;
	unsigned FindPackageFile_hook(void* This, wchar_t* InName, FGuid* Guid, FString* OutFileName, const wchar_t* Language, unsigned Unk);


	// ! ULinkerLoad::operator<<Name hook for listing bad object name containing exports
	using tULinkerSerializeName = void* (ULinkerLoad* self, SFXName* name);
	extern tULinkerSerializeName* ULinkerSerializeName_orig;
	void* ULinkerSerializeName_hook(ULinkerLoad* self, SFXName* name);

	// ! I/O Failure
	// ============================
	using tIOFailure = void(wchar_t* filename);
	extern tIOFailure* IOFailure_orig;
	void IOFailure_hook(wchar_t* filename);

#ifdef SDK_TARGET_LE3
	// ! AK::SoundEngine::PostEvent
	// The LE3/LMP executable's numeric-ID overload has the following x64 ABI:
	// AkPlayingID PostEvent(AkUniqueID, AkGameObjectID, AkUInt32,
	//     AkCallbackFunc, void*, AkUInt32, AkExternalSourceInfo*, AkPlayingID).
	// Keep all parameters in the trampoline even though the logger only needs
	// the first two; callers commonly use the optional callback/source fields.
	// ===================================================
	using tAkCallbackFunc = void(*)(uint32_t callbackType, void* callbackInfo);
	using tPostWwiseEventById = uint32_t(uint32_t eventId, uint64_t gameObjectId,
		uint32_t flags, tAkCallbackFunc callback, void* cookie, uint32_t externalSourceCount,
		void* externalSources, uint32_t playingId);
	using tPostWwiseEventByAnsiName = uint32_t(char const* eventName, uint64_t gameObjectId,
		uint32_t flags, tAkCallbackFunc callback, void* cookie, uint32_t externalSourceCount,
		void* externalSources, uint32_t playingId);
	using tPostWwiseEventByWideName = uint32_t(wchar_t const* eventName, uint64_t gameObjectId,
		uint32_t flags, tAkCallbackFunc callback, void* cookie, uint32_t externalSourceCount,
		void* externalSources, uint32_t playingId);

	extern tPostWwiseEventById* PostWwiseEventById_orig;
	extern tPostWwiseEventByAnsiName* PostWwiseEventByAnsiName_orig;
	extern tPostWwiseEventByWideName* PostWwiseEventByWideName_orig;
	uint32_t PostWwiseEventById_hook(uint32_t eventId, uint64_t gameObjectId,
		uint32_t flags, tAkCallbackFunc callback, void* cookie, uint32_t externalSourceCount,
		void* externalSources, uint32_t playingId);
	uint32_t PostWwiseEventByAnsiName_hook(char const* eventName, uint64_t gameObjectId,
		uint32_t flags, tAkCallbackFunc callback, void* cookie, uint32_t externalSourceCount,
		void* externalSources, uint32_t playingId);
	uint32_t PostWwiseEventByWideName_hook(wchar_t const* eventName, uint64_t gameObjectId,
		uint32_t flags, tAkCallbackFunc callback, void* cookie, uint32_t externalSourceCount,
		void* externalSources, uint32_t playingId);

	enum AKRESULT
	{
		AK_NotImplemented = 0,          ///< This feature is not implemented.
		AK_Success = 1,          ///< The operation was successful.
		AK_Fail = 2,          ///< The operation failed.
		AK_PartialSuccess = 3,          ///< The operation succeeded partially.
		AK_NotCompatible = 4,          ///< Incompatible formats
		AK_AlreadyConnected = 5,          ///< The stream is already connected to another node.
		AK_InvalidFile = 7,          ///< The provided file is the wrong format or unexpected values causes the file to be invalid.
		AK_AudioFileHeaderTooLarge = 8,          ///< The file header is too large.
		AK_MaxReached = 9,          ///< The maximum was reached.
		AK_InvalidID = 14,         ///< The ID is invalid.
		AK_IDNotFound = 15,         ///< The ID was not found.
		AK_InvalidInstanceID = 16,         ///< The InstanceID is invalid.
		AK_NoMoreData = 17,         ///< No more data is available from the source.
		AK_InvalidStateGroup = 20,         ///< The StateGroup is not a valid channel.
		AK_ChildAlreadyHasAParent = 21,         ///< The child already has a parent.
		AK_InvalidLanguage = 22,         ///< The language is invalid (applies to the Low-Level I/O).
		AK_CannotAddItselfAsAChild = 23,         ///< It is not possible to add itself as its own child.
		AK_InvalidParameter = 31,         ///< Something is not within bounds, check the documentation of the function returning this code.
		AK_ElementAlreadyInList = 35,         ///< The item could not be added because it was already in the list.
		AK_PathNotFound = 36,         ///< This path is not known.
		AK_PathNoVertices = 37,         ///< Stuff in vertices before trying to start it
		AK_PathNotRunning = 38,         ///< Only a running path can be paused.
		AK_PathNotPaused = 39,         ///< Only a paused path can be resumed.
		AK_PathNodeAlreadyInList = 40,         ///< This path is already there.
		AK_PathNodeNotInList = 41,         ///< This path is not there.
		AK_DataNeeded = 43,         ///< The consumer needs more.
		AK_NoDataNeeded = 44,         ///< The consumer does not need more.
		AK_DataReady = 45,         ///< The provider has available data.
		AK_NoDataReady = 46,         ///< The provider does not have available data.
		AK_InsufficientMemory = 52,         ///< Memory error.
		AK_Cancelled = 53,         ///< The requested action was cancelled (not an error).
		AK_UnknownBankID = 54,         ///< Trying to load a bank using an ID which is not defined.
		AK_BankReadError = 56,         ///< Error while reading a bank.
		AK_InvalidSwitchType = 57,         ///< Invalid switch type (used with the switch container)
		AK_FormatNotReady = 63,         ///< Source format not known yet.
		AK_WrongBankVersion = 64,         ///< The bank version is not compatible with the current bank reader.
		AK_FileNotFound = 66,         ///< File not found.
		AK_DeviceNotReady = 67,         ///< Specified ID doesn't match a valid hardware device: either the device doesn't exist or is disabled.
		AK_BankAlreadyLoaded = 69,         ///< The bank load failed because the bank is already loaded.
		AK_RenderedFX = 71,         ///< The effect on the node is rendered.
		AK_ProcessNeeded = 72,         ///< A routine needs to be executed on some CPU.
		AK_ProcessDone = 73,         ///< The executed routine has finished its execution.
		AK_MemManagerNotInitialized = 74,         ///< The memory manager should have been initialized at this point.
		AK_StreamMgrNotInitialized = 75,         ///< The stream manager should have been initialized at this point.
		AK_SSEInstructionsNotSupported = 76,         ///< The machine does not support SSE instructions (required on PC).
		AK_Busy = 77,         ///< The system is busy and could not process the request.
		AK_UnsupportedChannelConfig = 78,         ///< Channel configuration is not supported in the current execution context.
		AK_PluginMediaNotAvailable = 79,         ///< Plugin media is not available for effect.
		AK_MustBeVirtualized = 80,         ///< Sound was Not Allowed to play.
		AK_CommandTooLarge = 81,         ///< SDK command is too large to fit in the command queue.
		AK_RejectedByFilter = 82,         ///< A play request was rejected due to the MIDI filter parameters.
		AK_InvalidCustomPlatformName = 83,         ///< Detecting incompatibility between Custom platform of banks and custom platform of connected application
		AK_DLLCannotLoad = 84,         ///< Plugin DLL could not be loaded, either because it is not found or one dependency is missing.
		AK_DLLPathNotFound = 85,         ///< Plugin DLL search path could not be found.
		AK_NoJavaVM = 86,         ///< No Java VM provided in AkInitSettings.
		AK_OpenSLError = 87,         ///< OpenSL returned an error.  Check error log for more details.
		AK_PluginNotRegistered = 88,         ///< Plugin is not registered.  Make sure to implement a AK::PluginRegistration class for it and use AK_STATIC_LINK_PLUGIN in the game binary.
		AK_DataAlignmentError = 89,         ///< A pointer to audio data was not aligned to the platform's required alignment (check AkTypes.h in the platform-specific folder)
		AK_DeviceNotCompatible = 90,         ///< Incompatible Audio device.
		AK_DuplicateUniqueID = 91,         ///< Two Wwise objects share the same ID.
		AK_InitBankNotLoaded = 92,         ///< The Init bank was not loaded yet, the sound engine isn't completely ready yet.
		AK_DeviceNotFound = 93,         ///< The specified device ID does not match with any of the output devices that the sound engine is currently using.
		AK_PlayingIDNotFound = 94,         ///< Calling a function with a playing ID that is not known.
		AK_InvalidFloatValue = 95,         ///< One parameter has a invalid float value such as NaN, INF or FLT_MAX.
		AK_FileFormatMismatch = 96,         ///< Media file format unexpected
		AK_NoDistinctListener = 97,         ///< No distinct listener provided for AddOutput
		AK_ResourceInUse = 99,         ///< Resource is in use and cannot be released.
		AK_InvalidBankType = 100,        ///< Invalid bank type. The bank type was either supplied through a function call (e.g. LoadBank) or obtained from a bank loaded from memory.
		AK_AlreadyInitialized = 101,        ///< Init() was called but that element was already initialized.
		AK_NotInitialized = 102,        ///< The component being used is not initialized. Most likely AK::SoundEngine::Init() was not called yet, or AK::SoundEngine::Term was called too early.
		AK_FilePermissionError = 103,        ///< The file access permissions prevent opening a file.
		AK_UnknownFileError = 104,        ///< Rare file error occured, as opposed to AK_FileNotFound or AK_FilePermissionError. This lumps all unrecognized OS file system errors.
		AK_TooManyConcurrentOperations = 105,        ///< When using StdStream, file operations can be blocking or not. When not blocking, operations need to be synchronized externally properly. If not, this error occurs.
		AK_InvalidFileSize = 106,        ///< The file requested was found and opened but is either 0 bytes long or not the expected size. This usually point toward a Low Level IO Hook implementation error.
		AK_Deferred = 107,        ///< Returned by functions to indicate to the caller the that the operation is done asynchronously. Used by Low Level IO Hook implementations when async operation are suppored by the hardware.
		AK_FilePathTooLong = 108,        ///< The combination of base path and file name exceeds maximum buffer lengths.
		AK_InvalidState = 109,        ///< This method should not be called when the object is in its current state.

		AKRESULT_Last                                ///< End of enum, invalid value.
	};

	// Reconstructed structure based on Ghidra analysis
	struct WwiseBankLoadParams
	{
		uint8_t  unk0[4];
		uint32_t bankID;       // +0x04: AkBankID (Hash)
		const char* szBankName; // +0x08: Pointer to Bank Name/Path (or NULL)
		// ... rest of internal struct fields
	};

	// ! ULinkerLoad::operator<<Name hook for listing bad object name containing exports
	using tParseWwiseBank = AKRESULT(void* param_1, WwiseBankLoadParams* param_2, void* param_3, int param_4, char param_5, char param6);
	extern tParseWwiseBank* ParseWwiseBank_orig;
	AKRESULT ParseWwiseBank_hook(void* param_1, WwiseBankLoadParams* param_2, void* param_3, int param_4, char param_5, char param6);

	// ! FMaterial::InitShaderMap
	// On a shipping/seekfree build, if a (non-default) material's shader map is missing or
	// incomplete, InitShaderMap() silently sets FMaterial::ShaderMap to NULL and returns FALSE
	// with no logging at all - this is the exact point where the renderer will subsequently
	// substitute GEngine->DefaultMaterial (the purple/white checkerboard) for that material.
	// We log the friendly name of the material whenever this happens.
	// ===================================================
	using tFGetMaterialFriendlyName = FString* (FMaterial* fmaterial, FString* OutStr);
	extern tFGetMaterialFriendlyName* FMaterial_GetFriendlyName;

	using tInitShaderMap = UBOOL(FMaterial* fmaterial, void* StaticParameters, EShaderPlatform Platform, int Quality);
	extern tInitShaderMap* InitShaderMap_orig;
	UBOOL InitShaderMap_hook(FMaterial* fmaterial, void* StaticParameters, EShaderPlatform Platform, int Quality);
#endif
}
