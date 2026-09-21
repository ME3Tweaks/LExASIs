#include "CoverClient/NavRuntime.hpp"
#include "CoverClient/Helpers.hpp"

#include <cmath>
#include <cstring>

namespace CoverClient
{
	ScopedSpawnableNav::ScopedSpawnableNav(UClass* Class)
	{
		if (Class == nullptr) { return; }
		Defaults = static_cast<ANavigationPoint*>(Class->ClassDefaultObject);
		if (Defaults == nullptr) { return; }
		SavedStatic = Defaults->bStatic;
		SavedNoDelete = Defaults->bNoDelete;
		Defaults->bStatic = 0;
		Defaults->bNoDelete = 0;
	}

	ScopedSpawnableNav::~ScopedSpawnableNav()
	{
		if (Defaults == nullptr) { return; }
		Defaults->bStatic = SavedStatic;
		Defaults->bNoDelete = SavedNoDelete;
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

	UReachSpec* CreateSpec(UClass* specClass, ANavigationPoint* start, ANavigationPoint* end,
		float radius, float height, int reachFlags)
	{
		if (specClass == nullptr || start == nullptr || end == nullptr || start == end)
		{
			return nullptr;
		}

		// ReachSpecs are outered to the level, the same as the nav points they join.
		auto* const spec = static_cast<UReachSpec*>(StaticConstructObjectOfClass(
			specClass, start->Outer, SFXName{}, static_cast<EObjectFlags>(0)));
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
	// AddToNavigationOctree:
	//   - early-outs unless CylinderComponent (+0x314) is set and OctreeNode (+0x270) is null
	//   - FNavigationOctreeObject::SetOwner  -> Owner = this, OwnerType = 1 | 4 for a marker
	//   - SetBox from the cylinder's CollisionRadius/CollisionHeight around Location
	//   - FNavigationOctree::AddObject
	//   - then adds every ReachSpec in PathList

	namespace
	{
		constexpr size_t k_vtableIndexAddToNavigationOctree = 265;
		constexpr size_t k_vtableIndexRemoveFromNavigationOctree = 266;
	}

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
}
