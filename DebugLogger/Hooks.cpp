#include "Common/Base.hpp"
#include "Hooks.hpp"
#include "DebugLogger/LE1Hooks.hpp"
#include "DebugLogger/LE2Hooks.hpp"
#include "DebugLogger/LE3Hooks.hpp"
#include "DebugLogger/VerboseLogger.hpp"
#include <thread>
#include <chrono>

namespace DebugLogger
{
	// Variables
	// ========================================

	// If every export's creation should be logged for deep debugging. Requires -debugexportcreation command line arg.
	bool bLogExportCreation = false;


	void InstallSharedHooks(::LESDK::Initializer& Init)
	{
		// OutputDebugStringW is called during ASI startup. Create its hook in a
		// disabled state so its trampoline is published before any thread can
		// enter the detour.
		#define INSTALL_OUTPUTDEBUGSTRINGW_DEFERRED(INIT, TARGET) \
			do { \
				OutputDebugStringW_orig = reinterpret_cast<tOutputDebugStringW*>((INIT).InstallHookDeferred("OutputDebugStringW", (TARGET), OutputDebugStringW_hook)); \
				CHECK_RESOLVED(OutputDebugStringW_orig); \
				(INIT).EnableHook("OutputDebugStringW"); \
			} while (false)

		// Debug String Output
		// ----------------------------------------
		auto const outputDebugStringW_target = Init.ResolveTyped<tOutputDebugStringW>(::LESDK::Address::FromAbsolute(OutputDebugStringW));
		CHECK_RESOLVED(outputDebugStringW_target);
		INSTALL_OUTPUTDEBUGSTRINGW_DEFERRED(Init, outputDebugStringW_target);
		#undef INSTALL_OUTPUTDEBUGSTRINGW_DEFERRED

		// Import resolution failures
		//----------------------------------------
		auto const CreateImport_target = Init.ResolveTyped<tCreateEntry>(BUILTIN_VERIFYIMPORT_RVA);
		CHECK_RESOLVED(CreateImport_target);
		CreateImport_orig = (tCreateEntry*)Init.InstallHook("CreateImport", CreateImport_target, CreateImport_hook);
		CHECK_RESOLVED(CreateImport_orig);

		// Export creation failures (with optional success logging for pinpointing issues)
		// ---------------------------------------
		bLogExportCreation = (nullptr != std::wcsstr(GetCommandLineW(), L" -debugexportcreation"));
		auto const CreateExport_target = Init.ResolveTyped<tCreateEntry>(BUILTIN_CREATEEXPORT_RVA);
		CHECK_RESOLVED(CreateExport_target);
		CreateExport_orig = (tCreateEntry*)Init.InstallHook("CreateExport", CreateExport_target, CreateExport_hook);
		CHECK_RESOLVED(CreateExport_orig);

		// Package loading (Blocking)
		// ---------------------------------------
		auto const LoadPackage_target = Init.ResolveTyped<tLoadPackage>(BUILTIN_LOADPACKAGE_RVA);
		CHECK_RESOLVED(LoadPackage_target);
		LoadPackage_orig = (tLoadPackage*)Init.InstallHook("LoadPackage", LoadPackage_target, LoadPackage_hook);
		CHECK_RESOLVED(LoadPackage_orig);

		// Package loading (Background)
		// ---------------------------------------
		auto const LoadPackageAsyncTick_target = Init.ResolveTyped<tLoadPackageAsyncTick>(BUILTIN_LOADPACKAGEASYNCTICK_RVA);
		CHECK_RESOLVED(LoadPackageAsyncTick_target);
		LoadPackageAsyncTick_orig = (tLoadPackageAsyncTick*)Init.InstallHook("LoadPackageAsyncTick", LoadPackageAsyncTick_target, LoadPackageAsyncTick_hook);
		CHECK_RESOLVED(LoadPackageAsyncTick_orig);

		// StaticAllocateObject failures where exports fail to load
		// ---------------------------------------
		auto const StaticAllocateObject_target = Init.ResolveTyped<tStaticAllocateObject>(BUILTIN_STATICALLOCATEOBJECT_RVA);
		CHECK_RESOLVED(StaticAllocateObject_target);
		StaticAllocateObject_orig = (tStaticAllocateObject*)Init.InstallHook("StaticAllocateObject", StaticAllocateObject_target, StaticAllocateObject_hook);
		CHECK_RESOLVED(StaticAllocateObject_orig);

		// LogInternal - allows logging from UnrealScript. This is very verbose,
		// so leave the game's logging path untouched when explicitly disabled.
		// ---------------------------------------
		if (!std::wcsstr(GetCommandLineW(), L"-nologinternal"))
		{
			auto const LogInternal_target = Init.ResolveTyped<tLogInternal>(BUILTIN_OBJECT_LOGINTERNAL_RVA);
			CHECK_RESOLVED(LogInternal_target);
			LogInternal_orig = (tLogInternal*)Init.InstallHook("LogInternal", LogInternal_target, LogInternal_hook);
			CHECK_RESOLVED(LogInternal_orig);
		}
		else
		{
			LEASI_INFO("LogInternal logging disabled (-nologinternal)");
		}

		// FErrorOutputDevice::Logf - for executable message logs
		// ---------------------------------------
		auto const FOutputDeviceLogf_target = Init.ResolveTyped<tFOutputDeviceLogf>(BUILTIN_APPLOGF_RVA);
		CHECK_RESOLVED(FOutputDeviceLogf_target);
		FOutputDeviceLogf_orig = (tFOutputDeviceLogf*)Init.InstallHook("FOutputDeviceLogf", FOutputDeviceLogf_target, FOutputDeviceLogf_hook);
		CHECK_RESOLVED(FOutputDeviceLogf_orig);

		// FErrorOutputDeviceLogf - for executable error logging
		// ---------------------------------------
		auto const FErrorOutputDeviceLogf_target = Init.ResolveTyped<tFOutputDeviceErrorLogf>(BUILTIN_APPERRORLOGF_RVA);
		CHECK_RESOLVED(FErrorOutputDeviceLogf_target);
		FErrorOutputDeviceLogf_orig = (tFOutputDeviceErrorLogf*)Init.InstallHook("FErrorOutputDeviceLogf", FErrorOutputDeviceLogf_target, FErrorOutputDeviceLogf_hook);
		CHECK_RESOLVED(FErrorOutputDeviceLogf_orig);

		// FindPackageFile - for logging file lookups (and seeing when they don't have a hit)
		// ---------------------------------------
		if (std::wcsstr(GetCommandLineW(), L" -findpackagefile")) // !0 means its in there somewhere.
		{
			auto const FindPackageFile_target = Init.ResolveTyped<t_FindPackageFile>(BUILTIN_FINDPACKAGEFILE_RVA);
			CHECK_RESOLVED(FindPackageFile_target);
			FindPackageFile_orig = reinterpret_cast<t_FindPackageFile*>(Init.InstallHook("FindPackageFile", FindPackageFile_target, FindPackageFile_hook));
			CHECK_RESOLVED(FindPackageFile_orig);
		}

		// appHandleIOFailure - for tracing I/O failure code paths
		// -----------------------------------------
		
		auto const iofailure_target = Init.ResolveTyped<tIOFailure>(LEASI_RVA(0x14d6f0));
		CHECK_RESOLVED(iofailure_target);
		IOFailure_orig = reinterpret_cast<tIOFailure*>(Init.InstallHook("IOFailure", iofailure_target, IOFailure_hook));
		CHECK_RESOLVED(IOFailure_orig);


		/*auto const ULinkLoaderLoadSerializeName_target = Init.ResolveTyped<tFOutputDeviceErrorLogf>(LEASI_RVA(0xfa580));
		CHECK_RESOLVED(ULinkLoaderLoadSerializeName_target);
		ULinkerSerializeName_orig = (tULinkerSerializeName*)Init.InstallHook("ULinkerSerializeName", ULinkLoaderLoadSerializeName_target, ULinkerSerializeName_hook);
		CHECK_RESOLVED(ULinkerSerializeName_orig);*/

		InstallVerboseLoggerHooks(Init);
		LEASI_INFO("Hooks initialized");
	}

#pragma region OutputDebugStringW
	tOutputDebugStringW* OutputDebugStringW_orig = nullptr;
	void WINAPI OutputDebugStringW_hook(LPCWSTR lpcszString)
	{
		static thread_local bool inOutputDebugStringWHook = false;
		auto const original = OutputDebugStringW_orig;
		if (original == nullptr)
			return;
		auto const safeString = lpcszString != nullptr ? lpcszString : L"";

		if (inOutputDebugStringWHook)
		{
			original(safeString);
			return;
		}

		inOutputDebugStringWHook = true;
		struct RecursionGuard final
		{
			bool& active;
			~RecursionGuard() { active = false; }
		} recursionGuard{ inOutputDebugStringWHook };

		original(safeString);

		try
		{
			// Strip trailing newline before feeding to LEASI_INFO
			// Maybe consider prefixing with [DEBUG] or similar in future
			std::wstring strToLog = lpcszString != nullptr ? lpcszString : L"<null>";
			if (!strToLog.empty() && strToLog.back() == L'\n')
			{
				strToLog.pop_back();
			}

			LEASI_INFO(L"{}", strToLog);
			LEASI_FLUSH();
		}
		catch (...)
		{
			// OutputDebugStringW must remain fail-open if capture or logging fails.
		}
	}
#pragma endregion

#pragma region VerifyImport/CreateExport

