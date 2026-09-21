#include "CoverClient/LevelSnapshot.hpp"
#include "CoverClient/LevelTables.hpp"
#include "CoverClient/Helpers.hpp"
#include "Common/Base.hpp"
#include "Common/Objects.hpp"

#include <cstdio>
#include <string>
#include <unordered_map>
#include <vector>

#include <Windows.h>

namespace CoverClient
{
	namespace
	{
		// ! Format
		// ========================================
		//
		// Little-endian throughout, which is what both ends are. Every variable-length thing is
		// count-prefixed, and the header carries a magic and a version, so a host built against
		// the wrong revision says so instead of reading garbage -- the whole complaint note 01
		// makes about the pipe.
		//
		//   header      "MILANSNP", int version, string packageName, string levelName,
		//               int nodeCount, int coverLinkCount
		//   nodes       nodeCount x { kind, name, location, rotation, cylinder,
		//                             owning link + slot index, PathList }
		//   cover       coverLinkCount x { node index, link properties, slots }
		//   tables      CoverLinkRefs, CoverIndexPairs, NavRefs, NavRefIndicies
		//
		// Node references are always an index into the node table, or -1 for "this points at
		// something outside the snapshot". -1 is not a failure to be silently dropped: it means
		// the live network referenced an actor we are not writing, and the counts reported at
		// the end of the dump are how that gets noticed.
		constexpr char k_magic[8] = { 'M', 'I', 'L', 'A', 'N', 'S', 'N', 'P' };

		/// Bumped whenever the layout below changes in any way. The host refuses a version it
		/// does not know rather than reading the fields it expects out of the wrong offsets.
		constexpr int k_formatVersion = 1;

		enum class ENodeKind : int
		{
			Unknown = 0,
			PathNode = 1,
			CoverSlotMarker = 2,
			CoverLink = 3,
		};

		/// The two FCoverSlot flags that must never reach the package. `bSelected` is the
		/// editor's selection highlight and `bFailedToFindSurface` is a placement diagnostic;
		/// both are noise in a shipped level, and the whole 0xDC bitfield is written as one
		/// dword so they would otherwise ride along.
		constexpr unsigned int k_transientSlotFlags = 0x01000000u | 0x02000000u;

		/// FCoverSlot's boolean bitfield and ACoverLink's, neither of which LESDK gives a named
		/// member -- they are declared as individual `unsigned long : 1` bits, so the word has
		/// to be read by offset.
		constexpr size_t k_slotFlagsOffset = 0xDC;
		constexpr size_t k_linkFlagsOffset = 0x404;

		// ! Byte writer
		// ========================================

		class Writer final
		{
		public:
			void Raw(const void* data, size_t size)
			{
				const auto* const bytes = static_cast<const unsigned char*>(data);
				Bytes.insert(Bytes.end(), bytes, bytes + size);
			}

			void I32(int value) { Raw(&value, sizeof(value)); }
			void U32(unsigned int value) { Raw(&value, sizeof(value)); }
			void F32(float value) { Raw(&value, sizeof(value)); }
			void U8(unsigned char value) { Bytes.push_back(value); }

			/// int byte count, then UTF-8 bytes, no terminator. Narrow because every name in a
			/// UE3 package is ASCII in practice, and a length-prefixed byte run is the one
			/// string shape neither side can get wrong.
			void Str(const std::string& value)
			{
				I32(static_cast<int>(value.size()));
				if (!value.empty()) { Raw(value.data(), value.size()); }
			}

			void Vector(const FVector& v) { F32(v.X); F32(v.Y); F32(v.Z); }
			void Rotator(const FRotator& r) { I32(r.Pitch); I32(r.Yaw); I32(r.Roll); }

			const std::vector<unsigned char>& Data() const { return Bytes; }

		private:
			std::vector<unsigned char> Bytes;
		};

		// ! Small helpers
		// ========================================

