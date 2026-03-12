#include "CoverClient/Client.hpp"
#include "CoverClient/PathfindingGeneration.hpp"
#include "CoverClient/PipeHandler.hpp"
#include "CoverClient/Utilities/PathUtilities.hpp"

namespace CoverClient
{

	void SendNodesToCoverHost() {

		// PersistentLevel -> TheWorld -> ContainingPackage
		auto worldName = (*GWorld)->PersistentLevel->Outer->Outer->Name.GetName();
		LEASI_INFO("WorldName: {}", worldName);
		WriteCommand(CoverHostCommand::LevelName);
		WriteStringA(worldName);

		// Write out the node data
		DWORD numBytesWritten = 0;
		// Send a command byte first so the host knows what to expect
		WriteCommand(CoverHostCommand::PathfindingNodeList);
		// Then send the number of nodes
		DWORD nodeCount = static_cast<DWORD>(allNetworkNodes.size());
		LEASI_DEBUG(L"STREAM: NodeCount: {}", nodeCount);
		WriteFile(CreateHostPipe(), &nodeCount, 4, &numBytesWritten, NULL);

		// Then send each node's data
		for (const auto& node : allNetworkNodes) {
			node->WriteNodeToHost();
		}
	}
}