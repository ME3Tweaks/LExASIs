#include "Common/Base.hpp"
#include "HotReload/Entry.hpp"
#include "HotReload/SharedVersion.h"

SPI_PLUGINSIDE_SUPPORT(SDK_TARGET_NAME_W ASI_NAME_NO_SPACE_W, DEVELOPER_W, L"" VERSION_STRING_W, SPI_GAME_SDK_TARGET, SPI_VERSION_ANY);
SPI_PLUGINSIDE_POSTLOAD;
SPI_PLUGINSIDE_ASYNCATTACH;

SPI_IMPLEMENT_ATTACH
{
    ::LESDK::Initializer Init{ InterfacePtr, SDK_TARGET_NAME_A ASI_NAME_NO_SPACE_A };
    ::HotReload::InitPatchMemory(Init);
    return true;
}

SPI_IMPLEMENT_DETACH
{
    LEASI_UNUSED(InterfacePtr);
#ifdef _DEBUG
    ::LESDK::TerminateConsole();
#endif
    return true;
}

#define CREATEFILEW_FLAG_ADDR ::LESDK::Address::FromOffset(0xa4072)

namespace HotReload
{
    /**
     * \brief Patches a segment of process memory.
     * \param address The address to start the overwrite of patch data to
     * \param patch The patch data
     * \param patchSize The size of patch
     * \return true if patched, false otherwise
     */
    bool PatchMemory(void* address, const void* patch, const SIZE_T patchSize)
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

    void InitPatchMemory(::LESDK::Initializer& Init)
    {
        void* PatchOffset = Init.Resolve(CREATEFILEW_FLAG_ADDR);
        const BYTE patchData[] = { 0x03 }; // Original in 0x1 : READ SHARE (NOT WRITE). 3 is 0x1 | 0x2 which is READ WRITE SHARE
        if (PatchOffset != nullptr)
        {
            if (!PatchMemory(PatchOffset, patchData, 1)) {
				MessageBoxW(NULL, L"Failed to patch memory for hot reload support.", L"Hot Reload", MB_ICONERROR);
            }
        }
    }
}
