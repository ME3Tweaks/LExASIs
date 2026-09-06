#include "CoverClient/CoverExport.hpp"
#include "CoverClient/CoverGeneration.hpp"
#include "CoverClient/ScoutHarness.hpp"
#include "CoverClient/Helpers.hpp"
#include "CoverClient/Classes/CoverLink.hpp"
#include "CoverClient/Classes/CoverSlotMarker.hpp"
#include "CoverClient/Utilities/PathUtilities.hpp"
#include "Common/Base.hpp"

#include <cmath>
#include <set>
#include <string>

namespace CoverClient
{
	namespace
	{
		/// Same one-liner ScoutHarness keeps privately; duplicated rather than exported
		/// because two callers is not yet a reason to give it a home.
		std::string PathOf(UObject* object)
		{
			if (object == nullptr) { return std::string{ "<null>" }; }
			const wchar_t* const data = *object->GetFullPath();
			return (data != nullptr) ? ws2s(data) : std::string{};
		}

		/// The ACoverLinks already imported, so a second cc.importlinks after a second sweep
		/// adds only what is new instead of duplicating everything.
		std::set<ACoverLink*> g_ImportedLinks;

		/// ECoverAction values. Only the three a slot can fire from are derived here; the full
		/// enum is in CoverLink.cpp where the host wire format names them.
		constexpr unsigned char k_caLeanLeft = 3;
		constexpr unsigned char k_caLeanRight = 4;
		constexpr unsigned char k_caPopUp = 5;

		/// FCoverSlot keeps its two dozen booleans in one bitfield word at +0xDC. Copying the
		/// word is exact and cannot miss a flag the way transcribing twenty-five assignments
		/// can -- and this struct has already cost us one bug from a mis-transcribed field.
		constexpr int k_slotFlagsOffset = 0xDC;

		unsigned long ReadSlotFlags(const FCoverSlot& slot)
		{
			return *reinterpret_cast<const unsigned long*>(
				reinterpret_cast<const unsigned char*>(&slot) + k_slotFlagsOffset);
		}

		void WriteSlotFlags(FCoverSlot& slot, unsigned long flags)
		{
			*reinterpret_cast<unsigned long*>(
				reinterpret_cast<unsigned char*>(&slot) + k_slotFlagsOffset) = flags;
		}

		/// Copies the parts of an engine slot the host needs, into a slot we own.
		///
		/// Deliberately field-by-field rather than a struct assignment: FCoverSlot holds seven
		/// TArrays, and a shallow copy would leave both slots pointing at the same Data
		/// allocation with both believing they own it.
		void CopySlotForExport(const FCoverSlot& src, FCoverSlot& dst)
		{
			dst.LocationOffset = src.LocationOffset;
			dst.RotationOffset = src.RotationOffset;
			dst.MantleTarget = src.MantleTarget;
			dst.TurnTargetPackedProperties = src.TurnTargetPackedProperties;
			dst.CoverTurnTargetPackedProperties = src.CoverTurnTargetPackedProperties;
			dst.ExtraCost = src.ExtraCost;
			dst.LeanTraceDist = src.LeanTraceDist;
			dst.ForceCoverType = src.ForceCoverType;
			dst.CoverType = src.CoverType;
			dst.LocationDescription = src.LocationDescription;
			WriteSlotFlags(dst, ReadSlotFlags(src));

			// Actions has to be DERIVED, not copied. The 32768u sweep settled this: all 1826
			// imported slots arrived with an empty array, because LE3 stripped GetFireActions
			// and BuildFireLinks (note 02) and nothing else in the shipped binary fills it.
			// Copying an empty array is not neutral either -- it wipes the CA_PopUp that
			// InitializeSlot puts there, so importing made the slot worse than not importing.
			//
			// The source-action list from GetFireActions (UnCover.cpp:2066): the actions a
			// slot can FIRE from, one per enabled firing pose. CA_Default belongs only to the
			// destination list, and FireLinks adds it there, so it is not added here.
			//
			// This matters twice over: BuildOtherLinks iterates Slot.Actions and takes the
			// first action that can see a nav point, so an empty array means no danger cover
			// at all; and the array is serialised to the host as enum names.
			dst.Actions.Clear();
			if (src.bLeanLeft != 0)  { dst.Actions.Add(k_caLeanLeft); }
			if (src.bLeanRight != 0) { dst.Actions.Add(k_caLeanRight); }
			if (src.bCanPopUp != 0)  { dst.Actions.Add(k_caPopUp); }
			if (src.Actions.Count() > 0)
			{
				// If a future LE3 path ever does populate this, prefer the engine's own answer
				// over ours rather than silently disagreeing with it.
				dst.Actions.Clear();
				const int actionCount = static_cast<int>(src.Actions.Count());
				for (int i = 0; i < actionCount; i++)
				{
					dst.Actions.Add(src.Actions(i));
				}
			}

			// ExposedCoverPackedProperties and DangerCoverPackedProperties are ours to fill,
			// not the engine's to hand over: LE3 stripped the producers (note 02), so they are
			// always empty on a link we generated. Left empty for the compute passes.
			dst.ExposedCoverPackedProperties.Clear();
			dst.DangerCoverPackedProperties.Clear();
		}
	}

