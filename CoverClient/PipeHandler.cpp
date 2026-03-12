#include "CoverClient/PipeHandler.hpp"

namespace CoverClient
{
	HANDLE _pipeHandle = nullptr;

	HANDLE CreateHostPipe() {
		if (_pipeHandle == nullptr || _pipeHandle == INVALID_HANDLE_VALUE) {
			// Create pipe handle
			_pipeHandle = CreateFileW(L"\\\\.\\pipe\\milancoverhost", GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
		}
		return _pipeHandle;
	}

	void CloseHostPipe() {
		if (_pipeHandle != nullptr && _pipeHandle != INVALID_HANDLE_VALUE) {
			CloseHandle(_pipeHandle);
			_pipeHandle = nullptr;
		}
	}

	/// <summary>
	/// Writes sized data to a pipe by first writing the data size, then the data itself.
	/// </summary>
	/// <param name="pipeHandle">The handle to the pipe to write to.</param>
	/// <param name="size">The size in bytes of the data to write.</param>
	/// <param name="data">A pointer to the data buffer to write.</param>
	void WriteSizedData(DWORD size, const void* data) {
		DWORD numBytesWritten = 0;
		WriteFile(CreateHostPipe(), &size, 4, &numBytesWritten, NULL);
		WriteFile(CreateHostPipe(), data, size, &numBytesWritten, NULL);
	}

	/// <summary>
	/// Writes a 32-bit integer value to a host pipe.
	/// </summary>
	/// <param name="value">The integer value to write to the pipe.</param>
	void WriteInt(const DWORD value) {
		DWORD numBytesWritten = 0;
		WriteFile(CreateHostPipe(), &value, 4, &numBytesWritten, NULL);
	}

	/// <summary>
	/// Writes a command to CoverHost telling it to switch reading modes.
	/// </summary>
	/// <param name="pipeHandle"></param>
	/// <param name="command"></param>
	void WriteCommand(CoverHostCommand command) {
		DWORD numBytesWritten = 0;
		BYTE cmd_sc = static_cast<BYTE>(command);
		WriteFile(CreateHostPipe(), &cmd_sc, 1, &numBytesWritten, NULL);
	}

	// Writes a narrow (ASCII/UTF-8) string in UnrealString format:
	//  - 4 byte length (DWORD) of the string in bytes, not including the null terminator
	//  - raw string bytes (no null terminator)
	void WriteStringA(const char* data) {
		if (data == nullptr) {
			DWORD zero = 0;
			DWORD num = 0;
			WriteFile(CreateHostPipe(), &zero, 4, &num, NULL);
			return;
		}

		DWORD numBytesWritten = 0;
		DWORD len = static_cast<DWORD>(std::strlen(data));
		// Prefix with 4-byte length (not including null)
		WriteFile(CreateHostPipe(), &len, 4, &numBytesWritten, NULL);
		if (len > 0) {
			WriteFile(CreateHostPipe(), data, len, &numBytesWritten, NULL);
		}
	}

	// Writes a wide (UTF-16) string in UnrealString format:
	//  - 4 byte signed value equal to -(length_in_chars * 2)
	//  - UTF-16LE string bytes (length_in_chars * 2), not including the null terminator
	void WriteStringW(const wchar_t* data) {
		if (data == nullptr) {
			int32_t zero = 0;
			DWORD num = 0;
			WriteFile(CreateHostPipe(), &zero, 4, &num, NULL);
			return;
		}

		DWORD numBytesWritten = 0;
		size_t charCount = std::wcslen(data);
		// Unreal expects a negative 4-byte value: -2 * number_of_characters
		int32_t prefix = -static_cast<int32_t>(charCount * sizeof(wchar_t));
		WriteFile(CreateHostPipe(), &prefix, 4, &numBytesWritten, NULL);
		if (charCount > 0) {
			DWORD byteCount = static_cast<DWORD>(charCount * sizeof(wchar_t));
			WriteFile(CreateHostPipe(), data, byteCount, &numBytesWritten, NULL);
		}
	}
}