	// LOG FAILED IMPORTS
	tCreateEntry* CreateImport_orig = nullptr;
	UObject* CreateImport_hook(ULinkerLoad* Context, int i)
	{
		UObject* object = CreateImport_orig(Context, i);
		if (object == nullptr)
		{
			FObjectImport importEntry = Context->ImportMap(i);
#if defined(SDK_TARGET_LE2)
			static bool bLoggedFirstBaseFailure = false;
			std::wstring filenameStr(Context->Filename.Chars());
			// Filter out shipped startup files because they generate tons of imports 
			// that developers will never care about.
			// Also makes game startup much slower to log all of them.
			if (filenameStr.find(L"\\Startup_00_Shared") != std::wstring::npos
				|| filenameStr.find(L"\\Startup_DLC_UNC_Moment01") != std::wstring::npos
				|| filenameStr.find(L"\\Startup_HEN_VT") != std::wstring::npos
				|| filenameStr.find(L"\\Startup_PRE_Cerberus") != std::wstring::npos
				|| filenameStr.find(L"\\Startup_PRE_Collectors") != std::wstring::npos
				|| filenameStr.find(L"\\Startup_PRE_DA") != std::wstring::npos
				|| filenameStr.find(L"\\Startup_PRE_Terminus") != std::wstring::npos
				|| filenameStr.find(L"\\Startup_PRE_General") != std::wstring::npos
				|| filenameStr.find(L"\\Startup_PRO_Gulp01") != std::wstring::npos
				|| filenameStr.find(L"\\Startup_PRO_Pepper01") != std::wstring::npos
				|| filenameStr.find(L"\\Startup_PRO_Pepper02") != std::wstring::npos
				|| filenameStr.find(L"\\Startup_DLC_UNC_Hammer01") != std::wstring::npos
				|| filenameStr.find(L"\\Startup_Kasumi") != std::wstring::npos
				|| filenameStr.find(L"\\Startup_CON_Pack01") != std::wstring::npos
				|| filenameStr.find(L"\\Startup_MCR_03") != std::wstring::npos
				|| filenameStr.find(L"\\Startup_UNC_Pack01") != std::wstring::npos
				|| filenameStr.find(L"\\Startup_CER_02") != std::wstring::npos
				|| filenameStr.find(L"\\Startup_Part01") != std::wstring::npos
				|| filenameStr.find(L"\\Startup_DLC_DHME1") != std::wstring::npos
				|| filenameStr.find(L"\\Startup_Pack02") != std::wstring::npos
				|| filenameStr.find(L"\\Startup_UPD_Patch02") != std::wstring::npos
				|| filenameStr.find(L"\\Startup_UPD_Patch03") != std::wstring::npos
				|| filenameStr.find(L"\\Startup_METR_Patch01") != std::wstring::npos
				|| filenameStr.find(L"\\Startup_EXP_Part01") != std::wstring::npos
				|| filenameStr.find(L"\\Startup_EXP_Part02") != std::wstring::npos
				|| filenameStr.find(L"\\Startup_CON_Pack02") != std::wstring::npos
				) {
				if (!bLoggedFirstBaseFailure) {
					LEASI_INFO("Note: Import resolution failures in LE2's shipping startup files are suppressed for performance.");
					bLoggedFirstBaseFailure = true;
				}
				// do not log these.
				return object;
			}
#endif

			LEASI_WARN("Could not resolve #{}: {} ({}) in {}", -i - 1, importEntry.ObjectName.GetName(), importEntry.ClassName.GetName(), Context->Filename);
			LEASI_FLUSH();
		}
		return object;
	}

