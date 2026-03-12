#include "CoverClient/Classes/PathfindingNode.hpp"
#include "CoverClient/Utilities/PathUtilities.hpp"
#include <LESDK/Headers.hpp>
#include "Common/Base.hpp"


namespace CoverClient {
	const char* PathNode::GetNavName() const {
		return "PathNode";
	}

	void PathNode::WriteNodeToHost() const {
		NavigationPoint::WriteNodeToHost();

		// Write custom data here.
	}
}