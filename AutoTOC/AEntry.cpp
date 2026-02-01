#include "Common/Base.hpp"
#include "Common/DefaultLogger.hpp"
#include <LESDK/Init.hpp>
#include <SPI.h>
#include "LESDK/Headers.hpp"
#include "AutoTOC/AEntry.hpp"
#include "AutoTOC/SharedVersion.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <windows.h>
#include "Strsafe.h"

#pragma pack(1)

//#define LOGGING
//#define PROFILING
#include "Common/Mount.hpp"

using namespace std;

#ifdef LOGGING
#define LOG(...) LEASI_INFO(__VA_ARGS__)
#else
// Nothing
#define LOG(...)
#endif

#ifdef PROFILING
chrono::time_point<chrono::steady_clock> startTime;
#define LOG_DURATION(desc) LOG("PROFILING: {}. Duration: {}ms", desc, std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - startTime).count())
#else
#define LOG_DURATION(desc)
#endif

SPI_PLUGINSIDE_SUPPORT(SDK_TARGET_NAME_W ASI_NAME_NO_SPACE_W, DEVELOPER_W, L"" VERSION_STRING_W, SPI_GAME_SDK_TARGET, SPI_VERSION_ANY);
SPI_PLUGINSIDE_PRELOAD;
SPI_PLUGINSIDE_ASYNCATTACH; // Should this be ASYNCATTACH instead?


SPI_IMPLEMENT_ATTACH
{
	LEASI_UNUSED(InterfacePtr);
#ifdef LOGGING
	Common::SetupDefaultLogger(SDK_TARGET_NAME_A, ASI_NAME_NO_SPACE_A);
#endif
	TCHAR path[MAX_PATH];
	HMODULE hModule = GetModuleHandle(NULL);
	GetModuleFileName(hModule, path, MAX_PATH);
	AutoToc(std::filesystem::path(path));
	return true;
}

SPI_IMPLEMENT_DETACH
{
	LEASI_UNUSED(InterfacePtr);
#ifdef LOGGING
	LESDK::TerminateConsole();
#endif
	return true;
}

void AutoToc(const std::filesystem::path& path)
{
	// Convert to std::filesystem::path for modern C++ path manipulation
	std::filesystem::path fsPath(path);

	// Get the game base folder (two levels up from the initial path)
	fsPath = fsPath.parent_path().parent_path().parent_path();

#ifdef PROFILING
	LOG("PROFILING: Started");
	startTime = std::chrono::high_resolution_clock::now();
#endif

	// Process basegame TOC
	const std::filesystem::path baseGamePath = fsPath;
	const std::filesystem::path tocPath = baseGamePath / "BioGame" / "PCConsoleTOC.bin";

	LOG("writing basegame toc...");


	writeTOC(tocPath, baseGamePath, false);

	LOG_DURATION("finished basegame TOC");

#if defined SDK_TARGET_LE2 || defined SDK_TARGET_LE3
	LOG("TOCing dlc:");

	const std::filesystem::path dlcPath = fsPath / "BioGame" / "DLC";

	if (std::filesystem::is_directory(dlcPath))
	{
		// Use C++ directory iteration instead of Windows API
		for (const auto& entry : std::filesystem::directory_iterator(dlcPath))
		{
			if (entry.is_directory())
			{
				const std::string dirName = entry.path().filename().string();

				// Check if directory name starts with "DLC_"
				if (dirName.length() > 4 && dirName.substr(0, 4) == "DLC_")
				{
					LOG("writing toc for {}", dirName);

					const std::filesystem::path dlcBaseDir = entry.path();
					const std::filesystem::path dlcTocPath = dlcBaseDir / "PCConsoleTOC.bin";

					writeTOC(dlcTocPath, dlcBaseDir / "", true);
				}
			}
		}
	}
	else
	{
		LOG("DLC directory not found!");
	}
	LOG_DURATION("finished all DLC TOCs");

#endif
	LOG("done");
}