		std::string NameOf(UObject* object)
		{
			if (object == nullptr) { return std::string{}; }
			const wchar_t* const data = *object->GetName();
			return (data != nullptr) ? ws2s(data) : std::string{};
		}

		std::string ClassNameOf(UObject* object)
		{
			if (object == nullptr || object->Class == nullptr) { return std::string{}; }
			return NameOf(object->Class);
		}

		ABioWorldInfo* GetWorldInfo()
		{
			auto* const engine = ::Common::FindFirstObject<USFXEngine, true>();
			return (engine == nullptr)
				? nullptr
				: static_cast<ABioWorldInfo*>(engine->GetCurrentWorldInfo());
		}

		ENodeKind KindOf(ANavigationPoint* point)
		{
			if (point == nullptr) { return ENodeKind::Unknown; }
			if (point->Cast<ACoverLink>() != nullptr) { return ENodeKind::CoverLink; }
			if (point->Cast<ACoverSlotMarker>() != nullptr) { return ENodeKind::CoverSlotMarker; }
			if (point->Cast<APathNode>() != nullptr) { return ENodeKind::PathNode; }
			return ENodeKind::Unknown;
		}

		/// Reads one of the two bitfield words described above.
		unsigned int FlagsAt(const void* object, size_t offset)
		{
			return *reinterpret_cast<const unsigned int*>(
				static_cast<const unsigned char*>(object) + offset);
		}

		// ! The node table
		// ========================================
		//
		// Built by walking WorldInfo::NavigationPointList, which is the chain the engine itself
		// enumerates navigation with and which Spawn threads every nav actor onto. Using it
		// rather than our own spawn lists means markers the ENGINE created on our behalf
		// (ACoverLink::AddMyMarker makes its own) are picked up without being tracked
		// separately.

		struct NodeTable
		{
			std::vector<ANavigationPoint*> Nodes;
			std::unordered_map<const void*, int> IndexOf;
			std::vector<ACoverLink*> Links;

			int Find(const void* actor) const
			{
				if (actor == nullptr) { return -1; }
				const auto it = IndexOf.find(actor);
				return (it == IndexOf.end()) ? -1 : it->second;
			}

			bool Add(ANavigationPoint* point)
			{
				if (point == nullptr || IndexOf.count(point) != 0) { return false; }
				IndexOf.emplace(point, static_cast<int>(Nodes.size()));
				Nodes.push_back(point);
				return true;
			}
		};

		/// Guard against a malformed chain looping back on itself. The nav list is spliced by
		/// hand in several places in this client, and an accidental self-reference there would
		/// otherwise hang the game inside a console command.
		constexpr int k_maxChainWalk = 100000;

		NodeTable CollectNodes(ABioWorldInfo* wi)
		{
			NodeTable table;
			if (wi == nullptr) { return table; }

			std::vector<ANavigationPoint*> chain;
			auto* node = wi->NavigationPointList;
			for (int i = 0; node != nullptr && i < k_maxChainWalk;
				i++, node = node->nextNavigationPoint)
			{
				chain.push_back(node);
			}

			// Markers before the link that owns them. The host writes exports in table order
			// and a CoverLink's slots reference its markers, so emitting the link first would
			// mean forward references in a format that deliberately has none.
			for (auto* const point : chain)
			{
				auto* const link = point->Cast<ACoverLink>();
				if (link == nullptr) { continue; }

				for (unsigned int s = 0; s < link->Slots.Count(); s++)
				{
					table.Add(link->Slots(s).SlotMarker);
				}
				// Links is the list the cover blocks are written from, so it has to stay in
				// step with the node table exactly: a link that somehow appeared on the chain
				// twice would otherwise get two blocks claiming the same node index.
				if (table.Add(link))
				{
					table.Links.push_back(link);
				}
			}

			// Then everything else, in chain order: path nodes, and any marker whose link is
			// not on the chain.
			for (auto* const point : chain)
			{
				table.Add(point);
			}
			return table;
		}

