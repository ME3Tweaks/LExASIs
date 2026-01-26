// dllmain.cpp : Defines the entry point for the DLL application.

//Change the solution platform to either LE or ME3 to change which game this will work with
#ifdef _WIN64
#define LEGENDARY_EDITION
#endif

#include <chrono>
#include <fstream>
#include <map>
#include <set>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <windows.h>
#include "Shlwapi.h"
#include "Strsafe.h"
#include "AutoTOC/SharedVersion.h"

#ifndef LEGENDARY_EDITION
#include "../detours/detours.h"
#endif

#ifndef LEGENDARY_EDITION
#pragma comment(lib, "detours.lib") //Library needed for Hooking part.
#endif

#pragma pack(1)

//#define LOGGING
//#define PROFILING

#ifdef LEGENDARY_EDITION
#include "AutoTOC/IniFile.hpp"
#else
#include "TOCUpdater.h"
#endif

using namespace std;

enum class MEGame
{
	ME3,
	LE1,
	LE2,
	LE3
};

MEGame Game;

typedef unsigned short ushort;

struct fileData {
	string fileName;
	int fileSize;
	unsigned hash;

	fileData(const string fileName, const int fileSize, const unsigned hash) : fileName(fileName), fileSize(fileSize), hash(hash) {}
};

void write(ofstream& file, void* data, streamsize size);

void writeTOC(TCHAR  tocPath[MAX_PATH], TCHAR  baseDir[MAX_PATH], bool isDLC);

void write(ofstream& file, BYTE data);

void write(ofstream& file, ushort data);

void write(ofstream& file, int data);

void getFiles(vector<fileData>& files, TCHAR* basepath, TCHAR* searchPath);

#ifdef LEGENDARY_EDITION
void getLE1Files(vector<fileData>& files, TCHAR* basepath);
#endif


void AutoToc(TCHAR path[MAX_PATH]);

#if defined LOGGING || defined PROFILING
ofstream logFile;
#endif

#ifdef PROFILING
chrono::time_point<chrono::steady_clock> startTime;
#define LOG_DURATION(desc) logFile << "PROFILING: " << desc << ". duration: " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - startTime).count() << "ms\n";
#else
#define LOG_DURATION(desc)
#endif


#ifndef LEGENDARY_EDITION

using tProcessEvent = void(__thiscall*)(void*, void*, void*, void*);
tProcessEvent ProcessEvent = reinterpret_cast<tProcessEvent>(0x00453120);

static HHOOK msgHook = nullptr;
LRESULT CALLBACK wndProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (nCode >= HC_ACTION)
	{
		auto* const msgStruct = reinterpret_cast<LPCWPSTRUCT>(lParam);
		if (msgStruct->message == WM_APP + 'T' + 'O' + 'C')
		{
#ifdef LOGGING
			logFile << "Recieved Message From ME3Explorer..." << endl;
#endif
			TOCUpdater tocUpdater;
			const bool success = tocUpdater.TryUpdate();
#ifdef LOGGING
			logFile << (success ? "TOC Updated!" : "TOC Update FAILED!") << endl;
#endif
		}
	}
	return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

bool hookedMessages = false;
void __fastcall HookedPE(void* pObject, void* edx, void* pFunction, void* pParms, void* pResult)
{
	if (!hookedMessages)
	{
		hookedMessages = true;
		msgHook = SetWindowsHookEx(WH_CALLWNDPROC, wndProc, nullptr, GetCurrentThreadId());
	}
	ProcessEvent(pObject, pFunction, pParms, pResult);
}

void onAttach()
{
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread()); //This command set the current working thread to the game current thread.
	DetourAttach(&(PVOID&)ProcessEvent, HookedPE); //This command will start your Hook.
	DetourTransactionCommit();
}
#endif

#ifdef _USRDLL 
BOOL APIENTRY DllMain(HMODULE hModule, DWORD  reason, LPVOID)
{
	TCHAR path[MAX_PATH];
	GetModuleFileName(hModule, path, MAX_PATH);
#ifndef LEGENDARY_EDITION
	if (reason == DLL_PROCESS_ATTACH)
	{
		AutoToc(path);
		CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)onAttach, NULL, 0, NULL);
	}
	if (reason == DLL_PROCESS_DETACH)
	{
		UnhookWindowsHookEx(msgHook);
#ifdef LOGGING
		logFile.close();
#endif

	}
