#pragma once

#include <LESDK/Headers.hpp>
#include <LESDK/Init.hpp>
#include <SPI.h>


namespace HotReload
{
    bool PatchMemory(void* address, const void* patch, const SIZE_T patchSize);
    void InitPatchMemory(::LESDK::Initializer& Init);
}