		// ! Writing
		// ========================================

		void WriteSpecs(Writer& w, const NodeTable& table, ANavigationPoint* point)
		{
			const unsigned int count = point->PathList.Count();
			w.I32(static_cast<int>(count));
			for (unsigned int i = 0; i < count; i++)
			{
				auto* const spec = point->PathList(i);
				if (spec == nullptr)
				{
					w.Str(std::string{});
					w.I32(-1); w.I32(0); w.I32(0); w.I32(0); w.I32(0); w.I32(-1);
					continue;
				}

				// The spec's CLASS is the move it describes -- ReachSpec for a walk,
				// SlotToSlotReachSpec for the cover chain, MantleReachSpec /
				// CoverSlipReachSpec / SwatTurnReachSpec / CoverTurnReachSpec for the special
				// moves. The host has to create an export of that exact class, so the name
				// goes over verbatim rather than being flattened to an enum we would then have
				// to keep in step at both ends.
				w.Str(ClassNameOf(spec));
				w.I32(table.Find(spec->End.Actor));
				w.I32(spec->Distance);
				w.I32(spec->CollisionRadius);
				w.I32(spec->CollisionHeight);
				w.I32(spec->reachFlags);

				// SpecDirection only exists on USlotToSlotReachSpec; -1 means "this class has
				// no direction", which is not the same as direction 0.
				auto* const slotSpec = spec->Cast<USlotToSlotReachSpec>();
				w.I32((slotSpec != nullptr) ? static_cast<int>(slotSpec->SpecDirection) : -1);
			}
		}

		void WriteNodes(Writer& w, const NodeTable& table)
		{
			for (auto* const point : table.Nodes)
			{
				w.I32(static_cast<int>(KindOf(point)));

				// The game's own name for the actor ("CoverLink_12"). Not used to build
				// anything -- it is what makes a snapshot readable next to a `profile cover
				// self` line or a cc.explainlink dump when the two disagree.
				w.Str(NameOf(point));
				w.Vector(point->location);
				w.Rotator(point->Rotation);

				auto* const cylinder = point->CylinderComponent;
				w.F32((cylinder != nullptr) ? cylinder->CollisionRadius : 0.0f);
				w.F32((cylinder != nullptr) ? cylinder->CollisionHeight : 0.0f);

				// A marker's owning link and slot. The host needs both to write OwningSlot,
				// and the link needs its markers' indices to write SlotMarker -- the same pair
				// of pointers seen from the two ends.
				auto* const marker = point->Cast<ACoverSlotMarker>();
				w.I32((marker != nullptr) ? table.Find(marker->OwningSlot.Link) : -1);
				w.I32((marker != nullptr) ? marker->OwningSlot.SlotIdx : -1);

				WriteSpecs(w, table, point);
			}
		}

		/// A cover reference -- MantleTarget, SlipTarget, OverlapClaims -- as (node, slot,
		/// direction). The runtime form carries a Guid as well, but a same-level reference
		/// resolves by object index, and the Guid half only matters for the cross-level maps
		/// this snapshot does not produce.
		void WriteCoverRef(Writer& w, const NodeTable& table, const FCoverReference& ref)
		{
			w.I32(table.Find(ref.Actor));
			w.I32(ref.SlotIdx);
			w.I32(ref.Direction);
		}

