#pragma once

#include <LESDK/Headers.hpp>
#include <LESDK/Init.hpp>
#include <map>
#include <SPI.h>

namespace Autoload
{
    void InitializeLogger();
    void InitializeGlobals(::LESDK::Initializer& Init);
    void InitializeHooks(::LESDK::Initializer& Init);
}