const unsigned crcTable[] = { 0, 79764919, 159529838, 222504665, 319059676, 398814059, 445009330, 507990021, 638119352, 583659535, 797628118, 726387553, 890018660, 835552979, 1015980042, 944750013, 1276238704,
							  1221641927, 1167319070, 1095957929, 1595256236, 1540665371, 1452775106, 1381403509, 1780037320, 1859660671, 1671105958, 1733955601, 2031960084, 2111593891, 1889500026, 1952343757,
							  2552477408, 2632100695, 2443283854, 2506133561, 2334638140, 2414271883, 2191915858, 2254759653, 3190512472, 3135915759, 3081330742, 3009969537, 2905550212, 2850959411, 2762807018,
							  2691435357, 3560074640, 3505614887, 3719321342, 3648080713, 3342211916, 3287746299, 3467911202, 3396681109, 4063920168, 4143685023, 4223187782, 4286162673, 3779000052, 3858754371,
							  3904687514, 3967668269, 881225847, 809987520, 1023691545, 969234094, 662832811, 591600412, 771767749, 717299826, 311336399, 374308984, 453813921, 533576470, 25881363, 88864420,
							  134795389, 214552010, 2023205639, 2086057648, 1897238633, 1976864222, 1804852699, 1867694188, 1645340341, 1724971778, 1587496639, 1516133128, 1461550545, 1406951526, 1302016099,
							  1230646740, 1142491917, 1087903418, 2896545431, 2825181984, 2770861561, 2716262478, 3215044683, 3143675388, 3055782693, 3001194130, 2326604591, 2389456536, 2200899649, 2280525302,
							  2578013683, 2640855108, 2418763421, 2498394922, 3769900519, 3832873040, 3912640137, 3992402750, 4088425275, 4151408268, 4197601365, 4277358050, 3334271071, 3263032808, 3476998961,
							  3422541446, 3585640067, 3514407732, 3694837229, 3640369242, 1762451694, 1842216281, 1619975040, 1682949687, 2047383090, 2127137669, 1938468188, 2001449195, 1325665622, 1271206113,
							  1183200824, 1111960463, 1543535498, 1489069629, 1434599652, 1363369299, 622672798, 568075817, 748617968, 677256519, 907627842, 853037301, 1067152940, 995781531, 51762726, 131386257,
							  177728840, 240578815, 269590778, 349224269, 429104020, 491947555, 4046411278, 4126034873, 4172115296, 4234965207, 3794477266, 3874110821, 3953728444, 4016571915, 3609705398, 3555108353,
							  3735388376, 3664026991, 3290680682, 3236090077, 3449943556, 3378572211, 3174993278, 3120533705, 3032266256, 2961025959, 2923101090, 2868635157, 2813903052, 2742672763, 2604032198, 2683796849,
							  2461293480, 2524268063, 2284983834, 2364738477, 2175806836, 2238787779, 1569362073, 1498123566, 1409854455, 1355396672, 1317987909, 1246755826, 1192025387, 1137557660, 2072149281, 2135122070,
							  1912620623, 1992383480, 1753615357, 1816598090, 1627664531, 1707420964, 295390185, 358241886, 404320391, 483945776, 43990325, 106832002, 186451547, 266083308, 932423249, 861060070, 1041341759,
							  986742920, 613929101, 542559546, 756411363, 701822548, 3316196985, 3244833742, 3425377559, 3370778784, 3601682597, 3530312978, 3744426955, 3689838204, 3819031489, 3881883254, 3928223919, 4007849240,
							  4037393693, 4100235434, 4180117107, 4259748804, 2310601993, 2373574846, 2151335527, 2231098320, 2596047829, 2659030626, 2470359227, 2550115596, 2947551409, 2876312838, 2788305887, 2733848168, 3165939309,
							  3094707162, 3040238851, 2985771188, };
