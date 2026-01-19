#pragma once

#include <string>
#include <sstream>
#include <cstring>
#include <Windows.h>

#include <LESDK/Headers.hpp>

namespace LEXInterop
{
    // WM_COPYDATA identifiers for LEX communication
    constexpr unsigned long SENT_FROM_LE1 = 0x02AC00C7;
    constexpr unsigned long SENT_FROM_LE2 = 0x02AC00C6;
    constexpr unsigned long SENT_FROM_LE3 = 0x02AC00C5;

    // Convert narrow string to wide string
    inline std::wstring s2ws(const std::string& str)
    {
        if (str.empty()) return std::wstring();
        int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), nullptr, 0);
        std::wstring wstr(size_needed, 0);
        MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), wstr.data(), size_needed);
        return wstr;
    }

    // Convert wide string to narrow string
    inline std::string ws2s(const std::wstring& wstr)
    {
        if (wstr.empty()) return std::string();
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), nullptr, 0, nullptr, nullptr);
        std::string str(size_needed, 0);
        WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), str.data(), size_needed, nullptr, nullptr);
        return str;
    }

    // Check if a command string starts with a prefix and advance past it
    inline bool IsCmd(char** command, const char* prefix)
    {
        const size_t len = strlen(prefix);
        if (strncmp(*command, prefix, len) == 0)
        {
            *command += len;
            return true;
        }
        return false;
    }

    // Check if a string starts with a prefix
    inline bool StartsWith(const char* prefix, const std::string& str)
    {
        return str.rfind(prefix, 0) == 0;
    }

    // Get the game-specific COPYDATA identifier
    inline unsigned long GetGameCopydataId()
    {
#if defined(SDK_TARGET_LE1)
        return SENT_FROM_LE1;
#elif defined(SDK_TARGET_LE2)
        return SENT_FROM_LE2;
#elif defined(SDK_TARGET_LE3)
        return SENT_FROM_LE3;
#endif
    }

    // Get the game-specific pipe name
    inline const wchar_t* GetPipeName()
    {
#if defined(SDK_TARGET_LE1)
        return L"\\\\.\\pipe\\LEX_LE1_COMM_PIPE";
#elif defined(SDK_TARGET_LE2)
        return L"\\\\.\\pipe\\LEX_LE2_COMM_PIPE";
#elif defined(SDK_TARGET_LE3)
        return L"\\\\.\\pipe\\LEX_LE3_COMM_PIPE";
#endif
    }

    // Send a string message to Legendary Explorer via WM_COPYDATA
    inline void SendStringToLEX(const std::wstring& message, UINT timeout = 100)
    {
        HWND handle = FindWindowW(nullptr, L"Legendary Explorer");
        if (handle)
        {
            COPYDATASTRUCT cds{};
            cds.dwData = GetGameCopydataId();
            cds.cbData = static_cast<DWORD>((message.length() + 1) * sizeof(wchar_t));
            cds.lpData = const_cast<wchar_t*>(message.c_str());
            SendMessageTimeoutW(handle, WM_COPYDATA, 0, reinterpret_cast<LPARAM>(&cds), 0, timeout, nullptr);
        }
    }

    // Get the containing map name for an object (outermost package)
    inline SFXName GetContainingMapName(const UObject* object)
    {
        const UObject* obj = object;
        SFXName name{};
        while (obj->Class && obj->Outer)
        {
            obj = obj->Outer;
            name = obj->Name;
        }
        return name;
    }

    // FString output stream operator for use with wstringstream
    inline std::wostream& operator<<(std::wostream& out, FString const& fString)
    {
        if (fString.Chars() && fString.Length() > 0)
        {
            out.write(fString.Chars(), fString.Length());
        }
        return out;
    }

    inline bool PatchMemory(void* address, const void* patch, const SIZE_T patchSize)
    {
        //make the memory we're going to patch writeable
        DWORD  oldProtect;
        if (!VirtualProtect(address, patchSize, PAGE_EXECUTE_READWRITE, &oldProtect))
            return false;

        //overwrite with our patch
        memcpy(address, patch, patchSize);

        //restore the memory's old protection level
        VirtualProtect(address, patchSize, oldProtect, &oldProtect);
        FlushInstructionCache(GetCurrentProcess(), address, patchSize);
        return true;
    }

    // Extract next space-delimited parameter from command string
    inline std::string GetCommandParam(std::string& commandStr)
    {
        const auto spacePos = commandStr.find(' ', 0);
        if (spacePos == std::string::npos)
        {
            return commandStr;
        }
        auto param = commandStr.substr(0, spacePos);
        commandStr = commandStr.substr(spacePos + 1);
        return param;
    }
}
