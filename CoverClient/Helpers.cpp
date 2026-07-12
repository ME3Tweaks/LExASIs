#include "CoverClient/Helpers.hpp"

namespace CoverClient
{
    void SetVTable(void** objectStartAddress, void* newVTableStartAddress) {
        *objectStartAddress = newVTableStartAddress; // Set the vtable pointer to the new table
    }

    void* GetVTable(void*** objectStartAddress) {
        return **objectStartAddress;
    }

	tStaticConstructObject* static_construct_object = nullptr;
	tSingleLineCheck* SingleLineCheck = nullptr;

	std::string ws2s(const std::wstring& wstr)
	{
		if (wstr.empty()) return {};
		int size = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), nullptr, 0, nullptr, nullptr);
		std::string result(size, 0);
		WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), result.data(), size, nullptr, nullptr);
		return result;
	}

	std::wstring s2ws(const std::string& str)
	{
		if (str.empty()) return {};
		int size = MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), nullptr, 0);
		std::wstring result(size, 0);
		MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), result.data(), size);
		return result;
	}
}