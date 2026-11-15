#pragma once

#include <LESDK/Headers.hpp>
#include <LESDK/Init.hpp>
#include <SPI.h>


namespace ShaderClient
{
    // Applies the LE3/LMP early-startup compatibility patch for the minimum-resolution guard.
    void DisableMinimumResolutionGuard(::LESDK::Initializer& Init);
    void StopResolutionDialogWatchdog();

    void InitializeGlobals(::LESDK::Initializer& Init);
    void InitializeHooks(::LESDK::Initializer& Init);

    // Terminates the game immediately.
    void ExitGame();
}