	void CmdImportLinks()
	{
		LEASI_INFO("=== cc.importlinks ===");

		const auto& spawned = GetSpawnedLinks();
		if (spawned.empty())
		{
			LEASI_ERROR("No links to import. Run cc.spawnlink or cc.sweep first.");
			return;
		}

		int importedLinks = 0, importedSlots = 0, skippedLinks = 0, skippedSlots = 0;
		int slotsWithNoActions = 0, offsetMismatches = 0;
		int totalActions = 0;

		for (auto* const link : spawned)
		{
			if (link == nullptr)
			{
				continue;
			}
			if (g_ImportedLinks.find(link) != g_ImportedLinks.end())
			{
				skippedLinks++;
				continue;
			}

			const int slotCount = static_cast<int>(link->Slots.Count());
			if (slotCount == 0)
			{
				LEASI_WARN("{} has no slots; not imported.", PathOf(link));
				skippedLinks++;
				continue;
			}

			auto modelLink = std::make_unique<CoverLink>(link->location, link->Rotation);

			for (int i = 0; i < slotCount; i++)
			{
				const FCoverSlot& engineSlot = link->Slots(i);

				// A disabled slot is not cover -- AutoAdjustSlot cleared bEnabled because it
				// found no wall -- and shipping one would put dead cover in the package.
				if (engineSlot.bEnabled == 0)
				{
					skippedSlots++;
					continue;
				}

				const FVector slotLoc = link->GetSlotLocation(i, FALSE);
				const FRotator slotRot = link->GetSlotRotation(i, FALSE);

				// Slots are pushed BEFORE their owning link, and the link writes each slot's
				// networkIndex. CoverHost resolves the reference by index, so a link that
				// serialised before its slots would point at nodes that do not exist yet.
				allNetworkNodes.push_back(std::make_unique<CoverSlotMarker>(slotLoc, slotRot));
				auto* const marker =
					static_cast<CoverSlotMarker*>(allNetworkNodes.back().get());
				marker->networkIndex = static_cast<int>(allNetworkNodes.size()) - 1;

				// InitializeSlot already gave us an owned slot; fill it from the engine's.
				CopySlotForExport(engineSlot, *marker->slot);

				const int actionCount = static_cast<int>(marker->slot->Actions.Count());
				totalActions += actionCount;
				if (actionCount == 0) { slotsWithNoActions++; }

				// The wire format recomputes LocationOffset from the link and slot world
				// transforms at send time. The engine already worked one out; if the two
				// disagree, one of the two is wrong and the package would not match what is
				// in the level. Cheap to check here, invisible everywhere else.
				const FVector recomputed =
					CalculateLocationOffset(link->location, link->Rotation, slotLoc);
				const float dx = recomputed.X - engineSlot.LocationOffset.X;
				const float dy = recomputed.Y - engineSlot.LocationOffset.Y;
				const float dz = recomputed.Z - engineSlot.LocationOffset.Z;
				if (std::sqrt(dx * dx + dy * dy + dz * dz) > 1.0f)
				{
					if (offsetMismatches == 0)
					{
						LEASI_WARN("{} slot {}: our LocationOffset ({:.1f}, {:.1f}, {:.1f}) "
							"disagrees with the engine's ({:.1f}, {:.1f}, {:.1f}). The host "
							"would receive a slot in the wrong place.",
							PathOf(link), i, recomputed.X, recomputed.Y, recomputed.Z,
							engineSlot.LocationOffset.X, engineSlot.LocationOffset.Y,
							engineSlot.LocationOffset.Z);
					}
					offsetMismatches++;
				}

				marker->SizeNode();
				marker->ComputeApproximateLineOfFire();
				modelLink->AddCoverSlot(marker);
				importedSlots++;
			}

			modelLink->SizeNode();
			modelLink->ComputeApproximateLineOfFire();

			allNetworkNodes.push_back(std::move(modelLink));
			auto* const stored = static_cast<CoverLink*>(allNetworkNodes.back().get());
			stored->networkIndex = static_cast<int>(allNetworkNodes.size()) - 1;
			coverLinks.push_back(stored);

			g_ImportedLinks.insert(link);
			importedLinks++;
		}

		LEASI_INFO("Imported {} link(s) and {} slot(s). Skipped {} link(s) already imported "
			"and {} disabled slot(s).",
			importedLinks, importedSlots, skippedLinks, skippedSlots);
		LEASI_INFO("allNetworkNodes now holds {} node(s); coverLinks holds {}.",
			static_cast<int>(allNetworkNodes.size()), static_cast<int>(coverLinks.size()));

		if (importedSlots > 0)
		{
			LEASI_INFO("Slot Actions: {} across {} slot(s), {} with none.",
				totalActions, importedSlots, slotsWithNoActions);
		}
		if (slotsWithNoActions > 0)
		{
			// Actions are derived from the lean and pop-up flags, so a slot with none has
			// none of those flags -- it is a mid-run slot that cannot fire from cover at all.
			// A few of those is normal; most of them is a BuildSlotInfo problem, not an
			// Actions problem.
			LEASI_WARN("{} of {} slot(s) have no firing action, because they have no lean and "
				"no pop-up. They will produce no fire links and no danger cover.",
				slotsWithNoActions, importedSlots);
		}
		if (offsetMismatches > 0)
		{
			LEASI_ERROR("{} slot(s) had a LocationOffset our maths and the engine's disagree "
				"about. Do not send this to the host until that is resolved.",
				offsetMismatches);
		}

		LEASI_INFO("Next: cc.computefirelinks / cc.computeexposedcover run on these now. "
			"cc.computedangernavs needs path nodes in the world first.");
	}
}