//crcTable was generated by this code:
//static void initCRCTable()
//{
//	const unsigned CRC_POLYNOMIAL = 0x04C11DB7;
//	for (unsigned idx = 0; idx < 256; idx++)
//	{
//		// Generate CRCs based on the polynomial
//		for (unsigned crc = idx << 24, bitIdx = 8; bitIdx != 0; bitIdx--)
//		{
//			crc = ((crc & 0x80000000) == 0x80000000) ? (crc << 1) ^ CRC_POLYNOMIAL : crc << 1;
//			crcTable[idx] = crc;
//		}
//	}
//}

static unsigned HashFileName(const std::string& strToHash)
{
	unsigned hash = 0;

	for (size_t i = 0; i < strToHash.length(); ++i)
	{
		hash = hash >> 8 & 0x00FFFFFF ^ crcTable[(hash ^ static_cast<BYTE>(toupper(strToHash[i]))) & 0x000000FF];
		hash = hash >> 8 & 0x00FFFFFF ^ crcTable[hash & 0x000000FF];
	}
	return hash;
}

void writeTOC(const std::filesystem::path& tocPath, const std::filesystem::path& baseDir, const bool isDLC)
{
	vector<fileData> files;
	// LE3 has 9k+ files in basegame, so reserve some space to avoid multiple allocations
	files.reserve(10000);
	LOG("getting files..");
	if (isDLC)
	{
		getFiles(files, baseDir, "");
	}
	else
	{
#if defined SDK_TARGET_LE1
		getLE1Files(files, baseDir);
#else           
		getFiles(files, baseDir, "BioGame\\");
		getFiles(files, baseDir, "Engine\\");
#endif
	}

	LOG("got file list for TOC: {}", tocPath.string());
	LOG("calculating hash table..");

	// Single-pass algorithm: test a small set of candidate sizes
	const size_t minTableSize = files.size() / 2;
	const std::vector<size_t> candidateSizes = {
		files.size(),
		files.size() * 3 / 4,
		files.size() * 5 / 8,
		minTableSize
	};

	size_t tableSize = files.size();
	size_t bestFillRatio = 0;
	std::vector<unsigned> hashBuckets;
	hashBuckets.reserve(files.size()); // Pre-allocate once

	for (const auto& file_data : files)
	{
		hashBuckets.push_back(file_data.hash);
	}

	// Test each candidate size to find the best distribution
	for (size_t candidateSize : candidateSizes)
	{
		std::vector<bool> bucketUsed(candidateSize, false); // Faster than set for presence check
		size_t uniqueCount = 0;

		for (unsigned hash : hashBuckets)
		{
			size_t bucket = hash % candidateSize;
			if (!bucketUsed[bucket])
			{
				bucketUsed[bucket] = true;
				uniqueCount++;
			}
		}

		// Accept if we have >75% fill ratio
		if (candidateSize - uniqueCount <= candidateSize / 4)
		{
			tableSize = candidateSize;
			bestFillRatio = uniqueCount;
			break;
		}

		// Track best option as fallback
		if (uniqueCount > bestFillRatio)
		{
			tableSize = candidateSize;
			bestFillRatio = uniqueCount;
		}
	}

	LOG("{} buckets for {} entries. {} buckets filled.", tableSize, files.size(), bestFillRatio);

	vector<vector<fileData>> buckets(tableSize);

	for (const auto& file_data : files)
	{
		buckets[file_data.hash % tableSize].emplace_back(file_data);
	}

       LOG("created hash table");
       LOG("building TOC file in memory..");
       std::vector<char> tocBuffer;
       tocBuffer.reserve(1024 * 1024); // Reserve 1MB, adjust as needed

       //header
       int magic = 0x3AB70C13;
       int zeroInt = 0;
       tocBuffer.insert(tocBuffer.end(), reinterpret_cast<const char*>(&magic), reinterpret_cast<const char*>(&magic) + sizeof(magic));
       tocBuffer.insert(tocBuffer.end(), reinterpret_cast<const char*>(&zeroInt), reinterpret_cast<const char*>(&zeroInt) + sizeof(zeroInt));

       tocBuffer.insert(tocBuffer.end(), reinterpret_cast<const char*>(&tableSize), reinterpret_cast<const char*>(&tableSize) + sizeof(tableSize));

       size_t entryPos = tableSize * 8;
       int tablePos = 0;
       //write the table
       std::vector<size_t> entryOffsets(buckets.size(), 0);
       for (size_t i = 0; i < buckets.size(); ++i)
       {
               const auto& bucket = buckets[i];
               if (bucket.empty())
               {
                       tocBuffer.insert(tocBuffer.end(), reinterpret_cast<const char*>(&zeroInt), reinterpret_cast<const char*>(&zeroInt) + sizeof(zeroInt));
                       tocBuffer.insert(tocBuffer.end(), reinterpret_cast<const char*>(&zeroInt), reinterpret_cast<const char*>(&zeroInt) + sizeof(zeroInt));
               }
               else
               {
                       int offset = static_cast<int>(entryPos - tablePos);
                       int bucketSize = static_cast<int>(bucket.size());
                       tocBuffer.insert(tocBuffer.end(), reinterpret_cast<const char*>(&offset), reinterpret_cast<const char*>(&offset) + sizeof(offset));
                       tocBuffer.insert(tocBuffer.end(), reinterpret_cast<const char*>(&bucketSize), reinterpret_cast<const char*>(&bucketSize) + sizeof(bucketSize));

                       for (const auto& file_data : bucket)
                       {
                               ushort entryLength = ushort(0x1D + file_data.fileName.length());
                               auto pad = (4 - entryLength % 4) % 4;
                               entryLength += (ushort)pad;
                               entryPos += entryLength;
                       }
               }
               tablePos += 8;
       }

       //write the entries
       std::vector<size_t> entrySizePositions;
       for (size_t i = 0; i < buckets.size(); ++i)
       {
               const auto& bucket = buckets[i];
               for (const auto& file_data : bucket)
               {
                       const size_t stringLength = file_data.fileName.length();
                       ushort entryLength = ushort(0x1D + stringLength);
                       ushort pad = (4 - entryLength % 4) % 4;
                       entryLength += pad;
                       // Record position for last entry size
                       entrySizePositions.push_back(tocBuffer.size());
                       // Buffer the entry in memory
                       tocBuffer.insert(tocBuffer.end(), reinterpret_cast<const char*>(&entryLength), reinterpret_cast<const char*>(&entryLength) + sizeof(entryLength));
                       ushort zeroUShort = 0;
                       tocBuffer.insert(tocBuffer.end(), reinterpret_cast<const char*>(&zeroUShort), reinterpret_cast<const char*>(&zeroUShort) + sizeof(zeroUShort));
                       tocBuffer.insert(tocBuffer.end(), reinterpret_cast<const char*>(&file_data.fileSize), reinterpret_cast<const char*>(&file_data.fileSize) + sizeof(file_data.fileSize));
                       for (int j = 0; j < 5; ++j)
                               tocBuffer.insert(tocBuffer.end(), reinterpret_cast<const char*>(&zeroInt), reinterpret_cast<const char*>(&zeroInt) + sizeof(zeroInt));
                       tocBuffer.insert(tocBuffer.end(), file_data.fileName.c_str(), file_data.fileName.c_str() + stringLength);
                       BYTE zeroByte = 0;
                       tocBuffer.push_back(static_cast<char>(zeroByte));
                       for (ushort p = 0; p < pad; ++p)
                               tocBuffer.push_back(static_cast<char>(zeroByte));
               }
       }

       //last entry doesn't have to have a size for some reason
       if (!entrySizePositions.empty())
       {
               ushort zeroUShort = 0;
               size_t lastPos = entrySizePositions.back();
               std::memcpy(&tocBuffer[lastPos], &zeroUShort, sizeof(zeroUShort));
       }

       LOG("writing TOC file to disk..");
       std::ofstream toc(tocPath, std::ios::out | std::ios::binary | std::ios::trunc);
       toc.write(tocBuffer.data(), tocBuffer.size());
       toc.close();
}


