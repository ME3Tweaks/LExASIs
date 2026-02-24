#pragma once

namespace ShaderClient
{
	// Opens a pipe to MilanShaderHost
	HANDLE CreateHostPipe();

	// Closes the pipe to MilanShaderHost
	void CloseHostPipe();
}