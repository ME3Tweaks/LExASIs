#include "CoverClient/FireLinks.hpp"
#include "CoverClient/LevelTables.hpp"
#include "CoverClient/ScoutHarness.hpp"
#include "CoverClient/Helpers.hpp"
#include "Common/Base.hpp"
#include "Common/Objects.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace CoverClient
{
	namespace
	{
		// ! Gate constants -- UE3 UnCover.cpp:2066 GetFireActions, transcribed in note 03
		//
		// MaxFireLinkDist is read off the CDO rather than hardcoded: note 06 measured LE3's
		// value as 3000 against UE3's 2048, and a future game may differ again.
		constexpr float k_minFireLinkAngle = 0.45f;        // MINFIRELINKANGLE
		constexpr float k_minFireAngleDist = 128.0f;       // COVERLINK_MinFireAngleDist
		constexpr float k_defaultMaxFireLinkDist = 3000.0f;

		constexpr unsigned char k_ctNone = 0;
		constexpr unsigned char k_ctStanding = 1;
		constexpr unsigned char k_ctMidLevel = 2;

		constexpr unsigned char k_caDefault = 0;

		// ! Per-slot cap -- a runaway guard, sized from shipped data
		//
		// The UE3 gates bound WHICH pairs link, not HOW MANY, so in principle a dense room
		// could give a slot hundreds of links. Measured against three shipped levels
		// (note 06), that fear was overblown -- BioWare's own numbers are:
		//
		//   level                   slots  linked  total   mean/slot  median  max
		//   CerMir_125Land            132   80%     2605     19.7       21     70
		//   Kro001_605FourthPaths     540   76%    16718     31.0       34    139
		//   Cat003_550BalconyCombat   405   81%     7666     18.9       17     93
		//
		// So a couple of dozen per slot is NORMAL and 139 is shipped. The cap sits above the
		// observed maximum with headroom: on data shaped like BioWare's it never fires, and it
		// only exists so a pathological level cannot turn this pass into the problem. If the
		// warning does appear, that is evidence our gates are looser than theirs -- worth
		// investigating rather than quietly raising.
		//
		// (An earlier value of 16 was picked blind from a single quoted slot and would have
		// truncated 65% of CerMir's linked slots.)
		constexpr size_t k_maxFireLinksPerSlot = 192;

		// The ULevel cover/nav reference tables, the FCoverIndexPair layout and the vtable
		// slots that read and allocate in them all live in LevelTables.hpp -- serialisation
		// needs the identical offsets, and two copies of them would be free to drift.

		std::string NameOf(UObject* object)
		{
			if (object == nullptr) { return std::string{ "<null>" }; }
			const wchar_t* const data = *object->GetName();
			return (data != nullptr) ? ws2s(data) : std::string{};
		}


		bool ResolvePackedRef(ACoverLink* anyLink, int refIdx, FCoverInfo& out)
		{
			if (anyLink == nullptr) { return false; }
			using tGetCoverInfo = bool(ACoverLink*, int, FCoverInfo*);
			auto* const vtable = *reinterpret_cast<void***>(anyLink);
			auto* const fn = reinterpret_cast<tGetCoverInfo*>(
				vtable[k_vtableGetCoverInfoFromPackedRef]);
			out = FCoverInfo{};
			return fn(anyLink, refIdx, &out);
		}

		/// Index into ULevel::CoverIndexPairs for (target, slotIdx), allocated in `owner`'s
		/// level -- the value that goes in bits 0-15 of a packed ref. -1 if it cannot be had.
		///
		/// This is the engine's own find-or-add, `ACoverLink::FindCoverReference` (vtable 283,
		/// LE3 0x7FF74AD37AD0), the exact inverse of the slot-284 decoder. It appends `target`
		/// to CoverLinkRefs if absent, then appends {linkIdx, slotIdx} to CoverIndexPairs,
		/// growing both through ExpandTArray_ + FHeapAllocator::ResizeAllocation.
		///
		/// The first version of this file grew those two arrays by hand through LESDK's
		/// TArray. That worked only because sdkMalloc/sdkFree and the engine's FArray both
		/// route to GMalloc -- an assumption about an array we do not own. Calling the native
		/// removes it, and cannot disagree with the decoder about layout.
		///
		/// `owner` is the link whose slot will hold the reference, because FindCoverReference
		/// resolves its tables from `this->Outer` and the decoder later does the same on the
		/// link holding the FireLink. Passing the target here instead would allocate the index
		/// in the wrong level the moment the two are in different streaming levels.
		int FindOrAddCoverPairRef(ACoverLink* owner, ACoverLink* target, int slotIdx)
		{
			if (owner == nullptr || target == nullptr) { return -1; }
			if (slotIdx < 0 || slotIdx > 0xFF) { return -1; }

			using tFindCoverReference = int(ACoverLink*, ACoverLink*, unsigned int, int);
			auto* const vtable = *reinterpret_cast<void***>(owner);
			auto* const fn = reinterpret_cast<tFindCoverReference*>(
				vtable[k_vtableFindCoverReference]);

			const int refIdx = fn(owner, target, static_cast<unsigned int>(slotIdx), TRUE);
			// Bits 16-31 of the packed field are the DynamicLinkInfos index, so a ref that
			// does not fit in 16 bits cannot be encoded at all.
			return (refIdx >= 0 && refIdx <= 0xFFFF) ? refIdx : -1;
		}

		/// Bits 0-15 cover pair ref, bits 16-31 DynamicLinkInfos index. We build no dynamic
		/// cover, so the upper half is 0 -- the same value MilanCoverHost writes.
		constexpr int PackFireLinkRef(int coverPairRefIdx, int dynamicInfoIdx = 0)
		{
			return (coverPairRefIdx & 0xFFFF) | ((dynamicInfoIdx & 0xFFFF) << 16);
		}

		float GetMaxFireLinkDist()
		{
			auto* const cls = ACoverLink::StaticClass();
			if (cls != nullptr && cls->ClassDefaultObject != nullptr)
			{
				const float value = static_cast<ACoverLink*>(
					cls->ClassDefaultObject)->MaxFireLinkDist;
				if (value > 1.0f) { return value; }
			}
			return k_defaultMaxFireLinkDist;
		}

		// ! GetExposedInfo / BuildOtherLinks constants (note 03, note 08)
		constexpr float k_exposureDot = 0.4f;          // COVERLINK_ExposureDot
		constexpr float k_edgeCheckDot = 0.25f;        // COVERLINK_EdgeCheckDot
		constexpr float k_edgeExposureDot = 0.85f;     // COVERLINK_EdgeExposureDot
		constexpr float k_dangerDist = 1536.0f;        // UCONST_COVERLINK_DangerDist

		// Caps, sized the same way k_maxFireLinksPerSlot was -- above the shipped maximum
		// (exposed 51, danger 137 across 1077 slots) so they never bite on sane data.
		constexpr size_t k_maxExposedPerSlot = 128;
		constexpr size_t k_maxDangerPerSlot = 256;

		/// Index into ULevel::NavRefIndicies for `nav`, allocated in `owner`'s level.
		/// `ACoverLink::FindNavReference` (vtable 281, LE3 0x7FF74AD37C50) -- the nav-table
		/// twin of FindCoverReference, and the one BuildOtherLinks itself calls.
		///
		/// Danger cover indexes THIS table, not CoverIndexPairs. They are different arrays and
		/// getting them crossed yields a network that loads clean and behaves wrongly.
		int FindOrAddNavRef(ACoverLink* owner, ANavigationPoint* nav)
		{
			if (owner == nullptr || nav == nullptr) { return -1; }
			using tFindNavReference = int(ACoverLink*, ANavigationPoint*, int);
			auto* const vtable = *reinterpret_cast<void***>(owner);
			auto* const fn = reinterpret_cast<tFindNavReference*>(
				vtable[k_vtableFindNavReference]);
			const int refIdx = fn(owner, nav, TRUE);
			return (refIdx >= 0 && refIdx <= 0xFFFF) ? refIdx : -1;
		}

		/// Both exposed and danger cover pack a 0-255 scale into bits 16-23 and leave 24-31
		/// clear (verified over 4570 shipped entries, note 08).
		constexpr int PackScaledRef(int refIdx, int scale8)
		{
			return (refIdx & 0xFFFF) | ((scale8 & 0xFF) << 16);
		}

		void RotatorAxes(const FRotator& rot, FVector& axisX, FVector& axisY, FVector& axisZ)
		{
			const float pitchRad = UnrealRotationUnitsToRadians(rot.Pitch);
			const float yawRad = UnrealRotationUnitsToRadians(rot.Yaw);
			const float rollRad = UnrealRotationUnitsToRadians(rot.Roll);
			const float SP = std::sin(pitchRad), CP = std::cos(pitchRad);
			const float SY = std::sin(yawRad), CY = std::cos(yawRad);
			const float SR = std::sin(rollRad), CR = std::cos(rollRad);
			axisX = { CP * CY,                   CP * SY,                  SP };
			axisY = { SR * SP * CY - CR * SY,    SR * SP * SY + CR * CY,  -SR * CP };
			axisZ = { -(CR * SP * CY + SR * SY), CY * SR - CR * SP * SY,   CR * CP };
		}

		/// Writes a std::vector<int> into a slot's packed array without ever letting the
		/// engine array grow with elements in it. TArray<int> is trivially copyable so this is
		/// not strictly required, but doing it the same way everywhere means the one shape
		/// that IS unsafe (see ResetFireLinks) is never the odd one out.
		void WritePackedArray(TArray<int>& target, const std::vector<int>& values)
		{
			target.Clear();
			target.Shrink();
			if (values.empty()) { return; }
			target.Reserve(static_cast<unsigned int>(values.size()));
			for (const int value : values) { target.Add(value); }
		}

		/// UE3 builds the source action list from the slot's own capabilities and always adds
		/// CA_Default to the *destination* list -- a target can be shot at while just sitting
		/// in cover, but you cannot fire from a slot without leaning or popping up. `Actions`
		/// is already exactly that source list (the pass in CmdBuildLinks derives it the same
		/// way), so read it rather than re-deriving from the flags.
		void CollectSourceActions(const FCoverSlot& slot, std::vector<unsigned char>& out)
		{
			out.clear();
			const int count = static_cast<int>(slot.Actions.Count());
			for (int i = 0; i < count; i++)
			{
				out.push_back(slot.Actions(i));
			}
		}

		void CollectDestActions(const FCoverSlot& slot, std::vector<unsigned char>& out)
		{
			out.clear();
			out.push_back(k_caDefault);
			const int count = static_cast<int>(slot.Actions.Count());
			for (int i = 0; i < count; i++)
			{
				out.push_back(slot.Actions(i));
			}
		}

		// ! Why this file never lets FCoverSlot::FireLinks grow
		// ========================================
		//
		// LESDK's TArray grows through TArrayBase::DoRealloc, which sdkMallocs a RAW buffer
		// and then does:
		//
		//     std::move(OldData, OldData + MoveCount, Allocated);
		//
		// std::move is the *algorithm*: it move-ASSIGNS, `*dest = std::move(*src)`, into
		// memory that has never had a constructor run on it. For a trivially copyable element
		// that is harmless, which is why Actions (TArray<BYTE>) and PathList (TArray<UReachSpec*>)
		// have always been fine.
		//
		// FFireLink is not trivially copyable: it owns a TArray<BYTE> Interactions. Its
		// implicit move-assignment calls TArray::operator=(TArray&&), whose first act is
		// DoDestroyContents() on the destination -- reading a garbage Data pointer straight
		// out of fresh malloc and handing it to GMalloc->Free. That is an access violation in
		// ucrtbase on the second Add to any slot, and it is what crashed the first version of
		// this pass.
		//
		// Placement-new move-CONSTRUCTION is fine (TArray's move ctor std::exchanges the
		// source to null), and that is what Insert does once the capacity is there. So the
		// rule is: make the array exactly the right size while it is empty -- Reserve on a
		// zero-capacity array takes the DoAlloc path and moves nothing -- then append. Every
		// later Add finds NewCountItems <= CountMax and MoveCount == 0, so no FFireLink is
		// ever assigned over.
		//
		// Note Reserve on a NON-empty array is also unsafe here, and worse: it passes the
		// *capacity* as DoRealloc's OldCount, so it would move CountMax elements out of a
		// buffer where only CountItems are constructed. Hence the Shrink below -- Clear alone
		// keeps the buffer and would leave us on exactly that path.
		//
		// This is an LESDK limitation, not a CoverClient one. Anything writing an array of
		// non-trivial UE structs has to do the same dance.

		/// Returns a slot's FireLinks to a genuinely empty array -- destructed contents, freed
		/// buffer, zero capacity -- so the next Reserve allocates fresh instead of reallocating.
		void ResetFireLinks(FCoverSlot& slot)
		{
			slot.FireLinks.Clear();
			slot.FireLinks.Shrink();
		}

		/// One fire link, held in plain std:: storage until the exact count for a slot is
		/// known. Nothing constructs an FFireLink until then.
		struct PendingFireLink
		{
			int packedRef;
			float distSq;
			std::vector<unsigned char> interactions;
		};

		/// CanFireLinkHit. LE3 stripped the native, so this is the trace it performed:
		/// a zero-extent world line that stops at the first blocker.
		bool CanFireLinkHit(AActor* tracer, const FVector& from, const FVector& to)
		{
			if (SingleLineCheck == nullptr || GWorld == nullptr) { return false; }
			FCheckResult result{};
			FVector zeroExtent{};
			FVector start = from;
			FVector end = to;
			return SingleLineCheck(*GWorld, &result, tracer, &end, &start,
				TRACE_World | TRACE_StopAtAnyHit | TRACE_ComplexCollision,
				&zeroExtent, nullptr) == TRUE;
		}

		struct SlotRef
		{
			ACoverLink* link;
			int slotIdx;
			FVector location;
			FVector axisX;
		};

		void RotatorAxisX(const FRotator& rot, FVector& axisX)
		{
			const float pitchRad = UnrealRotationUnitsToRadians(rot.Pitch);
			const float yawRad = UnrealRotationUnitsToRadians(rot.Yaw);
			const float SP = std::sin(pitchRad), CP = std::cos(pitchRad);
			const float SY = std::sin(yawRad), CY = std::cos(yawRad);
			axisX = { CP * CY, CP * SY, SP };
		}
	}

	int BuildFireLinksOnSpawnedLinks()
	{
		const auto& links = GetSpawnedLinks();
		if (links.empty())
		{
			LEASI_WARN("FireLinks: no spawned links. Run cc.sweep first.");
			return 0;
		}

		auto* const engine = ::Common::FindFirstObject<USFXEngine, true>();
		auto* const wi = (engine != nullptr)
			? static_cast<ABioWorldInfo*>(engine->GetCurrentWorldInfo()) : nullptr;
		auto* const pc = (wi != nullptr) ? wi->GetALocalPlayerController() : nullptr;
		AActor* const tracer = (pc != nullptr) ? static_cast<AActor*>(pc->Pawn) : nullptr;
		if (tracer == nullptr)
		{
			LEASI_WARN("FireLinks: no player pawn to trace with.");
			return 0;
		}

		const float maxDist = GetMaxFireLinkDist();
		const float maxDistSq = maxDist * maxDist;
		const float minDistSq = k_minFireAngleDist * k_minFireAngleDist;

		// Flatten every enabled slot once. GetSlotLocation/GetSlotRotation are script calls;
		// doing them inside the O(n^2) loop would make this pass minutes long on a 1800-slot
		// sweep instead of seconds.
		std::vector<SlotRef> slots;
		for (auto* const link : links)
		{
			if (link == nullptr || ::Common::IsDestroyed(link)) { continue; }
			const int slotCount = static_cast<int>(link->Slots.Count());
			for (int i = 0; i < slotCount; i++)
			{
				if (link->Slots(i).bEnabled == 0) { continue; }
				// Idempotent: cc.sweep always starts from empty slots, but the standalone
				// cc.buildfirelinks does not. This also guarantees the zero-capacity state
				// the write phase below depends on.
				ResetFireLinks(link->Slots(i));
				SlotRef ref{};
				ref.link = link;
				ref.slotIdx = i;
				ref.location = link->GetSlotLocation(i, FALSE);
				RotatorAxisX(link->GetSlotRotation(i, FALSE), ref.axisX);
				slots.push_back(ref);
			}
		}

		LEASI_INFO("--- building FireLinks ---");
		LEASI_INFO("FireLinks: {} enabled slot(s) across {} link(s), MaxFireLinkDist {:.0f}.",
			slots.size(), links.size(), maxDist);

		int totalLinks = 0, totalInteractions = 0, slotsWithLinks = 0;
		int rejectedNoActions = 0, rejectedRange = 0, rejectedAngle = 0, rejectedBlocked = 0;
		int refFailures = 0, cappedSlots = 0;

		std::vector<unsigned char> srcActions, dstActions, interactions;
		std::vector<PendingFireLink> pending;

		for (const SlotRef& src : slots)
		{
			FCoverSlot& srcSlot = src.link->Slots(src.slotIdx);
			pending.clear();
			CollectSourceActions(srcSlot, srcActions);
			if (srcActions.empty())
			{
				// No lean, no pop-up: nothing can be fired from here, so it gets no links.
				// The slot is still valid cover for the player, it just never rates for AI.
				rejectedNoActions++;
				continue;
			}

			for (const SlotRef& dst : slots)
			{
				if (dst.link == src.link && dst.slotIdx == src.slotIdx) { continue; }

				const FVector diff{
					dst.location.X - src.location.X,
					dst.location.Y - src.location.Y,
					dst.location.Z - src.location.Z };
				const float distSq =
					diff.X * diff.X + diff.Y * diff.Y + diff.Z * diff.Z;
				if (distSq > maxDistSq || distSq < minDistSq) { rejectedRange++; continue; }

				const float dist = std::sqrt(distSq);
				const float invDist = (dist > 0.001f) ? (1.0f / dist) : 0.0f;
				const FVector dir{ diff.X * invDist, diff.Y * invDist, diff.Z * invDist };

				// FireAngleDot is the full 3D dot in UE3, and FireAngleDist rejects targets
				// that are close but far off-axis -- a slot facing 60 degrees away at 200u
				// passes the dot test and should not.
				const float fireAngleDot =
					dir.X * src.axisX.X + dir.Y * src.axisX.Y + dir.Z * src.axisX.Z;
				if (fireAngleDot < k_minFireLinkAngle ||
					fireAngleDot * dist < k_minFireAngleDist)
				{
					rejectedAngle++;
					continue;
				}

				FCoverSlot& dstSlot = dst.link->Slots(dst.slotIdx);
				CollectDestActions(dstSlot, dstActions);

				interactions.clear();
				for (const unsigned char sa : srcActions)
				{
					const FVector srcViewPt =
						src.link->GetSlotViewPoint(src.slotIdx, k_ctNone, sa);
					for (const unsigned char da : dstActions)
					{
						const FVector dstViewPt =
							dst.link->GetSlotViewPoint(dst.slotIdx, k_ctNone, da);
						if (!CanFireLinkHit(tracer, srcViewPt, dstViewPt)) { continue; }

						interactions.push_back(src.link->PackFireLinkInteractionInfo(
							srcSlot.CoverType, sa, dstSlot.CoverType, da));
					}
				}

				if (interactions.empty()) { rejectedBlocked++; continue; }

				const int pairRef = FindOrAddCoverPairRef(src.link, dst.link, dst.slotIdx);
				if (pairRef < 0)
				{
					// Nothing can reference this target, so the link would decode to null
					// cover and be worse than no link at all. Drop it and say so.
					refFailures++;
					continue;
				}

				PendingFireLink entry{};
				entry.packedRef = PackFireLinkRef(pairRef);
				entry.distSq = distSq;
				entry.interactions = interactions;
				pending.push_back(std::move(entry));
			}

			if (pending.empty()) { continue; }

			if (pending.size() > k_maxFireLinksPerSlot)
			{
				std::partial_sort(pending.begin(),
					pending.begin() + k_maxFireLinksPerSlot, pending.end(),
					[](const PendingFireLink& a, const PendingFireLink& b)
					{ return a.distSq < b.distSq; });
				pending.resize(k_maxFireLinksPerSlot);
				cappedSlots++;
			}

			// Size the array exactly once, while it is still empty (ResetFireLinks guaranteed
			// that above). Everything after this is an append into spare capacity, so no
			// FFireLink is ever move-assigned -- see the note on ResetFireLinks.
			srcSlot.FireLinks.Reserve(static_cast<unsigned int>(pending.size()));
			for (PendingFireLink& entry : pending)
			{
				FFireLink fireLink{};
				fireLink.Interactions.Reserve(
					static_cast<unsigned int>(entry.interactions.size()));
				for (const unsigned char packed : entry.interactions)
				{
					fireLink.Interactions.Add(packed);
				}
				fireLink.PackedProperties_CoverPairRefAndDynamicInfo = entry.packedRef;
				fireLink.bFallbackLink = 0;
				fireLink.bDynamicIndexInited = 0;

				totalInteractions += static_cast<int>(entry.interactions.size());
				srcSlot.FireLinks.Add(std::move(fireLink));
				totalLinks++;
			}
			slotsWithLinks++;
		}

		LEASI_INFO("FireLinks: wrote {} link(s) with {} interaction(s) onto {} slot(s).",
			totalLinks, totalInteractions, slotsWithLinks);
		LEASI_INFO("FireLinks: rejected {} slot(s) with no actions, {} pair(s) out of range, "
			"{} outside the firing cone, {} with no clear line.",
			rejectedNoActions, rejectedRange, rejectedAngle, rejectedBlocked);
		if (cappedSlots > 0)
		{
			LEASI_WARN("FireLinks: {} slot(s) had more than {} candidates and were capped to "
				"the nearest. That is a guard against unbounded growth, not shipped "
				"behaviour -- see k_maxFireLinksPerSlot.", cappedSlots, k_maxFireLinksPerSlot);
		}
		if (refFailures > 0)
		{
			LEASI_WARN("FireLinks: {} link(s) dropped because no CoverIndexPairs slot could be "
				"allocated for the target.", refFailures);
		}

		const int slotsStillEmpty = static_cast<int>(slots.size()) - slotsWithLinks;
		if (slotsStillEmpty > 0)
		{
			LEASI_WARN("FireLinks: {} enabled slot(s) still have none. Goal_AtCover::"
				"RateSlotMarker rejects those outright whenever the AI has an enemy, so they "
				"are invisible to cover selection.", slotsStillEmpty);
		}

		return totalLinks;
	}

	int BuildExposedCoverOnSpawnedLinks()
	{
		// ! GetExposedInfo, onto the live slots
		//
		// "Exposed" is the inverse of a fire link. A fire link says *I can shoot at that from
		// here*; an exposed entry says *that can shoot at ME here, from an angle this cover
		// does not stop*. The AI uses it to avoid picking cover that is already flanked, which
		// is why it is the most consistently filled array on a shipped slot (99% of 1077).
		//
		// Gate order is UE3's (note 03), and the first gates are deliberately the same ones
		// GetFireActions uses -- a threat that could not form a fire link to this slot cannot
		// expose it either.
		const auto& links = GetSpawnedLinks();
		if (links.empty())
		{
			LEASI_WARN("ExposedCover: no spawned links. Run cc.sweep first.");
			return 0;
		}

		auto* const engine = ::Common::FindFirstObject<USFXEngine, true>();
		auto* const wi = (engine != nullptr)
			? static_cast<ABioWorldInfo*>(engine->GetCurrentWorldInfo()) : nullptr;
		auto* const pc = (wi != nullptr) ? wi->GetALocalPlayerController() : nullptr;
		AActor* const tracer = (pc != nullptr) ? static_cast<AActor*>(pc->Pawn) : nullptr;
		if (tracer == nullptr)
		{
			LEASI_WARN("ExposedCover: no player pawn to trace with.");
			return 0;
		}

		const float maxDist = GetMaxFireLinkDist();
		const float maxDistSq = maxDist * maxDist;
		const float halfMax = maxDist * 0.5f;

		struct ExposedSlot
		{
			ACoverLink* link;
			int slotIdx;
			FVector location;
			FVector axisX;
			FVector axisY;
		};
		std::vector<ExposedSlot> slots;
		for (auto* const link : links)
		{
			if (link == nullptr || ::Common::IsDestroyed(link)) { continue; }
			const int count = static_cast<int>(link->Slots.Count());
			for (int i = 0; i < count; i++)
			{
				if (link->Slots(i).bEnabled == 0) { continue; }
				ExposedSlot entry{};
				entry.link = link;
				entry.slotIdx = i;
				entry.location = link->GetSlotLocation(i, FALSE);
				FVector axisZ{};
				RotatorAxes(link->GetSlotRotation(i, FALSE), entry.axisX, entry.axisY, axisZ);
				slots.push_back(entry);
			}
		}

		LEASI_INFO("--- building ExposedCover ---");

		int total = 0, slotsFilled = 0, capped = 0, refFailures = 0;
		std::vector<int> packed;
		std::vector<unsigned char> destActions, threatActions;

		for (const ExposedSlot& dest : slots)
		{
			FCoverSlot& destSlot = dest.link->Slots(dest.slotIdx);
			packed.clear();

			// Whether this slot can shoot back at all decides if the interaction test applies.
			CollectDestActions(destSlot, destActions);
			const bool destHasActions = destSlot.Actions.Count() > 0;

			for (const ExposedSlot& threat : slots)
			{
				if (threat.link == dest.link && threat.slotIdx == dest.slotIdx) { continue; }

				FVector toThreat{
					threat.location.X - dest.location.X,
					threat.location.Y - dest.location.Y,
					threat.location.Z - dest.location.Z };
				const float distSq = toThreat.X * toThreat.X
					+ toThreat.Y * toThreat.Y + toThreat.Z * toThreat.Z;
				if (distSq > maxDistSq) { continue; }

				const float dist = std::sqrt(distSq);
				const float invDist = (dist > 0.001f) ? (1.0f / dist) : 1.0f;
				toThreat.X *= invDist; toThreat.Y *= invDist; toThreat.Z *= invDist;

				// The threat must actually be facing us, at a usable angle and range.
				// toThreat points dest -> threat, so threat -> dest is its negation.
				const float fireAngleDot = -(threat.axisX.X * toThreat.X
					+ threat.axisX.Y * toThreat.Y + threat.axisX.Z * toThreat.Z);
				if (fireAngleDot < k_minFireLinkAngle ||
					fireAngleDot * dist < k_minFireAngleDist)
				{
					continue;
				}

				FCoverSlot& threatSlot = threat.link->Slots(threat.slotIdx);
				if (destHasActions)
				{
					// Mirrors GetFireActions' ValidInteractionList gate: some action pair has
					// to have a clear line, or the "threat" cannot reach us at all.
					CollectSourceActions(threatSlot, threatActions);
					if (threatActions.empty()) { continue; }

					bool anyValid = false;
					for (const unsigned char sa : threatActions)
					{
						if (anyValid) { break; }
						const FVector srcViewPt =
							threat.link->GetSlotViewPoint(threat.slotIdx, k_ctNone, sa);
						for (const unsigned char da : destActions)
						{
							const FVector dstViewPt =
								dest.link->GetSlotViewPoint(dest.slotIdx, k_ctNone, da);
							if (CanFireLinkHit(tracer, srcViewPt, dstViewPt))
							{
								anyValid = true;
								break;
							}
						}
					}
					if (!anyValid) { continue; }
				}

				// An edge slot leaning toward the threat is far more exposed than one facing
				// it head-on, so the acceptable angle tightens on the side it leans out of.
				float testDot = k_exposureDot;
				const float yDot = dest.axisY.X * toThreat.X
					+ dest.axisY.Y * toThreat.Y + dest.axisY.Z * toThreat.Z;
				if ((destSlot.bLeanLeft != 0 && yDot < -k_edgeCheckDot) ||
					(destSlot.bLeanRight != 0 && yDot > k_edgeCheckDot))
				{
					testDot = k_edgeExposureDot;
				}

				const float xDot = dest.axisX.X * toThreat.X
					+ dest.axisX.Y * toThreat.Y + dest.axisX.Z * toThreat.Z;
				if (xDot > testDot) { continue; }

				// xDot <= 0 means the threat is beside or behind the cover: a clean flank,
				// maximum exposure. Note 08 measured 32.6% of shipped entries at exactly 255,
				// which is this branch.
				float exposedScale = (xDot > 0.0f) ? (1.0f - (xDot / testDot)) : 1.0f;
				if (dist > halfMax)
				{
					exposedScale *= 1.0f - ((dist - halfMax) / halfMax);
				}
				if (exposedScale <= 0.0f) { continue; }

				const int refIdx =
					FindOrAddCoverPairRef(dest.link, threat.link, threat.slotIdx);
				if (refIdx < 0) { refFailures++; continue; }

				const int scale8 = static_cast<int>(exposedScale * 255.0f) & 0xFF;
				packed.push_back(PackScaledRef(refIdx, scale8));
			}

			if (packed.size() > k_maxExposedPerSlot)
			{
				packed.resize(k_maxExposedPerSlot);
				capped++;
			}
			WritePackedArray(destSlot.ExposedCoverPackedProperties, packed);
			if (!packed.empty()) { slotsFilled++; total += static_cast<int>(packed.size()); }
		}

		LEASI_INFO("ExposedCover: {} entries across {} of {} enabled slot(s).",
			total, slotsFilled, slots.size());
		if (capped > 0)
		{
			LEASI_WARN("ExposedCover: {} slot(s) capped at {} (shipped max is 51).",
				capped, k_maxExposedPerSlot);
		}
		if (refFailures > 0)
		{
			LEASI_WARN("ExposedCover: {} entries dropped, no cover ref available.",
				refFailures);
		}
		return total;
	}

	int BuildDangerCoverOnSpawnedLinks()
	{
		// ! BuildOtherLinks, onto the live slots (note 08)
		//
		// For each slot, every nav point in front of it within DangerDist that one of its own
		// firing actions can see. The AI reads it to know which approach routes a piece of
		// cover is dangerous from.
		const auto& links = GetSpawnedLinks();
		if (links.empty())
		{
			LEASI_WARN("DangerCover: no spawned links. Run cc.sweep first.");
			return 0;
		}

		auto* const engine = ::Common::FindFirstObject<USFXEngine, true>();
		auto* const wi = (engine != nullptr)
			? static_cast<ABioWorldInfo*>(engine->GetCurrentWorldInfo()) : nullptr;
		auto* const pc = (wi != nullptr) ? wi->GetALocalPlayerController() : nullptr;
		AActor* const tracer = (pc != nullptr) ? static_cast<AActor*>(pc->Pawn) : nullptr;
		if (wi == nullptr || tracer == nullptr)
		{
			LEASI_WARN("DangerCover: no world or player pawn to trace with.");
			return 0;
		}

		// Snapshot the nav list once. FindOrAddNavRef appends to the level's nav tables, and
		// walking a linked list while something else may touch it is not worth the risk.
		std::vector<ANavigationPoint*> navs;
		{
			int guard = 0;
			for (auto* nav = wi->NavigationPointList;
				nav != nullptr && guard < 100000;
				nav = nav->nextNavigationPoint, guard++)
			{
				navs.push_back(nav);
			}
		}
		if (navs.empty())
		{
			LEASI_WARN("DangerCover: NavigationPointList is empty.");
			return 0;
		}

		LEASI_INFO("--- building DangerCover ---");
		LEASI_INFO("DangerCover: {} nav point(s) in the world.", navs.size());

		int total = 0, slotsFilled = 0, enabledSlots = 0, capped = 0, refFailures = 0;
		std::vector<int> packed;
		std::vector<FVector> viewPoints;

		for (auto* const link : links)
		{
			if (link == nullptr || ::Common::IsDestroyed(link)) { continue; }
			const int slotCount = static_cast<int>(link->Slots.Count());
			for (int slotIdx = 0; slotIdx < slotCount; slotIdx++)
			{
				FCoverSlot& slot = link->Slots(slotIdx);
				if (slot.bEnabled == 0) { continue; }
				enabledSlots++;
				packed.clear();

				// BuildOtherLinks iterates Slot.Actions, so a slot with none produces nothing.
				const int actionCount = static_cast<int>(slot.Actions.Count());
				if (actionCount == 0) { continue; }

				const FVector slotLoc = link->GetSlotLocation(slotIdx, FALSE);
				FVector axisX{}, axisY{}, axisZ{};
				RotatorAxes(link->GetSlotRotation(slotIdx, FALSE), axisX, axisY, axisZ);

				// The view points depend only on the slot, not the nav, so hoist them.
				viewPoints.clear();
				for (int a = 0; a < actionCount; a++)
				{
					viewPoints.push_back(
						link->GetSlotViewPoint(slotIdx, k_ctNone, slot.Actions(a)));
				}

				for (auto* const nav : navs)
				{
					if (nav == nullptr || ::Common::IsDestroyed(nav)) { continue; }
					if (static_cast<AActor*>(nav) == static_cast<AActor*>(slot.SlotMarker))
					{
						continue;
					}

					const FVector navLoc = nav->LOCATION;
					const FVector toNav{
						navLoc.X - slotLoc.X, navLoc.Y - slotLoc.Y, navLoc.Z - slotLoc.Z };
					const float dist = std::sqrt(
						toNav.X * toNav.X + toNav.Y * toNav.Y + toNav.Z * toNav.Z);
					if (dist > k_dangerDist) { continue; }

					// Plain half-space test against the slot's facing -- no cone.
					const float invDist = (dist > 0.001f) ? (1.0f / dist) : 1.0f;
					const float dotP = (toNav.X * axisX.X + toNav.Y * axisX.Y
						+ toNav.Z * axisX.Z) * invDist;
					if (dotP < 0.0f) { continue; }

					// The target is the nav's location lifted 20u, not its centre.
					const FVector target{ navLoc.X, navLoc.Y, navLoc.Z + 20.0f };
					bool visible = false;
					for (const FVector& viewPt : viewPoints)
					{
						if (CanFireLinkHit(tracer, viewPt, target)) { visible = true; break; }
					}
					if (!visible) { continue; }

					const int refIdx = FindOrAddNavRef(link, nav);
					if (refIdx < 0) { refFailures++; continue; }

					// ! Reproducing a shipped bug on purpose
					//
					// BuildOtherLinks computes DangerCost in world units (0..DangerDist) and
					// then stores `(BYTE)(DangerCost / 1.f * 255)`. The divisor was plainly
					// meant to be DangerDist; as written, a cost of 300 becomes 76500, which
					// truncates to (-DangerCost) mod 256 -- a sawtooth of distance, not a
					// danger scale.
					//
					// Note 08 verified this shipped: across 2701 danger entries the values are
					// near-uniform over the whole byte (chi-square 54 on 15 df) and exactly-255
					// occurs 0.3% of the time, which is 1/256. The exposed array measured from
					// the same level is wildly non-uniform, which is what a real scale looks
					// like. So the runtime AI has only ever seen noise in this field.
					//
					// Emitting a correct value would make our levels the only ones in the game
					// where it means anything -- a behaviour change dressed as a bug fix, and
					// untested against BioWare's tuning. Reproduce it.
					const int dangerCost = static_cast<int>(
						k_dangerDist * (1.0f - (dist / k_dangerDist)));
					const int scale8 = (dangerCost * 255) & 0xFF;
					packed.push_back(PackScaledRef(refIdx, scale8));

					if (packed.size() >= k_maxDangerPerSlot) { capped++; break; }
				}

				WritePackedArray(slot.DangerCoverPackedProperties, packed);
				if (!packed.empty())
				{
					slotsFilled++;
					total += static_cast<int>(packed.size());
				}
			}
		}

		LEASI_INFO("DangerCover: {} entries across {} of {} enabled slot(s).",
			total, slotsFilled, enabledSlots);
		if (capped > 0)
		{
			LEASI_WARN("DangerCover: {} slot(s) capped at {} (shipped max is 137).",
				capped, k_maxDangerPerSlot);
		}
		if (refFailures > 0)
		{
			LEASI_WARN("DangerCover: {} entries dropped, no nav ref available.", refFailures);
		}
		return total;
	}

	int ApplyShippedSlotDefaults()
	{
		// Two fields where the CDO default and what every shipped level actually contains
		// disagree. Counted over 1077 slots in three levels:
		//
		//   bForceNoGroundAdjust  CDO FALSE      shipped TRUE on 1077/1077
		//   ForceCoverType        CDO CT_None    shipped == CoverType on 1077/1077
		//                                        (896 CT_MidLevel, 181 CT_Standing)
		//
		// bForceNoGroundAdjust tells the engine not to re-seat the slot against the ground at
		// runtime. Our slots are already seated by AutoAdjustSlot during the sweep, so a
		// re-adjust can only move them off a placement we measured. ForceCoverType pins the
		// type so nothing downgrades a slot we classified.
		const auto& links = GetSpawnedLinks();
		int touched = 0;
		for (auto* const link : links)
		{
			if (link == nullptr || ::Common::IsDestroyed(link)) { continue; }
			const int slotCount = static_cast<int>(link->Slots.Count());
			for (int i = 0; i < slotCount; i++)
			{
				FCoverSlot& slot = link->Slots(i);
				if (slot.bEnabled == 0) { continue; }
				slot.bForceNoGroundAdjust = 1;
				slot.ForceCoverType = slot.CoverType;
				touched++;
			}
		}
		LEASI_INFO("Slot defaults: bForceNoGroundAdjust and ForceCoverType set on {} slot(s).",
			touched);
		return touched;
	}

	void CmdFireLinkStats()
	{
		const auto& links = GetSpawnedLinks();
		if (links.empty())
		{
			LEASI_INFO("FireLinks: no spawned links. Run cc.sweep first.");
			return;
		}

		int enabledSlots = 0, withLinks = 0, totalLinks = 0, totalInteractions = 0;
		int checked = 0, resolved = 0, mismatched = 0;
		int withExposed = 0, totalExposed = 0, withDanger = 0, totalDanger = 0;
		int navChecked = 0, navResolved = 0;
		int noGroundAdjust = 0, forceTypeSet = 0;

		for (auto* const link : links)
		{
			if (link == nullptr || ::Common::IsDestroyed(link)) { continue; }
			const int slotCount = static_cast<int>(link->Slots.Count());
			for (int i = 0; i < slotCount; i++)
			{
				FCoverSlot& slot = link->Slots(i);
				if (slot.bEnabled == 0) { continue; }
				enabledSlots++;

				if (slot.bForceNoGroundAdjust != 0) { noGroundAdjust++; }
				if (slot.ForceCoverType == slot.CoverType) { forceTypeSet++; }

				const int ex = static_cast<int>(slot.ExposedCoverPackedProperties.Count());
				if (ex > 0) { withExposed++; totalExposed += ex; }

				const int dg = static_cast<int>(slot.DangerCoverPackedProperties.Count());
				if (dg > 0)
				{
					withDanger++;
					totalDanger += dg;
					// Danger refs go through the NAV tables, not CoverIndexPairs, so they need
					// their own round-trip or a crossed-table bug would hide behind a clean
					// fire-link check.
					if (navChecked < 16)
					{
						navChecked++;
						using tGetNavRef = ANavigationPoint*(ACoverLink*, int);
						auto* const vt = *reinterpret_cast<void***>(link);
						auto* const getNav = reinterpret_cast<tGetNavRef*>(vt[282]);
						const int navRef = slot.DangerCoverPackedProperties(0) & 0xFFFF;
						if (getNav(link, navRef) != nullptr) { navResolved++; }
					}
				}

				const int fl = static_cast<int>(slot.FireLinks.Count());
				if (fl == 0) { continue; }
				withLinks++;
				totalLinks += fl;

				for (int f = 0; f < fl; f++)
				{
					const FFireLink& fireLink = slot.FireLinks(f);
					totalInteractions += static_cast<int>(fireLink.Interactions.Count());

					// Round-trip the first few refs through the engine's own decoder. If the
					// ULevel offsets were wrong this fails here, loudly, instead of showing up
					// as cover the AI quietly never picks.
					if (checked >= 16) { continue; }
					checked++;
					const int refIdx =
						fireLink.PackedProperties_CoverPairRefAndDynamicInfo & 0xFFFF;
					FCoverInfo decoded{};
					if (!ResolvePackedRef(link, refIdx, decoded))
					{
						mismatched++;
						LEASI_WARN("FireLinks: ref {} on slot {} of {} does not resolve.",
							refIdx, i, NameOf(link));
						continue;
					}
					if (decoded.Link == nullptr) { mismatched++; continue; }
					resolved++;
				}
			}
		}

		LEASI_INFO("--- FireLinks stats ---");
		LEASI_INFO("{} of {} enabled slot(s) have fire links: {} link(s), {} interaction(s).",
			withLinks, enabledSlots, totalLinks, totalInteractions);
		LEASI_INFO("Packed refs sampled: {} checked, {} resolved, {} bad.",
			checked, resolved, mismatched);
		LEASI_INFO("ExposedCover: {} of {} slot(s), {} entries (shipped 99%, mean 18.4/slot).",
			withExposed, enabledSlots, totalExposed);
		LEASI_INFO("DangerCover:  {} of {} slot(s), {} entries (shipped 90%, mean 33.0/slot). "
			"Nav refs sampled: {} checked, {} resolved.",
			withDanger, enabledSlots, totalDanger, navChecked, navResolved);
		LEASI_INFO("Scalars: bForceNoGroundAdjust on {} of {}, ForceCoverType matching on {}.",
			noGroundAdjust, enabledSlots, forceTypeSet);
		if (navChecked > 0 && navResolved < navChecked)
		{
			LEASI_WARN("Some danger refs do not resolve -- check the NavRefs/NavRefIndicies "
				"offsets (0x2E4 / 0x2F4) for this build.");
		}

		if (!links.empty() && links.front() != nullptr)
		{
			auto* const level = LevelOf(links.front());
			auto* const refs = LevelArrayAt<ACoverLink*>(level, k_levelCoverLinkRefs);
			auto* const pairs = LevelArrayAt<FCoverIndexPair>(level, k_levelCoverIndexPairs);
			if (refs != nullptr && pairs != nullptr)
			{
				LEASI_INFO("Level tables: CoverLinkRefs {}, CoverIndexPairs {}.",
					refs->Count(), pairs->Count());
			}
		}

		if (mismatched > 0)
		{
			LEASI_WARN("Bad refs mean the ULevel offsets in FireLinks.cpp (0x2B4 "
				"CoverLinkRefs / 0x2C4 CoverIndexPairs) do not match this build.");
		}
	}
}
