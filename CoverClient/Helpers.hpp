#pragma once
#include <LESDK/Headers.hpp>
#include <string>

namespace CoverClient
{
	// Sets the vtable of an object to a new vtable. 
	void SetVTable(void** objectStartAddress, void* newVTableStartAddress);

	// Gets the vtable of an object.
	void* GetVTable(void*** objectStartAddress);

	// Converts a std::wstring to a std::string.
	std::string ws2s(const std::wstring& wstr);

	// Converts a std::string to a std::wstring.
	std::wstring s2ws(const std::string& str);

	using tStaticConstructObject = UObject* (UClass* InClass, UObject* InOuter, SFXName InName, EObjectFlags InFlags, UObject* InTemplate, void* Error, UObject* SubObjectRoot, void* InInstanceGraph);
	extern tStaticConstructObject* static_construct_object;

	// ! GWorld::SingleLineCheck hook for firing traces, the versions exposed to UnrealScript are insufficient
	using tSingleLineCheck = bool(UWorld* self, FCheckResult* hitInfo, AActor* actor, FVector* end, FVector* start, DWORD traceFlags, FVector* extent, ULightComponent* unknown);
	extern tSingleLineCheck* SingleLineCheck;

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
}