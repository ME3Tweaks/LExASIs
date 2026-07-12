#include "Common/Base.hpp"
#include "Common/Objects.hpp"
#include "LESDK/Headers.hpp"
#include "CoverClient/CoverGeneration.hpp"
#include "CoverClient/Classes/CoverLink.hpp"
#include "CoverClient/Utilities/PathUtilities.hpp"
#include "CoverClient/PipeHandler.hpp"

namespace CoverClient {
	static void WriteSlotToHost(CoverSlotMarker* marker);

	void CoverLink::WriteNodeToHost() const {
		NavigationPoint::WriteNodeToHost();

		// Write Slot Count.
		const DWORD slotsSize = (DWORD) slots.size();
		WriteInt(slotsSize);

		// Write each slot.
		for(const auto& slot : slots) {
			// Perform pre-serialization to the pipe
			// Compute the LocationOffset.
			slot->slot->LocationOffset = CalculateLocationOffset(this->position, this->rotation, slot->position);
			slot->slot->RotationOffset = FRotator(
				slot->rotation.Pitch - this->rotation.Pitch,
				slot->rotation.Yaw - this->rotation.Yaw,
				slot->rotation.Roll - this->rotation.Roll
			);



			// Write the index in the node list
			// This is so coverhost can look it up.
			// We must serialize the coverslotmarker before coverlink or this won't work.
			WriteInt(slot->networkIndex);

			// Write the slot information
			WriteSlotToHost(slot);
		}
	}

	const char* CoverLink::GetNavName() const {
		return "CoverLink";
	}

	void CoverLink::AddCoverSlot(CoverSlotMarker* marker) {
		slots.push_back(marker);
	}

