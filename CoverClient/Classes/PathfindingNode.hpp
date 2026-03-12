#pragma once

#include "CoverClient/Classes/NavigationPoint.hpp"
#include <LESDK/Headers.hpp>
#include <vector>

namespace CoverClient {

	/// <summary>
	/// Describes a single standard path node in the level
	/// </summary>
	class PathNode : public NavigationPoint {
	public:
		PathNode(FVector position, FRotator rotation) : NavigationPoint(position, rotation) {};
		void WriteNodeToHost() const override;
		const char* GetNavName() const override;
	};
}