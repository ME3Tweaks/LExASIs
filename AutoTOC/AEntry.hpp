#pragma once

#include <algorithm>
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

// Forward declarations and typedefs
typedef unsigned short ushort;

// Struct definitions
struct fileData {
	std::string fileName;
	int fileSize;
	unsigned hash;

	fileData(const std::string fileName, const int fileSize, const unsigned hash) : fileName(fileName), fileSize(fileSize), hash(hash) {}
};

struct caseInsensitiveCmp {
	bool operator() (const std::string& lhs, const std::string& rhs) const {
		return lstrcmpi(lhs.c_str(), rhs.c_str()) < 0;
	}
};

// Function declarations
void write(std::ofstream& file, void* data, std::streamsize size);
void write(std::ofstream& file, BYTE data);
void write(std::ofstream& file, ushort data);
void write(std::ofstream& file, int data);
void writeTOC(TCHAR tocPath[MAX_PATH], TCHAR baseDir[MAX_PATH], bool isDLC);
void getFiles(std::vector<fileData>& files, TCHAR* basepath, TCHAR* searchPath);
void getLE1Files(std::vector<fileData>& files, TCHAR* basepath);
void addToMap(std::map<std::string, std::pair<std::string, int>, caseInsensitiveCmp>& fileMap, const std::filesystem::path& basepath, const std::string& searchPath);
void AutoToc(TCHAR path[MAX_PATH]);
static unsigned HashFileName(const char* strToHash, int len);

// CRC Table
extern const unsigned crcTable[];
