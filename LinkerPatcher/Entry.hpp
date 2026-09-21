#pragma once

#include <LESDK/Headers.hpp>
#include <LESDK/Init.hpp>
#include "Common/Base.hpp"

namespace LinkerPatcher
{
    void InitializeGlobals(::LESDK::Initializer& Init);
    void InitializeHooks(::LESDK::Initializer& Init);
}
