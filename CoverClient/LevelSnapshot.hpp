#pragma once

#include <LESDK/Headers.hpp>

namespace CoverClient
{
	// ! Live level -> snapshot file -> pathfinding package
	// ========================================
	//
	// Everything the generated network consists of now exists as REAL ENGINE OBJECTS in the
	// running level: ACoverLinks with fully analysed FCoverSlots, ACoverSlotMarkers, APathNodes,
	// UReachSpecs, and the four ULevel reference tables the engine's own FindCoverReference /
	// FindNavReference allocated entries in. Serialisation is the step that turns that into a
	// `<Level>_Pathfinding.pcc`.
	//
	// It reads the LIVE OBJECTS, not the client-side node model in `allNetworkNodes`. That is
	// the whole point of this file, and it is a correction rather than a preference.
	//
	// Twice now -- for `Slot.Actions`, then for `Slot.FireLinks` -- a pass was written, was
	// correct, ran over the wrapper graph on its way to MilanCoverHost, and never touched a
	// live FCoverSlot. Both times the symptom was cover the AI would not enter, and both times
	// the computation had been right all along. A design with two graphs keeps producing that
	// bug, and serialisation is the last place the second graph still exists -- and the one
	// place where the failure is silent, because a package that is missing a field still loads.
	//
	// So the model is not extended to carry the new fields. The snapshot is taken from the
	// objects the game itself is using, which cannot disagree with what was tested in-game.
	//
	// ! Why the packed properties survive the trip
	// ========================================
	//
	// FireLinks, ExposedCoverPackedProperties, DangerCoverPackedProperties and both turn
	// targets store indices into ULevel::CoverIndexPairs / NavRefIndicies (see LevelTables.hpp).
	// The snapshot ships those four tables ALONGSIDE the slots, with the actor pointers in them
	// rewritten to snapshot node indices. So the host never re-derives a packed value and never
	// has to understand what one means: it writes the ints through unchanged and maps the tables
	// from node index to export UIndex. The old host-side `ResolveFireLinks` -- which rebuilt
	// the pairs table from scratch and had to guess -- goes away entirely.
	//
	// The in-memory and on-disk shapes of those tables agree, which is what makes this legal:
	// LegendaryExplorerCore parses them as `List<UIndex> CoverLinkRefs`,
	// `List<CoverIndexPair> CoverIndexPairs`, `List<UIndex> NavRefs`, `List<int> NavRefIndicies`.
	//
	// ! Why a file and not the pipe
	// ========================================
	//
	// The existing wire format is two hand-matched sequences of WriteFile / Read* calls with no
	// framing and no version, written in two repos -- note 01 calls it the single most fragile
	// part of the project, and note 04 Phase 6 wants it framed before it grows. It is about to
	// grow by an order of magnitude.
	//
	// A file gets the framing for free and buys three things the pipe cannot:
	//
	//   * A whole-level sweep is minutes of work inside one frozen Exec call. Losing it to a
	//     host-side exception, and having to re-sweep to try the next host fix, is the slowest
	//     possible iteration loop. Written once, a snapshot feeds the host as many times as the
	//     host needs.
	//   * The host can be developed, run and debugged with the game shut.
	//   * Two snapshots can be diffed. "The same sweep, before and after this change" is
	//     otherwise not a question that can be asked.
	//
	// The pipe stays for EditorPipe's live pose push, which genuinely is a stream.

	/// cc.snapshot [path] -- writes the live navigation network to a .milansnap file.
	///
	/// Run it after the network is built and tested in-game: cc.sweep, cc.pathnodes,
	/// cc.buildfirelinks / cc.buildexposedcover / cc.builddangercover / cc.slotdefaults.
	/// Whatever the AI was using a moment ago is exactly what gets written.
	///
	/// With no argument the file lands next to MassEffect3.exe in `MilanSnapshots\`, named for
	/// the persistent level.
	void CmdWriteLevelSnapshot(const wchar_t* pathOrNull);
}
