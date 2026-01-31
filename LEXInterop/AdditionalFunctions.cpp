#pragma once

#include "AdditionalFunctions.hpp"
#include "Utilities.hpp"
#include "Common/Utils.hpp"

namespace LEXInterop 
{
	t_FarMoveActor* FarMoveActor = nullptr;
	t_StaticConstructObject* static_construct_object = nullptr;
	void* moveActorMoveableCheckAddr = nullptr;

	void InitializeAdditionalFunctions(::LESDK::Initializer& Init)
	{
		static_construct_object = Init.ResolveTyped<t_StaticConstructObject>(BUILTIN_STATICCONSTRUCTOBJECT_PHOOK);
		CHECK_RESOLVED(static_construct_object);

		FarMoveActor = Init.ResolveTyped<t_FarMoveActor>(BUILTIN_UWORLD_FARMOVEACTOR_RVA);
		CHECK_RESOLVED(FarMoveActor);

		//This disables MoveActor's check for bStatic and bMoveable, so that we can rotate anything
#if defined(SDK_TARGET_LE1) 
		auto moveActorMoveableCheck = ::LESDK::Address::FromPattern("e0 4c 8b 8d 18 04 00 00 48 8b b5 28 04 00 00");
#elif defined(SDK_TARGET_LE2)
		auto moveActorMoveableCheck = ::LESDK::Address::FromPattern("dc 4c 8b 8d 68 04 00 00 48 8b b5 78 04 00 00");
#elif defined(SDK_TARGET_LE3)
		auto moveActorMoveableCheck = ::LESDK::Address::FromPattern("dc 4c 8b 8d 68 04 00 00 48 8b b5 78 04 00 00");
#endif
		moveActorMoveableCheckAddr = Init.ResolveTyped<void*>(moveActorMoveableCheck);
		CHECK_RESOLVED(moveActorMoveableCheckAddr);
	}

	BOOL FarMove(AActor* actor, FVector& destPos, const BOOL test, const BOOL noCollisionCheck, const BOOL attachMove)
	{
		if (!FarMoveActor)
		{
			LEASI_ERROR("FarMove is not initialized!");
			return false;
		}
		auto firstOffset = (void*)((intptr_t)FarMoveActor + 40);
		BYTE originalFirstByte[1];
		originalFirstByte[0] = ((BYTE*)firstOffset)[0];

		auto secondOffset = (void*)((intptr_t)FarMoveActor + 51);
		BYTE originalSecondByte[1];
		originalSecondByte[0] = ((BYTE*)secondOffset)[0];

		// Patch FarMoveActor to skip bStatic and bMovable checks
		constexpr BYTE relOffsetChange[] = { 0x26 }; // REL OFFSET (Same in all 3 games)
		// Change JNZ jump offset to point to location test code (post checks)
		PatchMemory(firstOffset, relOffsetChange, 1);
		// Not sure if this is actually required but here to ensure the other jump can't occur
		constexpr BYTE jumpInstructionChange[] = { 0xEB }; // JMP NEAR
		// Change JNE to JMP when testing bStatic/bMovable
		PatchMemory(secondOffset, jumpInstructionChange, 1);
		
		bool retVal = FarMoveActor(*GWorld, actor, destPos, test, noCollisionCheck, attachMove
#if defined(SDK_TARGET_LE3)
			, 0
#endif
		);

		// Restore original code
		PatchMemory(firstOffset, originalFirstByte, 1);
		PatchMemory(secondOffset, originalSecondByte, 1);

		return retVal;
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