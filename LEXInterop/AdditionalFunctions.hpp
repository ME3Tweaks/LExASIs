#pragma once

#include <LESDK/Headers.hpp>
#include "Common/Base.hpp"
#include "Common/Objects.hpp"
#include "Utilities.hpp"


namespace LEXInterop 
{
	// Additional global functions and utilities
	void InitializeAdditionalFunctions(::LESDK::Initializer& Init);

	


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