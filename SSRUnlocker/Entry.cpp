#include <LESDK/Common/Common.hpp>
#include <LESDK/Init.hpp>
#include <LESDK/Headers.hpp>

#include <SPI.h>
#include "Common/Base.hpp"
#include "Common/ProxyConsoleLease.hpp"
#include "SSRUnlocker/SharedVersion.h"

#include <memory>
#include <string>
#include <string_view>

#define GAME_NAME_WITH_ASI_A SDK_TARGET_NAME_A ASI_NAME_NO_SPACE_A
#define GAME_NAME_WITH_ASI_W SDK_TARGET_NAME_W ASI_NAME_NO_SPACE_W

#ifndef SDK_TARGET_LE1
    // This targets the only game with environment reflection probes.
    #error SSRUnlocker target only supports LE1 at the moment.
#endif

SPI_PLUGINSIDE_SUPPORT(SDK_TARGET_NAME_W ASI_NAME_NO_SPACE_W, DEVELOPER_W, L"" VERSION_STRING_W, SPI_GAME_SDK_TARGET, SPI_VERSION_V4);
SPI_PLUGINSIDE_POSTLOAD;
SPI_PLUGINSIDE_ASYNCATTACH;

namespace SSRUnlocker
{
#ifdef _DEBUG
    std::shared_ptr<Common::ProxyConsoleLease> ConsoleLease;

    void WriteLine(std::wstring_view const message)
    {
        if (ConsoleLease == nullptr)
            return;

        int const utf8Length = WideCharToMultiByte(CP_UTF8, 0, message.data(), static_cast<int>(message.size()), nullptr, 0, nullptr, nullptr);
        if (utf8Length <= 0)
            return;

        std::string utf8(static_cast<size_t>(utf8Length), '\0');
        if (WideCharToMultiByte(CP_UTF8, 0, message.data(), static_cast<int>(message.size()), utf8.data(), utf8Length, nullptr, nullptr) == utf8Length)
            (void)ConsoleLease->Write(utf8);
    }
#endif
}


// ! Logging macro.
// ========================================

#ifdef _DEBUG
    #define writeln(message)     ::SSRUnlocker::WriteLine(GAME_NAME_WITH_ASI_W L" - " message L"\n");
#else
    #define writeln(message)
#endif


// ! ReflectionUpdateRequired hook.
// ========================================

#define REFLECTIONUPDATEREQUIRED_PAT ::LESDK::Address::FromPattern("48 89 5C 24 10 48 89 6C 24 18 48 89 74 24 20 57 41 54 41 55 41 56 41 57 48 83 EC 40 48 8B 01 48 8B F2 4C 8B F9 FF 50 48")
using tReflectionUpdateRequired = unsigned long(void*, void*);
tReflectionUpdateRequired* ReflectionUpdateRequired_orig = nullptr;
unsigned long ReflectionUpdateRequired_hook(void* a, void* b)
{
    (void)a; (void)b;
    // !!! DANGER !!!
    return TRUE;
}


// ! ASI Entrypoint.
// ========================================

SPI_IMPLEMENT_ATTACH
{
    LESDK::Initializer Context(InterfacePtr, GAME_NAME_WITH_ASI_A);
#ifdef _DEBUG
    ::SSRUnlocker::ConsoleLease = Common::ProxyConsoleLease::Create(Context, ASI_NAME_NO_SPACE_A);
#endif
    // writeln(L"Attach - hello from LE1GPUCrusher v2!");

    auto* const ReflectionUpdateRequired = Context.Resolve(REFLECTIONUPDATEREQUIRED_PAT);
    if (ReflectionUpdateRequired == nullptr) [[unlikely]]
    {
        writeln(L"Attach - failed to find the pattern for 'ReflectionUpdateRequired'");
#ifdef _DEBUG
        ::SSRUnlocker::ConsoleLease.reset();
#endif
        return false;
    }

    ReflectionUpdateRequired_orig = (tReflectionUpdateRequired*)Context.InstallHook("ReflectionUpdateRequired", ReflectionUpdateRequired, ReflectionUpdateRequired_hook);
    if (ReflectionUpdateRequired_orig == nullptr) [[unlikely]]
    {
        writeln(L"Attach - failed to hook 'ReflectionUpdateRequired'");
#ifdef _DEBUG
        ::SSRUnlocker::ConsoleLease.reset();
#endif
        return false;
    }

    return true;
}

// --------------------------------------------------
SPI_IMPLEMENT_DETACH
{
    (void)InterfacePtr;

#ifdef _DEBUG
    ::SSRUnlocker::ConsoleLease.reset();
#endif

    return true;
}
