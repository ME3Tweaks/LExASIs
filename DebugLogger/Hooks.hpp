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
		UClass* objectClass, // What class of object is being instantiated?
		UObject* inObject, // The 'Outer' of the object will be set to this 
		SFXName a3, // Name of object?
		long long loadFlags,
		void* a5, // often 0
		void* errorDevice, //Often GError
		const wchar_t* a7, // Often 0
		void* a8, // Often 0
		void* a9); // Often 0
	extern tStaticAllocateObject* StaticAllocateObject_orig;
	tStaticAllocateObject StaticAllocateObject_hook;






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
