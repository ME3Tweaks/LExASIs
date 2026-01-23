#include "Common/Base.hpp"
#include "Hooks.hpp"
#include "DebugLogger/LE1Hooks.hpp"
#include "DebugLogger/LE2Hooks.hpp"
#include "DebugLogger/LE3Hooks.hpp"

namespace DebugLogger
{

	void InstallSharedHooks(::LESDK::Initializer& Init)
	{
		// Debug String Output
		// ----------------------------------------
		auto const outputDebugStringW_target = Init.ResolveTyped<tOutputDebugStringW>(::LESDK::Address::FromAbsolute(OutputDebugStringW));
		CHECK_RESOLVED(outputDebugStringW_target);
		OutputDebugStringW_orig = (tOutputDebugStringW*)Init.InstallHook("OutputDebugStringW", outputDebugStringW_target, OutputDebugStringW_hook);
		CHECK_RESOLVED(OutputDebugStringW_orig);

		/*
		// UObject::ProcessEvent hook for UnrealScript Activated() logging
		// ----------------------------------------
		auto const UObject_ProcessEvent_target = Init.ResolveTyped<t_UObject_ProcessEvent>(BUILTIN_PROCESSEVENT_PHOOK);
		CHECK_RESOLVED(UObject_ProcessEvent_target);
		UObject_ProcessEvent_orig = (t_UObject_ProcessEvent*)Init.InstallHook("UObject::ProcessEvent", UObject_ProcessEvent_target, UObject_ProcessEvent_hook);
		CHECK_RESOLVED(UObject_ProcessEvent_orig);

		// UObject::ProcessInternal hook for native Activated() logging
		// ----------------------------------------
		auto const UObject_ProcessInternal_target = Init.ResolveTyped<t_UObject_ProcessInternal>(BUILTIN_PROCESSINTERNAL_PHOOK);
		CHECK_RESOLVED(UObject_ProcessInternal_target);
		UObject_ProcessInternal_orig = (t_UObject_ProcessInternal*)Init.InstallHook("UObject::ProcessInternal", UObject_ProcessInternal_target, UObject_ProcessInternal_hook);
		CHECK_RESOLVED(UObject_ProcessInternal_orig);
		*/

		LEASI_INFO("hooks initialized");
	}

	tOutputDebugStringW* OutputDebugStringW_orig = nullptr;
	void OutputDebugStringW_hook(LPCWSTR lpcszString)
	{
		OutputDebugStringW_orig(lpcszString);

		// Strip trailing newline before feeding to LEASI_INFO
		// Maybe consider prefixing with [DEBUG] or similar in future
		std::wstring strToLog(lpcszString);
		if (!strToLog.empty() && strToLog.back() == L'\n')
		{
			strToLog.pop_back();
		}

		LEASI_INFO(strToLog);
	}

	// ! UObject::ProcessEvent hook
	// ========================================

	t_UObject_ProcessEvent* UObject_ProcessEvent_orig = nullptr;
	void UObject_ProcessEvent_hook(UObject* Context, UFunction* Function, void* Parms, void* Result)
	{
		UObject_ProcessEvent_orig(Context, Function, Parms, Result);
	}

	// ! UObject::ProcessInternal hook
	// ========================================

	t_UObject_ProcessInternal* UObject_ProcessInternal_orig = nullptr;
	void UObject_ProcessInternal_hook(UObject* Context, FFrame* Stack, void* Result)
	{
		auto func = Stack->Node;
		auto obj = Stack->Object;

		auto funcName = func->GetName();
		if (obj->IsA(USequenceOp::StaticClass()) && funcName.Equals(L"Activated"))
		{
			const auto op = reinterpret_cast<USequenceOp*>(Context);
			auto fullPath = op->GetFullPath();
			auto className = op->Class->Name.ToString();

		}

		UObject_ProcessInternal_orig(Context, Stack, Result);
	}
}