void write(ofstream& file, void* data, const streamsize size) {
	file.write(static_cast<char*>(data), size);
}

void write(ofstream& file, BYTE data) {
	file.write(reinterpret_cast<char*>(&data), 1);
}

void write(ofstream& file, ushort data) {
	file.write(reinterpret_cast<char*>(&data), 2);
}

void write(ofstream& file, int data) {
	file.write(reinterpret_cast<char*>(&data), 4);
}

bool isTocableExtension(std::string_view ext) {
	// Order from most common to least common for performance
	if (ext == ".pcc"

#if defined SDK_TARGET_LE2 || defined SDK_TARGET_LE3
		|| ext == ".afc"
#endif
		|| ext == ".bik"
		|| ext == ".bin"

#if defined SDK_TARGET_LE2 || defined SDK_TARGET_LE3
		|| ext == ".tlk"
#endif
		|| ext == ".txt"

#if defined SDK_TARGET_LE3
		|| ext == ".cnd"
#endif
		|| ext == ".upk"
		|| ext == ".tfc"
		|| ext == ".usf"
		|| ext == ".ini"

#if defined SDK_TARGET_LE1
		|| ext == ".isb"
#else
		|| ext == ".dlc"
#endif
		)
		return true;
	return false;
}

void getFiles(vector<fileData>& files, const std::filesystem::path& basepath, const std::string& searchPath) {
	const std::filesystem::path enumeratePath = basepath / searchPath;

	LOG("enumerating files: {}", enumeratePath.string());

	if (!std::filesystem::exists(enumeratePath))
	{
		return;
	}

	std::string relativePathBuffer;
	relativePathBuffer.reserve(MAX_PATH);
	for (const auto& entry : std::filesystem::directory_iterator(enumeratePath))
	{
		const std::string filename = entry.path().filename().string();

		if (entry.is_directory())
		{
			if (filename != "DLC" &&
				filename != "Patches" &&
				filename != "Splash" &&
				filename != "Config")
			{
				relativePathBuffer = searchPath;
				relativePathBuffer += filename;
				relativePathBuffer += "\\";
				LOG("getting files from Directory: {}", filename);
				getFiles(files, basepath, relativePathBuffer);
			}
		}
		else if (entry.is_regular_file())
		{
			std::string_view ext = entry.path().extension().string();
			if (isTocableExtension(ext))
			{
				relativePathBuffer = searchPath;
				relativePathBuffer += filename;
				const int fileSize = static_cast<int>(entry.file_size());
				LOG("found file: {}\t{}", relativePathBuffer, fileSize);
				files.emplace_back(relativePathBuffer, fileSize, HashFileName(filename));
			}
		}
	}
}

