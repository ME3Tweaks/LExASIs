#pragma once

#include "AdditionalFunctions.hpp"
#include "Utilities.hpp"
#include "Common/Utils.hpp"

namespace LEXInterop 
{
	t_FarMoveActor* FarMoveActor = nullptr;
	t_StaticConstructObject* static_construct_object = nullptr;

	void InitializeAdditionalFunctions(::LESDK::Initializer& Init)
	{
		static_construct_object = Init.ResolveTyped<t_StaticConstructObject>(BUILTIN_STATICCONSTRUCTOBJECT_PHOOK);
		CHECK_RESOLVED(static_construct_object);


#if defined(SDK_TARGET_LE1) || defined(SDK_TARGET_LE2)
		auto farMoveHookAddress = ::LESDK::Address::FromPostHook(/*"40 55 53 57 41*/ "54 41 56 48 8d 6c 24 d9 48 81 ec a0 00 00 00");
#elif defined(SDK_TARGET_LE3)
		auto farMoveHookAddress = ::LESDK::Address::FromPostHook(/*"40 55 53 57 41*/ "54 41 57 48 8d 6c 24 e1 48 81 ec b0 00 00 00");
#endif
		FarMoveActor = Init.ResolveTyped<t_FarMoveActor>(farMoveHookAddress);
		CHECK_RESOLVED(FarMoveActor);

		// Patch FarMoveActor to skip bStatic and bMovable checks
		constexpr BYTE relOffsetChange[] = { 0x26 }; // REL OFFSET (Same in all 3 games)
		// Change JNZ jump offset to point to location test code (post checks)
		PatchMemory((void*)((intptr_t)FarMoveActor + 40), relOffsetChange, 1);
		// Not sure if this is actually required but here to ensure the other jump can't occur
		constexpr BYTE jumpInstructionChange[] = { 0xEB }; // JMP NEAR
		// Change JNE to JMP when testing bStatic/bMovable
		PatchMemory((void*)((intptr_t)FarMoveActor + 51), jumpInstructionChange, 1);

		//This disables MoveActor's check for bStatic and bMoveable, so that we can rotate anything
#if defined(SDK_TARGET_LE1) 
		auto moveActorMoveableCheck = ::LESDK::Address::FromPattern("e0 4c 8b 8d 18 04 00 00 48 8b b5 28 04 00 00");
#elif defined(SDK_TARGET_LE2)
		auto moveActorMoveableCheck = ::LESDK::Address::FromPattern("dc 4c 8b 8d 68 04 00 00 48 8b b5 78 04 00 00");
#elif defined(SDK_TARGET_LE3)
		auto moveActorMoveableCheck = ::LESDK::Address::FromPattern("dc 4c 8b 8d 68 04 00 00 48 8b b5 78 04 00 00");
#endif
		constexpr BYTE jumpZero[] = { 0x0 }; // set JNZ to "jump" zero bytes
		PatchMemory(Init.ResolveTyped<void*>(moveActorMoveableCheck), jumpZero, 1);
	}

	BOOL FarMove(AActor* actor, FVector& destPos, const BOOL test, const BOOL noCollisionCheck, const BOOL attachMove)
	{
		if (!FarMoveActor)
		{
			LEASI_ERROR("FarMove is not initialized!");
			return false;
		}
		return FarMoveActor(*GWorld, actor, destPos, test, noCollisionCheck, attachMove
#if defined(SDK_TARGET_LE3)
			, 0
#endif
		);
	}

	void DrawDebugLine(const FVector& start, const FVector& end, const FLinearColor& color, const float thickness, const bool persistent)
	{
		ULineBatchComponent* lineBatcher = persistent ? (*GWorld)->PersistentLineBatcher : (*GWorld)->LineBatcher;
		lineBatcher->FPrimitiveDrawInterfaceVfTable->DrawLine(reinterpret_cast<BYTE*>(lineBatcher) + offsetof(ULineBatchComponent, FPrimitiveDrawInterfaceVfTable), start, end, color, 1, thickness);
	}

	void DrawCoordinateSystem(const FVector& position, const float scale, const float thickness)
	{
		ULineBatchComponent* lineBatcher = (*GWorld)->PersistentLineBatcher;
		const auto DrawLine = lineBatcher->FPrimitiveDrawInterfaceVfTable->DrawLine;
		const auto self = reinterpret_cast<BYTE*>(lineBatcher) + offsetof(ULineBatchComponent, FPrimitiveDrawInterfaceVfTable);
		DrawLine(self, position, position + FVector{ scale, 0.f, 0.f }, FLinearColor{ 1, 0, 0, 1 }, 1, thickness);
		DrawLine(self, position, position + FVector{ 0.f, scale, 0.f }, FLinearColor{ 0, 1, 0, 1 }, 1, thickness);
		DrawLine(self, position, position + FVector{ 0.f, 0.f, scale }, FLinearColor{ 0, 0, 1, 1 }, 1, thickness);
	}
}