#else
	if (reason == DLL_PROCESS_ATTACH)
	{
		AutoToc(path);
#ifdef LOGGING
		logFile.close();
#endif

	}
#endif

	return TRUE;
}
#else
int main()
{
	TCHAR path[MAX_PATH];
	//exe version is only for testing purposes right now, so just hardcode the path to where the asi ought to go
	StringCchCopy(path, MAX_PATH, "D:\\SteamLibrary\\steamapps\\common\\Mass Effect Legendary Edition\\Game\\ME1\\Binaries\\Win64\\ASI\\AutoTOCasiLE.asi");
	AutoToc(path);
#ifdef LOGGING
	logFile.close();
#endif
}
#endif



void AutoToc(TCHAR path[MAX_PATH])
{

	PathRemoveFileSpec(path);
#if defined LOGGING || defined PROFILING
	StringCchCat(path, MAX_PATH, "\\AutoTOC.log");
	logFile.open(path);
	logFile << "AutoTOC log:\n";
	PathRemoveFileSpec(path);
#endif

#ifdef PROFILING
	logFile << "PROFILING: Started\n\n";
	startTime = std::chrono::high_resolution_clock::now();
#endif

	//get us to the game base folder
	PathRemoveFileSpec(path);
	PathRemoveFileSpec(path);
	PathRemoveFileSpec(path);
#ifdef LEGENDARY_EDITION 
	size_t strLen;
	StringCchLength(path, MAX_PATH, &strLen);

	switch (path[strLen - 1])
	{
	case '1':
		Game = MEGame::LE1;
		break;
	case '2':
		Game = MEGame::LE2;
		break;
	case '3':
		Game = MEGame::LE3;
		break;
	default:
		const std::string message = "AutoTOC asi is unable to determine which Mass Effect game it is running in! This game path is unexpected:\n\n";
		MessageBox(nullptr,
			(message + path).c_str(),
			"Mass Effect - ",
			MB_OK | MB_ICONERROR);
		exit(1);
	}
#else
	Game = MEGame::ME3;
#endif


	StringCchCat(path, MAX_PATH, "\\");


	TCHAR baseDir[MAX_PATH];
	StringCchCopy(baseDir, MAX_PATH, path);
	TCHAR tocPath[MAX_PATH];
	StringCchCopy(tocPath, MAX_PATH, path);
#ifdef LEGENDARY_EDITION 
	StringCchCat(tocPath, MAX_PATH, "BioGame\\PCConsoleTOC.bin");
#else
	StringCchCat(tocPath, MAX_PATH, "BIOGame\\PCConsoleTOC.bin");
#endif
#ifdef LOGGING
	logFile << "\nwriting basegame toc..\n";
#endif // LOGGING
	writeTOC(tocPath, baseDir, false);

	LOG_DURATION("finished basegame TOC")

		if (Game != MEGame::LE1)
		{
#ifdef LOGGING
			logFile << "\nTOCing dlc:\n";
#endif // LOGGING
#ifdef LEGENDARY_EDITION 
			StringCchCat(path, MAX_PATH, "BioGame\\DLC\\");
#else
			StringCchCat(path, MAX_PATH, "BIOGame\\DLC\\");
#endif

			if (PathIsDirectory(path))
			{
				StringCchCopy(baseDir, MAX_PATH, path);

				WIN32_FIND_DATA fd;
				HANDLE hFind = INVALID_HANDLE_VALUE;
				TCHAR enumeratePath[MAX_PATH];
				StringCchCopy(enumeratePath, MAX_PATH, path);
				StringCchCat(enumeratePath, MAX_PATH, "*");
				hFind = FindFirstFile(enumeratePath, &fd);
				do
				{
					if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
					{
						// 0 means true for lstrcmp
						if (strlen(fd.cFileName) > 3 && fd.cFileName[0] == L'D' && fd.cFileName[1] == L'L' && fd.cFileName[2] == L'C')
						{
#ifdef LOGGING
							logFile << "writing toc for " << fd.cFileName << "\n";
#endif // LOGGING
							StringCchCopy(baseDir, MAX_PATH, path);
							StringCchCat(baseDir, MAX_PATH, fd.cFileName);
							StringCchCat(baseDir, MAX_PATH, "\\");
							StringCchCopy(tocPath, MAX_PATH, baseDir);
							StringCchCat(tocPath, MAX_PATH, "PCConsoleTOC.bin");
							writeTOC(tocPath, baseDir, true);
						}
					}
				} while (FindNextFile(hFind, &fd) != 0);
				FindClose(hFind);
			}
#ifdef LOGGING
			else
			{
				logFile << "DLC directory not found!\n";
			}
#endif // LOGGING
			LOG_DURATION("finished all DLC TOCs")
		}

#if defined LOGGING || defined PROFILING
	logFile << "\ndone\n";
	logFile.flush();
#endif // LOGGING
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

static unsigned HashFileName(const char* strToHash, int len)
{
	unsigned hash = 0;

	for (int i = 0; i < len; ++i)
	{
		hash = hash >> 8 & 0x00FFFFFF ^ crcTable[(hash ^ static_cast<BYTE>(toupper(strToHash[i]))) & 0x000000FF];
		hash = hash >> 8 & 0x00FFFFFF ^ crcTable[hash & 0x000000FF];
	}
	return hash;
}

void writeTOC(TCHAR tocPath[MAX_PATH], TCHAR baseDir[MAX_PATH], const bool isDLC)
{
	vector<fileData> files;
#ifdef LOGGING
	logFile << "getting files..\n";
#endif // LOGGING
	if (isDLC)
	{
		getFiles(files, baseDir, "");
	}
	else
	{
#ifdef LEGENDARY_EDITION
		if (Game == MEGame::LE1)
		{
			getLE1Files(files, baseDir);
		}
		else
		{
			getFiles(files, baseDir, "BioGame\\");
			getFiles(files, baseDir, "Engine\\");
		}
#else
		getFiles(files, baseDir, "BIOGame\\");
#endif

	}

	LOG_DURATION("got file list for TOC: " << tocPath)
#ifdef LOGGING
		logFile << "\ncalculating hash table..\n";
#endif // LOGGING
	size_t tableSize = files.size();
	const size_t minTableSize = tableSize / 2;
	std::set<unsigned> uniques;
	while (true)
	{
		for (auto file_data : files)
		{
			uniques.insert(file_data.hash % tableSize);
		}
		if (tableSize - uniques.size() <= tableSize / 4)
		{
			break;
		}
		tableSize -= tableSize / 4;
		if (tableSize <= minTableSize)
		{
			break;
		}
		uniques.clear();
	}

#ifdef LOGGING
	logFile << tableSize << " buckets for " << files.size() << " entries. " << uniques.size() << " buckets filled.\n";
#endif // LOGGING

	vector<vector<fileData>> buckets(tableSize);

	for (auto file_data : files)
	{
		buckets[file_data.hash % tableSize].emplace_back(file_data);
	}

	LOG_DURATION("created hash table")

#ifdef LOGGING
		logFile << "\nwriting file data..\n";
#endif // LOGGING
	ofstream toc;
	toc.open(tocPath, ios::out | ios::binary | ios::trunc);
	//header
	write(toc, int(0x3AB70C13)); //magic number
	write(toc, int(0)); //zero

	write(toc, int(tableSize));

	int entryPos = tableSize * 8;
	int tablePos = 0;
	//write the table
	for (int i = 0; i < buckets.size(); ++i)
	{
		auto bucket = buckets[i];
		if (bucket.empty())
		{
			write(toc, int(0));
			write(toc, int(0));
		}
		else
		{
			write(toc, int(entryPos - tablePos));
			write(toc, int(bucket.size()));

			for (auto file_data : bucket)
			{
				//size of entry: everything before the string, the stringlength, and the null character
				ushort entryLength = ushort(0x1D + file_data.fileName.length());
#ifdef LEGENDARY_EDITION
				auto pad = (4 - entryLength % 4) % 4;
				entryLength += pad;
#endif
				entryPos += entryLength;
			}
		}
		tablePos += 8;

	}
	streampos lastentrySizePos = 0;
	//write the entries
	for (int i = 0; i < buckets.size(); ++i)
	{
		auto bucket = buckets[i];
		for (auto file_data : bucket)
		{
			const int stringLength = file_data.fileName.length();
			//size of entry: everything before the string, the stringlength, and the null character
			ushort entryLength = ushort(0x1D + stringLength);
#ifdef LEGENDARY_EDITION
			auto pad = (4 - entryLength % 4) % 4;
			entryLength += pad;
#endif
			lastentrySizePos = toc.tellp();
			write(toc, entryLength);
			write(toc, static_cast<ushort>(0));
			write(toc, file_data.fileSize);
			write(toc, int(0));
			write(toc, int(0));
			write(toc, int(0));
			write(toc, int(0));
			write(toc, int(0));
			toc.write(file_data.fileName.c_str(), stringLength);
			write(toc, BYTE(0));
#ifdef LEGENDARY_EDITION
			//align
			for (; pad > 0; --pad)
			{
				write(toc, BYTE(0));
			}
#endif	
		}
	}

	if (lastentrySizePos > 0)
	{
		toc.seekp(lastentrySizePos);
		//last entry doesn't have to have a size for some reason
		write(toc, ushort(0));
	}
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

void getFiles(vector<fileData>& files, TCHAR* basepath, TCHAR* searchPath) {
	WIN32_FIND_DATA fd;
	LARGE_INTEGER fileSize;
	HANDLE hFind = INVALID_HANDLE_VALUE;
	TCHAR enumeratePath[MAX_PATH];
	TCHAR tmpPath[MAX_PATH];
	TCHAR* ext;
	StringCchCopy(enumeratePath, MAX_PATH, basepath);
	StringCchCat(enumeratePath, MAX_PATH, searchPath);
	StringCchCat(enumeratePath, MAX_PATH, "*");

#ifdef LOGGING
	logFile << "\nenumerating files..\n";
#endif // LOGGING
	hFind = FindFirstFile(enumeratePath, &fd);
	do
	{
		StringCchCopy(tmpPath, MAX_PATH, searchPath);
		StringCchCat(tmpPath, MAX_PATH, fd.cFileName);
		if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			// 0 means true for lstrcmp
			if (lstrcmp(PathFindFileName(fd.cFileName), "DLC") != 0 &&
				lstrcmp(PathFindFileName(fd.cFileName), "Patches") != 0 &&
				lstrcmp(PathFindFileName(fd.cFileName), "Splash") != 0 &&
#ifdef LEGENDARY_EDITION 
				lstrcmp(PathFindFileName(fd.cFileName), "Config") != 0 &&
#endif
				lstrcmp(PathFindFileName(fd.cFileName), ".") != 0 &&
				lstrcmp(PathFindFileName(fd.cFileName), "..") != 0)
			{
#ifdef LOGGING
				logFile << "getting files from Directory:" << fd.cFileName << "\n";
#endif // LOGGING
				StringCchCat(tmpPath, MAX_PATH, "\\");
				getFiles(files, basepath, tmpPath);
			}

		}
		else
		{
			//allowed = { "*.pcc", "*.afc", "*.bik", "*.bin", "*.tlk", "*.txt", "*.cnd", "*.upk", "*.tfc", "*.isb", "*.usf" }
			ext = PathFindExtension(tmpPath);
			if (lstrcmp(ext, ".pcc") == 0
				|| lstrcmp(ext, ".afc") == 0
				|| lstrcmp(ext, ".bik") == 0
				|| lstrcmp(ext, ".bin") == 0
				|| lstrcmp(ext, ".tlk") == 0
				|| lstrcmp(ext, ".txt") == 0
				|| lstrcmp(ext, ".cnd") == 0
				|| lstrcmp(ext, ".upk") == 0
				|| lstrcmp(ext, ".tfc") == 0
#ifdef LEGENDARY_EDITION
				|| lstrcmp(ext, ".isb") == 0
				|| lstrcmp(ext, ".usf") == 0
				|| lstrcmp(ext, ".ini") == 0
				|| lstrcmp(ext, ".dlc") == 0
#endif

				)
			{
				fileSize.HighPart = fd.nFileSizeHigh;
				fileSize.LowPart = fd.nFileSizeLow;
				string fileName = tmpPath;
#ifdef LOGGING
				logFile << "found file: ";
				logFile.flush();
				logFile.write(fileName.c_str(), fileName.size() + 1);
				logFile << "\n";
#endif // LOGGING
				files.emplace_back(fileName, int(fileSize.QuadPart), HashFileName(fd.cFileName, strlen(fd.cFileName)));
			}

		}
	} while (FindNextFile(hFind, &fd) != 0);
	FindClose(hFind);
}

#ifdef LEGENDARY_EDITION
struct caseInsensitiveCmp {
	bool operator() (const std::string& lhs, const std::string& rhs) const {
		return lstrcmpi(lhs.c_str(), rhs.c_str()) < 0;
	}
};

void addToMap(std::map<std::string, std::pair<std::string, int>, caseInsensitiveCmp>& fileMap, TCHAR* basepath, TCHAR* searchPath)
{
	WIN32_FIND_DATA fd;
	LARGE_INTEGER fileSize;
	TCHAR enumeratePath[MAX_PATH];
	TCHAR tmpPath[MAX_PATH];
	TCHAR* ext;
	StringCchCopy(enumeratePath, MAX_PATH, basepath);
	StringCchCat(enumeratePath, MAX_PATH, searchPath);
	StringCchCat(enumeratePath, MAX_PATH, "*");

#ifdef LOGGING
	logFile << "enumerating files: " << enumeratePath << "\n";
#endif // LOGGING
	HANDLE hFind = FindFirstFile(enumeratePath, &fd);
	do
	{
		StringCchCopy(tmpPath, MAX_PATH, searchPath);
		StringCchCat(tmpPath, MAX_PATH, fd.cFileName);
		if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			// 0 means true for lstrcmp
			if (lstrcmp(PathFindFileName(fd.cFileName), "DLC") != 0 &&
				lstrcmp(PathFindFileName(fd.cFileName), "Patches") != 0 &&
				lstrcmp(PathFindFileName(fd.cFileName), "Splash") != 0 &&
#ifdef LEGENDARY_EDITION 
				lstrcmp(PathFindFileName(fd.cFileName), "Config") != 0 &&
#endif
				lstrcmp(PathFindFileName(fd.cFileName), ".") != 0 &&
				lstrcmp(PathFindFileName(fd.cFileName), "..") != 0)
			{
#ifdef LOGGING
				logFile << "\ngetting files from Directory:" << fd.cFileName << "\n";
#endif // LOGGING
				StringCchCat(tmpPath, MAX_PATH, "\\");
				addToMap(fileMap, basepath, tmpPath);
			}

		}
		else
		{
			//allowed = { "*.pcc", "*.afc", "*.bik", "*.bin", "*.tlk", "*.txt", "*.cnd", "*.upk", "*.tfc", "*.isb", "*.usf" }
			ext = PathFindExtension(tmpPath);
			if (lstrcmp(ext, ".pcc") == 0
				|| lstrcmp(ext, ".afc") == 0
				|| lstrcmp(ext, ".bik") == 0
				|| lstrcmp(ext, ".bin") == 0
				|| lstrcmp(ext, ".tlk") == 0
				|| lstrcmp(ext, ".txt") == 0
				|| lstrcmp(ext, ".cnd") == 0
				|| lstrcmp(ext, ".upk") == 0
				|| lstrcmp(ext, ".tfc") == 0
#ifdef LEGENDARY_EDITION
				|| lstrcmp(ext, ".isb") == 0
				|| lstrcmp(ext, ".usf") == 0
				|| lstrcmp(ext, ".ini") == 0
				|| lstrcmp(ext, ".dlc") == 0
#endif

				)
			{
				fileSize.HighPart = fd.nFileSizeHigh;
				fileSize.LowPart = fd.nFileSizeLow;
				string fileName = tmpPath;
#ifdef LOGGING
				logFile << "found file: ";
				logFile.write(fileName.c_str(), fileName.size());
				logFile << "\n";
#endif // LOGGING
				fileMap[fd.cFileName] = std::make_pair(fileName, int(fileSize.QuadPart));
			}

		}
	} while (FindNextFile(hFind, &fd) != 0);
	FindClose(hFind);
}