		void WriteSlot(Writer& w, const NodeTable& table, const FCoverSlot& slot)
		{
			// Actions -- ECoverAction bytes, verbatim. GetFireActions and BuildOtherLinks both
			// iterate this rather than a fixed list, so a slot's action set decides what its
			// fire links and danger entries could ever have been.
			w.I32(static_cast<int>(slot.Actions.Count()));
			for (unsigned int i = 0; i < slot.Actions.Count(); i++) { w.U8(slot.Actions(i)); }

			// FireLinks. The packed int goes through unchanged: it indexes CoverIndexPairs,
			// which is written out below, so re-deriving it here would be re-deriving a value
			// the engine already agreed with in-game.
			w.I32(static_cast<int>(slot.FireLinks.Count()));
			for (unsigned int i = 0; i < slot.FireLinks.Count(); i++)
			{
				const FFireLink& fireLink = slot.FireLinks(i);
				w.I32(fireLink.PackedProperties_CoverPairRefAndDynamicInfo);
				w.I32(static_cast<int>(fireLink.Interactions.Count()));
				for (unsigned int k = 0; k < fireLink.Interactions.Count(); k++)
				{
					w.U8(fireLink.Interactions(k));
				}
				w.U8(fireLink.bFallbackLink ? 1 : 0);
			}
			// RejectedFireLinks is CPF_Transient and is not written, deliberately.

			w.I32(static_cast<int>(slot.ExposedCoverPackedProperties.Count()));
			for (unsigned int i = 0; i < slot.ExposedCoverPackedProperties.Count(); i++)
			{
				w.I32(slot.ExposedCoverPackedProperties(i));
			}

			w.I32(static_cast<int>(slot.DangerCoverPackedProperties.Count()));
			for (unsigned int i = 0; i < slot.DangerCoverPackedProperties.Count(); i++)
			{
				w.I32(slot.DangerCoverPackedProperties(i));
			}

			w.I32(static_cast<int>(slot.SlipTarget.Count()));
			for (unsigned int i = 0; i < slot.SlipTarget.Count(); i++)
			{
				WriteCoverRef(w, table, slot.SlipTarget(i));
			}

			w.I32(static_cast<int>(slot.OverlapClaims.Count()));
			for (unsigned int i = 0; i < slot.OverlapClaims.Count(); i++)
			{
				WriteCoverRef(w, table, slot.OverlapClaims(i));
			}

			// MantleTarget is a single reference, not an array, and an empty one is an Actor of
			// null rather than a count of zero -- so it gets a presence byte.
			const bool bHasMantle = slot.MantleTarget.Actor != nullptr;
			w.U8(bHasMantle ? 1 : 0);
			if (bHasMantle) { WriteCoverRef(w, table, slot.MantleTarget); }

			w.Vector(slot.LocationOffset);
			w.Rotator(slot.RotationOffset);
			w.I32(table.Find(slot.SlotMarker));

			w.I32(slot.TurnTargetPackedProperties);
			w.I32(slot.CoverTurnTargetPackedProperties);
			w.I32(slot.ExtraCost);
			w.F32(slot.LeanTraceDist);

			// The whole 0xDC bitfield as one dword, minus the two transient bits. Sending the
			// engine's own word rather than twenty-five booleans means a flag cannot be lost by
			// being forgotten at one end: the host unpacks with the same bit table, which is
			// LESDK's FCoverSlot declaration.
			w.U32(FlagsAt(&slot, k_slotFlagsOffset) & ~k_transientSlotFlags);

			w.U8(slot.ForceCoverType);
			w.U8(slot.CoverType);
			w.U8(slot.LocationDescription);

			// SlotOwner (a live APawn) and SlotValidAfterTime (transient) are runtime claim
			// state and are deliberately absent.
		}

		void WriteCoverLinks(Writer& w, const NodeTable& table)
		{
			for (auto* const link : table.Links)
			{
				w.I32(table.Find(link));

				w.F32(link->MaxFireLinkDist);
				w.F32(link->AlignDist);
				w.F32(link->AutoCoverSlotInterval);
				w.F32(link->StandHeight);
				w.F32(link->MidHeight);
				w.F32(link->SlipDist);
				w.F32(link->TurnDist);
				w.F32(link->DangerScale);
				w.F32(link->InvalidateDistance);
				w.F32(link->CircularRadius);
				w.Vector(link->CircularOrigin);

				// The 0x404 bitfield, same reasoning as the slot flags. GLOBAL_bUseSlotMarkers
				// is a config global rather than per-link state, but it lives in this word, and
				// masking it out on the way through would be a decision the host cannot see.
				w.U32(FlagsAt(link, k_linkFlagsOffset));
				w.U8(link->LocationDescription);

				w.I32(static_cast<int>(link->Slots.Count()));
				for (unsigned int s = 0; s < link->Slots.Count(); s++)
				{
					WriteSlot(w, table, link->Slots(s));
				}
			}
		}