void addToMap(std::map<std::string, std::pair<std::string, int>, caseInsensitiveCmp>& fileMap, const std::filesystem::path& basepath, const std::string& searchPath)
{
	const std::filesystem::path enumeratePath = basepath / searchPath;

	LOG("enumerating files: {}", enumeratePath.string());

	if (!std::filesystem::exists(enumeratePath))
	{
		return;
	}

	std::string relativePathBuffer;
	relativePathBuffer.reserve(MAX_PATH);

	for (const auto& entry : std::filesystem::directory_iterator(enumeratePath))
	{
		const std::string filename = entry.path().filename().string();

		if (entry.is_directory())
		{
			// Skip excluded directories
			if (filename != "DLC" &&
				filename != "Patches" &&
				filename != "Splash" &&
				filename != "Config")
			{
				relativePathBuffer = searchPath;
				relativePathBuffer += filename;
				relativePathBuffer += "\\";
				LOG("getting files from Directory: {}", filename);
				addToMap(fileMap, basepath, relativePathBuffer);
			}
		}
		else if (entry.is_regular_file())
		{
			std::string_view ext = entry.path().extension().string();
			if (isTocableExtension(ext))
			{
				relativePathBuffer = searchPath;
				relativePathBuffer += filename;
				const int fileSize = static_cast<int>(entry.file_size());
				LOG("found file: {}\t{}", relativePathBuffer, relativePathBuffer.size());
				fileMap[filename] = std::make_pair(relativePathBuffer, fileSize);
			}
		}
	}
}

