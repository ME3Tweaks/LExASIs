#pragma once
#include "CoverClient/Classes/CoverLink.hpp"
#include "CoverClient/Classes/CoverSlotMarker.hpp"

namespace CoverClient {
    extern std::vector<CoverLink*> coverLinks;
	void GenerateCoverFromCamera();
	void DrawCover();
}