		/// How many table entries pointed at an actor outside the snapshot. Anything above zero
		/// means the network references cover or nav the package will not contain, which is a
		/// real finding rather than a rounding error -- so it is counted and reported instead
		/// of quietly written as -1.
		struct TableStats
		{
			int DanglingCoverRefs{ 0 };
			int DanglingNavRefs{ 0 };
		};

		TableStats WriteLevelTables(Writer& w, const NodeTable& table, UObject* level)
		{
			TableStats stats;

			if (level == nullptr)
			{
				// Four empty tables rather than a short file: the reader's offsets stay valid
				// and the missing data shows up as cover with no resolvable refs, which the
				// host can report. A truncated file would just look corrupt.
				LEASI_WARN("cc.snapshot: nav actors have no Outer level -- the reference "
					"tables cannot be read, and every packed ref will be dead.");
				w.I32(0); w.I32(0); w.I32(0); w.I32(0);
				return stats;
			}

			auto* const coverRefs = LevelArrayAt<ACoverLink*>(level, k_levelCoverLinkRefs);
			auto* const pairs = LevelArrayAt<FCoverIndexPair>(level, k_levelCoverIndexPairs);
			auto* const navRefs = LevelArrayAt<ANavigationPoint*>(level, k_levelNavRefs);
			auto* const navIdx = LevelArrayAt<int>(level, k_levelNavRefIndicies);

			w.I32(static_cast<int>(coverRefs->Count()));
			for (unsigned int i = 0; i < coverRefs->Count(); i++)
			{
				const int index = table.Find((*coverRefs)(i));
				if (index < 0) { stats.DanglingCoverRefs++; }
				w.I32(index);
			}

			w.I32(static_cast<int>(pairs->Count()));
			for (unsigned int i = 0; i < pairs->Count(); i++)
			{
				const FCoverIndexPair& pair = (*pairs)(i);
				w.I32(pair.CoverIndexIdx);
				w.I32(static_cast<int>(pair.SlotIdx));
			}

			w.I32(static_cast<int>(navRefs->Count()));
			for (unsigned int i = 0; i < navRefs->Count(); i++)
			{
				const int index = table.Find((*navRefs)(i));
				if (index < 0) { stats.DanglingNavRefs++; }
				w.I32(index);
			}

			w.I32(static_cast<int>(navIdx->Count()));
			for (unsigned int i = 0; i < navIdx->Count(); i++) { w.I32((*navIdx)(i)); }

			return stats;
		}

		// ! Output path
		// ========================================

		std::wstring DefaultSnapshotPath(const std::string& levelName)
		{
			wchar_t exePath[MAX_PATH] = {};
			const DWORD length = ::GetModuleFileNameW(nullptr, exePath, MAX_PATH);
			std::wstring directory = (length > 0 && length < MAX_PATH)
				? std::wstring{ exePath }
				: std::wstring{};
			const size_t slash = directory.find_last_of(L"\\/");
			if (slash != std::wstring::npos)
			{
				directory.resize(slash);
			}

			directory += L"\\MilanSnapshots";
			::CreateDirectoryW(directory.c_str(), nullptr);
			return directory + L"\\" + s2ws(levelName) + L".milansnap";
		}