	FString currentSerializingFile;
	int currentCreatingExportIdx = -1;

	tCreateEntry* CreateExport_orig = nullptr;
	UObject* CreateExport_hook(ULinkerLoad* Context, int i)
	{
		currentSerializingFile = Context->Filename;
		currentCreatingExportIdx = i;
		if (bLogExportCreation) {
			LEASI_INFO(L"Creating Export {} in {}", i + 1, Context->Filename);
		}
		UObject* object = CreateExport_orig(Context, i);
		if (object != nullptr) {
			if (bLogExportCreation) {
				LEASI_INFO(L"Loaded Export {} ({}) in {}", i + 1, object->GetName(), Context->Filename);
			}
		}
		else {
			// Inspect export to see if it should have actually been loaded per flags
			FObjectExport& exp = Context->ExportMap(i);

			// Check whether we already loaded the object and if not whether the context flags allow loading it.
			if ((exp.ObjectFlags & Context->ContextFlags) != 0) {
				// Should have loaded
				LEASI_CRIT(L"FAILED TO CREATE EXPORT #{} from {}!", i + 1, Context->Filename);
				LEASI_FLUSH();
			}
			else {
				// wasn't marked to load, this is ok
			}

		}
		if (bLogExportCreation) {
			LEASI_FLUSH();
		}
		/*if (object == nullptr)
		{
			FObjectImport importEntry = Context->ImportMap(i);
			LEASI_INFO(L"Could not resolve #%d: %hs (%hs) in file: %s\n", -i - 1, importEntry.ObjectName.GetName(), importEntry.ClassName.GetName(), Context->Filename.Data), true);
			logger.flush();
		}*/
		return object;
	}
#pragma endregion

#pragma region LoadPackage / LoadPackageAsync

