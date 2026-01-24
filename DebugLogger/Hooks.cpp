#include "Common/Base.hpp"
#include "Hooks.hpp"
#include "DebugLogger/LE1Hooks.hpp"
#include "DebugLogger/LE2Hooks.hpp"
#include "DebugLogger/LE3Hooks.hpp"
#include <thread>
#include <chrono>

namespace DebugLogger
{
	// Variables
	// ========================================

	// If every export's creation should be logged for deep debugging. Requirse -debugexportcreation command line arg.
	bool bLogExportCreation = false;


	void InstallSharedHooks(::LESDK::Initializer& Init)
	{
		// Debug String Output
		// ----------------------------------------
		auto const outputDebugStringW_target = Init.ResolveTyped<tOutputDebugStringW>(::LESDK::Address::FromAbsolute(OutputDebugStringW));
		CHECK_RESOLVED(outputDebugStringW_target);
		OutputDebugStringW_orig = (tOutputDebugStringW*)Init.InstallHook("OutputDebugStringW", outputDebugStringW_target, OutputDebugStringW_hook);
		CHECK_RESOLVED(OutputDebugStringW_orig);

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



		/*
		// UObject::ProcessEvent hook for UnrealScript Activated() logging
		// ----------------------------------------
		auto const UObject_ProcessEvent_target = Init.ResolveTyped<t_UObject_ProcessEvent>(BUILTIN_PROCESSEVENT_PHOOK);
		CHECK_RESOLVED(UObject_ProcessEvent_target);
		UObject_ProcessEvent_orig = (t_UObject_ProcessEvent*)Init.InstallHook("UObject::ProcessEvent", UObject_ProcessEvent_target, UObject_ProcessEvent_hook);
		CHECK_RESOLVED(UObject_ProcessEvent_orig);

		// UObject::ProcessInternal hook for native Activated() logging
		// ----------------------------------------
		auto const UObject_ProcessInternal_target = Init.ResolveTyped<t_UObject_ProcessInternal>(BUILTIN_PROCESSINTERNAL_PHOOK);
		CHECK_RESOLVED(UObject_ProcessInternal_target);
		UObject_ProcessInternal_orig = (t_UObject_ProcessInternal*)Init.InstallHook("UObject::ProcessInternal", UObject_ProcessInternal_target, UObject_ProcessInternal_hook);
		CHECK_RESOLVED(UObject_ProcessInternal_orig);
		*/

		LEASI_INFO("Hooks initialized");
	}

#pragma region OutputDebugStringW
	tOutputDebugStringW* OutputDebugStringW_orig = nullptr;
	void OutputDebugStringW_hook(LPCWSTR lpcszString)
	{
		OutputDebugStringW_orig(lpcszString);

		// Strip trailing newline before feeding to LEASI_INFO
		// Maybe consider prefixing with [DEBUG] or similar in future
		std::wstring strToLog(lpcszString);
		if (!strToLog.empty() && strToLog.back() == L'\n')
		{
			strToLog.pop_back();
		}

		LEASI_INFO(strToLog);
	}
#pragma endregion

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

	tCreateEntry* CreateExport_orig = nullptr;
	UObject* CreateExport_hook(ULinkerLoad* Context, int i)
	{
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
			LEASI_CRIT(L"FAILED TO CREATE EXPORT #{} from {}!", i + 1, Context->Filename);
			LEASI_FLUSH();
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
			exit(1);
		}
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
}
