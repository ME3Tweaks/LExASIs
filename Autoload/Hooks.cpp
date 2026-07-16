#include <thread>
#include <chrono>
#include "Common/Base.hpp"
#include "Common/DefaultLogger.hpp"
#include "Autoload/Hooks.hpp"
#include "Autoload/DLCPackage.hpp"
#include <format>
#include <iostream> // Crucial for std::wcout

namespace Autoload
{
	tRegisterTFC* RegisterTFC;

	bool bRegisteredISBs = false;
	bool bRegisteredTFCs = false;
	bool bStartedMounting = false;
	tProcessIni* ProcessIni_orig = nullptr;
	void ProcessIni_hook(ExtraContent* ExtraContent, FString* IniPath, FString* BasePath)
	{
		ProcessIni_orig(ExtraContent, IniPath, BasePath);

		if (!bStartedMounting)
		{
			bStartedMounting = true;
			LEASI_INFO(L"Mounting DLC mods");

			// Sort dlcs to mount autoload in ascending order from lowest to highest.
			std::sort(DLCPackage::dlcsToMount.begin(), DLCPackage::dlcsToMount.end(), [](const DLCPackage& a, const DLCPackage& b) { return a.MountPriority < b.MountPriority; });

			for (const auto& dlc : DLCPackage::dlcsToMount)
			{
				// Run the mounting method on each autoload we have
				LEASI_INFO(L"  Mounting DLC mod Autoload.ini {}", dlc.AutoloadPath.wstring());
				FString autoloadPath(dlc.AutoloadPath.c_str());
				ProcessIni_orig(ExtraContent, &autoloadPath, nullptr);
			}
			DLCPackage::GExtraContent = ExtraContent;
			LEASI_INFO(L"DLC mount complete");
		}
	}


	t_UGameEngine_Exec* UGameEngine_Exec_orig = nullptr;
	DWORD UGameEngine_Exec_hook(UGameEngine* const Context, WCHAR const* const Command, void* const Archive)
	{
		if (ECHUD) {
			std::wstring_view const CommandView{ Command };
			if (CommandView == L"profile autoload")
			{
				ECHUD->SetVisible(true);
			}
			else if (CommandView == L"profile none")
			{
				ECHUD->SetVisible(false);
			}
		}

		return UGameEngine_Exec_orig(Context, Command, Archive);
	}

	// Marks all objects as RF_Root that have a outermost outer that is the same as the listed
	// UPackage (which means they are from this package)
	void RegisterStartupFile(UPackage* package, bool shouldLog)
	{
		if (shouldLog) {
			LEASI_INFO("Mounting startup file: {}", package->Name.GetName());
		}

		UINT numRooted = 0;
		auto GObjects = UObject::GObjObjects;
		for (UINT i = 0; i < GObjects->Count(); i++)
		{
			auto obj = GObjects->GetData()[i];
			if (obj && obj->Outer)
			{
				// Must not be null and must have an outer (otherwise it's already an object at the root of the hierarchy, which should already be rooted... in theory...)
				UObject* outerMost = obj;
				while (outerMost->Outer != nullptr)
				{
					outerMost = outerMost->Outer; // Go to it's nullptr
				}

				// Root the entire package
				if (outerMost == package)
				{
					if (shouldLog) {
						LEASI_TRACE(L"  Rooting: {}", obj->GetFullName());
					}
					RootObject(obj);
					numRooted++;
				}
			}
		}

		if (shouldLog) {
			LEASI_INFO(L"  Rooted {} objects", numRooted);
		}
	}


	tInstallDownloadableContent* InstallDownloadableContent_orig = nullptr;
	void InstallDownloadableContent_hook(void* unk)
	{
		InstallDownloadableContent_orig(unk); // Will call processIni and mount the content

		for (UINT i = 0; i < DLCPackage::GExtraContent->GlobalPackages.Count(); i++)
		{
			auto globalPackageName = DLCPackage::GExtraContent->GlobalPackages.GetData()[i];
			// Find it in the GExtraContent loaded packages
			for (UINT j = 0; j < DLCPackage::GExtraContent->LoadedPackages.Count(); j++)
			{
				auto package = DLCPackage::GExtraContent->LoadedPackages.GetData()[j];
				const char* name1 = package->Name.GetName();
				const wchar_t* name2 = globalPackageName.Chars();

				auto shouldLog = (_wcsnicmp(name2, L"Startup_", 8) == 0);

				// Convert name1 to wide string for comparison
				std::wstring name1Wide(std::strlen(name1), L'\0');
				std::mbstowcs(&name1Wide[0], name1, std::strlen(name1));
				auto isSameName = _wcsicmp(name1Wide.c_str(), name2) == 0;
				if (isSameName)
				{
					// It's a match
					RegisterStartupFile(package, shouldLog);
					break; // Go to the next one
				}
			}
		}

		LEASI_INFO("All AutoLoaderEnabler tasks completed");
		Common::ShutdownLogger();
	}

