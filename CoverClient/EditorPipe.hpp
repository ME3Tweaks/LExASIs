#pragma once

#include <LESDK/Headers.hpp>

namespace CoverClient
{
	// Attempts to send player pawn position and rotation to the pathfinding editor pipe.
	// Only transmits when the values differ from the last sent data.
	void UpdateEditorPlayerData(const FVector& location, const FRotator& rotation);

	// Attempts to send camera position, rotation, and FOV to the pathfinding editor pipe.
	// Only transmits when the values differ from the last sent data.
	void UpdateEditorCameraData(const FVector& location, const FRotator& rotation, float fov);
}
