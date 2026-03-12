#pragma once

#include "CoverClient/Client.hpp"

namespace CoverClient
{
	// Opens a pipe to MilanShaderHost
	HANDLE CreateHostPipe();

	// Closes the pipe to MilanShaderHost
	void CloseHostPipe();

	void WriteSizedData(DWORD size, const void* data);

	/// <summary>
	/// Writes a 32-bit integer to the host pipe.
	/// </summary>
	/// <param name="value"></param>
	void WriteInt(const DWORD value);

	/// <summary>
	/// Writes a command to CoverHost telling it to switch reading modes.
	/// </summary>
	/// <param name="pipeHandle"></param>
	/// <param name="command"></param>
	void WriteCommand(CoverHostCommand command);

	// Writes a narrow (ASCII/UTF-8) string in UnrealString format:
	//  - 4 byte length (DWORD) of the string in bytes, not including the null terminator
	//  - raw string bytes (no null terminator)
	void WriteStringA(const char* data);

	// Writes a wide (UTF-16) string in UnrealString format:
	//  - 4 byte signed value equal to -(length_in_chars * 2)
	//  - UTF-16LE string bytes (length_in_chars * 2), not including the null terminator
	void WriteStringW(const wchar_t* data);
	
}