#pragma once

#include <LESDK/Headers.hpp>

namespace CoverClient
{
	// ! The level's cover / nav reference tables
	// ========================================
	//
	// Every packed property on an FCoverSlot -- fire links, exposed cover, danger cover, both
	// turn targets -- stores an INDEX, not an actor. The tables those indices point into live
	// on the ULevel, and they are the hinge the whole serialisation turns on: the packed ints
	// are meaningless without them, and they are meaningless in a different order.
	//
	// `ACoverLink::GetCoverInfoFromPackedRef` (vtable slot 284, LE3 0x7FF74AD38ED0) is the
	// engine's own decoder for bits 0-15 of every one of those fields, and it is short enough
	// to transcribe whole:
	//
	//     Level = CoverLink->Outer                          // UObject::Outer, 0x40
	//     if (refIdx < 0 || refIdx >= *(int*)(Level+0x2CC)) return false;
	//     pairs = *(void**)(Level+0x2C4);
	//     linkIdx = *(int*)(pairs + refIdx*8);
	//     if (linkIdx < 0 || linkIdx >= *(int*)(Level+0x2BC)) return false;
	//     out.Link    = *(ACoverLink**)(*(void**)(Level+0x2B4) + linkIdx*8);
	//     out.SlotIdx = *(BYTE*)(pairs + refIdx*8 + 4);
	//
	// So 0x2B4 is ULevel::CoverLinkRefs and 0x2C4 is ULevel::CoverIndexPairs, both plain
	// 16-byte TArrays (Data, Count, Max at +0/+8/+12), and a pair is {int; BYTE; pad[3]}.
	// That matches the shipped level binary exactly: LegendaryExplorerCore parses
	// BioD_CerMir_125Land's Level as 49 CoverLinkRefs and 158 CoverIndexPairs of
	// {CoverIndexIdx, SlotIdx}, and note 08's decode of that same level bounded
	// CoverIndexPairs at >136 and NavIndices at >210 -- 158 and 211.
	//
	// A packed ref is TWO HOPS, not one: refIdx -> CoverIndexPairs[refIdx] -> {link index,
	// slot} -> CoverLinkRefs[link index]. Danger cover takes the other pair of tables --
	// NavRefIndicies then NavRefs -- and crossing the two yields a network that loads without
	// complaint and behaves wrongly.
	//
	// The file format carries all four under these same names (LegendaryExplorerCore's
	// Level.cs: `List<UIndex> CoverLinkRefs`, `List<CoverIndexPair> CoverIndexPairs`,
	// `List<UIndex> NavRefs`, `List<int> NavRefIndicies`), which is what makes a verbatim
	// runtime dump of them a valid thing to serialise. Note the on-disk cover/nav ref lists
	// are plain object indices with no Guid: the Guid-carrying entries are the separate
	// CrossLevel*GuidRefs maps, for refs whose target lives in another level.
	//
	// Caveat worth keeping: these are the *level's* tables. If the level streams out and back
	// the appended entries go with it, and anything that re-resolves cross-level cover refs is
	// reading an array we grew. Fine for a harness, not for shipping.
	//
	// These offsets exist in exactly one place on purpose. Two copies that can drift is the
	// failure mode note 04 asks us to avoid, and a wrong offset here does not crash -- it
	// produces cover the AI silently never picks.

	constexpr size_t k_levelCoverLinkRefs = 0x2B4;      // TArray<ACoverLink*>,     stride 8
	constexpr size_t k_levelCoverIndexPairs = 0x2C4;    // TArray<FCoverIndexPair>, stride 8
	constexpr size_t k_levelNavRefs = 0x2E4;            // TArray<ANavigationPoint*>, stride 8
	constexpr size_t k_levelNavRefIndicies = 0x2F4;     // TArray<int>,             stride 4

	constexpr size_t k_vtableFindNavReference = 281;           // ACoverLink::FindNavReference
	constexpr size_t k_vtableGetNavRefForIdx = 282;            // ACoverLink::GetNavRefForIdx
	constexpr size_t k_vtableFindCoverReference = 283;         // ACoverLink::FindCoverReference
	constexpr size_t k_vtableGetCoverInfoFromPackedRef = 284;  // the decoder transcribed above

	/// ULevel::CoverIndexPairs element. Reflected structs in this engine pack to 4, so the
	/// BYTE sits at +4 and the whole thing is 8 -- which is the stride the decompilation
	/// indexes with.
	struct FCoverIndexPair
	{
		int CoverIndexIdx;
		unsigned char SlotIdx;
		unsigned char Pad[3];
	};
	static_assert(sizeof(FCoverIndexPair) == 8, "CoverIndexPairs stride must stay 8");

	/// The arrays live past the end of LESDK's ULevel (its reflected properties stop at
	/// 0x3A4 with the tail lumped into UnknownData01), so they are reached by offset.
	template <typename T>
	TArray<T>* LevelArrayAt(UObject* level, size_t offset)
	{
		if (level == nullptr) { return nullptr; }
		return reinterpret_cast<TArray<T>*>(
			reinterpret_cast<unsigned char*>(level) + offset);
	}

	/// A spawned actor is outered to the level it was spawned into, which is the same level
	/// the engine resolves its packed refs against.
	inline UObject* LevelOf(AActor* actor)
	{
		return (actor != nullptr) ? actor->Outer : nullptr;
	}
}
