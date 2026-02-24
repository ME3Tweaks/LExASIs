#include "ShaderClient/PipeHandler.hpp"

namespace ShaderClient
{
	HANDLE pipeHandle = nullptr;

	HANDLE CreateHostPipe() {
		if (pipeHandle == nullptr || pipeHandle == INVALID_HANDLE_VALUE) {
			// Create pipe handleNDLE
			pipeHandle = CreateFileW(L"\\\\.\\pipe\\milanshaderhost", GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
		}
		return pipeHandle;
	}

	void CloseHostPipe() {
		if (pipeHandle != nullptr && pipeHandle != INVALID_HANDLE_VALUE) {
			CloseHandle(pipeHandle);
			pipeHandle = nullptr;
		}
	}
}