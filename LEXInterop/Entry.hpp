#pragma once

#include <LESDK/Headers.hpp>
#include <LESDK/Init.hpp>
#include <SPI.h>

namespace LEXInterop
{
    void InitializeGlobals(::LESDK::Initializer& Init);
    void InitializeHooks(::LESDK::Initializer& Init);
    void StartPipeThread();
    void StopPipeThread();
}