void getLE1Files(vector<fileData>& files, TCHAR* basepath)
{
	std::map<int, std::string> dlcMount;
	map<int, std::string> dlcFriendlyNames;
	TCHAR enumeratePath[MAX_PATH];
	TCHAR dlcPath[MAX_PATH];
	WIN32_FIND_DATA fd;
	TCHAR tmpPath[MAX_PATH];
	std::map<std::string, std::pair<std::string, int>, caseInsensitiveCmp> fileMap;
	bool dlcFound = false;

	StringCchCopy(dlcPath, MAX_PATH, basepath);
	StringCchCat(dlcPath, MAX_PATH, "BioGame\\DLC\\");
	StringCchCopy(enumeratePath, MAX_PATH, dlcPath);
	StringCchCat(enumeratePath, MAX_PATH, "*");

#ifdef LOGGING
	logFile << "\nfinding LE1 DLCs... \n";
#endif // LOGGING
	HANDLE hFind = FindFirstFile(enumeratePath, &fd);
	do
	{
		if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY && strlen(fd.cFileName) > 4
			&& fd.cFileName[0] == L'D' && fd.cFileName[1] == L'L' && fd.cFileName[2] == L'C' && fd.cFileName[3] == L'_')
		{
			StringCchCopy(tmpPath, MAX_PATH, dlcPath);
			StringCchCat(tmpPath, MAX_PATH, fd.cFileName);
			StringCchCat(tmpPath, MAX_PATH, "\\AutoLoad.ini");
			const auto fileAttributes = GetFileAttributes(tmpPath);
			if (fileAttributes != INVALID_FILE_ATTRIBUTES)
			{
				const string iniPath(tmpPath);
				IniFile autoLoad(iniPath);
				auto strMount = autoLoad.readValue("ME1DLCMOUNT", "ModMount");
				if (!strMount.empty())
				{
					int mount = 0;
					try
					{
						mount = std::stoi(strMount);
					}
					catch (...)
					{
						mount = 0;
					}
					if (mount > 0)
					{
						dlcMount[mount] = fd.cFileName;
						dlcFriendlyNames[mount] = autoLoad.readValue("ME1DLCMOUNT", "ModName");
						dlcFound = true;
#ifdef LOGGING
						logFile << "Registered " << fd.cFileName << " (" << dlcFriendlyNames[mount] << ")  at mount: " << mount << "\n";
#endif // LOGGING
						continue;
					}
				}
#ifdef LOGGING
				logFile << fd.cFileName << " has an invalid AutoLoad.ini! exiting...\n";
#endif // LOGGING
				const std::string message = " has an improperly formatted AutoLoad.ini! Try re-installing the mod. If that doesn't fix the problem, contact the mod author.\n\nMass Effect will now close.";
				MessageBox(nullptr,
					(fd.cFileName + message).c_str(),
					"Mass Effect LE - Broken Mod Warning",
					MB_OK | MB_ICONERROR);
				exit(1);
			}
#ifdef LOGGING
			logFile << fd.cFileName << " does not have an AutoLoad.ini, skipping...\n";
#endif // LOGGING
		}
	} while (FindNextFile(hFind, &fd) != 0);
	FindClose(hFind);

