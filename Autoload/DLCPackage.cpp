#include "Autoload/DLCPackage.hpp"
#include "Autoload/ExtraContent.hpp"
#include "Common/Mount.hpp"
#include <filesystem>

// Define static members
bool DLCPackage::bContentScanComplete = false;
bool DLCPackage::bContentScanStarted = false;
ExtraContent* DLCPackage::GExtraContent = nullptr;
std::vector<DLCPackage> DLCPackage::dlcsToMount{};

void DLCPackage::ScanForDLCContent() {
	std::filesystem::path const dlcDirectory{ k_searchFoldersRoot };

	if (!std::filesystem::exists(dlcDirectory))
	{
		LEASI_INFO(L"DLC directory does not exist, skipping DLC content scan");
		DLCPackage::bContentScanComplete = true;
		return;
	}

	LEASI_INFO(L"Performing DLC content scan");

	FString MountLoadError{};
	for (std::filesystem::directory_entry const& gameDLCDir : std::filesystem::directory_iterator{ dlcDirectory })
	{
		std::filesystem::path const& currDLCPath = gameDLCDir.path();
		std::wstring const dlcName{ currDLCPath.filename().c_str() };

		if (!gameDLCDir.is_directory()) {
			// File
			continue;
		}

		if (!dlcName.starts_with(L"DLC_MOD_"))
		{
			LEASI_INFO(L"Skipping {}, not a DLC mod directory", currDLCPath.c_str());
			continue;
		}

		// Reset the error
		MountLoadError.Clear();

		DLCPackage dlc{};
		dlc.RootPath = currDLCPath;
		dlc.DLCName = dlcName;
		if (dlc.ParseDLC(MountLoadError)) {
			dlcsToMount.push_back(std::move(dlc));
		}
	}

	// Sort DLCs into descending mount priority order.
	std::sort(dlcsToMount.begin(), dlcsToMount.end(), DLCPackage::CompareMountPriority);
	DLCPackage::bContentScanComplete = true;
	LEASI_INFO(L"DLC content scan completed");

	// Print DLCs
	for (auto it = dlcsToMount.rbegin(); it != dlcsToMount.rend(); ++it) {
		LEASI_TRACE(L"DLC: {} - Mount Priority: {}", it->AutoloadPath.c_str(), it->MountPriority);
	}
}

void DLCPackage::InstallDLCContent() {

}


bool DLCPackage::ParseDLC(FString& outError)
{
	MountPriority = Common::TryReadMountPriority(RootPath, &outError);

	if (MountPriority < 0) [[unlikely]]
	{
		LEASI_ERROR(L"Failed to read mount priority for '{}': {}", DLCName, *outError);
		return false;
	}

	// Content Scan
	AutoloadPath = RootPath / "AutoLoad.ini";
	LEASI_INFO(L"Found DLC Autoload.ini: {}, mount {}", AutoloadPath.wstring(), MountPriority);

	auto fileIterator = std::filesystem::recursive_directory_iterator(RootPath);
	for (const auto& entry : fileIterator)
	{
		if (entry.is_directory())
		{
			LEASI_INFO(L"\tScanning {}", entry.path().c_str());
			continue;
		}

		auto extension = entry.path().extension();
		if (extension == L".tfc") {
			// Found TFC
			auto tfcPath = entry.path().c_str();
			LEASI_INFO(L"\t\tFound TFC: {}", tfcPath);
			DLCTFCsToRegister.push_back(tfcPath); // We have to wait until first registration attempt or we'll hit a null pointer
		}
		else if (extension == L".isb")
		{
			// Found ISB
			auto isbPath = entry.path().c_str();
			LEASI_INFO(L"\t\tFound ISB: {}", isbPath);
			ISBsToRegister.push_back(_wcsdup(isbPath)); // We have to wait until first registration attempt or we'll hit a null pointer
		}
	}

	return true;
}

// Used for sorting DLCPackage by MountPriority in descending order
bool DLCPackage::CompareMountPriority(DLCPackage const& Left, DLCPackage const& Right)
{
	return Left.MountPriority >= Right.MountPriority;
}