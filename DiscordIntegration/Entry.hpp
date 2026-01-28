#pragma once

#include <LESDK/Headers.hpp>
#include <LESDK/Init.hpp>
#include <SPI.h>


namespace DiscordIntegration
{
	extern ::LESDK::Initializer* HookManager;
    void InitializeGlobals();
}
