#pragma once

namespace CoverClient
{
	// Opens a pipe to MilanShaderHost
	HANDLE CreateHostPipe();

	// Closes the pipe to MilanShaderHost
	void CloseHostPipe();
}