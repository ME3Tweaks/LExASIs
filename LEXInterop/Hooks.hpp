#pragma once

#include <LESDK/Headers.hpp>
#include <LESDK/Init.hpp>
#include "Common/Base.hpp"

namespace LEXInterop
{
    // ProcessEvent hook for monitoring game state and processing actions
    using t_UObject_ProcessEvent = void(UObject* Context, UFunction* Function, void* Parms, void* Result);
    extern t_UObject_ProcessEvent* UObject_ProcessEvent_orig;
    void UObject_ProcessEvent_hook(UObject* Context, UFunction* Function, void* Parms, void* Result);

    // FindPackageFile hook for loading precached packages
    using t_FindPackageFile = unsigned(void* This, wchar_t* InName, FGuid* Guid, FString* OutFileName, const wchar_t* Language, unsigned Unk);
    extern t_FindPackageFile* FindPackageFile_orig;
    unsigned FindPackageFile_hook(void* This, wchar_t* InName, FGuid* Guid, FString* OutFileName, const wchar_t* Language, unsigned Unk);

    // SetLinker hook for tracking file origins of assets
    using t_SetLinker = void(UObject* Context, ULinkerLoad* Linker, int LinkerIndex);
    extern t_SetLinker* SetLinker_orig;
    void SetLinker_hook(UObject* Context, ULinkerLoad* Linker, int LinkerIndex);
}
