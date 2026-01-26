#pragma once

#include <LESDK/Headers.hpp>
#include "Common/Base.hpp"
#include "Common/Objects.hpp"
#include "Utilities.hpp"


namespace LEXInterop 
{
	// Additional global functions and utilities
	void InitializeAdditionalFunctions(::LESDK::Initializer& Init);

	enum class EObjectFlags : uint64_t
	{
		InSingularFunc = 0x0000000000000002, // In a singular function.
		StateChanged = 0x0000000000000004,   // Object did a state change.
		DebugPostLoad = 0x0000000000000008,   // For debugging Serialize calls.
		DebugSerialize = 0x0000000000000010,   // For debugging Serialize calls.
		DebugFinishDestroy = 0x0000000000000020,   // For debugging FinishDestroy calls.
		EdSelected = 0x0000000000000040,
		ZombieComponent = 0x0000000000000080,
		Protected = 0x0000000000000100, // Property can only be accessed by owning class or subclasses
		ClassDefaultObject = 0x0000000000000200, // this object is its class's default object
		ArchetypeObject = 0x0000000000000400, // this object is a template for another object - treat like a class default object
		ForceTagExp = 0x0000000000000800, //Force this object into the export table when saving
		TokenStreamAssembled = 0x0000000000001000,
		MisAlignedObject = 0x0000000000002000, // Object has desynced from c++ class (native classes, editor only)
		RootSet = 0x0000000000004000, // This object should not be garbage collected
		BeginDestroyed = 0x0000000000008000, // BeginDestroy has been called
		FinishDestroyed = 0x0000000000010000, // FinishDestroy has been called
		DebugBeginDestroyed = 0x0000000000020000, // If object is considered part of the root set (?)
		MarkedByCooker = 0x0000000000040000,
		LocalizedResource = 0x0000000000080000, // Resource object is localized
		InitializedProps = 0x0000000000100000, // have properties been initialized?
		PendingFieldPatches = 0x0000000000200000, // ScriptPatch system (not used in ME)
		IsCrossLevelReferenced = 0x0000000000400000, // This object has been pointed to by a cross-level reference, and therefore requires additional cleanup upon deletion

		Saved = 0x0000000080000000,
		Transactional = 0x0000000100000000,   // Object is transactional.
		Unreachable = 0x0000000200000000,   // Object is not reachable on the object graph.            
		Public = 0x0000000400000000, // Object is visible outside its package.
		TagImp = 0x0000000800000000, // Temporary import tag in load/save.
		TagExp = 0x0000001000000000, // Temporary export tag in load/save.
		Obsolete = 0x0000002000000000,   // Object marked as obsolete and should be replaced.
		TagGarbage = 0x0000004000000000, // Check during garbage collection.
		DisregardForGC = 0x0000008000000000,// Object is considered static // REPLACED Final = 0x0000008000000000,	// Object is not visible outside of class.
		PerObjectLocalized = 0x0000010000000000, // Object is localized by instance name, not by class.
		NeedLoad = 0x0000020000000000,   // During loading, indicates object needs loading.
		AsyncLoading = 0x0000040000000000, // Object is being async loaded
		NeedPostLoadSubobjects = 0x0000080000000000, // During load, Subobjects also need instanced
		Suppress = 0x0000100000000000,   //warning: Mirrored in UnName.h. Suppressed log name.
		InEndState = 0x0000200000000000,   // Within an EndState call.
		Transient = 0x0000400000000000,  // Don't save object.
		Cooked = 0x0000800000000000, // Content was cooked
		LoadForClient = 0x0001000000000000,  // In-file load for client.
		LoadForServer = 0x0002000000000000,  // In-file load for client.
		LoadForEdit = 0x0004000000000000,    // In-file load for client.
		Standalone = 0x0008000000000000,   // Keep object around for editing even if unreferenced.
		NotForClient = 0x0010000000000000,   // Don't load this object for the game client.
		NotForServer = 0x0020000000000000,   // Don't load this object for the game server.
		NotForEdit = 0x0040000000000000, // Don't load this object for the editor.
		// There is nothing in this slot.
		NeedPostLoad = 0x0100000000000000,   // Object needs to be postloaded.
		HasStack = 0x0200000000000000,   // Has execution stack.
		Native = 0x0400000000000000,   // Native (UClass only).
		Marked = 0x0800000000000000,   // Marked (for debugging).
		ErrorShutdown = 0x1000000000000000, // ShutdownAfterError called.
		PendingKill = 0x2000000000000000 // Object is pending destruction
	};


	extern void* moveActorMoveableCheckAddr;

	class ScopedMoveActorEnable
	{
		BYTE originalByte[1];
    public:
		ScopedMoveActorEnable()
		{
			originalByte[0] = ((BYTE*)moveActorMoveableCheckAddr)[0];
			constexpr BYTE jumpZero[] = { 0x0 }; // set JNZ to "jump" zero bytes
			PatchMemory(moveActorMoveableCheckAddr, jumpZero, 1);
		}
		~ScopedMoveActorEnable()
		{
			PatchMemory(moveActorMoveableCheckAddr, originalByte, 1);
		}
	};

	// Typedefs
#if defined(SDK_TARGET_LE1) || defined(SDK_TARGET_LE2)
	using t_FarMoveActor = BOOL(UWorld* world, AActor* actor, FVector& destPos, BOOL test, BOOL noCollisionCheck, BOOL attachMove);
#elif defined(SDK_TARGET_LE3)
	using t_FarMoveActor = BOOL(UWorld* world, AActor* actor, FVector& destPos, BOOL test, BOOL noCollisionCheck, BOOL attachMove, BOOL unknown);
#endif
	extern t_FarMoveActor* FarMoveActor;

	using t_StaticConstructObject = UObject * (UClass* InClass, UObject* InOuter, SFXName InName, EObjectFlags InFlags, UObject* InTemplate, void* Error, UObject* SubObjectRoot, void* InInstanceGraph);
	extern t_StaticConstructObject* static_construct_object;


	template<typename T>
	T* StaticConstructObject(UObject* InOuter, SFXName InName, EObjectFlags InFlags = static_cast<EObjectFlags>(0), UObject* InTemplate = nullptr, UObject* SubObjectRoot = nullptr)
	{
		if (!static_construct_object)
		{
			LEASI_ERROR("static_construct_object is not initialized!");
			return nullptr;
		}
		return static_cast<T*>(static_construct_object(T::StaticClass(), InOuter, InName, InFlags, InTemplate, *GError, SubObjectRoot, nullptr));
	}

	BOOL FarMove(AActor* actor, FVector& destPos, const BOOL test, const BOOL noCollisionCheck, const BOOL attachMove);

	void DrawDebugLine(const FVector& start, const FVector& end, const FLinearColor& color, const float thickness = 1.f, const bool persistent = true);

	void DrawCoordinateSystem(const FVector& position, const float scale, const float thickness = 1.f);
}