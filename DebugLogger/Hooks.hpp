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
    using tOutputDebugStringW = void(LPCWSTR lpcszString);
    extern tOutputDebugStringW* OutputDebugStringW_orig;
    void OutputDebugStringW_hook(LPCWSTR lpcszString);

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
	using tFOutputDeviceErrorLogf = void(void* outputDevice, int* code, wchar_t* formatStr, void* param1, void* param2, void* param3, void* param4);
	extern tFOutputDeviceErrorLogf* FErrorOutputDeviceLogf_orig;
	void FErrorOutputDeviceLogf_hook(void* outputDevice, int* code, wchar_t* formatStr, void* param1, void* param2, void* param3, void* param4);


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
}
