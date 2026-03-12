#pragma once
#include <LESDK/Headers.hpp>
#include "CoverClient/Classes/PathfindingNode.hpp"

#define PRINT_DEBUG_INFO false

namespace CoverClient
{
	enum class CoverHostCommand : BYTE
	{
		PathfindingNodeList = 1,
		LevelName = 2,
	};

	void SendNodesToCoverHost();
}