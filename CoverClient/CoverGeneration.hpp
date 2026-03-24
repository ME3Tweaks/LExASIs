#pragma once
#include "CoverClient/Classes/CoverLink.hpp"
#include "CoverClient/Classes/CoverSlotMarker.hpp"
#include <unordered_map>
#include <vector>

namespace CoverClient {
	extern std::vector<CoverLink*> coverLinks;

	/// Describes one source/target cover type + action pair that has line of sight.
	struct FireLinkInteraction {
		unsigned char srcCoverType;  // 1=CT_Standing, 2=CT_MidLevel
		unsigned char srcAction;     // 3=CA_LeanLeft, 4=CA_LeanRight, 5=CA_PopUp
		unsigned char dstCoverType;
		unsigned char dstAction;
		FVector srcFirePos;          // World-space firing position for the source action
		FVector dstFirePos;          // World-space firing position for the destination action
	};

	// LINK COMPUTATION ======================
	/// Intermediate fire link result for a single target slot reachable from a source slot.
	struct ComputedFireLink {
		CoverLink* targetLink;
		int targetSlotIndex;
		std::vector<FireLinkInteraction> interactions;
	};

	/// Records how much a single threatening slot exposes a destination slot.
	struct DangerCoverEntry {
		CoverSlotMarker* srcSlot;   // The slot that threatens this slot
		float exposedScale;         // 0..1 danger level (1 = fully flanked/exposed)
	};

	/// Records danger from a navigation point to a cover slot.
	struct DangerNavEntry {
		NavigationPoint* node;
		float exposureScale;
	};

	/// Per-slot computed fire links, keyed by the source CoverSlotMarker.
	extern std::unordered_map<CoverSlotMarker*, std::vector<ComputedFireLink>> computedFireLinks;

	/// Per-slot exposed nodes: keyed by the destination slot, value is the list of threatening nav points and their exposure scales.
	extern std::unordered_map<CoverSlotMarker*, std::vector<DangerCoverEntry>> computedExposedCover;

	/// Per-slot danger info: keyed by the destination slot, value is the list of threatening slots and their exposure scales.
	extern std::unordered_map<CoverSlotMarker*, std::vector<DangerNavEntry>> computedDangerNavs;

	// ==========================================

	
	void GenerateCoverFromCamera();
	
	// Global compute
	void ComputeFireLinks();
	void ComputeDangerNavs();
	void ComputeExposedCover();

	// Single compute
	/// Computes fire links from a single source slot against all other eligible slots in coverLinks.
	std::vector<ComputedFireLink> ComputeFireLinksForSlot(CoverLink* srcLink, CoverSlotMarker* srcSlot, AActor* pawn);
	/// Computes exposed cover links which indicate which cover links/slots have line of sight to use nearby
	std::vector<DangerCoverEntry> ComputeExposedCoverForSlot(CoverSlotMarker* csm, AActor* pawn);
	/// Computes danger navs from a single source slot against other navigation points nearby
	std::vector<DangerNavEntry> ComputeDangerNavsForSlot(CoverLink* srcLink, CoverSlotMarker* srcSlot);

	FVector ComputeFirePosition(const CoverSlotMarker* csm, unsigned char action);
	FVector ComputeFirePosition(ACoverLink* link, int slotIdx, unsigned char action);

	void DrawCover();
}