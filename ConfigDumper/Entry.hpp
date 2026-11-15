#pragma once

#include <SPI.h>
#include <LESDK/Headers.hpp>
#include <LESDK/Init.hpp>

namespace ConfigDumper
{
	// ! Initialization functions
	// ========================================

	void InitializeGlobals(::LESDK::Initializer& Init);
	void InitializeHooks(::LESDK::Initializer& Init);
}
