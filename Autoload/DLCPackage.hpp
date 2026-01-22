#include "Common/Base.hpp"
#include <filesystem>
#include <LESDK/Headers.hpp>
#include "Autoload/ExtraContent.hpp"

class DLCPackage {
public:
	// Packages are already setup for mount order as they done via
	// AutoTOC.

	// Path to the root of this DLC
	std::filesystem::path RootPath;
	// Path to the Autoload.ini file
	std::filesystem::path AutoloadPath;
	// The name of the DLC folder
	std::wstring DLCName;

	// Priority of this DLC to mount additional content - we mount from high to low,
	// as ISB/TFC do not get set if they are, and we want highest priority to win.
	int MountPriority;

	// The list of pending TFCs to register once the first package file tries to load
	std::vector<std::wstring> DLCTFCsToRegister;
	// The list of pending ISBs to register once the first package file tries to load
	std::vector<std::wstring> ISBsToRegister;

	// Parses the DLC folder for content. This does not install the content.
	bool ParseDLC(FString&);

	
	// Static ==================================================================
	// Location of DLC folder relative to working directory
	static constexpr std::wstring_view k_searchFoldersRoot = L"../../BioGame/DLC/";

	// The extra content object that is passed to the original ProcessIni call
	static ExtraContent* GExtraContent;

	// List of parsed DLCs to mount
	static std::vector<DLCPackage> dlcsToMount;

	// Bool indicating if DLC content scan has begun and should not run again.
	static bool bContentScanStarted;

	// Bool indicating if DLC content scan has completed.
	static bool bContentScanComplete;

	// Inventories the DLC folder of the game.
	static void ScanForDLCContent();

	// Registers scanned content into the game for use.
	static void InstallDLCContent();

	// Used to make std::sort put higher priority mounts first.
	static bool CompareMountPriority(DLCPackage const& Left, DLCPackage const& Right);
};