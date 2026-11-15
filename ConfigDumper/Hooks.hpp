#pragma once

#include <LESDK/Headers.hpp>

namespace ConfigDumper
{
	// ! UEngine::Exec hook for the manual trigger command.
	// ========================================

	using t_UEngine_Exec = unsigned(UEngine* Context, wchar_t* cmd, void* unk);
	extern t_UEngine_Exec* UEngine_Exec_orig;
	unsigned UEngine_Exec_hook(UEngine* Context, wchar_t* cmd, void* unk);
}
