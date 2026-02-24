#include "MilanShaderCompilerShared.h"
#include "LESDK/Headers.hpp"

NamedParameterAllocation CreateNamedParameterAllocationW(
	const wchar_t* name,
	uint16_t bufferIndex,
	uint16_t baseIndex,
	uint16_t size,
	uint16_t samplerIndex)
{
	NamedParameterAllocation param;
	param.Allocation.BufferIndex = bufferIndex;
	param.Allocation.BaseIndex = baseIndex;
	param.Allocation.Size = size;
	param.Allocation.SamplerIndex = samplerIndex;
	wcscpy(param.Name, name);
	return param;
}

NamedParameterAllocation CreateNamedParameterAllocation(
	const char* name,
	uint16_t bufferIndex,
	uint16_t baseIndex,
	uint16_t size,
	uint16_t samplerIndex)
{
	auto const ansi_len = static_cast<UINT>(strlen(name));
	auto const wide_len = LESDK::GetWideLengthAnsi(name, ansi_len);
	std::wstring wide_str(wide_len, L'\0');

	DWORD error = 0;
	if (LESDK::EncodeWideFromAnsi(name, ansi_len, wide_str.data(), wide_len, &error)) {
		return CreateNamedParameterAllocationW(
			wide_str.c_str(),
			bufferIndex,
			baseIndex,
			size,
			samplerIndex
		);
	}

	// Fallback: return a zero-initialized allocation with an empty name on failure
	return CreateNamedParameterAllocationW(L"", bufferIndex, baseIndex, size, samplerIndex);
}
