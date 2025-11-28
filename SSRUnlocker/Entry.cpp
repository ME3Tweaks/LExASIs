#include <LESDK/Common/Common.hpp>
#include <LESDK/Init.hpp>
#include <SPI.h>

#include "Common/Base.hpp"


#ifndef SDK_TARGET_LE1
    // This targets the only game with environment reflection probes.
    #error SSRUnlocker target only supports LE1 at the moment.
#endif

SPI_PLUGINSIDE_SUPPORT(L"LE1SSRUnlocker", L"d00telemental", L"0.2.0", SPI_GAME_LE1, SPI_VERSION_ANY);
SPI_PLUGINSIDE_POSTLOAD;
SPI_PLUGINSIDE_ASYNCATTACH;


// ! Logging macro.
// ========================================

#ifdef _DEBUG
    #define writeln(frmt, ...)   fwprintf_s(stdout, L"LE1SSRUnlocker - " frmt "\n", __VA_ARGS__);
#else
    #define writeln(frmt, ...)
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
#ifdef _DEBUG
    LESDK::InitializeConsole();
#endif

    LESDK::Initializer Context(InterfacePtr, "LE1SSRUnlocker");
    // writeln(L"Attach - hello from LE1GPUCrusher v2!");

    auto* const ReflectionUpdateRequired = Context.Resolve(REFLECTIONUPDATEREQUIRED_PAT);
    if (ReflectionUpdateRequired == nullptr) [[unlikely]]
    {
        writeln(L"Attach - failed to find the pattern for 'ReflectionUpdateRequired'");
        return false;
    }

    ReflectionUpdateRequired_orig = (tReflectionUpdateRequired*)Context.InstallHook("ReflectionUpdateRequired", ReflectionUpdateRequired, ReflectionUpdateRequired_hook);
    if (ReflectionUpdateRequired_orig == nullptr) [[unlikely]]
    {
        writeln(L"Attach - failed to hook 'ReflectionUpdateRequired'");
        return false;
    }

    return true;
}

// --------------------------------------------------
SPI_IMPLEMENT_DETACH
{
    (void)InterfacePtr;

#ifdef _DEBUG
    LESDK::TerminateConsole();
#endif

    return true;
}