void getLE1Files(vector<fileData>& files, const std::filesystem::path& basepath)
{
	std::map<int, std::string> dlcMount;
	// std::map<int, std::string> dlcFriendlyNames;
	std::map<std::string, std::pair<std::string, int>, caseInsensitiveCmp> fileMap;
	bool dlcFound = false;

	std::filesystem::path dlcPath = basepath / "BioGame" / "DLC";

	LOG("finding LE1 DLCs...");
	if (std::filesystem::exists(dlcPath) && std::filesystem::is_directory(dlcPath))
	{
		for (const auto& entry : std::filesystem::directory_iterator(dlcPath))
		{
			if (entry.is_directory())
			{
				const std::string dirName = entry.path().filename().string();
				if (dirName.length() > 8 && dirName.substr(0, 8) == "DLC_MOD_")
				{
					FString error;
					int mount = Common::TryReadMountPriority(entry.path(), &error);

					if (mount > 0)
					{
						// Successfully read mount priority
						// IniFile autoLoad((entry.path() / "AutoLoad.ini").string());
						dlcMount[mount] = dirName;
						dlcFound = true;
						LOG("Registered {} at mount: {}", dirName, mount);
					}
					else if (std::filesystem::exists(entry.path() / "AutoLoad.ini"))
					{
						// AutoLoad.ini exists but failed to read mount priority
						LOG("{} has an invalid AutoLoad.ini! exiting...", dirName);
						std::wstring errorMsg = !error.Empty() ? error.Chars() : L"Unknown error";
						std::wstring message = L" has an improperly formatted AutoLoad.ini! Try re-installing the mod. If that doesn't fix the problem, contact the mod author.\n\nError: ";
						std::wstring fullMessage = std::wstring(dirName.begin(), dirName.end()) + message + errorMsg + L"\n\nMass Effect will now close.";
						MessageBoxW(nullptr,
							fullMessage.c_str(),
							L"Mass Effect LE - Broken Mod Warning",
							MB_OK | MB_ICONERROR);
						exit(1);
					}
					else
					{
						LOG("{} does not have an AutoLoad.ini, skipping...", dirName);
					}
				}
			}
		}
	}

	if (!dlcFound)
	{
		LOG("No DLC found");
	}

	LOG("building file map...");

	addToMap(fileMap, basepath, "BioGame\\");
	addToMap(fileMap, basepath, "Engine\\");

	std::ofstream loadOrderTxt(dlcPath.string() + "\\LoadOrder.Txt");
	loadOrderTxt << "This is an auto-generated file for informational purposes only. Editing it will not change the load order.\n\n";
	if (dlcFound)
	{
		for (const auto& [mount, dlcName] : dlcMount)
		{
			const std::string dlcSubPath = (std::filesystem::path("BioGame") / "DLC" / dlcName / "").string();
			addToMap(fileMap, basepath, dlcSubPath);

			loadOrderTxt << mount << ": " << dlcName << std::endl;
		}
	}
	else
	{
		loadOrderTxt << "No valid dlc found!";
	}
	loadOrderTxt.close();

	LOG("LE1 TOC in text form:");
	for (const auto& [fileName, filePair] : fileMap)
	{
		LOG("{}    {}", filePair.first, filePair.second);
		files.emplace_back(filePair.first, filePair.second, HashFileName(fileName));
	}
}