#ifdef LOGGING
	if (!dlcFound)
	{
		logFile << "\nNo DLC found\n";
	}
	logFile << "\nbuilding file map... \n";
#endif // LOGGING

	addToMap(fileMap, basepath, "BioGame\\");
	addToMap(fileMap, basepath, "Engine\\");

	std::ofstream loadOrderTxt(string(dlcPath) + "LoadOrder.Txt");
	loadOrderTxt << "This is an auto-generated file for informational purposes only. Editing it will not change the load order.\n\n";
	if (dlcFound)
	{
		for (auto&& pair : dlcMount)
		{
			std::string dlcName = pair.second;
			StringCchCopy(tmpPath, MAX_PATH, "BioGame\\DLC\\");
			StringCchCat(tmpPath, MAX_PATH, dlcName.c_str());
			StringCchCat(tmpPath, MAX_PATH, "\\");
			addToMap(fileMap, basepath, tmpPath);

			loadOrderTxt << pair.first << ": " << dlcName << " (" << dlcFriendlyNames[pair.first] << ")" << std::endl;
		}
	}
	else
	{
		loadOrderTxt << "No valid dlc found!";
	}
	loadOrderTxt.close();

#ifdef LOGGING
	logFile << "\nLE1 TOC in text form: \n\n";
#endif // LOGGING
	for (auto&& pair : fileMap)
	{
#ifdef LOGGING
		logFile << pair.second.first << "    " << pair.second.second << "\n";
#endif // LOGGING
		files.emplace_back(pair.second.first, pair.second.second, HashFileName(pair.first.c_str(), pair.first.length()));
	}
}
#endif

