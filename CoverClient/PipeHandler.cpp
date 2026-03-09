#include "CoverClient/PipeHandler.hpp"

namespace CoverClient
{
	HANDLE pipeHandle = nullptr;

	HANDLE CreateHostPipe() {
		if (pipeHandle == nullptr || pipeHandle == INVALID_HANDLE_VALUE) {
			// Create pipe handle
			pipeHandle = CreateFileW(L"\\\\.\\pipe\\milancoverhost", GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
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