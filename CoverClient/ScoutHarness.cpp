#include "CoverClient/ScoutHarness.hpp"
#include "CoverClient/Helpers.hpp"
#include "CoverClient/Utilities/PathUtilities.hpp"
#include "CoverClient/Utilities/DebugTools.hpp"
#include "CoverClient/CoverEdges.hpp"
#include "Common/Base.hpp"
#include "Common/Objects.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace CoverClient
{
	namespace
	{
		// RVAs are for LE3. Derived from the Ghidra image base 0x7FF74A890000 --
		// see Notes/02-le3-native-cover-functions.md.
		constexpr size_t k_rvaGetScout = 0x65CA80;      // FPathBuilder::GetScout
		constexpr size_t k_rvaDestroyScout = 0x65A260;  // FPathBuilder::DestroyScout

		using tGetScout = AScout* ();
		using tDestroyScout = void ();

		tGetScout* GetScout_orig = nullptr;
		tDestroyScout* DestroyScout_orig = nullptr;

		/// The scout the harness owns. Null until AcquireScout succeeds.
		AScout* g_HarnessScout = nullptr;

		/// While true, GetScout returns g_HarnessScout and DestroyScout does nothing.
		bool g_bScoutHeld = false;

		AScout* GetScout_hook()
		{
			if (g_bScoutHeld && g_HarnessScout != nullptr)
			{
				return g_HarnessScout;
			}
			return GetScout_orig();
		}

		void DestroyScout_hook()
		{
			if (g_bScoutHeld)
			{
				// AutoAdjustSlot and BuildSlotInfo both call this on the way out. Letting it
				// run would destroy our scout after a single slot, so swallow it.
				return;
			}
			DestroyScout_orig();
		}

		/// Returns the local player's pawn, or nullptr. Used both as a spawn reference actor
		/// and as the origin for "nearest CoverLink" searches.
		APawn* GetPlayerPawn()
		{
			auto* const engine = ::Common::FindFirstObject<USFXEngine, true>();
			if (engine == nullptr)
			{
				return nullptr;
			}

			auto* const wi = static_cast<ABioWorldInfo*>(engine->GetCurrentWorldInfo());
			if (wi == nullptr)
			{
				return nullptr;
			}

			auto* const pc = wi->GetALocalPlayerController();
			return (pc != nullptr) ? pc->Pawn : nullptr;
		}

		ABioWorldInfo* GetWorldInfo()
		{
			auto* const engine = ::Common::FindFirstObject<USFXEngine, true>();
			if (engine == nullptr)
			{
				return nullptr;
			}
			return static_cast<ABioWorldInfo*>(engine->GetCurrentWorldInfo());
		}

		/// LEASI_* format strings in this file are narrow, so anything that comes back from
		/// the SDK as an FString has to be narrowed before it can be logged alongside the
		/// plain char* flag names.
		std::string Narrow(const FString& value)
		{
			const wchar_t* const data = *value;
			return (data != nullptr) ? ws2s(data) : std::string{};
		}

		std::string PathOf(UObject* object)
		{
			return (object != nullptr) ? Narrow(object->GetFullPath()) : std::string{ "<null>" };
		}

		/// Trailing "_<digits>" of an object's name, which is how UE3 names spawned actors
		/// and how the console reports them.
		int TrailingNumber(UObject* object)
		{
			if (object == nullptr) { return -1; }
			const std::string name = Narrow(object->GetName());
			const size_t underscore = name.find_last_of('_');
			if (underscore == std::string::npos || underscore + 1 >= name.size())
			{
				return -1;
			}
			int value = 0;
			for (size_t i = underscore + 1; i < name.size(); i++)
			{
				if (name[i] < '0' || name[i] > '9') { return -1; }
				value = (value * 10) + (name[i] - '0');
			}
			return value;
		}

		float DistanceSquared(const FVector& a, const FVector& b)
		{
			const float dx = a.X - b.X;
			const float dy = a.Y - b.Y;
			const float dz = a.Z - b.Z;
			return (dx * dx) + (dy * dy) + (dz * dz);
		}

		// ! Slot analysis snapshot
		// ========================================
		//
		// Everything AutoAdjustSlot / BuildSlotInfoInternal is allowed to write. Captured
		// before and after so we can diff.

		struct SlotSnapshot
		{
			unsigned char coverType;
			unsigned char forceCoverType;
			unsigned long flags;        // the whole bitfield word at FCoverSlot+0xDC
			FVector locationOffset;
			FRotator rotationOffset;
			int  turnTarget;            // TurnTargetPackedProperties (swat turn)
			int  coverTurnTarget;       // CoverTurnTargetPackedProperties
			int  mantleTargetSlotIdx;
			void* mantleTargetActor;
			int  slipTargetCount;
			int  slipRefCount;
		};

		/// Reads the packed boolean word out of an FCoverSlot. The SDK exposes the bits as
		/// individual bitfield members, but for diffing we want the raw word -- it lives at
		/// FCoverSlot+0xDC, which is where bLeanLeft starts.
		unsigned long ReadSlotFlags(const FCoverSlot& slot)
		{
			return *reinterpret_cast<const unsigned long*>(
				reinterpret_cast<const unsigned char*>(&slot) + 0xDC);
		}

		SlotSnapshot TakeSnapshot(const FCoverSlot& slot)
		{
			SlotSnapshot snap{};
			snap.coverType = slot.CoverType;
			snap.forceCoverType = slot.ForceCoverType;
			snap.flags = ReadSlotFlags(slot);
			snap.locationOffset = slot.LocationOffset;
			snap.rotationOffset = slot.RotationOffset;
			snap.turnTarget = slot.TurnTargetPackedProperties;
			snap.coverTurnTarget = slot.CoverTurnTargetPackedProperties;
			snap.mantleTargetSlotIdx = slot.MantleTarget.SlotIdx;
			snap.mantleTargetActor = slot.MantleTarget.Actor;
			snap.slipTargetCount = static_cast<int>(slot.SlipTarget.Count());
			snap.slipRefCount = static_cast<int>(slot.SlipRefs.Count());
			return snap;
		}

		struct FlagName { unsigned long mask; const char* name; };

		// Bit layout confirmed against both the LESDK header and the decompiled
		// BuildSlotInfoInternal. Notes/02-le3-native-cover-functions.md has the full table.
		constexpr FlagName k_slotFlagNames[] = {
			{ 0x00000001, "bLeanLeft" },
			{ 0x00000002, "bLeanRight" },
			{ 0x00000004, "bForceCanPopUp" },
			{ 0x00000008, "bCanPopUp" },
			{ 0x00000010, "bCanMantle" },
			{ 0x00000020, "bCanClimbUp" },
			{ 0x00000040, "bForceCanCoverSlip_Left" },
			{ 0x00000080, "bForceCanCoverSlip_Right" },
			{ 0x00000100, "bCanCoverSlip_Left" },
			{ 0x00000200, "bCanCoverSlip_Right" },
			{ 0x00000400, "bCanSwatTurn_Left" },
			{ 0x00000800, "bCanSwatTurn_Right" },
			{ 0x00001000, "bCanCoverTurn_Left" },
			{ 0x00002000, "bCanCoverTurn_Right" },
			{ 0x00004000, "bEnabled" },
			{ 0x00008000, "bAllowPopup" },
			{ 0x00010000, "bAllowMantle" },
			{ 0x00020000, "bAllowCoverSlip" },
			{ 0x00040000, "bAllowClimbUp" },
			{ 0x00080000, "bAllowSwatTurn" },
			{ 0x00100000, "bAllowCoverTurn" },
			{ 0x00200000, "bForceNoGroundAdjust" },
			{ 0x00400000, "bPlayerOnly" },
			{ 0x00800000, "bUnsafeCover" },
			{ 0x01000000, "bSelected" },
			{ 0x02000000, "bFailedToFindSurface" },
		};

		const char* CoverTypeName(unsigned char type)
		{
			switch (type)
			{
			case 0:  return "CT_None";
			case 1:  return "CT_Standing";
			case 2:  return "CT_MidLevel";
			default: return "CT_?";
			}
		}

		/// Logs the differences between two snapshots. Returns the number of fields that changed.
		int LogSnapshotDiff(int slotIdx, const SlotSnapshot& before, const SlotSnapshot& after)
		{
			int changes = 0;

			if (before.coverType != after.coverType)
			{
				LEASI_WARN("  slot {}: CoverType {} -> {}", slotIdx,
					CoverTypeName(before.coverType), CoverTypeName(after.coverType));
				changes++;
			}

			const unsigned long changedBits = before.flags ^ after.flags;
			if (changedBits != 0)
			{
				for (const auto& flag : k_slotFlagNames)
				{
					if ((changedBits & flag.mask) == 0)
					{
						continue;
					}
					const bool nowSet = (after.flags & flag.mask) != 0;
					LEASI_WARN("  slot {}: {} {} -> {}", slotIdx, flag.name,
						nowSet ? "FALSE" : "TRUE", nowSet ? "TRUE" : "FALSE");
					changes++;
				}

				// Anything outside the documented table means our bit map is incomplete.
				unsigned long knownMask = 0;
				for (const auto& flag : k_slotFlagNames)
				{
					knownMask |= flag.mask;
				}
				if ((changedBits & ~knownMask) != 0)
				{
					LEASI_ERROR("  slot {}: unmapped flag bits changed: 0x{:08X}",
						slotIdx, changedBits & ~knownMask);
					changes++;
				}
			}

			// The location/rotation adjust pass is the noisiest part -- report it separately
			// from the analysis flags so it doesn't drown the signal we actually care about.
			const float locDelta = std::sqrt(DistanceSquared(before.locationOffset, after.locationOffset));
			if (locDelta > 0.01f)
			{
				LEASI_INFO("  slot {}: LocationOffset moved {:.2f}u ({:.2f},{:.2f},{:.2f}) -> ({:.2f},{:.2f},{:.2f})",
					slotIdx, locDelta,
					before.locationOffset.X, before.locationOffset.Y, before.locationOffset.Z,
					after.locationOffset.X, after.locationOffset.Y, after.locationOffset.Z);
				changes++;
			}
			if (before.rotationOffset.Pitch != after.rotationOffset.Pitch ||
				before.rotationOffset.Yaw != after.rotationOffset.Yaw ||
				before.rotationOffset.Roll != after.rotationOffset.Roll)
			{
				LEASI_INFO("  slot {}: RotationOffset ({},{},{}) -> ({},{},{})", slotIdx,
					before.rotationOffset.Pitch, before.rotationOffset.Yaw, before.rotationOffset.Roll,
					after.rotationOffset.Pitch, after.rotationOffset.Yaw, after.rotationOffset.Roll);
				changes++;
			}

			if (before.turnTarget != after.turnTarget)
			{
				LEASI_WARN("  slot {}: TurnTargetPackedProperties {} -> {}", slotIdx,
					before.turnTarget, after.turnTarget);
				changes++;
			}
			if (before.coverTurnTarget != after.coverTurnTarget)
			{
				LEASI_WARN("  slot {}: CoverTurnTargetPackedProperties {} -> {}", slotIdx,
					before.coverTurnTarget, after.coverTurnTarget);
				changes++;
			}
			if (before.mantleTargetSlotIdx != after.mantleTargetSlotIdx ||
				before.mantleTargetActor != after.mantleTargetActor)
			{
				LEASI_WARN("  slot {}: MantleTarget slot {} -> {} (actor {} -> {})", slotIdx,
					before.mantleTargetSlotIdx, after.mantleTargetSlotIdx,
					before.mantleTargetActor != nullptr ? "set" : "none",
					after.mantleTargetActor != nullptr ? "set" : "none");
				changes++;
			}
			if (before.slipTargetCount != after.slipTargetCount)
			{
				LEASI_WARN("  slot {}: SlipTarget count {} -> {}", slotIdx,
					before.slipTargetCount, after.slipTargetCount);
				changes++;
			}
			if (before.slipRefCount != after.slipRefCount)
			{
				LEASI_WARN("  slot {}: SlipRefs count {} -> {}", slotIdx,
					before.slipRefCount, after.slipRefCount);
				changes++;
			}

			return changes;
		}

		/// Finds the CoverLink with slots closest to the player. Deliberately targets cover that
		/// shipped with the game, so the analysis can be diffed against known-good data.
		ACoverLink* FindNearestCoverLink(float& outDistance)
		{
			auto* const pawn = GetPlayerPawn();
			if (pawn == nullptr)
			{
				LEASI_ERROR("No player pawn; cannot pick a nearby CoverLink.");
				return nullptr;
			}

			ACoverLink* nearest = nullptr;
			float nearestDistSq = 0.0f;
			for (::Common::TypedObjectIterator<ACoverLink, true, false> it{}; it; ++it)
			{
				ACoverLink* const link = *it;
				if (link->Slots.Count() == 0)
				{
					continue;
				}

				const float distSq = DistanceSquared(link->LOCATION, pawn->LOCATION);
				if (nearest == nullptr || distSq < nearestDistSq)
				{
					nearest = link;
					nearestDistSq = distSq;
				}
			}

			if (nearest == nullptr)
			{
				LEASI_ERROR("No CoverLink with slots found in the level.");
				return nullptr;
			}

			outDistance = std::sqrt(nearestDistSq);
			return nearest;
		}

		/// ACoverLink::BuildSlotInfo is a virtual at vtable offset 0x898 (slot 275); it calls
		/// FPathBuilder::GetScout, then BuildSlotInfoInternal (slot 276, offset 0x8A0), then
		/// DestroyScout. Both of those outer calls are hooked by this harness.
		///
		/// We reach for the vtable rather than going through AutoAdjustSlot because
		/// AutoAdjustSlot only reaches BuildSlotInfo when the link has bAutoAdjust set, and
		/// shipped links generally do not -- which would silently skip the entire
		/// mantle/slip/turn analysis we are trying to test.
		constexpr size_t k_vtableIndexBuildSlotInfo = 275;

		void CallBuildSlotInfo(ACoverLink* link, int slotIdx)
		{
			using tBuildSlotInfo = void(ACoverLink*, int, int);
			auto* const vtable = *reinterpret_cast<void***>(link);
			auto* const fn = reinterpret_cast<tBuildSlotInfo*>(vtable[k_vtableIndexBuildSlotInfo]);
			fn(link, slotIdx, 0 /*bSeedPylon*/);
		}

		// ! ACoverLink::AddMyMarker -- runtime CoverSlotMarker / MantleMarker creation
		// ========================================
		//
		// A slot with no CoverSlotMarker is not enterable cover. The marker is a real
		// ANavigationPoint actor sitting at the slot, and it is what the rest of the game
		// resolves cover through -- ABioScout::CanCoverSlip abandons its nav search the moment
		// GetSlotMarker returns null, and the player's cover acquisition needs one too.
		//
		// In the editor the markers were created at build time and serialised into the level
		// package, which is why shipped cover works and ours did not: in-game AddCoverSlot only
		// creates markers inside its !GIsGame branch.
		//
		// But the *creation pass itself* survived into the shipping exe, at vtable slot 198 --
		// UE3's ANavigationPoint::AddMyMarker, overridden by ACoverLink. ASFXDynamicCoverLink
		// overrides it again (0x7FF74AE887B0) purely to spawn the dynamic marker subclass,
		// which is the shipping game telling us this is exactly how a CoverLink created after
		// play has begun is meant to be finished off. So rather than reimplement it we call it.
		//
		// It does, per slot:
		//   Slot.SlotMarker = SpawnActor(ACoverSlotMarker, GetSlotLocation(i), GetSlotRotation(i),
		//                                bNoCollisionFail = TRUE, Owner = link)
		//   Marker->OwningSlot = { link, i }
		//   Marker->ExtraCost  = Slot.ExtraCost
		//   Marker-><navflag 0x20> = link-><navflag 0x20>
		//   Marker->SetCollisionType(link->CollisionType)
		// and then, only where the slot has BOTH bAllowClimbUp and bAllowPopup, offsets forward
		// and up, asks the scout for the "Human" path size, and if that fits spawns an
		// AMantleMarker there as well.
		//
		// Two traps, both of which make it silently do nothing:
		constexpr size_t k_vtableIndexAddMyMarker = 198;

		/// Calls ACoverLink::AddMyMarker, working around both traps.
		///
		/// Trap 1: it returns 0 immediately unless `GLOBAL_bUseSlotMarkers` is set. That is a
		/// GlobalConfig property read off the instance, so we set it there.
		///
		/// Trap 2: ACoverSlotMarker and AMantleMarker are ANavigationPoints, and
		/// ANavigationPoint defaults bStatic and bNoDelete to true -- the same pair that made
		/// every ACoverLink spawn return null (UnLevAct.cpp:406) until we cleared them on the
		/// CDO. AddMyMarker calls SpawnActor itself, so the only place to intervene is the CDO,
		/// across the call. Without this it runs to completion and assigns null to every
		/// SlotMarker.
		///
		/// @returns Slots.Num() + however many MantleMarkers were created, or -1 on failure.
		int CallAddMyMarker(ACoverLink* link, AScout* scout)
		{
			if (link == nullptr || scout == nullptr)
			{
				return -1;
			}

			// The mantle branch dereferences the scout without a null check, so a link that
			// reaches it with no scout would fault rather than skip.
			link->GLOBAL_bUseSlotMarkers = 1;

			struct ScopedSpawnable
			{
				explicit ScopedSpawnable(UClass* cls)
				{
					if (cls == nullptr) { return; }
					defaults = static_cast<ANavigationPoint*>(cls->ClassDefaultObject);
					if (defaults == nullptr) { return; }
					savedStatic = defaults->bStatic;
					savedNoDelete = defaults->bNoDelete;
					defaults->bStatic = 0;
					defaults->bNoDelete = 0;
				}
				~ScopedSpawnable()
				{
					if (defaults == nullptr) { return; }
					defaults->bStatic = savedStatic;
					defaults->bNoDelete = savedNoDelete;
				}
				ScopedSpawnable(ScopedSpawnable const&) = delete;
				ScopedSpawnable& operator=(ScopedSpawnable const&) = delete;

				ANavigationPoint* defaults{ nullptr };
				unsigned long savedStatic{ 0 };
				unsigned long savedNoDelete{ 0 };
			};

			ScopedSpawnable slotMarkers{ ACoverSlotMarker::StaticClass() };
			ScopedSpawnable mantleMarkers{ AMantleMarker::StaticClass() };

			using tAddMyMarker = int(ACoverLink*, AActor*);
			auto* const vtable = *reinterpret_cast<void***>(link);
			auto* const fn = reinterpret_cast<tAddMyMarker*>(vtable[k_vtableIndexAddMyMarker]);
			return fn(link, reinterpret_cast<AActor*>(scout));
		}

		/// Destroys the markers a link owns and clears the slots' references, so a link can be
		/// rebuilt or despawned without leaking actors. AddMyMarker overwrites SlotMarker
		/// unconditionally, so calling it twice on one link would strand the first set.
		int DestroySlotMarkers(ACoverLink* link)
		{
			if (link == nullptr)
			{
				return 0;
			}

			int destroyed = 0;
			const int slots = static_cast<int>(link->Slots.Count());
			for (int i = 0; i < slots; i++)
			{
				FCoverSlot& slot = link->Slots(i);
				if (auto* const marker = slot.SlotMarker)
				{
					if (!::Common::IsDestroyed(marker))
					{
						marker->bStatic = 0;
						marker->bNoDelete = 0;
						marker->Destroy();
						destroyed++;
					}
					slot.SlotMarker = nullptr;
				}
			}
			return destroyed;
		}

		/// Whether every slot on the link came back with a marker. AddMyMarker reports a count
		/// but not which spawns failed, and a partially-marked link is worse than an unmarked
		/// one because it looks like it worked.
		int CountSlotMarkers(ACoverLink* link)
		{
			int count = 0;
			const int slots = static_cast<int>(link->Slots.Count());
			for (int i = 0; i < slots; i++)
			{
				if (link->Slots(i).SlotMarker != nullptr) { count++; }
			}
			return count;
		}

		/// Logs a slot's analysis flags in a readable form, for the "before" baseline.
		/// Every bit LESDK gives a name to in FCoverSlot's 0xDC dword, up to and including
		/// bFailedToFindSurface (0x02000000). Anything above that is not a reflected property
		/// in LE3 -- confirmed by enumerating the property names on 132 shipped cover slots,
		/// which produced exactly this set and nothing more.
		constexpr unsigned long k_knownSlotFlagMask = 0x03FFFFFFul;

		/// @param link  The slot's owner, used only to resolve the slot's world location.
		///              LocationOffset is link-relative, so a bare FCoverSlot cannot say where
		///              a slot actually sits -- and where the slots sit is the only way to tell
		///              a placement walk that stopped at the real edge from one that gave up
		///              early. Pass nullptr if the owner is not to hand.
		void LogSlotState(ACoverLink* link, int slotIdx, const FCoverSlot& slot)
		{
			const unsigned long flags = ReadSlotFlags(slot);

			std::string set{};
			for (const auto& flag : k_slotFlagNames)
			{
				if ((flags & flag.mask) != 0)
				{
					set += flag.name;
					set += ' ';
				}
			}

			if (link != nullptr)
			{
				const FVector loc = link->GetSlotLocation(slotIdx, FALSE);
				LEASI_INFO("  slot {}: {} @ ({:.1f}, {:.1f}, {:.1f}) flags=0x{:08X} [{}]",
					slotIdx, CoverTypeName(slot.CoverType), loc.X, loc.Y, loc.Z, flags, set);
			}
			else
			{
				LEASI_INFO("  slot {}: {} flags=0x{:08X} [{}]", slotIdx,
					CoverTypeName(slot.CoverType), flags, set);
			}

			// Bits 26-31 have no name in the SDK and never appear in shipped package data.
			// They still come back set in-game, and differently per slot, so they are either
			// LE3-only non-reflected bits or stale memory. Surface them rather than hiding
			// them -- but keep them out of any comparison, or diffs report phantom changes.
			const unsigned long unknown = flags & ~k_knownSlotFlagMask;
			if (unknown != 0)
			{
				LEASI_WARN("    unnamed bits 26-31 set: 0x{:08X} (not a reflected property in "
					"LE3; ignored for comparison)", unknown);
			}
		}
	}

	// ! Hook installation
	// ========================================

	void InitializeScoutHooks(::LESDK::Initializer& Init)
	{
		auto* const getScoutTarget = Init.ResolveTyped<tGetScout>(LEASI_RVA(k_rvaGetScout));
		if (getScoutTarget == nullptr)
		{
			LEASI_ERROR("ScoutHarness: could not resolve FPathBuilder::GetScout at RVA 0x{:X}", k_rvaGetScout);
			return;
		}
		GetScout_orig = reinterpret_cast<tGetScout*>(
			Init.InstallHook("FPathBuilder::GetScout", getScoutTarget, GetScout_hook));
		if (GetScout_orig == nullptr)
		{
			LEASI_ERROR("ScoutHarness: failed to hook FPathBuilder::GetScout");
			return;
		}

		auto* const destroyScoutTarget = Init.ResolveTyped<tDestroyScout>(LEASI_RVA(k_rvaDestroyScout));
		if (destroyScoutTarget == nullptr)
		{
			LEASI_ERROR("ScoutHarness: could not resolve FPathBuilder::DestroyScout at RVA 0x{:X}", k_rvaDestroyScout);
			return;
		}
		DestroyScout_orig = reinterpret_cast<tDestroyScout*>(
			Init.InstallHook("FPathBuilder::DestroyScout", destroyScoutTarget, DestroyScout_hook));
		if (DestroyScout_orig == nullptr)
		{
			LEASI_ERROR("ScoutHarness: failed to hook FPathBuilder::DestroyScout");
			return;
		}

		LEASI_INFO("ScoutHarness: FPathBuilder scout hooks installed.");
	}

	// ! Scout lifetime
	// ========================================

	bool IsScoutHeld()
	{
		return g_bScoutHeld && g_HarnessScout != nullptr;
	}

	AScout* AcquireScout()
	{
		if (GetScout_orig == nullptr || DestroyScout_orig == nullptr)
		{
			LEASI_ERROR("ScoutHarness: hooks are not installed, refusing to acquire a scout.");
			return nullptr;
		}

		// A scout from a previous acquire may have been garbage-collected or destroyed by a
		// level transition, so re-validate rather than trusting the cached pointer.
		if (g_HarnessScout != nullptr && ::Common::IsDestroyed(g_HarnessScout))
		{
			LEASI_WARN("ScoutHarness: cached scout was destroyed, respawning.");
			g_HarnessScout = nullptr;
		}

		if (g_HarnessScout == nullptr)
		{
			auto* const wi = GetWorldInfo();
			if (wi == nullptr)
			{
				LEASI_ERROR("ScoutHarness: no WorldInfo, cannot spawn a scout.");
				return nullptr;
			}

			UClass* const scoutClass = ABioScout::StaticClass();
			if (scoutClass == nullptr)
			{
				LEASI_ERROR("ScoutHarness: SFXGame.BioScout class is not loaded.");
				return nullptr;
			}

			// Spawn where the player is standing. The scout gets teleported around by
			// FarMoveActor inside the CanDoMove leaves anyway, so the initial spot only has
			// to be somewhere valid. bNoCollisionFail keeps a tight spot from failing us.
			auto* const pawn = GetPlayerPawn();
			const FVector spawnLoc = (pawn != nullptr) ? pawn->LOCATION : wi->LOCATION;
			const FRotator spawnRot{};

			auto* const spawned = wi->Spawn(scoutClass, nullptr, SFXName{},
				spawnLoc, spawnRot, nullptr, TRUE /*bNoCollisionFail*/, FALSE /*bFindSafeLocation*/);
			if (spawned == nullptr)
			{
				LEASI_ERROR("ScoutHarness: Spawn(BioScout) returned null.");
				return nullptr;
			}

			g_HarnessScout = static_cast<AScout*>(spawned);
			LEASI_INFO("ScoutHarness: spawned scout '{}' at ({:.1f}, {:.1f}, {:.1f})",
				PathOf(g_HarnessScout), spawnLoc.X, spawnLoc.Y, spawnLoc.Z);
		}

		g_bScoutHeld = true;
		return g_HarnessScout;
	}

	void ReleaseScout()
	{
		g_bScoutHeld = false;
	}

	ScopedScout::ScopedScout()
	{
		// Nesting is fine as long as only the outermost guard releases -- an inner guard
		// releasing early would let the next DestroyScout call through mid-build.
		const bool bWasHeld = IsScoutHeld();
		Scout = AcquireScout();
		bOwnsRelease = (Scout != nullptr) && !bWasHeld;
	}

	ScopedScout::~ScopedScout()
	{
		if (bOwnsRelease)
		{
			ReleaseScout();
		}
	}

	// ! cc.testscout
	// ========================================

	void CmdTestScout()
	{
		LEASI_INFO("=== cc.testscout ===");

		ScopedScout guard{};
		AScout* const scout = guard.Get();
		if (scout == nullptr)
		{
			LEASI_ERROR("Scout spike FAILED: could not acquire a scout.");
			return;
		}

		LEASI_INFO("Scout acquired: {}", PathOf(scout));
		LEASI_INFO("  class:   {}", PathOf(scout->Class));
		LEASI_INFO("  physics: {}", static_cast<int>(scout->Physics));

		if (scout->CylinderComponent != nullptr)
		{
			LEASI_INFO("  cylinder: radius={:.1f} height={:.1f}",
				scout->CylinderComponent->CollisionRadius,
				scout->CylinderComponent->CollisionHeight);
		}
		else
		{
			LEASI_WARN("  cylinder: NONE (SetPrototype is a no-op in LE3, so this may be expected)");
		}

		// The question this command exists to answer. ABioScout's CanDoMove leaves call
		// GetSize(FName("Human")); GetSize falls back to PathSizes(0) when the name is not
		// found, which faults on an empty array.
		const int pathSizeCount = static_cast<int>(scout->PathSizes.Count());
		LEASI_INFO("  PathSizes: {} entries", pathSizeCount);

		bool bFoundHuman = false;
		for (int i = 0; i < pathSizeCount; i++)
		{
			const FPathSizeInfo& info = scout->PathSizes(i);
			const char* const desc = info.Desc.GetName();
			LEASI_INFO("    [{}] {} radius={:.1f} height={:.1f} crouch={:.1f}",
				i, desc != nullptr ? desc : "<null>", info.Radius, info.Height, info.CrouchHeight);

			if (desc != nullptr && _stricmp(desc, "Human") == 0)
			{
				bFoundHuman = true;
			}
		}

		if (pathSizeCount == 0)
		{
			LEASI_ERROR("Scout spike FAILED: PathSizes is EMPTY. GetSize() would fault on "
				"PathSizes(0) -- calling BuildSlotInfo would crash the game.");
			return;
		}
		if (!bFoundHuman)
		{
			// Expected. Retail Default__BioScout.PathSizes is Common/Large/VeryLarge/Max/Vehicle
			// with no "Human", so GetSize falls back to PathSizes(0) = Common 34x90 -- which is
			// the right size regardless: shipped MantleReachSpecs carry exactly 34/90, so the
			// editor took the same fallback. Notes/06-shipped-data-reference.md has the dump.
			LEASI_INFO("PathSizes has no \"Human\" entry (expected). GetSize(\"Human\") falls back "
				"to PathSizes(0) = {} radius={:.1f} height={:.1f}, which matches the 34x90 on "
				"shipped MantleReachSpecs.",
				scout->PathSizes(0).Desc.GetName(), scout->PathSizes(0).Radius,
				scout->PathSizes(0).Height);
		}
		else
		{
			LEASI_WARN("PathSizes has a \"Human\" entry -- unexpected, retail LE3 does not. "
				"The CanDoMove leaves will use it instead of the Common 34x90 fallback.");
		}

		// Confirm the hooks actually route through us.
		AScout* const viaGetScout = GetScout_hook();
		if (viaGetScout != scout)
		{
			LEASI_ERROR("Scout spike FAILED: GetScout hook returned {} but we hold {}.",
				static_cast<void*>(viaGetScout), static_cast<void*>(scout));
			return;
		}
		LEASI_INFO("GetScout hook returns the harness scout. OK.");

		// And that DestroyScout is being swallowed. If this were leaking through, the scout
		// would be gone by the time the next slot was processed.
		DestroyScout_hook();
		if (::Common::IsDestroyed(scout))
		{
			LEASI_ERROR("Scout spike FAILED: scout was destroyed despite the DestroyScout hook.");
			return;
		}
		LEASI_INFO("DestroyScout hook suppressed the teardown. OK.");

		LEASI_INFO("Scout spike PASSED. Ready to try cc.testautoadjust.");
	}

	// ! cc.spawnlink -- Phase 1 step 2
	// ========================================

	namespace
	{
		/// Non-owning list of links this session spawned, so cc.despawnlinks can clean up.
		std::vector<ACoverLink*> g_SpawnedLinks;

		/// The walk's own account of every link it built, kept alive past the sweep.
		///
		/// FindCoverEdgesResult was being thrown away the moment the link existed, which meant
		/// the only way to ask "why is THAT link wrong" was to re-run the whole sweep with a
		/// new log line added. Keeping it turns that into a question anyone can ask of any
		/// link at any time -- which is what cc.explainlink and the in-world origin labels
		/// both need.
		std::map<ACoverLink*, FindCoverEdgesResult> g_LinkEdges;

		/// Half-angle of the aim cone as a tangent -- about 4 degrees. Anything inside it is
		/// "pointed at"; see the pick in CmdExplainLink.
		constexpr float k_aimCone = 0.07f;

		/// The ACoverLink CDO, which is where LE3 keeps AlignDist / AutoCoverSlotInterval /
		/// StandHeight / MidHeight and the lean offsets. Reading them beats hardcoding:
		/// LE3's StandHeight is 130 where UE3's was 160, and its AutoCoverSlotInterval is 256
		/// where UE3's was 175.
		ACoverLink* GetCoverLinkDefaults()
		{
			auto* const cls = ACoverLink::StaticClass();
			return (cls != nullptr) ? static_cast<ACoverLink*>(cls->ClassDefaultObject) : nullptr;
		}

		/// FRotationMatrix(rot).GetAxes(). Needed to reproduce AddCoverSlot's world-space to
		/// LocationOffset conversion when we seat the inherited archetype slot by hand.
		void RotatorToAxes(const FRotator& rot, FVector& axisX, FVector& axisY, FVector& axisZ)
		{
			const float pitchRad = UnrealRotationUnitsToRadians(rot.Pitch);
			const float yawRad = UnrealRotationUnitsToRadians(rot.Yaw);
			const float rollRad = UnrealRotationUnitsToRadians(rot.Roll);
			const float SP = std::sin(pitchRad), CP = std::cos(pitchRad);
			const float SY = std::sin(yawRad), CY = std::cos(yawRad);
			const float SR = std::sin(rollRad), CR = std::cos(rollRad);
			axisX = { CP * CY,                    CP * SY,                   SP };
			axisY = { SR * SP * CY - CR * SY,     SR * SP * SY + CR * CY,   -SR * CP };
			axisZ = { -(CR * SP * CY + SR * SY),  CY * SR - CR * SP * SY,    CR * CP };
		}

		// ! WorldInfo.CoverList membership
		// ========================================
		//
		// A level-loaded CoverLink is threaded onto AWorldInfo::CoverList through its
		// NextCoverLink field when the level is added to the world. A link we Spawn is not:
		// SpawnActor puts the actor in Level->Actors, and ANavigationPoint's own PostBeginPlay
		// gets it onto NavigationPointList, but nothing puts it on CoverList -- confirmed
		// in-game, where a level with no shipped cover reported CoverList empty while our ten
		// markers were all present on NavigationPointList.
		//
		// That is the whole difference between cover that draws and cover you can enter. The
		// engine accepts our slots: ACoverLink::FindSlots, the same bracketing search the
		// player's cover acquisition ends up in, returned a valid pair for the player standing
		// in front of them. Whatever *picks* the link never got that far, because it walks a
		// list we are not on.
		//
		// This is also why the original CoverHost round-trip worked and this does not: a
		// generated pathfinding package is loaded as level data, so its links go onto CoverList
		// the normal way. Splicing here only emulates level load for the live-session testing
		// path; it has no bearing on what gets serialised.
		//
		// Chains are always walked with a hard cap. A malformed one is a hang, not an error --
		// which is exactly what an earlier version of this cost.
		constexpr int k_maxChainWalk = 20000;

		bool IsOnCoverList(ABioWorldInfo* wi, ACoverLink* link)
		{
			if (wi == nullptr || link == nullptr)
			{
				return false;
			}
			auto* node = wi->CoverList;
			for (int i = 0; node != nullptr && i < k_maxChainWalk; i++, node = node->NextCoverLink)
			{
				if (node == link) { return true; }
			}
			return false;
		}

		/// Head-inserts the link, the way level load does. Idempotent: inserting a link that is
		/// already on the list would make NextCoverLink point at itself and hang every walker.
		bool SpliceIntoCoverList(ABioWorldInfo* wi, ACoverLink* link)
		{
			if (wi == nullptr || link == nullptr || IsOnCoverList(wi, link))
			{
				return false;
			}
			link->NextCoverLink = wi->CoverList;
			wi->CoverList = link;
			return true;
		}

		/// Removes the link again. ACoverLink::PostScriptDestroyed destroys the slot markers but
		/// does NOT touch CoverList -- nothing at runtime does, because nothing at runtime adds
		/// to it. So a link destroyed while still spliced leaves a dangling head or a dangling
		/// NextCoverLink, and the next walker reads freed memory.
		bool UnspliceFromCoverList(ABioWorldInfo* wi, ACoverLink* link)
		{
			if (wi == nullptr || link == nullptr)
			{
				return false;
			}

			if (wi->CoverList == link)
			{
				wi->CoverList = link->NextCoverLink;
				link->NextCoverLink = nullptr;
				return true;
			}

			auto* previous = wi->CoverList;
			for (int i = 0; previous != nullptr && i < k_maxChainWalk; i++)
			{
				if (previous->NextCoverLink == link)
				{
					previous->NextCoverLink = link->NextCoverLink;
					link->NextCoverLink = nullptr;
					return true;
				}
				previous = previous->NextCoverLink;
			}
			return false;
		}

		// ! Runtime ReachSpec construction
		// ========================================
		//
		// CoverSlotMarkers are joined to each other left-to-right by USlotToSlotReachSpecs.
		// Nothing in the shipping exe builds them: FPathBuilder retains only GetScout and
		// DestroyScout, and the only spec code left on ACoverSlotMarker is GetReachSpecClass
		// (which picks a class) and CanPrunePath (which protects one). A level-loaded marker
		// has its PathList because the specs were serialised, which is why the CoverHost
		// round-trip works and a live-spawned link does not.
		//
		// So we construct them. StaticConstructObject is already resolved for this ASI, and
		// LESDK's TArray grows through the game's own GMalloc, so PathList can be appended to
		// directly.

		/// UE3 EReachSpecFlags. A slot-to-slot move is a walk.
		constexpr int k_reachFlagWalk = 1;

		/// The pawn size a spec is built for. ABioScout::CanDoMove looks up "Human" and falls
		/// back to PathSizes(0); retail Default__BioScout ships Common/Large/VeryLarge/Max/
		/// Vehicle and no "Human", so entry 0 is what the cover analysis actually used and it
		/// is what these specs should be sized for too.
		bool GetScoutPathSize(AScout* scout, float& outRadius, float& outHeight)
		{
			if (scout == nullptr || scout->PathSizes.Count() == 0)
			{
				return false;
			}
			for (int i = 0; i < static_cast<int>(scout->PathSizes.Count()); i++)
			{
				const char* const desc = scout->PathSizes(i).Desc.GetName();
				if (desc != nullptr && _stricmp(desc, "Human") == 0)
				{
					outRadius = scout->PathSizes(i).Radius;
					outHeight = scout->PathSizes(i).Height;
					return true;
				}
			}
			outRadius = scout->PathSizes(0).Radius;
			outHeight = scout->PathSizes(0).Height;
			return true;
		}

		/// Creates one directed spec and appends it to Start's PathList.
		///
		/// @returns The spec, or nullptr if it could not be constructed.
		UReachSpec* CreateSpec(ANavigationPoint* start, ANavigationPoint* end,
			float radius, float height, int reachFlags)
		{
			if (start == nullptr || end == nullptr || start == end)
			{
				return nullptr;
			}

			// ReachSpecs are outered to the level, the same as the nav points they join.
			auto* const spec = StaticConstructObject<USlotToSlotReachSpec>(
				start->Outer, SFXName{}, static_cast<EObjectFlags>(0));
			if (spec == nullptr)
			{
				return nullptr;
			}

			const FVector a = start->LOCATION;
			const FVector b = end->LOCATION;
			const FVector delta{ b.X - a.X, b.Y - a.Y, b.Z - a.Z };
			const float distance = std::sqrt(
				delta.X * delta.X + delta.Y * delta.Y + delta.Z * delta.Z);

			spec->Start = start;
			spec->End.Actor = end;
			spec->End.Guid = FGuid{};
			spec->Direction = (distance > 0.001f)
				? FVector{ delta.X / distance, delta.Y / distance, delta.Z / distance }
				: FVector{};
			spec->Distance = static_cast<int>(distance);
			spec->CollisionRadius = static_cast<int>(radius);
			spec->CollisionHeight = static_cast<int>(height);
			spec->reachFlags = reachFlags;

			// Every shipped spec measured on BioD_CerMir_125Land has this set, and
			// ANavigationPoint::AddToNavigationOctree walks PathList and registers them along
			// with the marker -- so they do get added, and this is not the dead flag it looked
			// like before there was a shipped spec to read.
			spec->bAddToNavigationOctree = 1;

			start->PathList.Add(spec);
			return spec;
		}

		// ! Navigation octree registration
		// ========================================
		//
		// This is the gate. BioPlayerController::FindPlayerCover (0x7FF74B416F30) finds cover by
		// running NavigationOctreeRadiusCheck around the pawn and keeping every hit whose
		// FNavigationOctreeObject::OwnerType has bit 0x04 -- CoverSlotMarker. A marker that is
		// not in the octree is invisible to the player's cover search no matter how correct
		// everything else is, which is exactly what we measured: OctreeNode NULL, OwnerType
		// 0x00, and GetAllNavInRadius returning nothing at all.
		//
		// ANavigationPoint::AddToNavigationOctree survived as vtable slot 265
		// (0x7FF74AE88C20). It:
		//   - early-outs unless CylinderComponent (+0x314) is set and OctreeNode (+0x270) is null
		//   - FNavigationOctreeObject::SetOwner  -> Owner = this, OwnerType = 1 | 4 for a marker
		//   - SetBox from the cylinder's CollisionRadius/CollisionHeight around Location
		//   - FNavigationOctree::AddObject
		//   - then adds every ReachSpec in PathList
		//
		// That last step is why this has to run AFTER the spec pass, not before.
		constexpr size_t k_vtableIndexAddToNavigationOctree = 265;

		void CallAddToNavigationOctree(ANavigationPoint* point)
		{
			if (point == nullptr)
			{
				return;
			}
			using tAddToNavigationOctree = void(ANavigationPoint*);
			auto* const vtable = *reinterpret_cast<void***>(point);
			auto* const fn = reinterpret_cast<tAddToNavigationOctree*>(
				vtable[k_vtableIndexAddToNavigationOctree]);
			fn(point);
		}

		/// Result of one link's octree pass.
		struct OctreeResult
		{
			int registered{ 0 };
			int alreadyIn{ 0 };
			int noCylinder{ 0 };
			int stillOut{ 0 };
			int wrongOwnerType{ 0 };
		};

		OctreeResult RegisterMarkersInOctree(ACoverLink* link)
		{
			OctreeResult result{};
			if (link == nullptr)
			{
				return result;
			}

			const int slots = static_cast<int>(link->Slots.Count());
			for (int i = 0; i < slots; i++)
			{
				auto* const marker = link->Slots(i).SlotMarker;
				if (marker == nullptr)
				{
					continue;
				}

				if (marker->NavOctreeObject.OctreeNode != nullptr)
				{
					result.alreadyIn++;
					continue;
				}

				// The early-out that would make this silently do nothing. A marker spawned at
				// runtime gets its CylinderComponent from the CDO like any other subobject, but
				// if that ever comes back null the octree add is skipped without a word.
				if (marker->CylinderComponent == nullptr)
				{
					result.noCylinder++;
					continue;
				}

				CallAddToNavigationOctree(marker);

				if (marker->NavOctreeObject.OctreeNode == nullptr)
				{
					result.stillOut++;
				}
				else
				{
					result.registered++;
					// OwnerType must carry bit 0x04 or FindPlayerCover skips the hit even though
					// the octree returns it.
					if ((marker->NavOctreeObject.OwnerType & 0x04) == 0)
					{
						result.wrongOwnerType++;
					}
				}
			}
			return result;
		}

		/// Result of one link's spec pass, so the command can report honestly.
		struct SpecBuildResult
		{
			int created{ 0 };
			int skippedNoMarker{ 0 };
			int skippedDisabled{ 0 };
			int failed{ 0 };
		};

		/// Joins each marker to its immediate neighbours, both directions -- the "left to right"
		/// chain. Slot order along the link IS the left-to-right order, which is what
		/// FindCoverEdges' right-hand walk produces.
		///
		/// Only adjacent pairs: UE3 built the slot chain that way, and CanCoverSlip / mantle /
		/// swat-turn targets get their own spec classes rather than being folded in here.
		SpecBuildResult BuildSlotToSlotSpecs(ACoverLink* link, float radius, float height)
		{
			SpecBuildResult result{};
			if (link == nullptr)
			{
				return result;
			}

			const int slots = static_cast<int>(link->Slots.Count());
			for (int i = 0; i + 1 < slots; i++)
			{
				auto* const a = link->Slots(i).SlotMarker;
				auto* const b = link->Slots(i + 1).SlotMarker;
				if (a == nullptr || b == nullptr)
				{
					result.skippedNoMarker++;
					continue;
				}

				// A disabled slot is not cover, so nothing should path onto it. Leaving it in
				// the chain would also break the chain's meaning: neighbours either side of a
				// dead slot are not actually adjacent.
				if (!link->Slots(i).bEnabled || !link->Slots(i + 1).bEnabled)
				{
					result.skippedDisabled++;
					continue;
				}

				if (CreateSpec(a, b, radius, height, k_reachFlagWalk) != nullptr)
				{
					result.created++;
				}
				else
				{
					result.failed++;
				}

				if (CreateSpec(b, a, radius, height, k_reachFlagWalk) != nullptr)
				{
					result.created++;
				}
				else
				{
					result.failed++;
				}
			}
			return result;
		}

		/// ANavigationPoint::RemoveFromNavigationOctree, vtable slot 266 (0x7FF74AE90EF0). Takes
		/// the marker and every ReachSpec on its PathList back out of the octree.
		/// ANavigationPoint::PostScriptDestroyed calls this itself, so destroying a marker
		/// cleans up after itself -- this is only needed when we tear down a live one.
		constexpr size_t k_vtableIndexRemoveFromNavigationOctree = 266;

		void CallRemoveFromNavigationOctree(ANavigationPoint* point)
		{
			if (point == nullptr)
			{
				return;
			}
			using tRemove = void(ANavigationPoint*);
			auto* const vtable = *reinterpret_cast<void***>(point);
			auto* const fn = reinterpret_cast<tRemove*>(
				vtable[k_vtableIndexRemoveFromNavigationOctree]);
			fn(point);
		}

		/// Empties every marker's PathList so the pass can be re-run. The specs themselves are
		/// left to garbage collection -- they are UObjects, not actors, and nothing else refers
		/// to them once the list is cleared.
		///
		/// The octree comes first. RemoveFromNavigationOctree walks PathList to find the spec
		/// entries to unregister, so clearing the list while the marker is still registered
		/// would strand those entries in the octree pointing at collected objects.
		int ClearSlotSpecs(ACoverLink* link)
		{
			if (link == nullptr)
			{
				return 0;
			}
			int cleared = 0;
			const int slots = static_cast<int>(link->Slots.Count());
			for (int i = 0; i < slots; i++)
			{
				if (auto* const marker = link->Slots(i).SlotMarker)
				{
					if (marker->NavOctreeObject.OctreeNode != nullptr)
					{
						CallRemoveFromNavigationOctree(marker);
					}
					cleared += static_cast<int>(marker->PathList.Count());
					marker->PathList.Clear();
				}
			}
			return cleared;
		}

		/// Whether a link spawned earlier this session already covers this surface point.
		///
		/// FindCoverEdges converges on the geometry rather than on the click: the sandbox run
		/// seeded two links 343u apart on one wall face and both walked out to the same twelve
		/// slots and the same 1546u span, flag-for-flag identical. That is the port working --
		/// but it means one link per click is wrong, and a Phase 3 sweep that samples a surface
		/// would emit a duplicate at every sample point.
		///
		/// A slot sits AlignDist out from the face it covers, so push each slot back along its
		/// own forward axis to recover the surface point it was placed against, and compare
		/// that. The facing test is what keeps the two sides of a thin wall apart -- the
		/// sandbox wall is only ~101u thick, so a distance test alone would fuse them.
		///
		/// @returns The link already covering the point, or nullptr.
		float DistanceToSegment(const FVector& p, const FVector& a, const FVector& b)
		{
			const FVector ab{ b.X - a.X, b.Y - a.Y, b.Z - a.Z };
			const float lenSq = ab.X * ab.X + ab.Y * ab.Y + ab.Z * ab.Z;

			float t = 0.0f;
			if (lenSq > 0.0001f)
			{
				t = ((p.X - a.X) * ab.X + (p.Y - a.Y) * ab.Y + (p.Z - a.Z) * ab.Z) / lenSq;
				t = (t < 0.0f) ? 0.0f : ((t > 1.0f) ? 1.0f : t);
			}

			const float dx = p.X - (a.X + ab.X * t);
			const float dy = p.Y - (a.Y + ab.Y * t);
			const float dz = p.Z - (a.Z + ab.Z * t);
			return std::sqrt(dx * dx + dy * dy + dz * dz);
		}

		/// Whether `link` already has cover along the surface at `surfacePoint`.
		///
		/// The test is against the SEGMENTS between consecutive projected slots, not against
		/// the projected points. Points alone was wrong and it mattered as soon as anything
		/// seeded automatically: slots sit up to AutoCoverSlotInterval (256u) apart, so a
		/// candidate landing between two of them is further than any sane radius from both and
		/// seeds a second link straight down the middle of the first.
		bool LinkCoversSurface(ACoverLink* link, const FVector& surfacePoint,
			const FVector& surfaceNormal, float alignDist)
		{
			if (link == nullptr)
			{
				return false;
			}

			// Half a slot interval either way: close enough that a new link would overlap the
			// existing one rather than extend it.
			const float radius = alignDist * 2.0f;

			bool bHavePrevious = false;
			FVector previous{};

			const int slotCount = static_cast<int>(link->Slots.Count());
			for (int i = 0; i < slotCount; i++)
			{
				FVector slotX{}, slotY{}, slotZ{};
				RotatorToAxes(link->GetSlotRotation(i, FALSE), slotX, slotY, slotZ);

				// A slot faces INTO its wall, so its forward is the inward normal. If this
				// surface faces the other way, it is the far side of the geometry. This test
				// is what keeps the two sides of a thin wall apart -- the sandbox wall is only
				// ~101u thick, so a distance test alone would fuse them.
				const float facing = slotX.X * -surfaceNormal.X
					+ slotX.Y * -surfaceNormal.Y + slotX.Z * -surfaceNormal.Z;
				if (facing < 0.9f)
				{
					// Break the run: the segment from a slot on this face to one on another
					// face cuts through the geometry and covers nothing.
					bHavePrevious = false;
					continue;
				}

				// A slot sits AlignDist out from the face it covers, so push it back along its
				// own forward axis to recover the surface point it was placed against.
				const FVector slotLoc = link->GetSlotLocation(i, FALSE);
				const FVector projected = {
					slotLoc.X + slotX.X * alignDist,
					slotLoc.Y + slotX.Y * alignDist,
					slotLoc.Z + slotX.Z * alignDist
				};

				if (bHavePrevious)
				{
					if (DistanceToSegment(surfacePoint, previous, projected) <= radius)
					{
						return true;
					}
				}
				else
				{
					const float dx = projected.X - surfacePoint.X;
					const float dy = projected.Y - surfacePoint.Y;
					const float dz = projected.Z - surfacePoint.Z;
					if (dx * dx + dy * dy + dz * dz <= radius * radius)
					{
						return true;
					}
				}

				previous = projected;
				bHavePrevious = true;
			}

			return false;
		}

		ACoverLink* FindLinkCoveringSurface(const FVector& surfacePoint,
			const FVector& surfaceNormal, float alignDist)
		{
			for (auto* const link : g_SpawnedLinks)
			{
				if (LinkCoversSurface(link, surfacePoint, surfaceNormal, alignDist))
				{
					return link;
				}
			}
			return nullptr;
		}

		/// The same question asked of the cover the LEVEL shipped with. cc.spawnlink does not
		/// ask it -- if you aim at shipped cover you presumably meant to -- but a sweep that
		/// did not would carpet every shipped level with duplicates of its own cover.
		ACoverLink* FindWorldLinkCoveringSurface(ABioWorldInfo* wi, const FVector& surfacePoint,
			const FVector& surfaceNormal, float alignDist)
		{
			if (wi == nullptr)
			{
				return nullptr;
			}

			auto* node = wi->CoverList;
			for (int i = 0; node != nullptr && i < k_maxChainWalk; i++, node = node->NextCoverLink)
			{
				if (LinkCoversSurface(node, surfacePoint, surfaceNormal, alignDist))
				{
					return node;
				}
			}
			return nullptr;
		}

		/// Spawns a link against the surface at (point, normal) and runs the placement walk.
		///
		/// Factored out of CmdSpawnLink so that cc.sweep drives exactly the same pipeline
		/// rather than a parallel copy of it that can drift. Requires a scout to ALREADY be
		/// held: FindCoverEdges classifies each slot as it goes, and AutoAdjustSlot needs one.
		///
		/// @returns the spawned link, or nullptr if the spawn itself failed.
		ACoverLink* SpawnLinkOnSurface(ABioWorldInfo* wi, ACoverLink* defaults, AActor* tracer,
			const FVector& surfacePoint, const FVector& surfaceNormal,
			FindCoverEdgesResult& outEdges)
		{
			// Link faces INTO the wall and sits 128u out from it -- EditorAutoSetup's
			// convention.
			const FRotator linkRotation = FVectorToFRotator(
				FVector{ -surfaceNormal.X, -surfaceNormal.Y, -surfaceNormal.Z });
			const FVector linkLocation = {
				surfacePoint.X + surfaceNormal.X * 128.0f,
				surfacePoint.Y + surfaceNormal.Y * 128.0f,
				surfacePoint.Z + surfaceNormal.Z * 128.0f
			};

			// ANavigationPoint defaults bStatic=true and bNoDelete=true
			// (NavigationPoint.uc:533), and ACoverLink inherits both. UWorld::SpawnActor
			// refuses outright once play has begun -- "SpawnActor failed because class %s has
			// bStatic or bNoDelete" (UnLevAct.cpp:406) -- and Destroy() refuses the same pair
			// at UnLevAct.cpp:739. That is why every spawn attempt returned null and why
			// despawning would have failed too. Clear the flags on the CDO across the Spawn
			// call: the new actor copies its property values from CDO memory at construction,
			// so it comes out non-static and deletable while the CDO is put back as it was.
			const unsigned long savedStatic = defaults->bStatic;
			const unsigned long savedNoDelete = defaults->bNoDelete;
			defaults->bStatic = 0;
			defaults->bNoDelete = 0;

			auto* const spawned = wi->Spawn(ACoverLink::StaticClass(), nullptr, SFXName{},
				linkLocation, linkRotation, nullptr, TRUE /*bNoCollisionFail*/, FALSE);

			defaults->bStatic = savedStatic;
			defaults->bNoDelete = savedNoDelete;

			if (spawned == nullptr)
			{
				return nullptr;
			}
			auto* const link = static_cast<ACoverLink*>(spawned);
			// Belt and braces: if anything re-initialised the instance from the CDO, Destroy()
			// would start failing again and cc.despawnlinks would silently leak links.
			link->bStatic = 0;
			link->bNoDelete = 0;
			g_SpawnedLinks.push_back(link);

			// Nothing about a spawned link is visible on its own, so the overlay comes on with
			// the first one. cc.drawlinks toggles it back off.
			DrawSpawnedLinks = true;

			// Level load threads every CoverLink onto WorldInfo.CoverList; Spawn does not.
			// Without this the link is invisible to whatever selects cover, however good its
			// slots are.
			SpliceIntoCoverList(wi, link);

			// Without this AutoAdjustSlot runs only the cover-type pass and never reaches
			// BuildSlotInfo -- the exact trap cc.testautoadjust hit on shipped links.
			link->bAutoAdjust = 1;

			// EditorAutoSetup's starting point: Location + AxisX*96 + AxisZ*16, which lands
			// ~32u in front of the surface (Location is already 128u out along the normal)
			// and 16u up.
			FVector linkAxisX{}, linkAxisY{}, linkAxisZ{};
			RotatorToAxes(linkRotation, linkAxisX, linkAxisY, linkAxisZ);
			const FVector scanStart = {
				linkLocation.X + linkAxisX.X * 96.0f + linkAxisZ.X * 16.0f,
				linkLocation.Y + linkAxisX.Y * 96.0f + linkAxisZ.Y * 16.0f,
				linkLocation.Z + linkAxisX.Z * 96.0f + linkAxisZ.Z * 16.0f
			};

			outEdges = FindCoverEdges(link, tracer, scanStart,
				linkAxisX, linkAxisY, linkAxisZ);

			const int actualSlots = static_cast<int>(link->Slots.Count());
			for (int i = 0; i < actualSlots; i++)
			{
				link->AutoAdjustSlot(i, FALSE);
				link->AutoAdjustSlot(i, TRUE);
			}

			// Keep the provenance. See g_LinkEdges.
			g_LinkEdges[link] = outEdges;
			return link;
		}
	}

	const FindCoverEdgesResult* GetLinkEdges(ACoverLink* link)
	{
		const auto found = g_LinkEdges.find(link);
		return (found != g_LinkEdges.end()) ? &found->second : nullptr;
	}

	const SlotPlacement* GetSlotPlacement(ACoverLink* link, int slotIdx)
	{
		const auto* const edges = GetLinkEdges(link);
		if (edges == nullptr) { return nullptr; }
		for (const auto& placement : edges->placements)
		{
			if (placement.slotIdx == slotIdx) { return &placement; }
		}
		return nullptr;
	}

	const std::vector<ACoverLink*>& GetSpawnedLinks()
	{
		return g_SpawnedLinks;
	}

	void CmdSpawnLink()
	{
		LEASI_INFO("=== cc.spawnlink ===");

		auto* const wi = GetWorldInfo();
		if (wi == nullptr)
		{
			LEASI_ERROR("No WorldInfo.");
			return;
		}
		auto* const pc = wi->GetALocalPlayerController();
		if (pc == nullptr || pc->Pawn == nullptr || pc->PlayerCamera == nullptr)
		{
			LEASI_ERROR("Need a player controller with a pawn and camera.");
			return;
		}
		auto* const defaults = GetCoverLinkDefaults();
		if (defaults == nullptr)
		{
			LEASI_ERROR("Could not reach the ACoverLink CDO.");
			return;
		}

		// Aim at the cover surface, exactly as EditorAutoSetup did from an editor click.
		AActor* hitActor = nullptr;
		FVector hitLocation{}, hitNormal{};
		auto* const camera = static_cast<ASFXPlayerCamera*>(pc->PlayerCamera);
		if (!camera->GetTrace(&hitActor, &hitLocation, &hitNormal))
		{
			LEASI_ERROR("Camera trace hit nothing. Aim at a wall or waist-high cover.");
			return;
		}

		// EditorAutoSetup rejects near-horizontal surfaces -- you cannot take cover on a floor.
		const float upDot = std::abs(hitNormal.Z);
		if (upDot > 0.3f)
		{
			LEASI_ERROR("Surface normal is too horizontal (abs(N.Z) = {:.2f} > 0.30). That is a "
				"floor or ceiling, not cover. Aim at a vertical face.", upDot);
			return;
		}

		const float alignDist = defaults->AlignDist;
		const float slotInterval = defaults->AutoCoverSlotInterval;
		LEASI_INFO("Surface hit at ({:.1f}, {:.1f}, {:.1f}), normal ({:.2f}, {:.2f}, {:.2f})",
			hitLocation.X, hitLocation.Y, hitLocation.Z, hitNormal.X, hitNormal.Y, hitNormal.Z);
		LEASI_INFO("CDO: AlignDist={:.1f} AutoCoverSlotInterval={:.1f} StandHeight={:.1f} MidHeight={:.1f}",
			alignDist, slotInterval, defaults->StandHeight, defaults->MidHeight);

		if (auto* const covering = FindLinkCoveringSurface(hitLocation, hitNormal, alignDist))
		{
			LEASI_WARN("{} already covers this surface point. Spawning anyway, but the two "
				"links will overlap -- FindCoverEdges walks out to the same edges from any "
				"start on the same face, so this one will duplicate it. cc.despawnlinks and "
				"re-seed elsewhere if that was not deliberate.", PathOf(covering));
		}

		// The scout has to exist before placement starts: FindCoverEdges classifies each slot
		// as it goes (AutoAdjustSlot), because the walk reads the previous slot's CoverType
		// to decide where standing<->mid transitions belong.
		ScopedScout guard{};
		if (!guard)
		{
			LEASI_ERROR("Could not acquire a scout; aborting before placement.");
			return;
		}

		// Trace as the pawn, never as the link -- tracing as the link makes every probe hit its
		// own collision cylinder.
		auto* const tracer = pc->Pawn;

		FindCoverEdgesResult edges{};
		auto* const link = SpawnLinkOnSurface(wi, defaults, tracer, hitLocation, hitNormal, edges);
		if (link == nullptr)
		{
			LEASI_ERROR("Spawn(ACoverLink) returned null even with bStatic/bNoDelete cleared.");
			return;
		}
		LEASI_INFO("Spawned {} at ({:.1f}, {:.1f}, {:.1f}), spliced onto WorldInfo.CoverList.",
			PathOf(link), link->location.X, link->location.Y, link->location.Z);

		LEASI_INFO("FindCoverEdges: left edge {}, right edge {} ({} / {} iterations). "
			"{} slot(s) placed, {:.0f}u span.",
			edges.bFoundLeftEdge ? "found" : "NOT FOUND",
			edges.bFoundRightEdge ? "found" : "NOT FOUND",
			edges.leftWalkIterations, edges.rightWalkIterations,
			edges.slotsPlaced, edges.span);

		if (edges.floorRise > 8.0f || edges.floorRise < -8.0f)
		{
			LEASI_INFO("The floor moved {:+.0f}u across the right-hand walk; the walk line "
				"followed it.", edges.floorRise);
		}

		// Provenance. By the time a slot is readable it has been ground-snapped, re-aimed
		// and re-seated by AutoAdjustSlot, so its final position says nothing about which
		// decision produced it. `moved` and `turned` are the disagreement between the walk
		// and the engine's own surface sweep: a large one means the slot is no longer on
		// the surface the walk measured, which is a placement bug even when it ends up
		// enabled and looks fine in the overlay.
		if (!edges.placements.empty())
		{
			LEASI_INFO("--- placement provenance (placed by the walk -> seated by the engine) ---");
			for (const auto& p : edges.placements)
			{
				const float dx = p.seatedLocation.X - p.placedLocation.X;
				const float dy = p.seatedLocation.Y - p.placedLocation.Y;
				const float dz = p.seatedLocation.Z - p.placedLocation.Z;
				const float moved = std::sqrt(dx * dx + dy * dy + dz * dz);

				// Yaw is 16-bit and wraps, so difference it in rotation units first.
				const int rawTurn = static_cast<short>(
					static_cast<unsigned short>(p.seatedYaw - p.placedYaw));
				const float turned = static_cast<float>(rawTurn) * 360.0f / 65536.0f;

				const char* const fate = p.bDisabledFirstPass
					? (p.bRecovered ? " [disabled -> recovered]" : " [DISABLED, STAYED DEAD]")
					: "";

				LEASI_INFO("  slot {} {}: ({:.0f}, {:.0f}, {:.0f}) -> ({:.0f}, {:.0f}, {:.0f}), "
					"moved {:.0f}u, turned {:+.0f} deg{}",
					p.slotIdx, SlotOriginName(p.origin),
					p.placedLocation.X, p.placedLocation.Y, p.placedLocation.Z,
					p.seatedLocation.X, p.seatedLocation.Y, p.seatedLocation.Z,
					moved, turned, fate);

				// AlignDist is the whole scale of this algorithm; a slot that had to move
				// further than that to find a surface is on a different surface.
				if (moved > 36.0f || turned > 30.0f || turned < -30.0f)
				{
					LEASI_WARN("    slot {} was seated onto a surface the walk was not "
						"following. The walk put it against one wall and AutoAdjustSlot's "
						"sweep found a closer or better-aligned one -- check for a recess, "
						"a window reveal or a doorway jamb at that position.", p.slotIdx);
				}
			}
		}

		if (edges.slotsDisabled > 0)
		{
			// See ClassifySlot: the engine disables a slot whose *placed* facing has no
			// wall in front of it, then fixes the facing afterwards and never re-tests.
			if (edges.slotsDisabled == edges.slotsRecovered)
			{
				LEASI_INFO("AutoAdjustSlot disabled {} slot(s) on its first pass; all {} came "
					"back on the retry against the corrected facing.",
					edges.slotsDisabled, edges.slotsRecovered);
			}
			else
			{
				LEASI_WARN("AutoAdjustSlot disabled {} slot(s) and only {} recovered -- {} "
					"slot(s) are placed against no geometry at all and stay dead. That is a "
					"placement bug, not an analysis one.",
					edges.slotsDisabled, edges.slotsRecovered,
					edges.slotsDisabled - edges.slotsRecovered);
			}
		}

		if (!edges.bFoundLeftEdge)
		{
			// UE3 destroys the link outright here. Keep it so the failure can be inspected.
			LEASI_ERROR("Never found a left edge in {} iterations -- the walk did not terminate "
				"against any geometry. The link has been kept so you can look at it; "
				"cc.despawnlinks to remove.", edges.leftWalkIterations);
		}
		else if (!edges.bFoundRightEdge)
		{
			LEASI_WARN("Found the left edge but ran out of iterations walking right, so the right "
				"end of this link is wherever the walk gave up.");
		}

		const int actualSlots = static_cast<int>(link->Slots.Count());
		if (actualSlots == 0)
		{
			LEASI_ERROR("Placement produced no slots.");
			return;
		}

		LEASI_INFO("--- AutoAdjustSlot re-run over {} slots (bAutoAdjust=TRUE) ---", actualSlots);

		LEASI_INFO("--- what the engine decided ---");
		int standing = 0, midLevel = 0, none = 0, withMove = 0, withMantle = 0;
		int disabled = 0, unanalysed = 0;
		for (int i = 0; i < actualSlots; i++)
		{
			const FCoverSlot& slot = link->Slots(i);
			LogSlotState(link, i, slot);

			switch (slot.CoverType)
			{
			case 1:  standing++; break;
			case 2:  midLevel++; break;
			default: none++;     break;
			}

			constexpr unsigned long k_specialMoveMask =
				0x00000010 | 0x00000020 | 0x00000100 | 0x00000200 |
				0x00000400 | 0x00000800 | 0x00001000 | 0x00002000;
			const unsigned long flags = ReadSlotFlags(slot);
			if ((flags & k_specialMoveMask) != 0) withMove++;
			if ((flags & 0x00000010) != 0)        withMantle++;

			// A slot AutoAdjustSlot disabled never reached BuildSlotInfo, so every "can" bit
			// on it is still the archetype default rather than a result -- see note 06. That
			// is easy to misread as a working slot, and it would serialise as dead cover, so
			// call it out rather than letting it pass in the tally.
			//
			// The archetype (Default__CoverLink.Slots(0)) reads 0x001BFF10: bCanMantle, both
			// bCanCoverSlip_*, both bCanSwatTurn_*, both bCanCoverTurn_*, bEnabled, and the
			// bAllow* set. Matching that on everything except the lean bits means the analysis
			// never touched this slot.
			constexpr unsigned long k_archetypeFlags = 0x001BFF10;
			// Ignore the bits AutoAdjustSlot legitimately writes on its own (the leans and
			// bEnabled) plus the unnamed high bits, and compare what is left.
			constexpr unsigned long k_compareMask =
				k_knownSlotFlagMask & ~0x00000003ul /*leans*/ & ~0x00004000ul /*bEnabled*/;

			if ((flags & 0x00004000) == 0)
			{
				disabled++;
			}
			if ((flags & k_compareMask) == (k_archetypeFlags & k_compareMask))
			{
				unanalysed++;
				LEASI_WARN("    slot {} still carries the archetype defaults (0x{:08X} vs "
					"archetype 0x{:08X}) -- BuildSlotInfo did not run on it, so every 'can' "
					"bit above is an inherited default, NOT a result.", i, flags,
					k_archetypeFlags);
			}

			if (slot.MantleTarget.Actor != nullptr)
			{
				LEASI_INFO("    MantleTarget -> {} slot {} ({})",
					PathOf(reinterpret_cast<UObject*>(slot.MantleTarget.Actor)),
					slot.MantleTarget.SlotIdx,
					slot.MantleTarget.SlotIdx == -1 ? "climb-up onto a MantleMarker"
					                                : "vault-over to a cover slot");
			}
		}

		LEASI_INFO("Result: {} slots -- {} standing, {} midlevel, {} CT_None. "
			"{} with a special move, {} mantleable.",
			actualSlots, standing, midLevel, none, withMove, withMantle);

		if (disabled > 0 || unanalysed > 0)
		{
			LEASI_WARN("{} slot(s) disabled, {} still on archetype defaults. AutoAdjustSlot "
				"disables a slot it cannot seat against a surface, and a disabled slot never "
				"reaches BuildSlotInfo -- so it would serialise as dead cover. Usually our "
				"placement ran the slot off the end of the geometry (Phase 2). cc.buildlinks "
				"calls BuildSlotInfo directly and will analyse them anyway.",
				disabled, unanalysed);
		}

		if (none > 0)
		{
			LEASI_WARN("{} slot(s) came back CT_None, meaning the engine found no cover surface "
				"in front of them. Our crude placement probably ran them off the end of the "
				"geometry -- that is a placement bug (Phase 2), not an engine problem.", none);
		}

		// SlotMarker is null on everything AddCoverSlot creates in-game, and a slot with no
		// marker is not enterable cover -- ABioScout::CanCoverSlip also abandons its nav search
		// the moment GetSlotMarker returns null, so bCanCoverSlip_* can be set here while
		// SlipTarget stays empty. cc.buildlinks creates the markers.
		LEASI_INFO("Note: these slots have no CoverSlotMarker yet, so this cover is not "
			"enterable and SlipTarget/SlipRefs will not populate. Run cc.buildlinks.");
		// ABioScout::CanMantle iterates GWorld->Levels and each level's Actors array -- NOT
		// the nav/cover linked lists -- so Spawn alone is enough for this link to be found as
		// a mantle destination. But the analysis above ran when only the cover placed so far
		// existed, so run cc.buildlinks once every link is down.
		LEASI_INFO("Run cc.buildlinks after placing all links to redo the analysis with every "
			"link visible.");
		LEASI_INFO("Use cc.despawnlinks to remove the {} link(s) spawned this session.",
			g_SpawnedLinks.size());
	}

	// ! cc.sweep -- automatic cover discovery
	// ========================================
	//
	// Everything a sweep needs was already here; what was missing was a source of seed points.
	// Two properties are what make sweeping tractable at all:
	//
	//   * FindCoverEdges CONVERGES. Two seeds 343u apart on the same face produced
	//     byte-identical slots (note 04). So the sweep never has to work out where the middle
	//     of a wall is, or how long the wall is, or where it ends. It does not need GOOD
	//     seeds, only ENOUGH of them.
	//   * LinkCoversSurface answers "is this face already taken", so the duplicates that
	//     sampling everything necessarily produces can be thrown away.
	//
	// So: find where a player can stand, fan traces out from each such place, keep the
	// vertical faces, drop the ones already covered, and run the ordinary
	// spawn -> FindCoverEdges -> buildlinks pipeline on what is left.
	//
	// Candidates are judged and spawned ONE AT A TIME, never gathered and then built. That
	// ordering is the whole dedupe: each new link has to exist before the next candidate is
	// tested, or a wall hit by sixteen rays from four samples becomes sixty-four links.

	namespace
	{
		/// Rays per fan. At the reach below this leaves ~200u gaps at maximum range, which
		/// adjacent samples close. More rays is the wrong knob to turn for coverage -- closer
		/// samples is the right one, because a ray fired from further away is more likely to
		/// be blocked by something in between.
		constexpr int k_sweepRaysPerFan = 16;

		/// Floor under the automatic budgets, so a small sweep behaves exactly as it did
		/// before the budgets started scaling with the radius.
		constexpr int k_sweepMinSamples = 400;

		/// Ceiling on the automatic sample budget. Not a performance limit -- the radius is
		/// that -- but a bound on how much memory the visited set and the sample list can take.
		constexpr int k_sweepSampleCeiling = 200000;

		/// Past this the sweep is long enough that it needs to say so: the whole thing runs
		/// inside one Exec call, so the game is frozen until it finishes and an unwarned user
		/// would reasonably conclude it had hung.
		constexpr float k_sweepLongRadius = 8192.0f;

		constexpr float k_sweepDefaultRadius = 2048.0f;
		constexpr float k_sweepMinRadius = 256.0f;

		/// How often a long sweep says it is still alive.
		constexpr int k_sweepProgressEvery = 100;

		struct FloorSample
		{
			/// At walk height, i.e. rideHeight above floorZ.
			FVector location{};
			float   floorZ{ 0.0f };
		};

		/// Quantised sample position. The Z component is what stops the two floors of a
		/// multi-level room merging into one -- the existing path-node flood fill snaps to a
		/// world-aligned grid without one, which is note 04's Phase 3 complaint.
		struct SampleKey
		{
			int x{ 0 }, y{ 0 }, z{ 0 };

			bool operator<(const SampleKey& other) const
			{
				if (x != other.x) { return x < other.x; }
				if (y != other.y) { return y < other.y; }
				return z < other.z;
			}
		};

		struct SweepStats
		{
			int samples{ 0 };
			int sampleBudgetHit{ 0 };
			int neighboursNoFloor{ 0 };
			int neighboursBlocked{ 0 };

			int raysFired{ 0 };
			int rayMissed{ 0 };
			int rejectedHorizontal{ 0 };
			int rejectedNoFloorAtWall{ 0 };
			int rejectedWrongStorey{ 0 };
			int rejectedSeeded{ 0 };
			int linksStalled{ 0 };
			/// Links whose slots all sit inside one AlignDist of each other -- a "link" that
			/// is really a single point. 53 of 327 on the sandbox, and 185 links came out with
			/// exactly two slots, so this is now the biggest placement defect by count.
			int linksDegenerate{ 0 };
			/// Links whose right-hand walk climbed or dropped more than a slot interval.
			/// Cover that runs up a wall was invisible in the sweep summary until a
			/// screenshot showed it, which is the sort of defect a counter should be catching.
			int linksClimbed{ 0 };
			float worstClimb{ 0.0f };
			/// Links whose slots ended up stacked more vertically than they are spread
			/// horizontally -- cover running UP a wall rather than along it.
			///
			/// Separate from linksClimbed because that counter measures floor movement, and a
			/// walk that leaves the floor takes its slots somewhere the floor probe never
			/// follows: seven links rising 400u each were reported as one climber. This one
			/// measures the slots themselves, so it cannot be fooled the same way.
			/// Links whose left walk stopped at an inside corner instead of turning onto the
			/// adjoining face. Not a defect -- it is the fix for the drift that was relocating
			/// whole links onto a neighbouring wall -- but worth a count, because if it fires
			/// on nearly every link the angle threshold is too tight.
			int leftStoppedAtCorner{ 0 };
			/// Corners the right walk turned only via the second probe in the "wall ran out"
			/// branch. Zero would mean that probe is not the reason some corners were ending
			/// as end caps, and the fix should be looked for elsewhere.
			int rightCornersRecovered{ 0 };
			int linksVertical{ 0 };
			float worstVertical{ 0.0f };
			int firstVerticalIndex{ -1 };
			int slotsReseated{ 0 };
			int seedFloorMissing{ 0 };
			float worstSeatLift{ 0.0f };
			/// Which branch placed the slots of those degenerate links, indexed by SlotOrigin.
			/// Counting them is the whole point: the walk has a dozen ways to emit a slot and
			/// guessing which one is responsible from a final position is not possible --
			/// AutoAdjustSlot has ground-snapped, re-aimed and re-seated it by then.
			int degenerateOrigins[k_slotOriginCount]{};
			/// Every slot in the sweep by the branch that placed it, and how many of those the
			/// engine then refused. The degenerate tally settled one bug in a single run by
			/// naming a branch; there is no reason to only have that view for degenerate
			/// links. "Which branches are firing, and are their slots surviving" is the
			/// question behind most of what is left.
			int allOrigins[k_slotOriginCount]{};
			int disabledOrigins[k_slotOriginCount]{};
			int rejectedOurs{ 0 };
			int rejectedShipped{ 0 };
			int linksAdrift{ 0 };

			int linksSpawned{ 0 };
			int spawnFailed{ 0 };
			int noSlots{ 0 };
			int noLeftEdge{ 0 };
			int slotsTotal{ 0 };
			int slotsDead{ 0 };
		};

		/// A link's projected surface extent, padded by the dedupe radius.
		///
		/// Without this the dedupe is O(candidates x links x slots), and at whole-level scale
		/// that is not a constant factor to shrug at: hundreds of thousands of rays against a
		/// list growing into the hundreds of links is billions of slot tests, which is the
		/// difference between a sweep that takes a minute and one that never returns. The box
		/// is a conservative pre-filter -- inside it, run the real segment test; outside it,
		/// skip the link without touching a single slot.
		struct CoverBounds
		{
			ACoverLink* link{ nullptr };
			FVector min{};
			FVector max{};

			bool Contains(const FVector& p) const
			{
				return p.X >= min.X && p.X <= max.X
					&& p.Y >= min.Y && p.Y <= max.Y
					&& p.Z >= min.Z && p.Z <= max.Z;
			}
		};

		CoverBounds BoundsOf(ACoverLink* link, float alignDist)
		{
			CoverBounds bounds{};
			bounds.link = link;

			const int slotCount = (link != nullptr) ? static_cast<int>(link->Slots.Count()) : 0;
			if (slotCount == 0)
			{
				// Inverted, so Contains() is false for every point and the link is never
				// tested. A link with no slots covers no surface.
				bounds.min = FVector{ 1.0f, 1.0f, 1.0f };
				bounds.max = FVector{ -1.0f, -1.0f, -1.0f };
				return bounds;
			}

			for (int i = 0; i < slotCount; i++)
			{
				FVector slotX{}, slotY{}, slotZ{};
				RotatorToAxes(link->GetSlotRotation(i, FALSE), slotX, slotY, slotZ);
				const FVector slotLoc = link->GetSlotLocation(i, FALSE);
				const FVector projected = {
					slotLoc.X + slotX.X * alignDist,
					slotLoc.Y + slotX.Y * alignDist,
					slotLoc.Z + slotX.Z * alignDist
				};

				if (i == 0)
				{
					bounds.min = projected;
					bounds.max = projected;
					continue;
				}
				bounds.min.X = std::min(bounds.min.X, projected.X);
				bounds.min.Y = std::min(bounds.min.Y, projected.Y);
				bounds.min.Z = std::min(bounds.min.Z, projected.Z);
				bounds.max.X = std::max(bounds.max.X, projected.X);
				bounds.max.Y = std::max(bounds.max.Y, projected.Y);
				bounds.max.Z = std::max(bounds.max.Z, projected.Z);
			}

			// LinkCoversSurface accepts anything within alignDist*2 of the surface, so the box
			// has to be that much larger or it would reject points the real test would take.
			const float pad = alignDist * 2.0f;
			bounds.min = { bounds.min.X - pad, bounds.min.Y - pad, bounds.min.Z - pad };
			bounds.max = { bounds.max.X + pad, bounds.max.Y + pad, bounds.max.Z + pad };
			return bounds;
		}

		/// Every surface point that has already produced a link, quantised.
		///
		/// This exists because LinkCoversSurface is not sufficient on its own, and the
		/// 32768u sweep proved it: five pairs of EXACTLY coincident seed points each produced
		/// two separate links. LinkCoversSurface asks "do this link's slots cover this
		/// surface", and a link whose walk wandered off down an adjoining face answers no for
		/// its own seed point. Remembering the seeds answers a different and simpler question
		/// -- "have we already tried here" -- which is true regardless of where the walk ended
		/// up, and it is the question that actually prevents the duplicate.
		///
		/// Cell size is AlignDist, checked across the 27 neighbouring cells, so anything
		/// within AlignDist of a previous seed is certainly caught and anything beyond
		/// 3*AlignDist certainly is not.
		struct SeedKey
		{
			int x{ 0 }, y{ 0 }, z{ 0 };

			bool operator<(const SeedKey& other) const
			{
				if (x != other.x) { return x < other.x; }
				if (y != other.y) { return y < other.y; }
				return z < other.z;
			}
		};

		SeedKey SeedKeyOf(const FVector& p, float cell)
		{
			return SeedKey{
				static_cast<int>(std::floor(p.X / cell)),
				static_cast<int>(std::floor(p.Y / cell)),
				static_cast<int>(std::floor(p.Z / cell)) };
		}

		bool WasAlreadySeeded(const std::set<SeedKey>& seeds, const FVector& p, float cell)
		{
			const SeedKey key = SeedKeyOf(p, cell);
			for (int dz = -1; dz <= 1; dz++)
			{
				for (int dy = -1; dy <= 1; dy++)
				{
					for (int dx = -1; dx <= 1; dx++)
					{
						const SeedKey probe{ key.x + dx, key.y + dy, key.z + dz };
						if (seeds.find(probe) != seeds.end()) { return true; }
					}
				}
			}
			return false;
		}

		/// How far the nearest slot of `link` ended up from the point that seeded it.
		///
		/// A large value means FindCoverEdges walked away from the seed -- around a corner,
		/// or off along an adjoining face -- and left the seeded surface uncovered. That is
		/// both a placement defect and the reason the surface dedupe cannot see it.
		float SeedDrift(ACoverLink* link, const FVector& seed)
		{
			const int slotCount = (link != nullptr) ? static_cast<int>(link->Slots.Count()) : 0;
			if (slotCount == 0) { return 0.0f; }

			float nearest = -1.0f;
			for (int i = 0; i < slotCount; i++)
			{
				const FVector slotLoc = link->GetSlotLocation(i, FALSE);
				const float dx = slotLoc.X - seed.X;
				const float dy = slotLoc.Y - seed.Y;
				const float dz = slotLoc.Z - seed.Z;
				const float d = std::sqrt(dx * dx + dy * dy + dz * dz);
				if (nearest < 0.0f || d < nearest) { nearest = d; }
			}
			return nearest;
		}

		ACoverLink* FindCoveringLink(const std::vector<CoverBounds>& bounds,
			const FVector& surfacePoint, const FVector& surfaceNormal, float alignDist)
		{
			for (const auto& entry : bounds)
			{
				if (!entry.Contains(surfacePoint))
				{
					continue;
				}
				if (LinkCoversSurface(entry.link, surfacePoint, surfaceNormal, alignDist))
				{
					return entry.link;
				}
			}
			return nullptr;
		}

		/// Enumerates the places a player could stand within `radius` of `origin`, by walking
		/// the floor rather than by snapping a grid over the world.
		///
		/// The neighbour test is two traces and both matter. The floor has to be there, AND
		/// the straight line from here to there has to be clear -- without the second the fill
		/// leaks through walls, out of the play space, and seeds cover on the far side of the
		/// level. This is the same "where can you stand" question Phase 3's path-node network
		/// asks, and it wants this answer rather than its current 280u world-aligned grid.
		std::vector<FloorSample> SampleFloor(AActor* tracer, const FVector& origin,
			float radius, float spacing, float rideHeight, float maxRise, float maxDrop,
			int maxSamples, SweepStats& stats)
		{
			std::vector<FloorSample> samples{};

			float originFloorZ = 0.0f;
			if (ProbeFloor(tracer, origin, maxRise, maxDrop, originFloorZ) != FloorProbe::Found)
			{
				return samples;
			}

			std::set<SampleKey> visited{};
			const auto keyOf = [spacing](const FVector& v)
			{
				return SampleKey{
					static_cast<int>(std::round(v.X / spacing)),
					static_cast<int>(std::round(v.Y / spacing)),
					// Two walkable surfaces closer together than a slope step is are the same
					// surface; anything further apart is a different storey.
					static_cast<int>(std::round(v.Z / 72.0f))
				};
			};

			FloorSample start{};
			start.floorZ = originFloorZ;
			start.location = { origin.X, origin.Y, originFloorZ + rideHeight };
			visited.insert(keyOf(start.location));
			samples.push_back(start);

			// Breadth-first, so the budget is spent on the ground nearest the player rather
			// than on one long tendril off into a corridor.
			for (size_t head = 0; head < samples.size(); head++)
			{
				if (static_cast<int>(samples.size()) >= maxSamples)
				{
					stats.sampleBudgetHit = 1;
					break;
				}

				const FloorSample current = samples[head];

				for (int dy = -1; dy <= 1; dy++)
				{
					for (int dx = -1; dx <= 1; dx++)
					{
						if (dx == 0 && dy == 0) { continue; }
						if (static_cast<int>(samples.size()) >= maxSamples) { break; }

						const FVector flat{
							current.location.X + static_cast<float>(dx) * spacing,
							current.location.Y + static_cast<float>(dy) * spacing,
							current.location.Z
						};

						const float rx = flat.X - origin.X;
						const float ry = flat.Y - origin.Y;
						if (rx * rx + ry * ry > radius * radius) { continue; }

						// Probe from maxRise above the floor we are standing on, not from the
						// walk line -- see ProbeFloor. A surface higher than that comes back
						// Blocked, which is correct: it is a wall, not a step.
						float nextFloorZ = 0.0f;
						const FloorProbe probe = ProbeFloor(tracer, flat,
							(current.floorZ + maxRise) - flat.Z, maxDrop + rideHeight,
							nextFloorZ);
						if (probe != FloorProbe::Found)
						{
							stats.neighboursNoFloor++;
							continue;
						}

						const FVector next{ flat.X, flat.Y, nextFloorZ + rideHeight };
						const SampleKey key = keyOf(next);
						if (visited.find(key) != visited.end()) { continue; }

						FVector hitLocation{}, hitNormal{};
						auto* const blocker = tracer->Trace(next, current.location,
							FALSE /*bTraceActors*/, FVector{ 1.0f, 1.0f, 1.0f },
							0, &hitLocation, &hitNormal, nullptr);
						if (blocker != nullptr)
						{
							stats.neighboursBlocked++;
							continue;
						}

						visited.insert(key);
						FloorSample sample{};
						sample.floorZ = nextFloorZ;
						sample.location = next;
						samples.push_back(sample);
					}
				}
			}

			stats.samples = static_cast<int>(samples.size());
			return samples;
		}
	}

	void CmdSweep(float radius, int maxSamples, int maxLinks)
	{
		LEASI_INFO("=== cc.sweep ===");

		auto* const wi = GetWorldInfo();
		if (wi == nullptr)
		{
			LEASI_ERROR("No WorldInfo.");
			return;
		}
		auto* const pc = wi->GetALocalPlayerController();
		if (pc == nullptr || pc->Pawn == nullptr)
		{
			LEASI_ERROR("Need a player controller with a pawn.");
			return;
		}
		auto* const defaults = GetCoverLinkDefaults();
		if (defaults == nullptr)
		{
			LEASI_ERROR("Could not reach the ACoverLink CDO.");
			return;
		}

		if (radius <= 0.0f) { radius = k_sweepDefaultRadius; }
		if (radius < k_sweepMinRadius) { radius = k_sweepMinRadius; }

		const float alignDist = defaults->AlignDist;
		const float spacing = defaults->AutoCoverSlotInterval;
		const float midHeight = defaults->MidHeight;
		const float standHeight = defaults->StandHeight;

		// The fan reaches two slot intervals. Further than that and the ray is mostly finding
		// walls that some nearer sample will find with a cleaner line to them.
		const float fanReach = spacing * 2.0f;

		// A player can step up no further than they are tall, and can drop further than they
		// can climb. These bound the flood fill, not the cover walk -- the walk has its own,
		// tighter 45-degree slope limit in CoverEdges.
		const float maxRise = standHeight;
		const float maxDrop = standHeight * 2.0f;

		auto* const tracer = pc->Pawn;
		const FVector playerLoc = pc->Pawn->location;

		// The walk line rides a CoverLink cylinder above the floor; matching it here keeps the
		// sample points and the placement walk at the same height.
		const float rideHeight = alignDist;

		// One sample per grid cell inside the radius is the natural bound, so derive it rather
		// than keeping a fixed number that is wasteful at 512u and a silent truncation at
		// 16384u. Both budgets can still be overridden, because "how far" and "how much work"
		// are not always the same question -- a reconnaissance pass over a whole level wants a
		// huge radius and a small link cap.
		if (maxSamples <= 0)
		{
			const float cellsPerSide = radius / spacing;
			const float cells = 3.14159265f * cellsPerSide * cellsPerSide;
			maxSamples = static_cast<int>(cells) + 1;
			if (maxSamples < k_sweepMinSamples) { maxSamples = k_sweepMinSamples; }
			if (maxSamples > k_sweepSampleCeiling) { maxSamples = k_sweepSampleCeiling; }
		}
		// Links are limited by the dedupe far more than by any cap -- a wall is one link
		// however many rays find it -- so the default ceiling is simply "one per sample",
		// which in practice never binds.
		if (maxLinks <= 0) { maxLinks = maxSamples; }

		LEASI_INFO("Radius {:.0f}u, sample spacing {:.0f}u, {} rays per fan out to {:.0f}u, "
			"fan taken at floor+{:.0f} (MidHeight -- anything shorter is not cover).",
			radius, spacing, k_sweepRaysPerFan, fanReach, midHeight);
		LEASI_INFO("Budgets: {} sample(s), {} link(s).", maxSamples, maxLinks);

		if (radius > k_sweepLongRadius)
		{
			LEASI_WARN("This is a big sweep. All of it runs inside one Exec call, so the game "
				"will be frozen until it finishes -- possibly for minutes. It is not hung: "
				"the log gets a progress line every {} samples.", k_sweepProgressEvery);
		}

		ScopedScout guard{};
		if (!guard)
		{
			LEASI_ERROR("Could not acquire a scout; aborting before the sweep.");
			return;
		}

		SweepStats stats{};
		const auto samples = SampleFloor(tracer, playerLoc, radius, spacing, rideHeight,
			maxRise, maxDrop, maxSamples, stats);
		if (samples.empty())
		{
			LEASI_ERROR("No floor under the player, so there is nothing to sample from.");
			return;
		}

		LEASI_INFO("Floor sampling: {} standable point(s){}. Rejected {} neighbour(s) with no "
			"floor and {} with no clear line to them.",
			stats.samples,
			stats.sampleBudgetHit != 0 ? " (SAMPLE BUDGET HIT -- sweep is truncated)" : "",
			stats.neighboursNoFloor, stats.neighboursBlocked);

		// Snapshot what already covers something, once, instead of re-deriving it per ray.
		// Shipped cover never moves, and our own links only ever get added to below.
		std::vector<CoverBounds> shippedBounds{};
		for (auto* node = wi->CoverList; node != nullptr; node = node->NextCoverLink)
		{
			shippedBounds.push_back(BoundsOf(node, alignDist));
			if (static_cast<int>(shippedBounds.size()) >= k_maxChainWalk) { break; }
		}
		std::vector<CoverBounds> ourBounds{};
		for (auto* const link : GetSpawnedLinks())
		{
			ourBounds.push_back(BoundsOf(link, alignDist));
		}
		std::set<SeedKey> seeded{};
		bool bDumpedClimber = false;
		bool bDumpedDrifter = false;
		LEASI_INFO("Dedupe: {} link(s) the level shipped with, {} of ours already placed.",
			static_cast<int>(shippedBounds.size()) - static_cast<int>(ourBounds.size()),
			static_cast<int>(ourBounds.size()));

		LEASI_INFO("--- seeding ---");
		bool bLinkBudgetHit = false;
		int sampleIndex = 0;

		for (const auto& sample : samples)
		{
			if (bLinkBudgetHit) { break; }

			if ((++sampleIndex % k_sweepProgressEvery) == 0)
			{
				LEASI_INFO("  ... {} / {} samples, {} link(s) so far.",
					sampleIndex, stats.samples, stats.linksSpawned);
			}

			// Fire the fan AT MidHeight. That is the height test as well as the search:
			// anything shorter than mid-level cover is not cover, so kerbs and steps are
			// rejected without needing a second trace to measure them.
			const FVector fanOrigin{
				sample.location.X, sample.location.Y, sample.floorZ + midHeight };

			for (int ray = 0; ray < k_sweepRaysPerFan; ray++)
			{
				if (bLinkBudgetHit) { break; }

				const float angle = (2.0f * 3.14159265f * static_cast<float>(ray))
					/ static_cast<float>(k_sweepRaysPerFan);
				const FVector end{
					fanOrigin.X + std::cos(angle) * fanReach,
					fanOrigin.Y + std::sin(angle) * fanReach,
					fanOrigin.Z
				};

				stats.raysFired++;
				FVector hitLocation{}, hitNormal{};
				auto* const hit = tracer->Trace(end, fanOrigin, FALSE /*bTraceActors*/,
					FVector{ 1.0f, 1.0f, 1.0f }, 0, &hitLocation, &hitNormal, nullptr);
				if (hit == nullptr)
				{
					stats.rayMissed++;
					continue;
				}

				// EditorAutoSetup's own rule: you cannot take cover on a floor or a ceiling.
				if (std::abs(hitNormal.Z) > 0.3f)
				{
					stats.rejectedHorizontal++;
					continue;
				}

				// ! The cover has to belong to the floor it is standing on.
				//
				// The fan is taken at the height of the floor the SAMPLE is on and reaches
				// 512u. Stand on a crate and the ray crosses the room and hits the far wall,
				// which is cover -- but cover for someone on the crate's floor, not on the
				// wall's. Seeding it anyway puts a link 300u up a flat wall with nothing under
				// it, and the left walk immediately steps off into space and produces a
				// one-slot link.
				//
				// That is precisely what the sandbox showed: seeds land on the two real floors
				// (784 and 1084) or in thin bands 100u above them, and the bands are almost
				// entirely junk -- 16 of 17 links seeded at Z 1184 came out with a single
				// slot, and 20 of 23 at Z 884, against 1 of 113 at Z 784.
				//
				// Rejecting loses nothing: the wall's own floor gets sampled in its own right,
				// and seeds the same wall properly from down there.
				float floorAtWall = 0.0f;
				const FloorProbe wallFloor = ProbeFloor(tracer,
					FVector{ hitLocation.X + hitNormal.X * alignDist,
						hitLocation.Y + hitNormal.Y * alignDist,
						hitLocation.Z },
					8.0f, 1024.0f, floorAtWall);
				if (wallFloor != FloorProbe::Found)
				{
					// Nothing standable under it within 1024u. Not cover, whatever it is.
					stats.rejectedNoFloorAtWall++;
					continue;
				}
				if (std::abs(floorAtWall - sample.floorZ) > midHeight)
				{
					stats.rejectedWrongStorey++;
					continue;
				}

				// Cheapest and strictest test first: have we already tried this exact spot?
				if (WasAlreadySeeded(seeded, hitLocation, alignDist))
				{
					stats.rejectedSeeded++;
					continue;
				}
				if (FindCoveringLink(ourBounds, hitLocation, hitNormal, alignDist) != nullptr)
				{
					stats.rejectedOurs++;
					continue;
				}
				if (FindCoveringLink(shippedBounds, hitLocation, hitNormal, alignDist) != nullptr)
				{
					stats.rejectedShipped++;
					continue;
				}

				if (stats.linksSpawned >= maxLinks)
				{
					bLinkBudgetHit = true;
					break;
				}

				// Recorded before the spawn, not after: a seed that fails to produce a link
				// is still a seed we have tried, and re-trying it would fail identically.
				seeded.insert(SeedKeyOf(hitLocation, alignDist));

				FindCoverEdgesResult edges{};
				auto* const link = SpawnLinkOnSurface(wi, defaults, tracer,
					hitLocation, hitNormal, edges);
				if (link == nullptr)
				{
					stats.spawnFailed++;
					continue;
				}

				stats.linksSpawned++;
				ourBounds.push_back(BoundsOf(link, alignDist));

				// See SeedDrift. One slot interval of slack: the walk legitimately lays its
				// nearest slot up to about that far from where it started.
				const float drift = SeedDrift(link, hitLocation);
				if (drift > spacing)
				{
					stats.linksAdrift++;

					// Dump the first drifter in full, the same way the first climber is
					// dumped. Drift is now the largest unexplained defect -- 65 of 291 links
					// in the 10:29 sweep -- and it is the mechanism behind the duplicate
					// cover, because seven seeds spread along a wall that all walk to the
					// same place produce seven identical links that no dedupe can see: the
					// seed memory says the seeds were far apart, and LinkCoversSurface asks
					// only whether a link covers its OWN seed.
					//
					// Full XYZ rather than the climber dump's Z alone: this failure is
					// horizontal, so the Z column is exactly the one that will not show it.
					if (!bDumpedDrifter)
					{
						bDumpedDrifter = true;
						LEASI_WARN("  drift detail for {} (nearest slot {:.0f}u from its seed "
							"at ({:.0f}, {:.0f}, {:.0f}), {} slot(s), {:.0f}u span):",
							PathOf(link), drift, hitLocation.X, hitLocation.Y, hitLocation.Z,
							link->Slots.Count(), edges.span);
						for (const auto& placement : edges.placements)
						{
							LEASI_WARN("    slot {} {}: placed ({:.0f}, {:.0f}, {:.0f}) -> "
								"seated ({:.0f}, {:.0f}, {:.0f}), yaw {:+} -> {:+}",
								placement.slotIdx, SlotOriginName(placement.origin),
								placement.placedLocation.X, placement.placedLocation.Y,
								placement.placedLocation.Z,
								placement.seatedLocation.X, placement.seatedLocation.Y,
								placement.seatedLocation.Z,
								placement.placedYaw, placement.seatedYaw);
						}
					}
				}
				const int slotCount = static_cast<int>(link->Slots.Count());
				stats.slotsTotal += slotCount;
				if (slotCount == 0) { stats.noSlots++; }
				if (!edges.bFoundLeftEdge) { stats.noLeftEdge++; }

				const int dead = edges.slotsDisabled - edges.slotsRecovered;
				stats.slotsDead += dead;
				if (edges.walkStalls > 0) { stats.linksStalled++; }

				// Real ramps exist and are followed deliberately, so this is a threshold, not
				// a zero test: one slot interval of climb along a single link is more than
				// any sane ramp in a cover run.
				for (const auto& placement : edges.placements)
				{
					const auto index = static_cast<size_t>(placement.origin);
					if (index >= std::size(stats.allOrigins)) { continue; }
					stats.allOrigins[index]++;
					const bool bAlive =
						placement.slotIdx < static_cast<int>(link->Slots.Count())
						&& link->Slots(placement.slotIdx).bEnabled != 0;
					if (!bAlive) { stats.disabledOrigins[index]++; }
				}

				stats.slotsReseated += edges.slotsReseatedOffSurface;
				stats.seedFloorMissing += edges.seedFloorMissing;
				if (edges.worstSeatLift > stats.worstSeatLift)
				{
					stats.worstSeatLift = edges.worstSeatLift;
				}

				// A link is "vertical" when its slots rise further than they spread. Cover is
				// a horizontal structure by definition, so this is never a legitimate shape,
				// whatever the geometry -- unlike floor rise, which a real ramp produces.
				if (edges.verticalExtent > edges.horizontalExtent
					&& edges.verticalExtent > alignDist)
				{
					stats.linksVertical++;
					if (edges.verticalExtent > stats.worstVertical)
					{
						stats.worstVertical = edges.verticalExtent;
					}
					if (stats.firstVerticalIndex < 0)
					{
						stats.firstVerticalIndex =
							static_cast<int>(GetSpawnedLinks().size()) - 1;
					}
				}

				stats.leftStoppedAtCorner += edges.leftStoppedAtCorner;
				stats.rightCornersRecovered += edges.rightCornersRecovered;

				const float rightClimb = std::abs(edges.floorRise);
				const float leftClimb = std::abs(edges.leftFloorRise);
				const float climb = (leftClimb > rightClimb) ? leftClimb : rightClimb;
				if (climb > spacing)
				{
					stats.linksClimbed++;
					if (climb > stats.worstClimb) { stats.worstClimb = climb; }

					// Dump the first offender slot by slot. Which BRANCH first lifts the Z is
					// the only question left, and it cannot be read off the finished link --
					// two fixes have now been aimed at this by reasoning from a screenshot and
					// neither hit it. One link of output per sweep buys the answer outright.
					if (!bDumpedClimber)
					{
						bDumpedClimber = true;
						LEASI_WARN("  climb detail for {} (left walk {:+.0f}u, right walk "
							"{:+.0f}u, seeded at Z {:.0f}):",
							PathOf(link), edges.leftFloorRise, edges.floorRise,
							hitLocation.Z);
						for (const auto& placement : edges.placements)
						{
							LEASI_WARN("    slot {} {}: placed Z {:.0f} -> seated Z {:.0f} "
								"({}{})",
								placement.slotIdx, SlotOriginName(placement.origin),
								placement.placedLocation.Z, placement.seatedLocation.Z,
								placement.bReseatedOffSurface ? "off-surface, " : "",
								(placement.slotIdx < static_cast<int>(link->Slots.Count())
									&& link->Slots(placement.slotIdx).bEnabled != 0)
									? "enabled" : "disabled");
						}
					}
				}

				// Degenerate: every slot within one AlignDist of the first. Measured from the
				// placements rather than from `span`, because span is first-to-last and a
				// three-slot link can return to where it started.
				if (slotCount > 1 && edges.span < alignDist)
				{
					stats.linksDegenerate++;
					for (const auto& placement : edges.placements)
					{
						const auto index = static_cast<size_t>(placement.origin);
						if (index < std::size(stats.degenerateOrigins))
						{
							stats.degenerateOrigins[index]++;
						}
					}
				}

				LEASI_INFO("  {} @ ({:.0f}, {:.0f}, {:.0f}): {} slot(s), {:.0f}u span{}{}{}",
					PathOf(link), hitLocation.X, hitLocation.Y, hitLocation.Z,
					slotCount, edges.span,
					edges.bFoundLeftEdge ? "" : ", LEFT EDGE NOT FOUND",
					dead > 0 ? ", slots left dead" : "",
					edges.walkStalls > 0 ? ", WALK STALLED" : "");
			}
		}

		LEASI_INFO("--- sweep result ---");
		LEASI_INFO("{} link(s), {} slot(s) from {} sample(s) and {} ray(s).",
			stats.linksSpawned, stats.slotsTotal, stats.samples, stats.raysFired);
		LEASI_INFO("Rays: {} hit nothing, {} hit a floor or ceiling, {} had no floor under the "
			"cover, {} were cover for a different storey, {} landed where we had already "
			"seeded, {} on cover we had already made, {} on cover the level shipped with.",
			stats.rayMissed, stats.rejectedHorizontal, stats.rejectedNoFloorAtWall,
			stats.rejectedWrongStorey, stats.rejectedSeeded,
			stats.rejectedOurs, stats.rejectedShipped);

		if (stats.linksAdrift > 0)
		{
			LEASI_WARN("{} of {} link(s) put their nearest slot more than one slot interval "
				"from the point that seeded them. The walk left the seeded surface -- around "
				"a corner or off along an adjoining face -- so that surface is still "
				"uncovered and only the seed memory stops it being seeded again.",
				stats.linksAdrift, stats.linksSpawned);
		}

		{
			std::string tally{};
			for (size_t i = 0; i < std::size(stats.allOrigins); i++)
			{
				if (stats.allOrigins[i] == 0) { continue; }
				if (!tally.empty()) { tally += ", "; }
				tally += SlotOriginName(static_cast<SlotOrigin>(i));
				tally += " ";
				tally += std::to_string(stats.allOrigins[i]);
				if (stats.disabledOrigins[i] > 0)
				{
					tally += " (";
					tally += std::to_string(stats.disabledOrigins[i]);
					tally += " dead)";
				}
			}
			LEASI_INFO("Slots by branch: {}.", tally);
		}
		if (stats.seedFloorMissing > 0)
		{
			LEASI_WARN("{} of {} link(s) could not find a floor under their own starting "
				"point. The scan start is only 32u out from the wall, so a downward box probe "
				"grazing the wall face is the likely cause; every height decision the walk "
				"makes is degraded until it finds one.",
				stats.seedFloorMissing, stats.linksSpawned);
		}
		if (stats.slotsReseated > 0)
		{
			LEASI_WARN("{} slot(s) were seated off the surface the walk measured -- the worst "
				"by {:.0f}u vertically -- and have been disabled. AutoAdjustSlot's FindSpot "
				"tail can drop a slot at the Z of whatever its last trace touched, which "
				"against a tall wall leaves cover floating in mid-air marked enabled and "
				"CT_Standing.", stats.slotsReseated, stats.worstSeatLift);
		}
		if (stats.linksClimbed > 0)
		{
			LEASI_WARN("{} of {} link(s) climbed or dropped more than one slot interval along "
				"EITHER walk, the worst by {:.0f}u. A genuine ramp does this; a flat wall does "
				"not. The left walk matters as much as the right: it decides where slot 0 "
				"lands, so a link can sit entirely at the wrong height with a right-hand rise "
				"of zero.", stats.linksClimbed, stats.linksSpawned, stats.worstClimb);
		}
		if (stats.leftStoppedAtCorner > 0)
		{
			LEASI_INFO("{} of {} link(s) had their left walk stop at an inside corner rather "
				"than turn onto the adjoining face. That is the intended behaviour -- the left "
				"walk only decides where the run STARTS -- but if it is firing on nearly every "
				"link, k_sameSurfaceCos is too tight.",
				stats.leftStoppedAtCorner, stats.linksSpawned);
		}
		if (stats.rightCornersRecovered > 0)
		{
			LEASI_INFO("{} inside corner(s) were turned by a late sideways probe rather than "
				"by the ordinary one at the top of the loop -- either the wall in front ran "
				"out a step early, or (far more often) the step toward the corner found no "
				"walkable floor and would have been called a drop. Those were all end caps "
				"before.", stats.rightCornersRecovered);
		}
		if (stats.linksVertical > 0)
		{
			LEASI_WARN("{} of {} link(s) have their slots stacked more vertically than they "
				"are spread horizontally -- cover running UP a wall rather than along it, the "
				"worst over {:.0f}u. This is never a legitimate shape, unlike floor rise, "
				"which a real ramp produces. Run 'cc.explainlink {}' for the first one.",
				stats.linksVertical, stats.linksSpawned, stats.worstVertical,
				stats.firstVerticalIndex);
		}
		if (stats.linksDegenerate > 0)
		{
			std::string origins{};
			for (size_t i = 0; i < std::size(stats.degenerateOrigins); i++)
			{
				if (stats.degenerateOrigins[i] == 0) { continue; }
				if (!origins.empty()) { origins += ", "; }
				origins += SlotOriginName(static_cast<SlotOrigin>(i));
				origins += " ";
				origins += std::to_string(stats.degenerateOrigins[i]);
			}
			LEASI_WARN("{} of {} link(s) are degenerate -- every slot within one AlignDist of "
				"the first, so the link is really a single point. Their slots came from: {}.",
				stats.linksDegenerate, stats.linksSpawned, origins);
		}
		if (stats.linksStalled > 0)
		{
			LEASI_WARN("{} of {} link(s) had the right-hand walk stall -- an iteration that "
				"ended no further along the wall than it started, so it was forced forward or "
				"stopped. Before the guard existed these ran to the 150-iteration cap and "
				"produced links of 111-297 slots packed into a couple of hundred units.",
				stats.linksStalled, stats.linksSpawned);
		}

		// A candidate that survived every filter and still produced nothing is the interesting
		// failure, because it means the sweep and the placement walk disagree about what is
		// cover -- the sweep found a face the walk then would not follow.
		if (stats.spawnFailed > 0 || stats.noSlots > 0 || stats.noLeftEdge > 0)
		{
			LEASI_WARN("{} spawn(s) failed, {} link(s) came out with no slots, {} never found a "
				"left edge. Those seeds passed the fan's cover test but the placement walk "
				"would not follow the face -- worth looking at where they are.",
				stats.spawnFailed, stats.noSlots, stats.noLeftEdge);
		}
		if (stats.slotsDead > 0)
		{
			LEASI_WARN("{} slot(s) across the sweep stayed disabled after the retry, so they "
				"are placed against no geometry and would serialise as dead cover.",
				stats.slotsDead);
		}
		if (bLinkBudgetHit)
		{
			LEASI_WARN("Stopped at the {}-link budget. Pass a bigger one as the third "
				"argument (cc.sweep <radius> <maxSamples> <maxLinks>) -- but every link costs "
				"a full per-slot AutoAdjustSlot pass in this one frame.", maxLinks);
		}
		if (stats.sampleBudgetHit != 0)
		{
			LEASI_WARN("Stopped at the {}-sample budget, so the far side of the radius was "
				"never looked at. Raise it with cc.sweep <radius> <maxSamples>, or sweep from "
				"more places with a smaller radius.", maxSamples);
		}

		if (stats.linksSpawned == 0)
		{
			LEASI_INFO("Nothing new to cover here. cc.despawnlinks and re-run elsewhere.");
			return;
		}

		// Markers, ReachSpecs and the navigation octree registration -- without these the
		// links draw but cannot be entered. Running it here is the difference between a sweep
		// and a network.
		LEASI_INFO("--- running cc.buildlinks over everything the sweep made ---");
		CmdBuildLinks();
	}

	// ! cc.enemyspawnpoint -- put a real spawn point on generated cover
	// ========================================
	//
	// The point of this is to get an ENEMY standing on cover we made, so the AI's own cover
	// selection is the thing under test rather than the player's. Everything up to now has
	// been verified by walking into a slot; that only exercises the player path.

	namespace
	{
		/// Spawn points we made, so cc.despawnenemyspawnpoints can take them away again.
		std::vector<ASFXEnemySpawnPoint*> g_SpawnedEnemySpawnPoints;

		/// Every enabled slot of every link we have spawned, nearest to `from` first.
		std::vector<std::pair<float, FVector>> EnabledSlotsByDistance(const FVector& from)
		{
			std::vector<std::pair<float, FVector>> out;
			for (auto* const link : GetSpawnedLinks())
			{
				if (link == nullptr || ::Common::IsDestroyed(link)) { continue; }
				const int slotCount = static_cast<int>(link->Slots.Count());
				for (int i = 0; i < slotCount; i++)
				{
					// A disabled slot is not cover -- AutoAdjustSlot found no wall for it --
					// so spawning an enemy there would test nothing.
					if (link->Slots(i).bEnabled == 0) { continue; }

					const FVector loc = link->GetSlotLocation(i, FALSE);
					const float dx = loc.X - from.X;
					const float dy = loc.Y - from.Y;
					const float dz = loc.Z - from.Z;
					out.emplace_back(std::sqrt(dx * dx + dy * dy + dz * dz), loc);
				}
			}
			std::sort(out.begin(), out.end(),
				[](const std::pair<float, FVector>& a, const std::pair<float, FVector>& b)
				{ return a.first < b.first; });
			return out;
		}
	}

	void CmdSpawnEnemySpawnPoints(int count)
	{
		LEASI_INFO("=== cc.enemyspawnpoint ===");

		auto* const wi = GetWorldInfo();
		if (wi == nullptr)
		{
			LEASI_ERROR("No WorldInfo.");
			return;
		}
		auto* const pc = wi->GetALocalPlayerController();
		if (pc == nullptr || pc->Pawn == nullptr)
		{
			LEASI_ERROR("Need a player controller with a pawn.");
			return;
		}

		auto* const cls = ASFXEnemySpawnPoint::StaticClass();
		if (cls == nullptr)
		{
			LEASI_ERROR("Could not find class SFXGame.SFXEnemySpawnPoint.");
			return;
		}
		auto* const defaults = static_cast<ASFXEnemySpawnPoint*>(cls->ClassDefaultObject);
		if (defaults == nullptr)
		{
			LEASI_ERROR("SFXEnemySpawnPoint has no ClassDefaultObject.");
			return;
		}

		const auto slots = EnabledSlotsByDistance(pc->Pawn->location);
		if (slots.empty())
		{
			LEASI_ERROR("No enabled slots on any link we spawned. Run cc.spawnlink or "
				"cc.sweep first.");
			return;
		}

		if (count < 1) { count = 1; }
		const int wanted = (count < static_cast<int>(slots.size()))
			? count : static_cast<int>(slots.size());

		// SFXEnemySpawnPoint derives from ANavigationPoint, so it carries the same
		// bStatic/bNoDelete pair that made every CoverLink spawn return null: SpawnActor
		// refuses the class outright (UnLevAct.cpp:406) and Destroy refuses it too. Clear the
		// flags on the CDO across the Spawn calls -- the instance copies its property values
		// from CDO memory at construction -- then put the CDO back exactly as it was.
		const unsigned long savedStatic = defaults->bStatic;
		const unsigned long savedNoDelete = defaults->bNoDelete;
		defaults->bStatic = 0;
		defaults->bNoDelete = 0;

		int spawned = 0, failed = 0, registered = 0, noCylinder = 0, stillOut = 0;
		for (int i = 0; i < wanted; i++)
		{
			const FVector& at = slots[static_cast<size_t>(i)].second;

			// Facing is deliberately the pawn's, not the slot's. A slot faces INTO its wall;
			// an enemy spawned looking at the wall has its back to the fight.
			auto* const actor = wi->Spawn(cls, nullptr, SFXName{}, at,
				pc->Pawn->Rotation, nullptr, TRUE /*bNoCollisionFail*/, FALSE);
			if (actor == nullptr)
			{
				failed++;
				continue;
			}

			auto* const point = static_cast<ASFXEnemySpawnPoint*>(actor);
			point->bStatic = 0;
			point->bNoDelete = 0;
			g_SpawnedEnemySpawnPoints.push_back(point);
			spawned++;

			// Same trap the cover markers hit: PostBeginPlay threads a spawned
			// ANavigationPoint onto NavigationPointList, but nothing puts it in the navigation
			// octree, and every radius query reads the octree. Unregistered, it exists and is
			// invisible.
			if (point->CylinderComponent == nullptr)
			{
				noCylinder++;
			}
			else
			{
				CallAddToNavigationOctree(point);
				if (point->NavOctreeObject.OctreeNode != nullptr) { registered++; }
				else { stillOut++; }
			}

			LEASI_INFO("  {} at ({:.0f}, {:.0f}, {:.0f}), {:.0f}u from the player.",
				PathOf(point), at.X, at.Y, at.Z, slots[static_cast<size_t>(i)].first);
		}

		defaults->bStatic = savedStatic;
		defaults->bNoDelete = savedNoDelete;

		LEASI_INFO("Spawned {} of {} requested spawn point(s) on the nearest cover slots "
			"({} enabled slot(s) available). {} registered in the navigation octree.",
			spawned, wanted, static_cast<int>(slots.size()), registered);
		if (failed > 0)
		{
			LEASI_ERROR("{} Spawn call(s) returned null even with bStatic and bNoDelete "
				"cleared on the CDO.", failed);
		}
		if (noCylinder > 0)
		{
			LEASI_WARN("{} spawn point(s) have no CylinderComponent, so AddToNavigationOctree "
				"early-outs and they stay out of every radius query.", noCylinder);
		}
		if (stillOut > 0)
		{
			LEASI_WARN("{} spawn point(s) still have a null OctreeNode after the add.",
				stillOut);
		}
		LEASI_INFO("These are only the POINTS. Something still has to spawn enemies at them -- "
			"a kismet SpawnEnemy action or whatever your test harness drives.");
	}

	void CmdDespawnEnemySpawnPoints()
	{
		LEASI_INFO("=== cc.despawnenemyspawnpoints ===");
		if (g_SpawnedEnemySpawnPoints.empty())
		{
			LEASI_INFO("Nothing to remove.");
			return;
		}

		int destroyed = 0;
		for (auto* const point : g_SpawnedEnemySpawnPoints)
		{
			if (point == nullptr || ::Common::IsDestroyed(point)) { continue; }
			// Unlike a CoverLink there is no CoverList to unsplice from; NavigationPointList
			// is maintained by the engine's own Destroy path.
			if (point->NavOctreeObject.OctreeNode != nullptr)
			{
				CallRemoveFromNavigationOctree(point);
			}
			point->Destroy();
			destroyed++;
		}
		LEASI_INFO("Destroyed {} of {} spawn point(s).",
			destroyed, g_SpawnedEnemySpawnPoints.size());
		g_SpawnedEnemySpawnPoints.clear();
	}

	void CmdDespawnLinks()
	{
		LEASI_INFO("=== cc.despawnlinks ===");
		if (g_SpawnedLinks.empty())
		{
			LEASI_INFO("Nothing to remove.");
			return;
		}

		auto* const wi = GetWorldInfo();
		int destroyed = 0, markersDestroyed = 0, unspliced = 0;
		for (auto* link : g_SpawnedLinks)
		{
			if (link == nullptr || ::Common::IsDestroyed(link))
			{
				continue;
			}
			// cc.buildlinks spawns a CoverSlotMarker per slot. They are separate actors that
			// the link merely owns, so destroying the link does not take them with it.
			markersDestroyed += DestroySlotMarkers(link);
			// Must come before Destroy: nothing in the engine takes a link off CoverList, so a
			// destroyed link left spliced leaves the next walker reading freed memory.
			if (UnspliceFromCoverList(wi, link)) { unspliced++; }
			link->Destroy();
			destroyed++;
		}

		LEASI_INFO("Destroyed {} of {} spawned link(s), plus {} marker(s). {} removed from "
			"WorldInfo.CoverList.", destroyed, g_SpawnedLinks.size(), markersDestroyed,
			unspliced);
		g_SpawnedLinks.clear();
		g_LinkEdges.clear();
	}

	void CmdExplainLink(int requestedIndex, int requestedObjectNumber)
	{
		LEASI_INFO("=== cc.explainlink ===");

		// A C number wins: it is the one the game itself prints, so it is the one that can be
		// read off the screen without translating anything.
		if (requestedObjectNumber >= 0)
		{
			for (size_t i = 0; i < GetSpawnedLinks().size(); i++)
			{
				auto* const candidate = GetSpawnedLinks()[i];
				if (candidate != nullptr && !::Common::IsDestroyed(candidate)
					&& LinkObjectNumber(candidate) == requestedObjectNumber)
				{
					ExplainOneLink(candidate, static_cast<int>(i));
					return;
				}
			}
			LEASI_ERROR("No link named CoverLink_{} among the {} this session spawned.",
				requestedObjectNumber, static_cast<int>(GetSpawnedLinks().size()));
			return;
		}

		auto* const wi = GetWorldInfo();
		auto* const pc = (wi != nullptr) ? wi->GetALocalPlayerController() : nullptr;
		if (pc == nullptr || pc->Pawn == nullptr)
		{
			LEASI_ERROR("Need a player controller with a pawn.");
			return;
		}

		// An explicit index wins over any guess. The in-world labels read "L37 s1 ...", so a
		// link that is hard to aim at -- one slot among several at a corner, or one behind the
		// player -- can be named outright instead of hunted for with the crosshair.
		if (requestedIndex >= 0)
		{
			if (requestedIndex >= static_cast<int>(GetSpawnedLinks().size()))
			{
				LEASI_ERROR("No link L{}. The sweep made {}.",
					requestedIndex, static_cast<int>(GetSpawnedLinks().size()));
				return;
			}
			ExplainOneLink(GetSpawnedLinks()[static_cast<size_t>(requestedIndex)],
				requestedIndex);
			return;
		}

		// Pick what the player is LOOKING AT, not what is closest.
		//
		// Nearest-to-the-pawn picked a healthy link 175u away while the developer was aiming
		// at a broken one further off -- which is the normal case, because you stand next to
		// good cover and look at bad cover. An angular pick is also the only thing that can
		// select a slot floating in mid-air, and those are exactly the ones worth asking
		// about.
		// ! The ray has to start at the CAMERA, not at the pawn.
		//
		// Using the pawn's location as the eye systematically selects distant links. LE3's
		// camera is over the shoulder, so it sits roughly a metre behind and to the side of
		// the pawn -- and that fixed offset is a huge angular error for a near target and a
		// negligible one for a far target. Aiming at cover a few feet away, this picked links
		// 1321u and 2204u off at "4.0" and "2.9" degrees, while the cover actually under the
		// crosshair measured further off axis than either.
		//
		// GetPlayerViewPoint returns what the player is genuinely looking along, camera offset
		// and all, so near and far targets are finally judged on the same footing.
		FVector eye{};
		FRotator viewRotation{};
		pc->eventGetPlayerViewPoint(&eye, &viewRotation);
		if (eye.X == 0.0f && eye.Y == 0.0f && eye.Z == 0.0f)
		{
			// Should not happen, but a zeroed viewpoint would aim the pick at the world origin
			// and silently return whatever is nearest to it, which looks like a working
			// answer. Fall back to the old approximation and say so.
			eye = FVector{ pc->Pawn->location.X, pc->Pawn->location.Y,
				pc->Pawn->location.Z + 80.0f };
			viewRotation = pc->Rotation;
			LEASI_WARN("GetPlayerViewPoint returned nothing; aiming from the pawn instead, "
				"which biases the pick toward distant links.");
		}

		FVector aimX{}, aimY{}, aimZ{};
		RotatorToAxes(viewRotation, aimX, aimY, aimZ);

		ACoverLink* nearest = nullptr;
		int nearestSlot = -1;
		float nearestDist = -1.0f;
		float bestOffset = -1.0f;
		for (auto* const link : GetSpawnedLinks())
		{
			if (link == nullptr || ::Common::IsDestroyed(link)) { continue; }
			const int slotCount = static_cast<int>(link->Slots.Count());
			for (int i = 0; i < slotCount; i++)
			{
				const FVector at = link->GetSlotLocation(i, FALSE);
				const FVector toSlot = {
					at.X - eye.X, at.Y - eye.Y, at.Z - eye.Z };
				const float along = ::Dot(toSlot, aimX);

				// Behind the camera, or so far off it is not what anyone is pointing at.
				if (along <= 1.0f || along > 8192.0f) { continue; }

				const FVector perp = {
					toSlot.X - aimX.X * along,
					toSlot.Y - aimX.Y * along,
					toSlot.Z - aimX.Z * along };
				// Angular, not absolute: a slot 2000u away is "pointed at" with a much larger
				// perpendicular offset than one at arm's length.
				const float offset =
					std::sqrt(perp.X * perp.X + perp.Y * perp.Y + perp.Z * perp.Z) / along;

				// Inside the cone, NEAREST wins; outside it, straightest wins.
				//
				// Angle alone picked a slot 2378u away at 2.7 degrees over the corner the
				// developer was standing next to, because a distant slot can be closer to the
				// axis than a near one while being nowhere near what is under the crosshair.
				// This is what a crosshair actually does: everything inside a small cone is
				// "pointed at", and of those you mean the first one.
				const bool bInCone = offset <= k_aimCone;
				const bool bBestInCone = bestOffset >= 0.0f && bestOffset <= k_aimCone;
				const float dist = std::sqrt(toSlot.X * toSlot.X + toSlot.Y * toSlot.Y
					+ toSlot.Z * toSlot.Z);

				bool bBetter = false;
				if (nearest == nullptr)              { bBetter = true; }
				else if (bInCone && !bBestInCone)    { bBetter = true; }
				else if (bInCone && bBestInCone)     { bBetter = dist < nearestDist; }
				else if (!bInCone && !bBestInCone)   { bBetter = offset < bestOffset; }

				if (bBetter)
				{
					bestOffset = offset;
					nearest = link;
					nearestSlot = i;
					nearestDist = dist;
				}
			}
		}

		if (nearest == nullptr)
		{
			LEASI_ERROR("Nothing in front of you. Aim at a slot -- or if there are no links at "
				"all, run cc.spawnlink or cc.sweep first.");
			return;
		}
		LEASI_INFO("Aimed at slot {} ({:.0f}u away, {:.1f} degrees off centre).{}",
			nearestSlot, nearestDist,
			std::atan(bestOffset) * 57.2957795f,
			(bestOffset > k_aimCone)
				? " Nothing was inside the aim cone, so this is only the straightest thing in "
				  "front of you -- read the L number off the label and pass it instead."
				: "");

		int linkIndex = -1;
		for (size_t i = 0; i < GetSpawnedLinks().size(); i++)
		{
			if (GetSpawnedLinks()[i] == nearest) { linkIndex = static_cast<int>(i); break; }
		}
		ExplainOneLink(nearest, linkIndex);
	}

	void ExplainOneLink(ACoverLink* nearest, int linkIndex)
	{
		if (nearest == nullptr || ::Common::IsDestroyed(nearest))
		{
			LEASI_ERROR("L{} no longer exists.", linkIndex);
			return;
		}

		const auto* const edges = GetLinkEdges(nearest);
		LEASI_INFO("C{} / L{} = {} at ({:.0f}, {:.0f}, {:.0f}).",
			LinkObjectNumber(nearest), linkIndex, PathOf(nearest),
			nearest->location.X, nearest->location.Y, nearest->location.Z);

		if (edges == nullptr)
		{
			LEASI_WARN("No placement record -- this link predates the current session's "
				"bookkeeping, or was not made by the walk.");
			return;
		}

		LEASI_INFO("Seeded at ({:.0f}, {:.0f}, {:.0f}) with the floor {} at Z {:.0f}. A slot "
			"far from this was moved by the walk; a seed that is already wrong is the sweep's "
			"fault, not the walk's.",
			edges->seedPoint.X, edges->seedPoint.Y, edges->seedPoint.Z,
			edges->bSeedHadFloor ? "found" : "NOT FOUND", edges->seedFloorZ);
		LEASI_INFO("Walk: left {} iteration(s) (edge {}), right {} iteration(s) (edge {}). "
			"Floor moved {:+.0f}u on the left walk and {:+.0f}u on the right. Span {:.0f}u, "
			"slots spread {:.0f}u horizontally and {:.0f}u vertically{}. {} stall(s).",
			edges->leftWalkIterations, edges->bFoundLeftEdge ? "found" : "NOT FOUND",
			edges->rightWalkIterations, edges->bFoundRightEdge ? "found" : "NOT FOUND",
			edges->leftFloorRise, edges->floorRise, edges->span,
			edges->horizontalExtent, edges->verticalExtent,
			(edges->verticalExtent > edges->horizontalExtent)
				? " -- THIS LINK RUNS UP A WALL" : "",
			edges->walkStalls);

		// What the walk saw at the moment it gave up on the right-hand end. Two theories
		// about this have been wrong already, both argued from finished slot positions.
		if (edges->cornerTurns > 0 || edges->iterationsWithoutFloor > 0
			|| edges->widestSlotGap > 0.0f)
		{
			LEASI_INFO("  Corners turned: {} ({} left the walk with NO floor reference). "
				"{} right-walk iteration(s) ran without one. Widest gap between consecutive "
				"slots: {:.0f}u.",
				edges->cornerTurns, edges->cornerTurnsWithoutFloor,
				edges->iterationsWithoutFloor, edges->widestSlotGap);
			if (edges->iterationsWithoutFloor > 0)
			{
				LEASI_WARN("  With no floor, IsStandingHere measures from the walk's own Z "
					"instead of the floor, so a stretch can classify as standing throughout "
					"and neither the transition pair nor the interval slot fires -- which is "
					"what a long leg with nothing on it looks like.");
			}
		}
		if (edges->transitionSeamsMissed > 0)
		{
			LEASI_INFO("  {} transition(s) could not find their seam and fell back to a single "
				"slot. Before that fallback existed the walk did nothing at all here, so the "
				"cover type never changed and the whole stretch was skipped.",
				edges->transitionSeamsMissed);
		}
		LEASI_INFO("  Right end branch: {}{}", SlotOriginName(edges->rightEndOrigin),
			edges->bRightEndProbed
				? " (reached via the wall-ran-out path)"
				: " (the wall-ran-out path never ran)");
		if (edges->bRightEndProbed)
		{
			if (edges->rightEndProbeDistance < 0.0f)
			{
				LEASI_WARN("  Right end: the wall in front ran out and the sideways probe "
					"found NOTHING within {:.0f}u along the direction of travel, so the "
					"inside-corner branch could not fire. However tight the corner looks, "
					"there is no geometry for that probe to hit.{}",
					36.0f * 2.0f,
					edges->bRightEndSlotSuppressed
						? " The terminal slot was also suppressed for landing within half an "
						  "AlignDist of the previous one, which is why the run ends on a "
						  "transition slot." : "");
			}
			else
			{
				LEASI_INFO("  Right end: sideways probe hit at {:.0f}u, alignment {:.2f} "
					"(near 1 = the wall we were already following, near 0 = a genuine "
					"perpendicular face).{}",
					edges->rightEndProbeDistance, edges->rightEndProbeAlignment,
					edges->bRightEndSlotSuppressed
						? " Terminal slot suppressed as too close to the previous one." : "");
			}
		}
		else if (edges->bRightEndSlotSuppressed)
		{
			LEASI_INFO("  Right end: terminal slot suppressed for landing within half an "
				"AlignDist of the previous one. UE3 does the same at a full AlignDist, so a "
				"pillar about one pawn wide legitimately ends on a transition slot.");
		}

		const int slotCount = static_cast<int>(nearest->Slots.Count());
		// ! Why the engine's own check rather than a probe of our own.
		//
		// "I cannot get into that cover" is not answerable from slot coordinates. A slot can
		// be enabled, correctly typed, correctly faced and still be unusable because the pawn
		// does not fit, or because the marker the pawn actually navigates to is somewhere
		// other than the slot. ACoverLink::IsValidClaim is the rule the game itself applies,
		// so it cannot disagree with what happens when you walk up to the slot -- which a
		// hand-rolled box trace certainly can.
		//
		// Called twice, differing only in bSkipOverlapCheck, because the difference between
		// the two answers is the diagnosis: rejected by both is a slot the engine dislikes for
		// its own reasons, rejected only with the overlap check ON is a slot with something in
		// the way -- geometry, or another claimant.
		APawn* claimant = nullptr;
		if (auto* const wi = GetWorldInfo())
		{
			if (auto* const pc = wi->GetALocalPlayerController())
			{
				claimant = pc->Pawn;
			}
		}
		if (claimant == nullptr)
		{
			LEASI_INFO("No player pawn, so claimability is not reported below.");
		}

		for (const auto& placement : edges->placements)
		{
			const bool bValid = placement.slotIdx >= 0 && placement.slotIdx < slotCount;
			const FCoverSlot* const slot =
				bValid ? &nearest->Slots(placement.slotIdx) : nullptr;
			const int yawDelta = placement.seatedYaw - placement.placedYaw;

			LEASI_INFO("  slot {:2} {:22} placed ({:.0f}, {:.0f}, {:.0f}) -> seated "
				"({:.0f}, {:.0f}, {:.0f}), yaw {:+} -> {:+} ({:+}), {}, {}{}{}",
				placement.slotIdx, SlotOriginName(placement.origin),
				placement.placedLocation.X, placement.placedLocation.Y,
				placement.placedLocation.Z,
				placement.seatedLocation.X, placement.seatedLocation.Y,
				placement.seatedLocation.Z,
				placement.placedYaw, placement.seatedYaw, yawDelta,
				(slot != nullptr && slot->bEnabled != 0) ? "enabled" : "DISABLED",
				(slot != nullptr) ? CoverTypeName(slot->CoverType) : "?",
				placement.bDisabledFirstPass
					? (placement.bRecovered ? ", recovered on retry" : ", failed both passes")
					: "",
				placement.bReseatedOffSurface ? ", RESEATED OFF SURFACE" : "");

			if (!bValid) { continue; }

			// The marker, not the slot, is what the pawn paths to -- so a marker that is not
			// where its slot is means the cover is entered from somewhere other than where it
			// looks like it should be. Reported as an offset because zero is the expected
			// value and any other number is the finding.
			const FVector slotLoc = nearest->GetSlotLocation(placement.slotIdx, FALSE);
			auto* const marker = (slot != nullptr) ? slot->SlotMarker : nullptr;
			float markerOffset = -1.0f;
			if (marker != nullptr)
			{
				const FVector at = marker->LOCATION;
				const float dx = at.X - slotLoc.X;
				const float dy = at.Y - slotLoc.Y;
				const float dz = at.Z - slotLoc.Z;
				markerOffset = std::sqrt((dx * dx) + (dy * dy) + (dz * dz));
			}

			if (claimant != nullptr)
			{
				const bool bClaimable =
					nearest->IsValidClaim(claimant, placement.slotIdx, TRUE, FALSE) != 0;
				const bool bClaimableNoOverlap =
					nearest->IsValidClaim(claimant, placement.slotIdx, TRUE, TRUE) != 0;

				const char* verdict = "claimable";
				if (!bClaimable && bClaimableNoOverlap)
				{
					verdict = "NOT CLAIMABLE -- fails only the overlap check, so something is "
						"in the way";
				}
				else if (!bClaimable)
				{
					verdict = "NOT CLAIMABLE even with the overlap check skipped";
				}

				if (marker != nullptr)
				{
					LEASI_INFO("           {}; marker {:.0f}u from the slot at "
						"({:.0f}, {:.0f}, {:.0f})",
						verdict, markerOffset,
						marker->LOCATION.X, marker->LOCATION.Y, marker->LOCATION.Z);
				}
				else
				{
					LEASI_WARN("           {}; NO SLOT MARKER, so this slot is not enterable "
						"cover at all", verdict);
				}
			}

			// Slips are how a slot connects to a DIFFERENT link; without one, a run is a
			// dead end however healthy its own slots are.
			if (slot != nullptr && slot->SlipTarget.Count() > 0)
			{
				for (UINT sIdx = 0; sIdx < slot->SlipTarget.Count(); sIdx++)
				{
					const FCoverReference& ref = slot->SlipTarget(sIdx);
					auto* const target = ref.Actor;
					const FVector at = (target != nullptr) ? target->LOCATION : FVector{};
					const float dx = at.X - slotLoc.X;
					const float dy = at.Y - slotLoc.Y;
					const float dz = at.Z - slotLoc.Z;
					LEASI_INFO("           slip {} {:.0f}u to {} at "
						"({:.0f}, {:.0f}, {:.0f})",
						(ref.Direction > 0) ? "right" : "left",
						std::sqrt((dx * dx) + (dy * dy) + (dz * dz)),
						(target != nullptr) ? PathOf(target) : "<null>",
						at.X, at.Y, at.Z);
				}
			}
			else if (slot != nullptr
				&& (placement.slotIdx == 0 || placement.slotIdx == slotCount - 1))
			{
				LEASI_INFO("           no slip from this edge slot, so the run ends here.");
			}
		}
	}

	// ! cc.buildslips -- cross-link cover slips
	// ========================================
	//
	// Within a link, adjacent slots are joined by SlotToSlot ReachSpecs and the player moves
	// along the chain. BETWEEN links there was nothing at all, which is why cover meeting at
	// a seam cannot be traversed -- "the slots work, the transitions don't".
	//
	// Every constant here is measured from shipped cover rather than chosen. All 46 slips in
	// BioD_CerMir_125Land run 95u to 1507u, sit within 69u of level, and -- 46 times out of
	// 46 -- start at an EDGE slot, with slot 0 taking Direction -1 and the last slot taking
	// Direction +1. See note 06.

	namespace
	{
		/// Longest slip to create. Shipped maximum is 1507u; this is that rounded up.
		constexpr float k_maxSlipDistance = 1536.0f;
		/// Slip ReachSpecs use the shipped slot-to-slot size, the same as the within-link
		/// chain -- a slip is the same kind of move, it just crosses a link boundary.
		constexpr float k_slipSpecRadius = 40.0f;
		constexpr float k_slipSpecHeight = 95.0f;

		/// Shortest slip worth making. Shipped slips bottom out at 95u, but a genuine inside
		/// corner puts two edge slots ~32u apart and that one is wanted -- so this is set
		/// only high enough to drop the degenerate case, two links whose edge slots are
		/// effectively in the same place. The first run produced slips as short as 3u.
		constexpr float k_minSlipDistance = 16.0f;
		/// Most a slip may rise or fall. Shipped maximum is 69u.
		constexpr float k_maxSlipRise = 96.0f;
		/// How far round from "straight out of the open side of the run" a target may sit. A
		/// slip continues PAST the edge; something behind the slot is not a slip, it is the
		/// cover we just walked along.
		constexpr float k_minSlipAhead = 0.25f;

		/// Whether a pawn could plausibly cross from one slot to the other.
		///
		/// A zero-extent line rather than a pawn-sized box: a box test fails against the very
		/// geometry the two runs are built on whenever the slip passes close to a corner, and
		/// the engine re-checks reachability when the move is actually attempted. This only
		/// has to be right enough not to offer a slip straight through a wall.
		bool SlipPathClear(AActor* tracer, const FVector& from, const FVector& to, float lift)
		{
			if (tracer == nullptr) { return false; }
			const FVector a{ from.X, from.Y, from.Z + lift };
			const FVector b{ to.X, to.Y, to.Z + lift };
			FVector hitLocation{}, hitNormal{};
			const FVector zeroExtent{ 0.0f, 0.0f, 0.0f };
			return tracer->Trace(b, a, 0, zeroExtent, TRUE,
				&hitLocation, &hitNormal, nullptr) == nullptr;
		}

		/// One end of a cover run -- the only place a slip may start.
		struct SlipEdge
		{
			ACoverLink*       link{ nullptr };
			int               slotIdx{ 0 };
			int               direction{ 0 };   // -1 for slot 0, +1 for the last slot
			FVector           location{};
			FVector           outward{};        // along the wall, away from the run
			ACoverSlotMarker* marker{ nullptr };
		};
	}

	int LinkObjectNumber(ACoverLink* link)
	{
		return TrailingNumber(link);
	}

	int LinkSpawnIndex(ACoverLink* link)
	{
		for (size_t i = 0; i < GetSpawnedLinks().size(); i++)
		{
			if (GetSpawnedLinks()[i] == link) { return static_cast<int>(i); }
		}
		return -1;
	}

	void CmdSlips(float radius)
	{
		LEASI_INFO("=== cc.slips ===");

		auto* const wi = GetWorldInfo();
		auto* const pc = (wi != nullptr) ? wi->GetALocalPlayerController() : nullptr;
		if (pc == nullptr || pc->Pawn == nullptr)
		{
			LEASI_ERROR("Need a player pawn.");
			return;
		}
		if (radius <= 0.0f) { radius = 512.0f; }
		const FVector at = pc->Pawn->LOCATION;
		const float radiusSq = radius * radius;

		struct Row
		{
			float   distance{ 0.0f };
			std::string text{};
		};
		std::vector<Row> rows;

		for (auto* const link : g_SpawnedLinks)
		{
			if (link == nullptr || ::Common::IsDestroyed(link)) { continue; }
			const int count = static_cast<int>(link->Slots.Count());
			if (count == 0) { continue; }

			const int indices[2] = { 0, count - 1 };
			for (int e = 0; e < 2; e++)
			{
				const int slotIdx = indices[e];
				if (e == 1 && count == 1) { continue; }

				const FVector loc = link->GetSlotLocation(slotIdx, FALSE);
				const float distSq = DistanceSquared(loc, at);
				if (distSq > radiusSq) { continue; }

				const FCoverSlot& slot = link->Slots(slotIdx);
				std::string detail;
				if (slot.SlipTarget.Count() == 0)
				{
					detail = "NO SLIP";
				}
				for (UINT i = 0; i < slot.SlipTarget.Count(); i++)
				{
					const FCoverReference& ref = slot.SlipTarget(i);
					auto* const target = ref.Actor;
					const FVector tl = (target != nullptr) ? target->LOCATION : FVector{};
					const float dx = tl.X - loc.X;
					const float dy = tl.Y - loc.Y;
					const float dz = tl.Z - loc.Z;
					if (!detail.empty()) { detail += "; "; }
					const std::string targetPath =
						(target != nullptr) ? PathOf(target) : std::string{ "<null>" };
					char slipText[512]{};
					_snprintf_s(slipText, _TRUNCATE, "slip %s %.0fu to %s",
						(ref.Direction > 0) ? "right" : "left",
						std::sqrt((dx * dx) + (dy * dy) + (dz * dz)),
						targetPath.c_str());
					detail += slipText;
				}

				Row row{};
				row.distance = std::sqrt(distSq);
				char text[1024]{};
				_snprintf_s(text, _TRUNCATE,
					"  %5.0fu away  C%d / L%d s%d %s (%.0f, %.0f, %.0f) "
					"[allow %d, canLeft %d, canRight %d, %s] %s",
					row.distance, LinkObjectNumber(link), LinkSpawnIndex(link), slotIdx,
					(slotIdx == 0) ? "LEFT edge " : "RIGHT edge",
					loc.X, loc.Y, loc.Z,
					static_cast<int>(slot.bAllowCoverSlip),
					static_cast<int>(slot.bCanCoverSlip_Left),
					static_cast<int>(slot.bCanCoverSlip_Right),
					(slot.bEnabled != 0) ? "enabled" : "DISABLED",
					detail.c_str());
				row.text = text;
				rows.push_back(std::move(row));
			}
		}

		std::sort(rows.begin(), rows.end(),
			[](const Row& a, const Row& b) { return a.distance < b.distance; });

		LEASI_INFO("{} edge slot(s) within {:.0f}u of you, nearest first. A slip needs a "
			"target here AND a matching one coming back.",
			static_cast<int>(rows.size()), radius);
		for (const auto& row : rows)
		{
			LEASI_INFO("{}", row.text);
		}
		if (rows.empty())
		{
			LEASI_INFO("Nothing in range -- widen it with 'cc.slips 2048'.");
		}
	}

	void CmdClearSlips()
	{
		LEASI_INFO("=== cc.clearslips ===");
		int cleared = 0;
		for (auto* const link : g_SpawnedLinks)
		{
			if (link == nullptr || ::Common::IsDestroyed(link)) { continue; }
			for (int i = 0; i < static_cast<int>(link->Slots.Count()); i++)
			{
				FCoverSlot& slot = link->Slots(i);
				cleared += static_cast<int>(slot.SlipTarget.Count());
				slot.SlipTarget.Clear();
				slot.bCanCoverSlip_Left = 0;
				slot.bCanCoverSlip_Right = 0;
			}
		}
		LEASI_INFO("Cleared {} slip target(s).", cleared);
	}

	void CmdBuildSlips()
	{
		LEASI_INFO("=== cc.buildslips ===");

		if (g_SpawnedLinks.empty())
		{
			LEASI_INFO("No spawned links. Use cc.sweep or cc.spawnlink first.");
			return;
		}

		auto* const wi = GetWorldInfo();
		auto* const pc = (wi != nullptr) ? wi->GetALocalPlayerController() : nullptr;
		auto* const tracer = (pc != nullptr) ? static_cast<AActor*>(pc->Pawn) : nullptr;
		if (tracer == nullptr)
		{
			LEASI_ERROR("Need a player pawn to trace with.");
			return;
		}

		auto* const cls = ACoverLink::StaticClass();
		auto* const defaults = (cls != nullptr)
			? static_cast<ACoverLink*>(cls->ClassDefaultObject) : nullptr;
		const float midHeight = (defaults != nullptr) ? defaults->MidHeight : 70.0f;

		// Re-runnable: a second pass would otherwise stack duplicates on every slot, the same
		// way the ReachSpec pass had to learn to clear PathList first.
		CmdClearSlips();

		// ! bAllowCoverSlip is a permission, and it is probably clear on the archetype.
		//
		// All 132 slots in BioD_CerMir_125Land carry bAllowCoverSlip = true, including the 86
		// with no SlipTarget at all -- and UE3 only serialises properties that DIFFER from
		// the archetype, so the archetype default is almost certainly false and the editor
		// sets it on everything.
		//
		// This project has already been caught by exactly this once: bAllowClimbUp is clear
		// on the CoverLink archetype, which is why AddMyMarker never made a single
		// MantleMarker. A SlipTarget on a slot that does not allow slipping would be data the
		// engine never reads, and it would look identical in the log to a slip that works.
		//
		// Counted rather than set silently, because the count is the evidence for what the
		// archetype default actually is: a large number on the first run means it was clear.
		int allowSet = 0;
		for (auto* const link : g_SpawnedLinks)
		{
			if (link == nullptr || ::Common::IsDestroyed(link)) { continue; }
			for (int i = 0; i < static_cast<int>(link->Slots.Count()); i++)
			{
				FCoverSlot& slot = link->Slots(i);
				if (slot.bAllowCoverSlip == 0)
				{
					slot.bAllowCoverSlip = 1;
					allowSet++;
				}
			}
		}
		if (allowSet > 0)
		{
			LEASI_WARN("{} slot(s) had bAllowCoverSlip clear and have been set. Shipped cover "
				"has it set on every slot, and a clear flag would have made every SlipTarget "
				"below dead data -- the same way bAllowClimbUp being clear on the archetype "
				"stops MantleMarkers being made.", allowSet);
		}
		else
		{
			LEASI_INFO("Every slot already allowed cover slips.");
		}

		// Two entries per link rather than one per slot, so the O(n^2) pick below is 600
		// candidates for a 300-link sweep -- cheap enough not to need the bounds boxes the
		// seed dedupe uses.
		std::vector<SlipEdge> edges;
		for (auto* const link : g_SpawnedLinks)
		{
			if (link == nullptr || ::Common::IsDestroyed(link)) { continue; }
			const int count = static_cast<int>(link->Slots.Count());
			if (count == 0) { continue; }

			const int indices[2] = { 0, count - 1 };
			const int directions[2] = { -1, 1 };
			for (int e = 0; e < 2; e++)
			{
				const int slotIdx = indices[e];
				FVector axisX{}, axisY{}, axisZ{};
				RotatorToAxes(link->GetSlotRotation(slotIdx, FALSE), axisX, axisY, axisZ);

				SlipEdge entry{};
				entry.link = link;
				entry.slotIdx = slotIdx;
				entry.direction = directions[e];
				entry.location = link->GetSlotLocation(slotIdx, FALSE);
				// +axisY is the direction the right-hand walk travels, so it is "right" along
				// the wall; the left edge looks the other way. A one-slot link is its own
				// left and right edge and legitimately gets both.
				entry.outward = (directions[e] > 0)
					? axisY : FVector{ -axisY.X, -axisY.Y, -axisY.Z };
				entry.marker = link->Slots(slotIdx).SlotMarker;
				edges.push_back(entry);
			}
		}

		int created = 0;
		int specsCreated = 0;
		int specsFailed = 0;
		int noMarker = 0;
		int rejectedBlocked = 0;
		int rejectedBehind = 0;
		int rejectedRise = 0;
		int rejectedRange = 0;
		float shortest = -1.0f;
		float longest = 0.0f;

		for (const auto& from : edges)
		{
			const SlipEdge* best = nullptr;
			float bestDist = 0.0f;

			for (const auto& to : edges)
			{
				if (to.link == from.link) { continue; }

				const float dx = to.location.X - from.location.X;
				const float dy = to.location.Y - from.location.Y;
				const float dz = to.location.Z - from.location.Z;
				const float horizontal = std::sqrt((dx * dx) + (dy * dy));
				const float dist = std::sqrt((dx * dx) + (dy * dy) + (dz * dz));

				if (dist > k_maxSlipDistance || dist < k_minSlipDistance)
				{
					rejectedRange++;
					continue;
				}
				if (std::abs(dz) > k_maxSlipRise) { rejectedRise++; continue; }
				if (horizontal < 1.0f) { continue; }

				const float ahead = ((dx / horizontal) * from.outward.X)
					+ ((dy / horizontal) * from.outward.Y);
				if (ahead < k_minSlipAhead) { rejectedBehind++; continue; }

				// Nearest wins, and the trace is left until last because it is the only
				// expensive test here.
				if (best != nullptr && dist >= bestDist) { continue; }
				if (!SlipPathClear(tracer, from.location, to.location, midHeight * 0.5f))
				{
					rejectedBlocked++;
					continue;
				}

				best = &to;
				bestDist = dist;
			}

			if (best == nullptr) { continue; }
			if (best->marker == nullptr) { noMarker++; continue; }

			FCoverSlot& slot = from.link->Slots(from.slotIdx);

			FCoverReference reference{};
			reference.Guid = FGuid{};   // zero, as shipped same-level slips are
			reference.Actor = static_cast<AActor*>(best->marker);
			// The engine cannot be reading this as an index: shipped values are float bit
			// patterns around +/-0.7 to 1.0 and shipped cover works. The marker in `Actor`
			// identifies the destination on its own, so writing the honest index costs
			// nothing and documents intent. See note 06.
			reference.SlotIdx = best->slotIdx;
			// The SOURCE edge's direction, not the target's.
			//
			// Shipped data is unambiguous -- slot 0 carries -1 and the last slot +1, 46 times
			// out of 46, matching bCanCoverSlip_Left/Right on the same slot exactly. Taking
			// it from the target instead produced a slot 0 whose flag said "can slip left"
			// and whose reference said Direction +1, so a lookup for a left slip would not
			// match the only entry there was.
			reference.Direction = from.direction;
			slot.SlipTarget.Add(reference);

			if (from.direction > 0) { slot.bCanCoverSlip_Right = 1; }
			else { slot.bCanCoverSlip_Left = 1; }

			// ! A slip also needs a ReachSpec, or nothing can move along it.
			//
			// Mgamerz found this by turning on `show paths`: the corner that DOES work --
			// slots 4 and 5 of C1, which wrap that corner inside one link -- has a diagonal
			// spec drawn between the two markers, and the corner that does not work has
			// nothing between its two links. Cover movement runs on the path network; the
			// SlipTarget property says a slip is permitted, but the spec is what the move
			// actually travels along.
			//
			// Same class and same shipped 40/95 size as the within-link chain, because this
			// is the same kind of move -- it just happens to cross a link boundary. Directed,
			// like every ReachSpec: the opposite edge builds its own on its own pass, which
			// is what makes the pair bidirectional.
			if (from.marker != nullptr
				&& CreateSpec(from.marker, best->marker,
					k_slipSpecRadius, k_slipSpecHeight, k_reachFlagWalk) != nullptr)
			{
				specsCreated++;
			}
			else
			{
				specsFailed++;
			}

			created++;
			if (shortest < 0.0f || bestDist < shortest) { shortest = bestDist; }
			if (bestDist > longest) { longest = bestDist; }
		}

		if (created > 0)
		{
			LEASI_INFO("Slips: {} created from {} edge slot(s), {:.0f}u to {:.0f}u.",
				created, static_cast<int>(edges.size()), shortest, longest);
		}
		else
		{
			LEASI_WARN("No slips created from {} edge slot(s). Shipped slips run 95u to "
				"1507u, so runs further apart than that genuinely cannot be slipped between.",
				static_cast<int>(edges.size()));
		}
		LEASI_INFO("Rejected: {} out of range, {} too much rise or drop, {} behind the slot "
			"rather than past its edge, {} with no clear path. {} chosen target(s) had no "
			"marker.",
			rejectedRange, rejectedRise, rejectedBehind, rejectedBlocked, noMarker);
		LEASI_INFO("Slip ReachSpecs: {} created, {} failed. Without these the SlipTarget "
			"property is a permission with no path to travel along -- the corner that works "
			"is the one with a spec drawn between its markers under 'show paths'.",
			specsCreated, specsFailed);
	}

	// ! cc.buildlinks -- Phase 1 step 3, the two-pass build
	// ========================================

	void CmdBuildLinks()
	{
		LEASI_INFO("=== cc.buildlinks ===");

		if (g_SpawnedLinks.empty())
		{
			LEASI_INFO("No spawned links. Use cc.spawnlink first.");
			return;
		}

		// ABioScout::CanMantle searches the world's cover list for a landing slot, so a slot
		// analysed while its opposite number did not yet exist was analysed against an
		// incomplete world. The editor got this for free by building every link before
		// running the per-slot pass; we have to do it explicitly.
		ScopedScout guard{};
		if (!guard)
		{
			LEASI_ERROR("Could not acquire a scout; aborting.");
			return;
		}

		int linkCount = 0, slotCount = 0;
		int gainedMantle = 0, lostMantle = 0, retargeted = 0;

		for (auto* link : g_SpawnedLinks)
		{
			if (link == nullptr || ::Common::IsDestroyed(link))
			{
				continue;
			}
			linkCount++;

			const int slots = static_cast<int>(link->Slots.Count());
			for (int i = 0; i < slots; i++)
			{
				// Snapshot only what the second pass can plausibly change.
				const FCoverSlot& before = link->Slots(i);
				const bool  hadMantle = (ReadSlotFlags(before) & 0x00000010ul) != 0;
				auto* const hadTarget = before.MantleTarget.Actor;
				const int   hadTargetSlot = before.MantleTarget.SlotIdx;

				CallBuildSlotInfo(link, i);

				const FCoverSlot& after = link->Slots(i);
				const bool  hasMantle = (ReadSlotFlags(after) & 0x00000010ul) != 0;
				auto* const hasTarget = after.MantleTarget.Actor;
				const int   hasTargetSlot = after.MantleTarget.SlotIdx;

				slotCount++;
				if (!hadMantle && hasMantle) gainedMantle++;
				if (hadMantle && !hasMantle) lostMantle++;
				if (hasTarget != nullptr &&
					(hasTarget != hadTarget || hasTargetSlot != hadTargetSlot))
				{
					retargeted++;
					LEASI_INFO("  {} slot {}: MantleTarget now {} slot {} ({})",
						PathOf(link), i,
						PathOf(reinterpret_cast<UObject*>(hasTarget)), hasTargetSlot,
						hasTargetSlot == -1 ? "climb-up onto a MantleMarker"
						                    : "vault-over to a cover slot");
				}
			}
		}

		LEASI_INFO("Second pass over {} link(s), {} slot(s): {} gained mantle, {} lost it, "
			"{} changed target.", linkCount, slotCount, gainedMantle, lostMantle, retargeted);

		if (gainedMantle == 0 && retargeted == 0 && lostMantle > 0)
		{
			LEASI_WARN("{} slot(s) LOST a mantle target the first pass had given them. "
				"BuildSlotInfo re-ran with more links in the world, so something the sweep "
				"placed since is now standing in the vault path.", lostMantle);
		}
		else if (gainedMantle == 0 && retargeted == 0)
		{
			LEASI_INFO("Nothing changed -- either the first pass already saw every link, or "
				"no link is positioned to mantle onto another.");
		}

		// ! CoverSlotMarkers
		// ========================================
		//
		// Markers come last, and only here rather than in cc.spawnlink, for the same reason the
		// analysis pass does: they are the finished article, and a link is not finished until
		// every other link exists. Re-running this command rebuilds them from scratch.
		// Idempotent repair, so a link spawned before this existed still ends up usable.
		int splicedNow = 0;
		for (auto* link : g_SpawnedLinks)
		{
			if (link != nullptr && !::Common::IsDestroyed(link) &&
				SpliceIntoCoverList(GetWorldInfo(), link))
			{
				splicedNow++;
			}
		}
		if (splicedNow > 0)
		{
			LEASI_INFO("Spliced {} link(s) onto WorldInfo.CoverList.", splicedNow);
		}

		LEASI_INFO("--- creating CoverSlotMarkers ---");

		int markersDestroyed = 0, markersCreated = 0, mantleMarkers = 0, incomplete = 0;
		int climbUpCapableSlots = 0;

		for (auto* link : g_SpawnedLinks)
		{
			if (link == nullptr || ::Common::IsDestroyed(link))
			{
				continue;
			}

			markersDestroyed += DestroySlotMarkers(link);

			const int slots = static_cast<int>(link->Slots.Count());
			for (int i = 0; i < slots; i++)
			{
				// AddMyMarker only places a MantleMarker where a slot has BOTH bAllowClimbUp
				// and bAllowPopup. bAllowClimbUp is clear on the CoverLink archetype, so by
				// default this pass produces none -- worth saying out loud, because climb-up
				// nodes are a project goal and their absence would otherwise look like a bug.
				if ((ReadSlotFlags(link->Slots(i)) & 0x00040000ul) != 0) { climbUpCapableSlots++; }
			}

			const int reported = CallAddMyMarker(link, guard.Get());
			if (reported < 0)
			{
				LEASI_ERROR("  {}: AddMyMarker could not be called.", PathOf(link));
				continue;
			}

			const int placed = CountSlotMarkers(link);
			markersCreated += placed;
			mantleMarkers += (reported - slots);

			if (placed != slots)
			{
				incomplete++;
				LEASI_WARN("  {}: only {} of {} slots got a marker. The rest are not enterable "
					"cover.", PathOf(link), placed, slots);
			}
		}

		LEASI_INFO("Markers: {} created ({} destroyed first), {} MantleMarker(s), {} link(s) "
			"incomplete.", markersCreated, markersDestroyed, mantleMarkers, incomplete);

		if (mantleMarkers == 0)
		{
			LEASI_INFO("No MantleMarkers: {} slot(s) have bAllowClimbUp. AddMyMarker requires "
				"bAllowClimbUp AND bAllowPopup, and bAllowClimbUp is clear on the CoverLink "
				"archetype, so climb-up nodes need it set on the slots first.",
				climbUpCapableSlots);
		}

		// ! ReachSpecs
		// ========================================
		//
		// Markers alone are inert: they have to be joined to each other. Nothing in the
		// shipping exe does it, so we build the specs ourselves -- see the notes above
		// BuildSlotToSlotSpecs.
		LEASI_INFO("--- building SlotToSlot ReachSpecs ---");

		float pathRadius = 0.0f, pathHeight = 0.0f;
		if (!GetScoutPathSize(guard.Get(), pathRadius, pathHeight))
		{
			LEASI_ERROR("Scout has no PathSizes; cannot size ReachSpecs. Skipping.");
		}
		else
		{
			// Measured on BioD_CerMir_125Land: shipped SlotToSlotReachSpecs are radius 40 /
			// height 95, while the MantleReachSpecs are 34/90 -- which is what this scout's
			// PathSizes(0) gives. So the scout size is the mantle size, not the walk size.
			// Match the shipped slot-to-slot figures rather than our own scout here.
			constexpr float k_shippedSlotToSlotRadius = 40.0f;
			constexpr float k_shippedSlotToSlotHeight = 95.0f;
			LEASI_INFO("Scout PathSize is radius {:.1f} / height {:.1f}; using the shipped "
				"slot-to-slot size {:.1f}/{:.1f} instead.", pathRadius, pathHeight,
				k_shippedSlotToSlotRadius, k_shippedSlotToSlotHeight);
			pathRadius = k_shippedSlotToSlotRadius;
			pathHeight = k_shippedSlotToSlotHeight;

			SpecBuildResult totals{};
			int specsCleared = 0;
			for (auto* link : g_SpawnedLinks)
			{
				if (link == nullptr || ::Common::IsDestroyed(link))
				{
					continue;
				}

				// Idempotent: markers were just rebuilt, but a re-run on unchanged markers
				// would otherwise stack duplicate specs on every PathList.
				specsCleared += ClearSlotSpecs(link);

				const SpecBuildResult r = BuildSlotToSlotSpecs(link, pathRadius, pathHeight);
				totals.created += r.created;
				totals.skippedNoMarker += r.skippedNoMarker;
				totals.skippedDisabled += r.skippedDisabled;
				totals.failed += r.failed;
			}

			LEASI_INFO("ReachSpecs: {} created ({} cleared first). Skipped {} pair(s) with a "
				"missing marker, {} with a disabled slot. {} construction failure(s).",
				totals.created, specsCleared, totals.skippedNoMarker, totals.skippedDisabled,
				totals.failed);
		}

		// ! Cross-link cover slips
		// ========================================
		//
		// After the markers and the within-link specs, because a slip needs both: its target
		// is a CoverSlotMarker, and it adds a ReachSpec of its own.
		//
		// BEFORE the octree, and that ordering is load-bearing. AddToNavigationOctree walks
		// PathList at registration time, so a spec added afterwards is never registered and
		// the move is invisible to anything that searches the octree.
		LEASI_INFO("--- building cross-link cover slips ---");
		CmdBuildSlips();

		// ! Navigation octree
		// ========================================
		//
		// Last, because AddToNavigationOctree registers the marker AND every ReachSpec on its
		// PathList in one pass. This is what actually makes the cover findable.
		LEASI_INFO("--- registering markers in the navigation octree ---");

		OctreeResult octree{};
		for (auto* link : g_SpawnedLinks)
		{
			if (link == nullptr || ::Common::IsDestroyed(link))
			{
				continue;
			}
			const OctreeResult r = RegisterMarkersInOctree(link);
			octree.registered += r.registered;
			octree.alreadyIn += r.alreadyIn;
			octree.noCylinder += r.noCylinder;
			octree.stillOut += r.stillOut;
			octree.wrongOwnerType += r.wrongOwnerType;
		}

		LEASI_INFO("Octree: {} marker(s) registered, {} already in. {} had no "
			"CylinderComponent, {} did not take, {} with the wrong OwnerType.",
			octree.registered, octree.alreadyIn, octree.noCylinder, octree.stillOut,
			octree.wrongOwnerType);

		if (octree.registered == 0 && octree.alreadyIn == 0)
		{
			LEASI_ERROR("No marker reached the octree, so BioPlayerController::FindPlayerCover "
				"cannot see this cover at all -- its whole search is a radius query over the "
				"octree keyed on CoverSlotMarkers.");
		}
		else if (octree.wrongOwnerType > 0)
		{
			LEASI_ERROR("{} marker(s) are in the octree but without OwnerType bit 0x04, so the "
				"cover search will skip them.", octree.wrongOwnerType);
		}

	}

	// ! cc.covercheck -- why is this cover not enterable?
	// ========================================

	void CmdCoverCheck()
	{
		LEASI_INFO("=== cc.covercheck ===");

		auto* const wi = GetWorldInfo();
		if (wi == nullptr)
		{
			LEASI_ERROR("No WorldInfo.");
			return;
		}
		auto* const pc = wi->GetALocalPlayerController();
		if (pc == nullptr || pc->Pawn == nullptr)
		{
			LEASI_ERROR("No player pawn.");
			return;
		}
		const FVector playerLoc = pc->Pawn->LOCATION;

		if (g_SpawnedLinks.empty())
		{
			LEASI_INFO("No spawned links this session -- reporting on the level's own cover "
				"only. That is the intended use in a shipped level.");
		}

		// ! Is the link in the world's cover list?
		//
		// A level-loaded CoverLink is threaded onto WorldInfo.CoverList through its
		// NextCoverLink field, and every ANavigationPoint onto NavigationPointList through
		// nextNavigationPoint. A link we Spawn is in neither: Spawn puts an actor in
		// Level->Actors and nothing else. ABioScout::CanMantle happens to iterate
		// GWorld->Levels directly, which is why mantle analysis worked without this -- but
		// anything that walks the world lists will silently skip our cover.
		//
		// Chains are walked with a hard cap: a malformed one is a hang, not an error.
		constexpr int k_maxChain = 20000;

		int coverListLength = 0;
		{
			auto* node = wi->CoverList;
			while (node != nullptr && coverListLength < k_maxChain)
			{
				coverListLength++;
				node = node->NextCoverLink;
			}
		}

		int navListLength = 0;
		{
			auto* node = wi->NavigationPointList;
			while (node != nullptr && navListLength < k_maxChain)
			{
				navListLength++;
				node = node->nextNavigationPoint;
			}
		}

		LEASI_INFO("WorldInfo.CoverList: {} link(s). NavigationPointList: {} point(s).",
			coverListLength, navListLength);

		auto isInCoverList = [&](ACoverLink* link)
		{
			auto* node = wi->CoverList;
			for (int i = 0; node != nullptr && i < k_maxChain; i++, node = node->NextCoverLink)
			{
				if (node == link) { return true; }
			}
			return false;
		};

		auto isInNavList = [&](ANavigationPoint* point)
		{
			auto* node = wi->NavigationPointList;
			for (int i = 0; node != nullptr && i < k_maxChain; i++,
				node = node->nextNavigationPoint)
			{
				if (node == point) { return true; }
			}
			return false;
		};

		for (auto* link : g_SpawnedLinks)
		{
			if (link == nullptr || ::Common::IsDestroyed(link))
			{
				continue;
			}

			const int slots = static_cast<int>(link->Slots.Count());
			LEASI_INFO("--- {} ({} slots) ---", PathOf(link), slots);
			LEASI_INFO("  in CoverList: {}   bDisabled: {}   bPlayerOnly: {}   IsEnabled(): {}"
				"   CollisionType: {}",
				isInCoverList(link) ? "YES" : "NO", link->bDisabled ? 1 : 0,
				link->bPlayerOnly ? 1 : 0, link->IsEnabled() ? 1 : 0,
				static_cast<int>(link->CollisionType));

			// ACoverLink::FindSlots is the engine's own "which two slots bracket this point"
			// search -- the same one the player's cover acquisition ends up in once it has a
			// link.
			//
			// It BREAKS ON THE FIRST PAIR that passes, in slot order, so the MaxDistance you
			// hand it decides the answer. A generous one is worse than useless: 1024u on an
			// L-shaped link returned slots 1/2 for a player standing next to slot 5, because
			// the far leg of the L was still "within range". Sweep from tight to loose instead,
			// and report the nearest slot as ground truth.
			int nearestSlot = -1;
			float nearestDist = 0.0f;
			for (int i = 0; i < slots; i++)
			{
				const FVector slotLoc = link->GetSlotLocation(i, FALSE);
				const float dx = slotLoc.X - playerLoc.X;
				const float dy = slotLoc.Y - playerLoc.Y;
				const float dz = slotLoc.Z - playerLoc.Z;
				const float d = std::sqrt(dx * dx + dy * dy + dz * dz);
				if (nearestSlot < 0 || d < nearestDist) { nearestSlot = i; nearestDist = d; }
			}
			LEASI_INFO("  nearest slot to the player: {} at {:.0f}u", nearestSlot, nearestDist);

			for (const float maxDist : { 64.0f, 128.0f, 256.0f, 1024.0f })
			{
				int leftSlot = -1, rightSlot = -1;
				const bool bFound = link->FindSlots(playerLoc, maxDist, &leftSlot, &rightSlot);
				LEASI_INFO("  FindSlots(player, {:.0f}u) -> {} (left {}, right {}){}",
					maxDist, bFound ? "FOUND" : "nothing", leftSlot, rightSlot,
					(bFound && nearestSlot >= 0 &&
						leftSlot != nearestSlot && rightSlot != nearestSlot)
						? "  <-- does NOT bracket the nearest slot" : "");
			}

			int markers = 0, markersInNavList = 0, blockedMarkers = 0;
			int totalSpecs = 0, markersWithNoSpecs = 0;
			int markersNotInOctree = 0, markersWrongOwnerType = 0;
			for (int i = 0; i < slots; i++)
			{
				auto* const marker = link->Slots(i).SlotMarker;
				if (marker == nullptr)
				{
					continue;
				}
				markers++;
				if (isInNavList(marker)) { markersInNavList++; }
				if (marker->bBlocked) { blockedMarkers++; }

				// Markers are joined to each other left-to-right by USlotToSlotReachSpecs, and
				// to mantle/slip/turn destinations by the other spec classes. Nothing we do at
				// runtime creates any of them: the whole path builder is editor-only, and only
				// GetReachSpecClass and CanPrunePath survive on ACoverSlotMarker. A
				// level-loaded marker has them because they were serialised.
				const int specs = static_cast<int>(marker->PathList.Count());
				totalSpecs += specs;
				if (specs == 0) { markersWithNoSpecs++; }

				// THE gate. BioPlayerController::FindPlayerCover (0x7FF74B416F30) does not walk
				// CoverList at all -- it runs NavigationOctreeRadiusCheck around the pawn and,
				// for every hit whose OwnerType has bit 0x04, reads OwningSlot off the owner as
				// a CoverSlotMarker. A marker whose NavOctreeObject.OctreeNode is null is not
				// in the octree, so the search cannot see it however good everything else is.
				if (marker->NavOctreeObject.OctreeNode == nullptr) { markersNotInOctree++; }
				if ((marker->NavOctreeObject.OwnerType & 0x04) == 0) { markersWrongOwnerType++; }

				if (i == 0)
				{
					const FVector loc = marker->LOCATION;
					LEASI_INFO("  marker 0: {} @ ({:.1f}, {:.1f}, {:.1f}) CollisionType {} "
						"bCollideActors {} bBlockActors {} OwningSlot {}/{} PathList {}",
						PathOf(marker), loc.X, loc.Y, loc.Z,
						static_cast<int>(marker->CollisionType),
						marker->bCollideActors ? 1 : 0, marker->bBlockActors ? 1 : 0,
						marker->OwningSlot.Link == link ? "link ok" : "LINK WRONG",
						marker->OwningSlot.SlotIdx, specs);
				}
			}

			LEASI_INFO("  markers: {} of {} slots, {} in NavigationPointList, {} bBlocked.",
				markers, slots, markersInNavList, blockedMarkers);
			LEASI_INFO("  ReachSpecs across those markers: {} total, {} marker(s) with NONE.",
				totalSpecs, markersWithNoSpecs);
			LEASI_INFO("  NAV OCTREE: {} marker(s) NOT in it, {} with the wrong OwnerType. "
				"(marker 0: OctreeNode={}, Owner={}, OwnerType=0x{:02X})",
				markersNotInOctree, markersWrongOwnerType,
				(markers > 0 && link->Slots(0).SlotMarker != nullptr &&
					link->Slots(0).SlotMarker->NavOctreeObject.OctreeNode != nullptr)
					? "set" : "NULL",
				(markers > 0 && link->Slots(0).SlotMarker != nullptr &&
					link->Slots(0).SlotMarker->NavOctreeObject.Owner ==
						reinterpret_cast<UObject*>(link->Slots(0).SlotMarker))
					? "self" : "not self",
				(markers > 0 && link->Slots(0).SlotMarker != nullptr)
					? link->Slots(0).SlotMarker->NavOctreeObject.OwnerType : 0);
		}

		// Run this in a shipped level too: the nearest BioWare link is the reference, and the
		// only honest way to tell which of these fields actually matters is to read one that
		// works.
		{
			float shippedDistance = 0.0f;
			auto* const nearest = FindNearestCoverLink(shippedDistance);
			const bool bOurs = nearest != nullptr &&
				std::find(g_SpawnedLinks.begin(), g_SpawnedLinks.end(), nearest)
					!= g_SpawnedLinks.end();
			if (nearest != nullptr && !bOurs)
			{
				const int slots = static_cast<int>(nearest->Slots.Count());
				int markers = 0, specs = 0;
				for (int i = 0; i < slots; i++)
				{
					if (auto* const marker = nearest->Slots(i).SlotMarker)
					{
						markers++;
						specs += static_cast<int>(marker->PathList.Count());
					}
				}
				LEASI_INFO("--- nearest NON-spawned link, for comparison: {} ({:.0f}u) ---",
					PathOf(nearest), shippedDistance);
				LEASI_INFO("  {} slots, {} markers, {} ReachSpecs, in CoverList: {}",
					slots, markers, specs, isInCoverList(nearest) ? "YES" : "NO");

				for (int i = 0; i < slots; i++)
				{
					auto* const marker = nearest->Slots(i).SlotMarker;
					if (marker == nullptr) { continue; }
					LEASI_INFO("  shipped marker {}: OctreeNode={} Owner={} OwnerType=0x{:02X} "
						"PathList={}", i,
						marker->NavOctreeObject.OctreeNode != nullptr ? "set" : "NULL",
						marker->NavOctreeObject.Owner == reinterpret_cast<UObject*>(marker)
							? "self" : "not self",
						marker->NavOctreeObject.OwnerType,
						static_cast<int>(marker->PathList.Count()));
					break;
				}

				// The reference values. We build specs with guessed reachFlags and no octree
				// entry; this reads what BioWare's actually carry so those guesses can be
				// replaced with facts.
				for (int i = 0; i < slots; i++)
				{
					auto* const marker = nearest->Slots(i).SlotMarker;
					if (marker == nullptr || marker->PathList.Count() == 0)
					{
						continue;
					}
					const int show = std::min(3, static_cast<int>(marker->PathList.Count()));
					for (int j = 0; j < show; j++)
					{
						auto* const spec = marker->PathList(j);
						if (spec == nullptr) { continue; }
						LEASI_INFO("  shipped spec [{}][{}]: {} dist={} radius={} height={} "
							"reachFlags=0x{:X} bAddToOctree={} bDisabled={} Start={} End={}",
							i, j, PathOf(spec->Class), spec->Distance, spec->CollisionRadius,
							spec->CollisionHeight, spec->reachFlags,
							spec->bAddToNavigationOctree ? 1 : 0, spec->bDisabled ? 1 : 0,
							spec->Start != nullptr ? PathOf(spec->Start) : "<null>",
							spec->End.Actor != nullptr
								? PathOf(reinterpret_cast<UObject*>(spec->End.Actor)) : "<null>");
					}
					break;
				}
			}
		}

		// The same query FindPlayerCover runs, through the engine's own entry point. If our
		// markers are absent from this, the cover search cannot possibly see them.
		{
			TArray<ANavigationPoint*> navList{};
			const FCylinder minSize{ 0.0f, 0.0f };
			// Declared on ANavigationPoint and static in script, so the CDO is a valid receiver.
			auto* const navCls = ANavigationPoint::StaticClass();
			auto* const navCDO = (navCls != nullptr)
				? static_cast<ANavigationPoint*>(navCls->ClassDefaultObject) : nullptr;
			if (navCDO != nullptr)
			{
				navCDO->GetAllNavInRadius(pc->Pawn, playerLoc, 1024.0f, FALSE, -1,
					minSize, &navList);
			}

			int ours = 0;
			for (int i = 0; i < static_cast<int>(navList.Count()); i++)
			{
				auto* const point = navList(i);
				for (auto* const link : g_SpawnedLinks)
				{
					if (link == nullptr || ::Common::IsDestroyed(link)) { continue; }
					for (int j = 0; j < static_cast<int>(link->Slots.Count()); j++)
					{
						if (reinterpret_cast<ANavigationPoint*>(link->Slots(j).SlotMarker) == point)
						{
							ours++;
						}
					}
				}
			}
			LEASI_INFO("GetAllNavInRadius(player, 1024u): {} nav point(s) returned, {} of them "
				"ours. This is the octree query FindPlayerCover runs.",
				static_cast<int>(navList.Count()), ours);
		}

		LEASI_INFO("Player at ({:.1f}, {:.1f}, {:.1f}).",
			playerLoc.X, playerLoc.Y, playerLoc.Z);
		LEASI_INFO("Reading: a tight FindSlots that brackets the nearest slot means the slots "
			"are fine and the problem is further up. Use cc.dumplink here and again on shipped "
			"cover in a shipped level, and diff the two.");
	}

	// ! cc.testbuildslotinfo
	// ========================================

	void CmdTestBuildSlotInfo()
	{
		LEASI_INFO("=== cc.testbuildslotinfo ===");
		LEASI_WARN("This MUTATES the live level's cover data (slot flags, MantleTarget, "
			"SlipTarget/SlipRefs, turn targets). Reload the level when you are done testing.");

		float distance = 0.0f;
		ACoverLink* const nearest = FindNearestCoverLink(distance);
		if (nearest == nullptr)
		{
			return;
		}

		const int slotCount = static_cast<int>(nearest->Slots.Count());
		LEASI_INFO("Target: {} ({} slots, {:.0f}u away)", PathOf(nearest), slotCount, distance);

		LEASI_INFO("--- state before ---");
		std::vector<SlotSnapshot> before;
		before.reserve(slotCount);
		for (int i = 0; i < slotCount; i++)
		{
			const FCoverSlot& slot = nearest->Slots(i);
			before.push_back(TakeSnapshot(slot));
			LogSlotState(nearest, i, slot);
		}

		ScopedScout guard{};
		if (!guard)
		{
			LEASI_ERROR("Could not acquire a scout; aborting before calling BuildSlotInfo.");
			return;
		}

		// BuildSlotInfo only runs for enabled slots when the engine calls it, so mirror that
		// rather than forcing it on slots the engine would never analyse.
		LEASI_INFO("--- running BuildSlotInfo (vtable slot {}) ---", k_vtableIndexBuildSlotInfo);
		int invoked = 0;
		for (int i = 0; i < slotCount; i++)
		{
			if ((ReadSlotFlags(nearest->Slots(i)) & 0x4000) == 0)
			{
				LEASI_INFO("  slot {}: skipped, bEnabled is FALSE", i);
				continue;
			}
			CallBuildSlotInfo(nearest, i);
			invoked++;
		}
		LEASI_INFO("BuildSlotInfo returned for {}/{} slots without crashing.", invoked, slotCount);

		LEASI_INFO("--- diff ---");
		int totalChanges = 0;
		for (int i = 0; i < slotCount; i++)
		{
			const SlotSnapshot after = TakeSnapshot(nearest->Slots(i));
			totalChanges += LogSnapshotDiff(i, before[i], after);
		}

		if (totalChanges == 0)
		{
			LEASI_INFO("BuildSlotInfo spike PASSED with an EXACT match across {} slots. "
				"ABioScout::CanDoMove reproduced BioWare's own mantle / climb / slip / turn "
				"analysis exactly.", slotCount);
		}
		else
		{
			LEASI_WARN("BuildSlotInfo ran cleanly but produced {} differences. Every field it "
				"touches is an analysis result, so unlike cc.testautoadjust there is no benign "
				"category here -- each difference means the in-game environment answered "
				"differently from the editor. Inspect the diff above.", totalChanges);
		}

		// A slot whose analysis flags all came back FALSE would produce an "exact match" against
		// shipped data that is also all FALSE, which looks like success but proves nothing. Say
		// plainly whether any special move was actually detected.
		int withMove = 0;
		for (int i = 0; i < slotCount; i++)
		{
			constexpr unsigned long k_specialMoveMask =
				0x00000010 | 0x00000020 |               // bCanMantle, bCanClimbUp
				0x00000100 | 0x00000200 |               // bCanCoverSlip_Left/Right
				0x00000400 | 0x00000800 |               // bCanSwatTurn_Left/Right
				0x00001000 | 0x00002000;                // bCanCoverTurn_Left/Right
			if ((ReadSlotFlags(nearest->Slots(i)) & k_specialMoveMask) != 0)
			{
				withMove++;
			}
		}
		if (withMove == 0)
		{
			LEASI_WARN("None of the {} slots came back with any special move set. That may simply "
				"be this cover's geometry, but it also means a matching diff proves little -- "
				"try a link next to something mantleable.", slotCount);
		}
		else
		{
			LEASI_INFO("{}/{} slots have at least one special move flag set.", withMove, slotCount);
		}
	}

	// ! cc.testautoadjust
	// ========================================

	void CmdTestAutoAdjust()
	{
		LEASI_INFO("=== cc.testautoadjust ===");

		float distance = 0.0f;
		ACoverLink* const nearest = FindNearestCoverLink(distance);
		if (nearest == nullptr)
		{
			return;
		}

		const int slotCount = static_cast<int>(nearest->Slots.Count());
		LEASI_INFO("Target: {} ({} slots, {:.0f}u away)", PathOf(nearest), slotCount, distance);
		LEASI_INFO("bAutoAdjust={} bCircular={} AlignDist={:.1f} StandHeight={:.1f} MidHeight={:.1f}",
			nearest->bAutoAdjust != 0, nearest->bCircular != 0,
			nearest->AlignDist, nearest->StandHeight, nearest->MidHeight);

		// Everything past the cover-type pass -- ground/orientation adjust, lean detection, and
		// the BuildSlotInfo call that drives ABioScout::CanDoMove -- sits behind this flag.
		// Shipped links usually have it clear, so without this warning an "exact match" here
		// reads as far stronger evidence than it is.
		if (nearest->bAutoAdjust == 0)
		{
			LEASI_WARN("This link has bAutoAdjust=FALSE, so AutoAdjustSlot will run ONLY the "
				"cover-type pass. Ground adjust, lean detection and BuildSlotInfo (and therefore "
				"the whole mantle / slip / turn analysis) are skipped. Use cc.testbuildslotinfo "
				"to exercise those.");
		}

		LEASI_INFO("--- state before ---");
		std::vector<SlotSnapshot> before;
		before.reserve(slotCount);
		for (int i = 0; i < slotCount; i++)
		{
			const FCoverSlot& slot = nearest->Slots(i);
			before.push_back(TakeSnapshot(slot));
			LogSlotState(nearest, i, slot);
		}

		ScopedScout guard{};
		if (!guard)
		{
			LEASI_ERROR("Could not acquire a scout; aborting before touching AutoAdjustSlot.");
			return;
		}

		// Same order the editor used in ACoverLink::AddCoverSlot: full pass, then lean-only.
		LEASI_INFO("--- running AutoAdjustSlot ---");
		for (int i = 0; i < slotCount; i++)
		{
			nearest->AutoAdjustSlot(i, FALSE);
			nearest->AutoAdjustSlot(i, TRUE);
		}

		LEASI_INFO("--- diff ---");
		int totalChanges = 0;
		for (int i = 0; i < slotCount; i++)
		{
			const SlotSnapshot after = TakeSnapshot(nearest->Slots(i));
			totalChanges += LogSnapshotDiff(i, before[i], after);
		}

		if (totalChanges == 0)
		{
			if (nearest->bAutoAdjust == 0)
			{
				LEASI_INFO("AutoAdjustSlot: exact match across {} slots -- but bAutoAdjust was "
					"FALSE, so this only proves the cover-type pass reproduces the shipped data. "
					"Mantle / climb / slip / swat turn / cover turn were NOT exercised; run "
					"cc.testbuildslotinfo for those.", slotCount);
			}
			else
			{
				LEASI_INFO("AutoAdjustSlot spike PASSED with an EXACT match: {} slots, nothing "
					"changed. The engine's own analysis reproduces the shipped data.", slotCount);
			}
		}
		else
		{
			LEASI_WARN("AutoAdjustSlot ran without crashing but produced {} differences across "
				"{} slots. Inspect the diff above -- location/rotation drift is expected "
				"(in-game collision differs from path-building collision); changed analysis "
				"flags are not.", totalChanges, slotCount);
		}
	}
}