		bool WriteToDisk(const std::wstring& path, const std::vector<unsigned char>& bytes)
		{
			FILE* file = nullptr;
			if (::_wfopen_s(&file, path.c_str(), L"wb") != 0 || file == nullptr)
			{
				return false;
			}
			const size_t written = bytes.empty()
				? 0
				: std::fwrite(bytes.data(), 1, bytes.size(), file);
			std::fclose(file);
			return written == bytes.size();
		}
	}

	void CmdWriteLevelSnapshot(const wchar_t* pathOrNull)
	{
		auto* const wi = GetWorldInfo();
		if (wi == nullptr)
		{
			LEASI_WARN("cc.snapshot: no WorldInfo -- is a level loaded?");
			return;
		}

		auto* const persistent = (*GWorld)->PersistentLevel;
		if (persistent == nullptr || persistent->Outer == nullptr
			|| persistent->Outer->Outer == nullptr)
		{
			LEASI_WARN("cc.snapshot: no PersistentLevel.");
			return;
		}

		// PersistentLevel -> TheWorld -> ContainingPackage, the same walk cc.sendtohost does.
		const std::string packageName = NameOf(persistent->Outer->Outer);

		const NodeTable table = CollectNodes(wi);
		if (table.Nodes.empty())
		{
			LEASI_WARN("cc.snapshot: WorldInfo.NavigationPointList is empty -- nothing to "
				"write. Run cc.sweep / cc.pathnodes first.");
			return;
		}

		// The tables belong to the level the nav actors are outered to, which is the one the
		// engine resolves their packed refs against. Taking it from a node rather than from
		// GWorld keeps those two the same object even when the network was built in a
		// streaming sublevel.
		UObject* const level = LevelOf(table.Nodes.front());

		Writer w;
		w.Raw(k_magic, sizeof(k_magic));
		w.I32(k_formatVersion);
		w.Str(packageName);
		w.Str(NameOf(persistent));
		w.I32(static_cast<int>(table.Nodes.size()));
		w.I32(static_cast<int>(table.Links.size()));

		WriteNodes(w, table);
		WriteCoverLinks(w, table);
		const TableStats stats = WriteLevelTables(w, table, level);

		const std::wstring path = (pathOrNull != nullptr && *pathOrNull != L'\0')
			? std::wstring{ pathOrNull }
			: DefaultSnapshotPath(packageName);

		if (!WriteToDisk(path, w.Data()))
		{
			LEASI_WARN(L"cc.snapshot: could not write {}", path);
			return;
		}

		int markers = 0;
		int pathNodes = 0;
		int slots = 0;
		int specs = 0;
		for (auto* const point : table.Nodes)
		{
			const ENodeKind kind = KindOf(point);
			if (kind == ENodeKind::CoverSlotMarker) { markers++; }
			else if (kind == ENodeKind::PathNode) { pathNodes++; }
			specs += static_cast<int>(point->PathList.Count());
		}
		for (auto* const link : table.Links)
		{
			slots += static_cast<int>(link->Slots.Count());
		}

		LEASI_INFO(L"cc.snapshot: wrote {}", path);
		LEASI_INFO("  {} bytes, format v{}", w.Data().size(), k_formatVersion);
		LEASI_INFO("  {} nodes: {} CoverLinks ({} slots), {} CoverSlotMarkers, {} PathNodes",
			table.Nodes.size(), table.Links.size(), slots, markers, pathNodes);
		LEASI_INFO("  {} ReachSpecs", specs);

		if (stats.DanglingCoverRefs > 0 || stats.DanglingNavRefs > 0)
		{
			// Expected on a level that shipped with its own cover: the level's tables hold its
			// entries as well as ours, and those actors are not ours to write. On
			// biop_covertest, which has neither cover nor pathfinding, anything here is a bug.
			LEASI_WARN("  {} CoverLinkRefs and {} NavRefs point outside the snapshot and were "
				"written as -1. Packed refs resolving through them will be dead in the package.",
				stats.DanglingCoverRefs, stats.DanglingNavRefs);
		}
	}
}
