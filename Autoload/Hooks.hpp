#pragma once

#include <LESDK/Headers.hpp>
#include <LESDK/Init.hpp>
#include "Common/Base.hpp"
#include "Autoload/ExtraContent.hpp"

namespace Autoload
{
	// Definitions
#define UGAMEENGINE_EXEC_RVA        ::LESDK::Address::FromOffset(0x3BD5D0)
#define REGISTER_TFC_RVA			::LESDK::Address::FromOffset(0x2628C0)
#define CACHECONTENT_WRAPPER_RVA    ::LESDK::Address::FromOffset(0x73760)
#define OPENFILE_READ_RVA	        ::LESDK::Address::FromOffset(0xF7910)
#define PROCESSINI_RVA		        ::LESDK::Address::FromOffset(0xC8BE0)
#define INSTALL_DLC_RVA		        ::LESDK::Address::FromOffset(0xB5D4D0)

	// Variables
	extern bool bRegisteredISBs;
	extern bool bRegisteredTFCs;
	extern bool bStartedMounting;

	// ! ExtraContent::ProcessIni
	// Autoload registration method
	// ========================================
	using tProcessIni = void(ExtraContent* ExtraContent, FString* IniPath, FString* BasePath);
	extern tProcessIni* ProcessIni_orig;
	void ProcessIni_hook(ExtraContent* ExtraContent, FString* IniPath, FString* BasePath);

	// ! InstallDownloadableContent
	// This method calls ProcessIni() to read the Inis and populate the 'ExtraContent' class object.
	// This method then loads packages based on what it read in (2DAs, etc)
	// This hook marks objects that load from items in the GlobalPackages array as rooted so they don't GC
	// This makes them behave like startup files in LE2/LE3 do
 	// ========================================
	using tInstallDownloadableContent = void(void* unk);
	extern tInstallDownloadableContent* InstallDownloadableContent_orig;
	void InstallDownloadableContent_hook(void* unk);

    // ! UGameEngine::Exec
	// For registering out custom console command for debugging
    // ========================================
    using t_UGameEngine_Exec = DWORD(UGameEngine* Context, WCHAR const* Command, void* Archive);
    extern t_UGameEngine_Exec* UGameEngine_Exec_orig;
    DWORD UGameEngine_Exec_hook(UGameEngine* Context, WCHAR const* Command, void* Archive);

	// ! UObject::ProcessEvent
	// Renders autoload profiler, allows toggling it.
	// ======================================================================
	using tProcessEvent = void(UObject* Context, UFunction* Function, void* Parms, void* Result);
	extern tProcessEvent* ProcessEvent_orig;
	void ProcessEvent_hook(UObject* Context, UFunction* Function, void* Parms, void* Result);

	// ! CacheContentWrapper
	// For ISB registration
	// ========================================
	using tCacheContentWrapper = void(long long parm1, const wchar_t* filePath, bool replaceIfExisting, bool warnIfExists);
	extern tCacheContentWrapper* CacheContentWrapper_orig;
	void CacheContentWrapper_hook(long long parm1, const wchar_t* filePath, bool replaceIfExisting, bool warnIfExists);

	// ! OpenFileRead
	// Called when files are opened for reading (Core.pcc is the first invocation)
	// ========================================
	using tOpenFileRead = void*(long long* parm1, void* parm2, wchar_t** parm3, void* parm4);
	extern tOpenFileRead* OpenFileRead_orig;
	void* OpenFileRead_hook(long long* parm1, void* parm2, wchar_t** filePath, void* parm4);

	// ! RegisterTFC
	// Allows adding a custom TFC file via path
	// =====================================================================
	using tRegisterTFC = void(FString* path);
	extern tRegisterTFC* RegisterTFC;

	// Sets the root object flag on the given object
	void RootObject(UObject* callingObject);
}
