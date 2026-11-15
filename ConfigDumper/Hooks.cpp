#include "ConfigDumper/Hooks.hpp"
#include "ConfigDumper/ConfigDump.hpp"
#include "Common/Base.hpp"

namespace ConfigDumper
{
	// ! Command parsing helper
	// ========================================

	bool IsCmd(wchar_t** cmd, const wchar_t* check)
	{
		const size_t len = wcslen(check);
		if (_wcsnicmp(*cmd, check, len) == 0)
		{
			*cmd += len;
			return true;
		}
		return false;
	}

	// ! UEngine::Exec hook
	// ========================================

	// Console command "DUMPRUNTIMECONFIG" -- run this only after DLC mount has finished, otherwise
	// the dump will not reflect DLC config deltas that haven't been applied yet.
	t_UEngine_Exec* UEngine_Exec_orig = nullptr;
	unsigned UEngine_Exec_hook(UEngine* Context, wchar_t* cmd, void* unk)
	{
		// Try the original handler first, same convention as ConsoleExtension.
		if (UEngine_Exec_orig(Context, cmd, unk))
		{
			return TRUE;
		}

		wchar_t* cursor = cmd;
		while (*cursor == L' ' || *cursor == L'\t')
		{
			cursor++;
		}

		if (IsCmd(&cursor, L"DUMPRUNTIMECONFIG"))
		{
			DumpRuntimeConfig();
			return TRUE;
		}

		if (IsCmd(&cursor, L"DUMPRESOLVEDCONFIG"))
		{
			DumpResolvedConfig();
			return TRUE;
		}

		return FALSE;
	}
}
