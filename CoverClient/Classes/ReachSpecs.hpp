#pragma once
#include "LESDK/Headers.hpp"

namespace CoverClient {
	/// <summary>
	/// ReachSpec defines a connection between two nodes in the pathfinding grid
	/// </summary>
	class ReachSpec {
	public:
		int index;
		float radius;
		float height;

		ReachSpec() = default;
		ReachSpec(int idx, float r, float h);

		virtual const char* ClassName() const;
	};


	class SlotToSlotReachSpec : public ReachSpec {
	public:
		SlotToSlotReachSpec() = default;
		SlotToSlotReachSpec(int idx, float r, float h);
		virtual const char* ClassName() const override;
	};
}