	tLoadPackage* LoadPackage_orig = nullptr;
	UPackage* LoadPackage_hook(UPackage* outer, wchar_t* packageName, ELoadFlags loadFlags)
	{
		LEASI_INFO(L"Loading package synchronously: {}", packageName);
		return LoadPackage_orig(outer, packageName, loadFlags);
	}

	tLoadPackageAsyncTick* LoadPackageAsyncTick_orig = nullptr;
	UINT LoadPackageAsyncTick_hook(UnLinker* linker, int a2, float a3)
	{
		// Logger writes after the call cause linker might be null to start with, it's populated when tick begins
		auto result = LoadPackageAsyncTick_orig(linker, a2, a3);
		LEASI_INFO(L"Loading package asynchronously: {}, {:2f}%", linker->PackageName, linker->EstimatedLoadPercentage);
		return result;
	}
#pragma endregion

#pragma region StaticAllocateObject

	void logAllocationFailure(UClass* instancingClass, UObject* outer, SFXName objClassName, UObject* archetype) {
		auto instancingClassName = instancingClass ? instancingClass->GetFullName() : nullptr;
		auto outerName = outer ? outer->GetFullName() : nullptr;
		auto objectName = objClassName.GetName();
		auto archetypeName = archetype ? archetype->GetFullName() : nullptr;
		LEASI_CRIT(L"ERROR ALLOCATING OBJECT! Some information that may help track down the problem:");
		LEASI_CRIT("\tInstancing class name: {}", instancingClassName);
		LEASI_CRIT("\tOuter ('Link' in modding tools): {}", outerName);
		LEASI_CRIT("\tName of object being created: {}", objectName);
		LEASI_CRIT(L"\tArchetype: {}", archetypeName);
		LEASI_WARN(L"DebugLogger: Terminating application due to crash in StaticAllocateObject(). See the DebugLogger log file.\n", true);
		LEASI_FLUSH();
	}