	tProcessEvent* ProcessEvent_orig = nullptr;
	void ProcessEvent_hook(UObject* Context, UFunction* Function, void* Parms, void* Result)
	{
		// Render autoload profiler HUD.
		ABioHUD* const BioHUD = Context->Cast<ABioHUD>();
		if (BioHUD != nullptr && (ECHUD == nullptr || ECHUD->Visible()) && Function->GetName().Equals(L"PostRender"))
		{
			if (!ECHUD)
			{
				// Init new object once we start processing events
				ECHUD = new ExtraContentHUD{ false };
			}
			ECHUD->Update(((ABioHUD*)Context)->Canvas, DLCPackage::GExtraContent);
			ECHUD->Draw();
		}

		ProcessEvent_orig(Context, Function, Parms, Result);
	}

	tCacheContentWrapper* CacheContentWrapper_orig = nullptr;
	void CacheContentWrapper_hook(long long parm1, const wchar_t* filePath, bool replaceIfExisting, bool warnIfExists)
	{
		if (!bRegisteredISBs)
		{
			bRegisteredISBs = true;
			LEASI_INFO(L"Registering DLC mod ISBs");
			for each(auto dlc in DLCPackage::dlcsToMount)
			{
				for each(auto isb in dlc.ISBsToRegister)
				{
					LEASI_INFO(L"  Registering ISB: {}", isb);
					CacheContentWrapper_orig(parm1, isb.c_str(), true, false);
				}
			}
			LEASI_INFO(L"ISB Registration complete");
		}
		CacheContentWrapper_orig(parm1, filePath, replaceIfExisting, warnIfExists);
	}

	void RegisterTFCs()
	{
		LEASI_INFO(L"Registering DLC mod TFCs");
		for each(auto dlc in DLCPackage::dlcsToMount)
		{
			for each(auto tfcPath in dlc.DLCTFCsToRegister)
			{
				LEASI_INFO(L"  Registering TFC: {}", tfcPath);
				FString tfcFString{ const_cast<wchar_t*>(tfcPath.c_str()) };
				RegisterTFC(&tfcFString);
			}
		}
		LEASI_INFO(L"TFC Registration complete");
	}

	tOpenFileRead* OpenFileRead_orig = nullptr;
	void* OpenFileRead_hook(long long* parm1, void* parm2, wchar_t** filePath, void* parm4)
	{
		if (!DLCPackage::bContentScanStarted)
		{
			DLCPackage::bContentScanStarted = true;

			// Wait up to 5 seconds.
			int i = 5;
			while (i > 0 && !DLCPackage::bContentScanComplete)
			{
				LEASI_INFO(L"Waiting for content scan to complete...");
				std::this_thread::sleep_for(std::chrono::seconds(1));
				i--;
			}

			if (i == 0 && !DLCPackage::bContentScanComplete)
			{
				LEASI_WARN(L"Content scan took too long, giving up waiting");
			}

			// Log what filename we hooked on for troubleshooting.
			std::wstring loadingFilename = std::filesystem::path(*filePath).filename().wstring();
			LEASI_INFO(L"TFC registration taking place when {} was about to read from disk", loadingFilename);

			RegisterTFCs();
		}

		// This call seems to be for when files are opened for reading (CreateFileW is called). This happens a lot with things like TFC reading but first hit is Core.pcc
		return OpenFileRead_orig(parm1, parm2, filePath, parm4);
	}

	void RootObject(UObject* callingObject)
	{
		if (callingObject) {
			callingObject->ObjectFlags |= EObjectFlags::RootSet; //RF_Root
		}
	}

	// Used in a bunch of places in code but not a lot actually of invocations.
	tTlkManagerGetSimpleString* TLKLookupSimple_orig = nullptr;
	FString* TLKLookupSimple_hook(UBioTlkManager* globalTalkTable, FString* outStr, int stringID, BOOL bParse) {
		outStr->Blank(); // Required as incoming strings seem to just be allocated but not initialized so they are just garbage data.
		auto result = FindTLKOverride(globalTalkTable, stringID, outStr, bParse);
		if (result) {
			return outStr;
		}
		
		return TLKLookupSimple_orig(globalTalkTable, outStr, stringID, bParse);
	}

	tTlkManagerGetString* TLKLookup_orig = nullptr;
	bool TLKLookup_hook(UBioTlkManager* globalTalkTable, int stringID, FString* outStr, BOOL bParse) {
		auto result = FindTLKOverride(globalTalkTable, stringID, outStr, bParse);
		if (result) {
			return result;
		}

		return TLKLookup_orig(globalTalkTable, stringID, outStr, bParse);
	}

	tTlkFileGetString* TlkFileGetString = nullptr;
	tTlkManagerTokenizeString* TlkManagerTokenizeString = nullptr;
	bool FindTLKOverride(UBioTlkManager* globalTalkTable, int stringID, FString* outStr, BOOL bParse){
		auto i = globalTalkTable->LoadedTLKs.Count();
		while (i > 0) { // 0th item should just be the basegame.
			auto tlkFile = globalTalkTable->LoadedTLKs.GetData()[i - 1];
			auto flags = tlkFile->ObjectFlags & 0x0020000000000000; // Only operate on 'NotForServer' which is override flag we use
			if (flags != 0) {
				if (TlkFileGetString(tlkFile, stringID, outStr)) {
					// We need to do the bParse one here too
					if (bParse) {
						TlkManagerTokenizeString(globalTalkTable, outStr);
					}
					return true;
				}
			}
			i--;
		}

		return false;
	}

}