	static void WriteSlotToHost(CoverSlotMarker* marker) {
		const FCoverSlot& slot = *marker->slot;
		auto pipeHandle = CreateHostPipe();
		DWORD numBytesWritten = 0;

		// ECoverAction enum names indexed by byte value (ECoverAction)
		static const char* const ECoverActionNames[] = {
			"CA_Default", "CA_BlindLeft", "CA_BlindRight", "CA_LeanLeft", "CA_LeanRight",
			"CA_PopUp", "CA_BlindUp", "CA_PeekLeft", "CA_PeekRight", "CA_PeekUp",
			"CA_SwatTurn", "CA_Aimback"
		};

		// Write 'Actions'
		// FCoverSlot::Actions stores byte enum values; receiver reads UnrealString enum names
		{
			DWORD count = (DWORD)slot.Actions.Count();
			WriteInt(count);
			for (const auto& actionByte : slot.Actions) {
				const char* name = (actionByte < ARRAYSIZE(ECoverActionNames)) ? ECoverActionNames[actionByte] : "CA_Default";
				WriteStringA(name);
			}
		}

		// Write 'FireLinks'
		{
			//DWORD count = (DWORD)slot.FireLinks.Count();
			//WriteInt(count);
			/*for (const auto& link : slot.FireLinks) {
				DWORD interactionCount = (DWORD)link.Interactions.Count();
				WriteInt(interactionCount);
				for (const auto& interaction : link.Interactions) {
					BYTE b = interaction;
					WriteFile(pipeHandle, &b, 1, &numBytesWritten, NULL);
				}
				WriteFile(pipeHandle, &link.PackedProperties_CoverPairRefAndDynamicInfo, 4, &numBytesWritten, NULL);
				BYTE bFallbackLink = link.bFallbackLink ? 1 : 0;
				WriteFile(pipeHandle, &bFallbackLink, 1, &numBytesWritten, NULL);
				BYTE bDynamicIndexInited = link.bDynamicIndexInited ? 1 : 0;
				WriteFile(pipeHandle, &bDynamicIndexInited, 1, &numBytesWritten, NULL);
			}*/

			// Write out our computed firelink data instead.
			auto it = computedFireLinks.find(marker);
			if (it != computedFireLinks.end()) {
				const auto& fireLinks = it->second;
				DWORD count = (DWORD)fireLinks.size();
				WriteInt(count);
				// This is very slow; we should have everything indexed
				for (const auto& link : fireLinks) {
					int targetIndex = -1;
					for (size_t i = 0; i < allNetworkNodes.size(); i++) {
						if (allNetworkNodes[i].get() == link.targetLink) {
							targetIndex = (int)i;
							break;
						}
					}
					WriteInt(targetIndex);
					WriteInt(link.targetSlotIndex);

					DWORD interactionCount = (DWORD)link.interactions.size();
					WriteInt(interactionCount);
					for (const auto& interaction : link.interactions) {
						WriteFile(pipeHandle, &interaction.srcCoverType, 1, &numBytesWritten, NULL);
						WriteFile(pipeHandle, &interaction.srcAction, 1, &numBytesWritten, NULL);
						WriteFile(pipeHandle, &interaction.dstCoverType, 1, &numBytesWritten, NULL);
						WriteFile(pipeHandle, &interaction.dstAction, 1, &numBytesWritten, NULL);
					}
				}
			} else {
				WriteInt(0);
			}
		}

		// Write 'ExposedCoverPackedProperties'
		{
			DWORD count = (DWORD)slot.ExposedCoverPackedProperties.Count();
			WriteInt(count);
			for (const auto& val : slot.ExposedCoverPackedProperties) {
				WriteFile(pipeHandle, &val, 4, &numBytesWritten, NULL);
			}
		}

		// Write 'DangerCoverPackedProperties'
		{
			DWORD count = (DWORD)slot.DangerCoverPackedProperties.Count();
			WriteInt(count);
			for (const auto& val : slot.DangerCoverPackedProperties) {
				WriteFile(pipeHandle, &val, 4, &numBytesWritten, NULL);
			}
		}

		// Skip 'SlipTarget', 'SlipRefs', 'OverlapClaims' - receiver does not read these from the wire (TODO on receiver side)

		// Write 'MantleTarget'
		// A SlotIdx of -1 indicates no mantle target
		{
			BYTE hasMantleTarget = (slot.MantleTarget.SlotIdx != -1) ? 1 : 0;
			WriteFile(pipeHandle, &hasMantleTarget, 1, &numBytesWritten, NULL);
			if (hasMantleTarget) {
				WriteFile(pipeHandle, &slot.MantleTarget.SlotIdx, 4, &numBytesWritten, NULL);
				WriteFile(pipeHandle, &slot.MantleTarget.Direction, 4, &numBytesWritten, NULL);
				WriteFile(pipeHandle, &slot.MantleTarget.Guid.A, 4, &numBytesWritten, NULL);
				WriteFile(pipeHandle, &slot.MantleTarget.Guid.B, 4, &numBytesWritten, NULL);
				WriteFile(pipeHandle, &slot.MantleTarget.Guid.C, 4, &numBytesWritten, NULL);
				WriteFile(pipeHandle, &slot.MantleTarget.Guid.D, 4, &numBytesWritten, NULL);
				// TYPE MISMATCH: MantleTarget.Actor is an AActor pointer; receiver expects an int32 package actor index.
				// Write 0 as placeholder - actor index resolution is not implemented.
				int actorIndex = 0;
				WriteFile(pipeHandle, &actorIndex, 4, &numBytesWritten, NULL);
			}
		}

		// Write 'LocationOffset'
		// Location offset is the offset from the location of the cover link to the location of the slot.
		WriteFile(pipeHandle, &slot.LocationOffset.X, 4, &numBytesWritten, NULL);
		WriteFile(pipeHandle, &slot.LocationOffset.Y, 4, &numBytesWritten, NULL);
		WriteFile(pipeHandle, &slot.LocationOffset.Z, 4, &numBytesWritten, NULL);

		// Write 'RotationOffset'
		// This is the slot rotation minues the cover link rotation, stored as a FRotator (Pitch, Yaw, Roll)
		WriteFile(pipeHandle, &slot.RotationOffset.Pitch, 4, &numBytesWritten, NULL);
		WriteFile(pipeHandle, &slot.RotationOffset.Yaw, 4, &numBytesWritten, NULL);
		WriteFile(pipeHandle, &slot.RotationOffset.Roll, 4, &numBytesWritten, NULL);

		// Skip 'SlotOwner' - always 0 at this stage, only set at runtime
		// Skip 'SlotMarker' - receiver resolves this from the CoverSlotMarker already sent

		// Write 'TurnTargetPackedProperties'
		WriteFile(pipeHandle, &slot.TurnTargetPackedProperties, 4, &numBytesWritten, NULL);

		// Write 'CoverTurnTargetPackedProperties'
		WriteFile(pipeHandle, &slot.CoverTurnTargetPackedProperties, 4, &numBytesWritten, NULL);

		// Write 'ExtraCost'
		WriteFile(pipeHandle, &slot.ExtraCost, 4, &numBytesWritten, NULL);

		// Skip 'LeanTraceDist' - receiver skips it

		// Write booleans as individual bytes (unpacked from FCoverSlot bitfields)
		auto WriteBool = [&](unsigned long bitfield) {
			BYTE b = bitfield ? 1 : 0;
			WriteFile(pipeHandle, &b, 1, &numBytesWritten, NULL);
		};
		WriteBool(slot.bLeanLeft);
		WriteBool(slot.bLeanRight);
		WriteBool(slot.bForceCanPopUp);
		WriteBool(slot.bCanPopUp);
		WriteBool(slot.bCanMantle);
		WriteBool(slot.bCanClimbUp);
		WriteBool(slot.bForceCanCoverSlip_Left);
		WriteBool(slot.bForceCanCoverSlip_Right);
		WriteBool(slot.bCanCoverSlip_Left);
		WriteBool(slot.bCanCoverSlip_Right);
		WriteBool(slot.bCanSwatTurn_Left);
		WriteBool(slot.bCanSwatTurn_Right);
		WriteBool(slot.bCanCoverTurn_Left);
		WriteBool(slot.bCanCoverTurn_Right);
		WriteBool(slot.bEnabled);
		WriteBool(slot.bAllowPopup);
		WriteBool(slot.bAllowMantle);
		WriteBool(slot.bAllowCoverSlip);
		WriteBool(slot.bAllowClimbUp);
		WriteBool(slot.bAllowSwatTurn);
		WriteBool(slot.bAllowCoverTurn);
		WriteBool(slot.bForceNoGroundAdjust);
		WriteBool(slot.bPlayerOnly);
		WriteBool(slot.bUnsafeCover);
		WriteBool(slot.bFailedToFindSurface);

		// Write 'ForceCoverType' as UnrealString - receiver applies this to both ForceCoverType and CoverType
		// TYPE MISMATCH: C++ stores byte enum value; receiver expects UnrealString enum name
		static const char* const ECoverTypeNames[] = { "CT_None", "CT_Standing", "CT_MidLevel" };
		const char* coverTypeName = (slot.ForceCoverType < ARRAYSIZE(ECoverTypeNames)) ? ECoverTypeNames[slot.CoverType] : "CT_None";
		WriteStringA(coverTypeName);
	}
}