	tStaticAllocateObject* StaticAllocateObject_orig = nullptr;
	UObject* StaticAllocateObject_hook(
		UClass* instancingClass,
		UObject* outer,
		SFXName objClassName,
		long long loadFlags,
		UObject* archetype,
		void* errorDev, // FOutputDevice
		const wchar_t* a7, // Ghidra shows this is pretty commonly 0
		void* instancePtr, // Ghidra shows this is pretty commonly 0
		void* a9) // Ghidra shows this is pretty commonly 0
	{
		__try {
			return StaticAllocateObject_orig(instancingClass, outer, objClassName, loadFlags, archetype, errorDev, a7, instancePtr, a9);
		}
		__except (EXCEPTION_EXECUTE_HANDLER) {
			// We failed to allocate an object
			// Game's gonna die. Let's log it

			// This has to be in a different function since it needs unwound and
			// that can't be done in __try __except
			logAllocationFailure(instancingClass, outer, objClassName, archetype);
			std::this_thread::sleep_for(std::chrono::seconds(8));
			// exit(1);
			return nullptr;
		}
	}
#pragma endregion

#pragma region LogInternal
	tLogInternal* LogInternal_orig = nullptr;
	void LogInternal_hook(UObject* callingObject, FFrame* stackFrame)
	{
		// Get the string argument to the method.
		BYTE* originalCodePointer = stackFrame->Code;
		BYTE nativeIndex = *stackFrame->Code++;
		FString stringArg;
		UObject* sfObject = stackFrame->Object;
		GNatives[nativeIndex](sfObject, stackFrame, &stringArg);

		LEASI_INFO(L"LogInternal() from {}: {}", callingObject->GetFullName(), stringArg);

		//restore the code pointer so LogInternal executes normally.
		stackFrame->Code = originalCodePointer;
		LogInternal_orig(callingObject, stackFrame);
	}

#pragma endregion LogInternal

#pragma region FOutputDevice::Logf
	tFOutputDeviceLogf* FOutputDeviceLogf_orig = nullptr;
	void FOutputDeviceLogf_hook(void* unknown, void* serializationFuncPtr, wchar_t* format_str, void* param1, void* param2, void* param3, void* param4)
	{
		LEASI_UNUSED_2(unknown, serializationFuncPtr);
		auto logMessage = FString::Printf(format_str, param1, param2, param3, param4);
		LEASI_INFO(L"appLogf: {}", logMessage);
	}

#pragma endregion FOutputDevice::Logf

#pragma region FErrorOutputDevice::Logf
	tFOutputDeviceErrorLogf* FErrorOutputDeviceLogf_orig = nullptr;
	void FErrorOutputDeviceLogf_hook(void* outputDevice, wchar_t* formatStr, void* param1, void* param2, void* param3, void* param4, void* param5, void* param6, void* param7, void* param8, void* param9)
	{
		LEASI_UNUSED(outputDevice);
		auto logMessage = FString::Printf(formatStr, param1, param2, param3, param4, param5, param6, param7, param8, param9);
		LEASI_ERROR(L"appLogErrorf: {}", logMessage);
	}
#pragma endregion FErrorOutputDevice::Logf


	// ! AppThrow
	// ========================================
	tAppThrowF* AppThrowF_orig = nullptr;
	void AppThrowF_hook(wchar_t* formatStr, void* param1, void* param2, void* param3, void* param4, void* param5, void* param6)
	{
		auto logMessage = FString::Printf(formatStr, param1, param2, param3, param4, param5, param6);
		LEASI_ERROR(L"appThrowf: {}", logMessage);
		LEASI_FLUSH();

		// Let the real appThrowf format (again) and throw the exception itself -
		// the throw has to originate from inside the game's own module so the
		// C++ EH runtime resolves its (image-relative) ThrowInfo correctly.
		AppThrowF_orig(formatStr, param1, param2, param3, param4, param5, param6);
	}

	// ! UObject::ProcessEvent hook
	// ========================================

	t_UObject_ProcessEvent* UObject_ProcessEvent_orig = nullptr;
	void UObject_ProcessEvent_hook(UObject* Context, UFunction* Function, void* Parms, void* Result)
	{
		UObject_ProcessEvent_orig(Context, Function, Parms, Result);
	}

	// ! UObject::ProcessInternal hook
	// ========================================

