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
		// All hooks below are installed deferred to prevent startup crashes

		// Debug String Output
		// ----------------------------------------
		LEASI_INSTALL_HOOK_DEFERRED(Init, OutputDebugStringW, tOutputDebugStringW, ::LESDK::Address::FromAbsolute(::OutputDebugStringW));

		// Import resolution failures
		//----------------------------------------
		LEASI_INSTALL_HOOK_DEFERRED(Init, CreateImport, tCreateEntry, BUILTIN_VERIFYIMPORT_RVA);

		// Export creation failures (with optional success logging for pinpointing context)
		// ---------------------------------------
		bLogExportCreation = (nullptr != std::wcsstr(GetCommandLineW(), L" -debugexportcreation"));
		LEASI_INSTALL_HOOK_DEFERRED(Init, CreateExport, tCreateEntry, BUILTIN_CREATEEXPORT_RVA);

		// Package loading (Blocking)
		// ---------------------------------------
		LEASI_INSTALL_HOOK_DEFERRED(Init, LoadPackage, tLoadPackage, BUILTIN_LOADPACKAGE_RVA);

		// Package loading (Background)
		// ---------------------------------------
		LEASI_INSTALL_HOOK_DEFERRED(Init, LoadPackageAsyncTick, tLoadPackageAsyncTick, BUILTIN_LOADPACKAGEASYNCTICK_RVA);

		// StaticAllocateObject failures where exports fail to load
		// ---------------------------------------
		LEASI_INSTALL_HOOK_DEFERRED(Init, StaticAllocateObject, tStaticAllocateObject, BUILTIN_STATICALLOCATEOBJECT_RVA);

		// LogInternal - allows logging from UnrealScript. This is very verbose, so allow turning it off
		// with -nologinternal
		// ---------------------------------------
		if (!std::wcsstr(GetCommandLineW(), L"-nologinternal"))
		{
			LEASI_INSTALL_HOOK_DEFERRED(Init, LogInternal, tLogInternal, BUILTIN_OBJECT_LOGINTERNAL_RVA);
		}
		else
		{
			LEASI_INFO("LogInternal logging disabled (-nologinternal)");
		}

		// FErrorOutputDevice::Logf - for executable message logs
		// ---------------------------------------
		LEASI_INSTALL_HOOK_DEFERRED(Init, FOutputDeviceLogf, tFOutputDeviceLogf, BUILTIN_APPLOGF_RVA);

		// FErrorOutputDeviceLogf - for executable error logging
		// ---------------------------------------
		LEASI_INSTALL_HOOK_DEFERRED(Init, FErrorOutputDeviceLogf, tFOutputDeviceErrorLogf, BUILTIN_APPERRORLOGF_RVA);

		// FindPackageFile - for logging file lookups (and seeing when they don't have a hit)
		// ---------------------------------------
		if (std::wcsstr(GetCommandLineW(), L" -findpackagefile"))
		{
			LEASI_INSTALL_HOOK_DEFERRED(Init, FindPackageFile, t_FindPackageFile, BUILTIN_FINDPACKAGEFILE_RVA);
		}

		// appHandleIOFailure - for tracing I/O failure code paths
		// -----------------------------------------
#if defined(SDK_TARGET_LE1)
		auto const IOFailure_rva = LEASI_RVA(0x1886d0);
#elif defined(SDK_TARGET_LE2)
		auto const IOFailure_rva = LEASI_RVA(0x1320c0);
#elif defined(SDK_TARGET_LE3)
		auto const IOFailure_rva = LEASI_RVA(0x14d6f0);
#endif
		LEASI_INSTALL_HOOK_DEFERRED(Init, IOFailure, tIOFailure, IOFailure_rva);

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
		if (original == nullptr) {
			return;
		}
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
					LEASI_INFO("Note: Import resolution failures in shipped startup files are suppressed for performance.");
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
	// Logs the UnrealScript call chain that a Stack.Logf() came from, starting at the
	// frame executing the offending opcode. Offsets are relative to the start of each
	// function's bytecode so they can be matched up in the script editor.
	void LogScriptStack(FFrame* frame)
	{
		constexpr int maxDepth = 64; // PreviousFrame chains are short; this only guards against a corrupt loop
		for (int depth = 0; frame != nullptr && depth < maxDepth; ++depth, frame = frame->PreviousFrame)
		{
			auto const node = frame->Node;
			auto const functionName = node ? node->GetFullName() : FString(L"<no function>");
			auto const objectName = frame->Object ? frame->Object->GetFullName() : FString(L"None");

			BYTE const* const scriptStart = node ? node->Script.GetData() : nullptr;
			BYTE const* const scriptEnd = scriptStart ? scriptStart + node->Script.Count() : nullptr;
			if (scriptStart != nullptr && frame->Code >= scriptStart && frame->Code <= scriptEnd)
			{
				LEASI_INFO(L"\t#{} {} +0x{:04X} (object: {})", depth, functionName, frame->Code - scriptStart, objectName);
			}
			else
			{
				LEASI_INFO(L"\t#{} {} (object: {})", depth, functionName, objectName);
			}
		}
	}

	tFOutputDeviceLogf* FOutputDeviceLogf_orig = nullptr;
	void FOutputDeviceLogf_hook(void* unknown, void* serializationFuncPtr, wchar_t* format_str, void* param1, void* param2, void* param3, void* param4)
	{
		LEASI_UNUSED(serializationFuncPtr);
		auto logMessage = FString::Printf(format_str, param1, param2, param3, param4);
		LEASI_INFO(L"appLogf: {}", logMessage);

		// UObject::execDynArraySort reports a missing comparison delegate via Stack.Logf(),
		// so the output device here is the FFrame of the script calling array.Sort().
		// The message alone doesn't say which script that is, so dump the script stack.
		if (format_str != nullptr && std::wcsncmp(format_str, L"Dynamic array Sort:", 19) == 0)
		{
			LEASI_INFO(L"Dynamic array Sort called from script stack:");
			LogScriptStack(static_cast<FFrame*>(unknown));
		}
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