	t_UObject_ProcessInternal* UObject_ProcessInternal_orig = nullptr;
	void UObject_ProcessInternal_hook(UObject* Context, FFrame* Stack, void* Result)
	{
		auto func = Stack->Node;
		auto obj = Stack->Object;

		auto funcName = func->GetName();
		if (obj->IsA(USequenceOp::StaticClass()) && funcName.Equals(L"Activated"))
		{
			const auto op = reinterpret_cast<USequenceOp*>(Context);
			auto fullPath = op->GetFullPath();
			auto className = op->Class->Name.ToString();

		}

		UObject_ProcessInternal_orig(Context, Stack, Result);
	}

	// ! ULinkerLoad::operator<<Name hook
	// ========================================
	tULinkerSerializeName* ULinkerSerializeName_orig = nullptr;
	void* ULinkerSerializeName_hook(ULinkerLoad* self, SFXName* name)
	{
		auto result = ULinkerSerializeName_orig(self, name);
		return result;
	}

	// ! FindPackageFile
	// ========================================
	t_FindPackageFile* FindPackageFile_orig = nullptr;
	unsigned FindPackageFile_hook(void* This, wchar_t* InName, FGuid* Guid, FString* OutFileName, const wchar_t* Language, unsigned Unk)
	{

		// First try the original file lookup
		auto result = FindPackageFile_orig(This, InName, Guid, OutFileName, Language, Unk);

		// If not found on disk and we have a valid name, check precache map
		if (!result)
		{
			LEASI_WARN(L"FindPackageFile: File not found {}", InName);
		}

		return result;
	}

	// ! I/O Failure
	// ========================================
	tIOFailure* IOFailure_orig = nullptr;
	void IOFailure_hook(wchar_t* filename)
	{
		if (filename == nullptr) {
			filename = L"Unknown file";
		}
		LEASI_ERROR(L"I/O failure operating on {}", filename);
	}

#ifdef SDK_TARGET_LE3
	// ! WwiseParseBank
	// ========================================
	inline std::wstring AkResultToWString(AKRESULT result)
	{
		switch (result)
		{
		case AK_NotImplemented:              return L"AK_NotImplemented";
		case AK_Success:                     return L"AK_Success";
		case AK_Fail:                        return L"AK_Fail";
		case AK_PartialSuccess:              return L"AK_PartialSuccess";
		case AK_NotCompatible:               return L"AK_NotCompatible";
		case AK_AlreadyConnected:            return L"AK_AlreadyConnected";
		case AK_InvalidFile:                 return L"AK_InvalidFile";
		case AK_AudioFileHeaderTooLarge:     return L"AK_AudioFileHeaderTooLarge";
		case AK_MaxReached:                  return L"AK_MaxReached";
		case AK_InvalidID:                   return L"AK_InvalidID";
		case AK_IDNotFound:                  return L"AK_IDNotFound";
		case AK_InvalidInstanceID:           return L"AK_InvalidInstanceID";
		case AK_NoMoreData:                  return L"AK_NoMoreData";
		case AK_InvalidStateGroup:           return L"AK_InvalidStateGroup";
		case AK_ChildAlreadyHasAParent:      return L"AK_ChildAlreadyHasAParent";
		case AK_InvalidLanguage:             return L"AK_InvalidLanguage";
		case AK_CannotAddItselfAsAChild:     return L"AK_CannotAddItselfAsAChild";
		case AK_InvalidParameter:            return L"AK_InvalidParameter";
		case AK_ElementAlreadyInList:        return L"AK_ElementAlreadyInList";
		case AK_PathNotFound:                return L"AK_PathNotFound";
		case AK_PathNoVertices:              return L"AK_PathNoVertices";
		case AK_PathNotRunning:              return L"AK_PathNotRunning";
		case AK_PathNotPaused:               return L"AK_PathNotPaused";
		case AK_PathNodeAlreadyInList:       return L"AK_PathNodeAlreadyInList";
		case AK_PathNodeNotInList:           return L"AK_PathNodeNotInList";
		case AK_DataNeeded:                  return L"AK_DataNeeded";
		case AK_NoDataNeeded:                return L"AK_NoDataNeeded";
		case AK_DataReady:                   return L"AK_DataReady";
		case AK_NoDataReady:                 return L"AK_NoDataReady";
		case AK_InsufficientMemory:          return L"AK_InsufficientMemory";
		case AK_Cancelled:                   return L"AK_Cancelled";
		case AK_UnknownBankID:               return L"AK_UnknownBankID";
		case AK_BankReadError:               return L"AK_BankReadError";
		case AK_InvalidSwitchType:           return L"AK_InvalidSwitchType";
		case AK_FormatNotReady:              return L"AK_FormatNotReady";
		case AK_WrongBankVersion:            return L"AK_WrongBankVersion";
		case AK_FileNotFound:                return L"AK_FileNotFound";
		case AK_DeviceNotReady:              return L"AK_DeviceNotReady";
		case AK_BankAlreadyLoaded:           return L"AK_BankAlreadyLoaded";
		case AK_RenderedFX:                  return L"AK_RenderedFX";
		case AK_ProcessNeeded:                return L"AK_ProcessNeeded";
		case AK_ProcessDone:                 return L"AK_ProcessDone";
		case AK_MemManagerNotInitialized:    return L"AK_MemManagerNotInitialized";
		case AK_StreamMgrNotInitialized:     return L"AK_StreamMgrNotInitialized";
		case AK_SSEInstructionsNotSupported: return L"AK_SSEInstructionsNotSupported";
		case AK_Busy:                        return L"AK_Busy";
		case AK_UnsupportedChannelConfig:    return L"AK_UnsupportedChannelConfig";
		case AK_PluginMediaNotAvailable:     return L"AK_PluginMediaNotAvailable";
		case AK_MustBeVirtualized:           return L"AK_MustBeVirtualized";
		case AK_CommandTooLarge:             return L"AK_CommandTooLarge";
		case AK_RejectedByFilter:            return L"AK_RejectedByFilter";
		case AK_InvalidCustomPlatformName:   return L"AK_InvalidCustomPlatformName";
		case AK_DLLCannotLoad:               return L"AK_DLLCannotLoad";
		case AK_DLLPathNotFound:             return L"AK_DLLPathNotFound";
		case AK_NoJavaVM:                    return L"AK_NoJavaVM";
		case AK_OpenSLError:                 return L"AK_OpenSLError";
		case AK_PluginNotRegistered:         return L"AK_PluginNotRegistered";
		case AK_DataAlignmentError:          return L"AK_DataAlignmentError";
		case AK_DeviceNotCompatible:         return L"AK_DeviceNotCompatible";
		case AK_DuplicateUniqueID:           return L"AK_DuplicateUniqueID";
		case AK_InitBankNotLoaded:           return L"AK_InitBankNotLoaded";
		case AK_DeviceNotFound:              return L"AK_DeviceNotFound";
		case AK_PlayingIDNotFound:           return L"AK_PlayingIDNotFound";
		case AK_InvalidFloatValue:           return L"AK_InvalidFloatValue";
		case AK_FileFormatMismatch:          return L"AK_FileFormatMismatch";
		case AK_NoDistinctListener:          return L"AK_NoDistinctListener";
		case AK_ResourceInUse:               return L"AK_ResourceInUse";
		case AK_InvalidBankType:             return L"AK_InvalidBankType";
		case AK_AlreadyInitialized:          return L"AK_AlreadyInitialized";
		case AK_NotInitialized:              return L"AK_NotInitialized";
		case AK_FilePermissionError:         return L"AK_FilePermissionError";
		case AK_UnknownFileError:            return L"AK_UnknownFileError";
		case AK_TooManyConcurrentOperations: return L"AK_TooManyConcurrentOperations";
		case AK_InvalidFileSize:             return L"AK_InvalidFileSize";
		case AK_Deferred:                    return L"AK_Deferred";
		case AK_FilePathTooLong:             return L"AK_FilePathTooLong";
		case AK_InvalidState:                return L"AK_InvalidState";
		default:                             return L"AK_UnknownResult";
		}
	}

	tParseWwiseBank* ParseWwiseBank_orig = nullptr;
	AKRESULT ParseWwiseBank_hook(void* param_1, WwiseBankLoadParams* param_2, void* param_3, int param_4, char param_5, char param6)
	{
		LEASI_DEBUG(L"WwiseParseBank id {}", param_2->bankID);
		auto result = ParseWwiseBank_orig(param_1, param_2, param_3, param_4, param_5, param6);
		if (result != AKRESULT::AK_Success)
		{
			LEASI_ERROR(L"WwiseParseBank failed with code {} ({}) for bank id {} (0x{:X})", AkResultToWString(result).c_str(), static_cast<int>(result), param_2->bankID, param_2->bankID);
		}
		return result;
	}

#endif
}
