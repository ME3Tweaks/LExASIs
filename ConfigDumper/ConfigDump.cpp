#include "ConfigDumper/ConfigDump.hpp"
#include "Common/Base.hpp"

#include <spdlog/details/windows_include.h>
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace ConfigDumper
{
	// ! Engine internals
	// ========================================
	//
	// GConfig and DumpConfigToArchive() are the exact same globals/routine the engine's own
	// "CONFIGHASH" console command uses (see ExecDispatcher1 in MassEffect3.exe). DumpConfigToArchive
	// walks GConfig's array of loaded FConfigFile objects and calls each one's virtual Dump() (vtable
	// slot 0x40), which in turn formats every section/property through FOutputDeviceError::Logf().
	// That Logf() does a *direct* (non-virtual) call, so it will invoke whatever the process-wide
	// FOutputDeviceError::Logf is currently bound to. If another loaded ASI hooks that same function
	// (e.g. DebugLogger's FErrorOutputDeviceLogf hook) and does not chain to the original, our capture
	// below will never run and the dump file will come out effectively empty -- if that happens, unload
	// the conflicting ASI and try again.
	//
	// RVAs (LE3/LMP, same binary):
	//   GConfig                RVA 0x17CD5B8 (data: holds the live FConfigCacheIni* pointer)
	//   DumpConfigToArchive     RVA 0x91ED0   (code: void(FConfigCacheIni*, FOutputDevice*))

	using t_DumpConfigToArchive = void(void* GConfigInstance, void* OutputDevice);

	void** GConfig_location = nullptr;
	t_DumpConfigToArchive* DumpConfigToArchive_fn = nullptr;

	// ! Minimal FOutputDevice stand-in
	// ========================================
	//
	// FOutputDevice::Logf()/FOutputDeviceError::Logf() only ever touch three things on the device
	// they're given: two flags at offsets 0x8/0xC that gate whether logging is suppressed (both must
	// read as 0 for us), and the vtable, calling slot 1 (offset 0x8) as Serialize(this, Text, Category)
	// with the fully-formatted line, and conditionally slot 2 (offset 0x10) as Flush(this). Layout
	// confirmed from the disassembly of both Logf() overloads.

	struct FakeOutputDeviceVTable
	{
		void (*Destructor)(void* This);
		void (*Serialize)(void* This, const wchar_t* Text, void* Category);
		void (*Flush)(void* This);
	};

	struct FakeOutputDevice
	{
		FakeOutputDeviceVTable* VTable;
		int32_t bSuppressed; // Must be 0, or Logf() silently drops every line.
		int32_t Reserved;    // Must be 0, same gate.
	};

	std::ofstream* g_captureFile = nullptr;
	size_t g_capturedLines = 0;

	std::string WideToUtf8(const wchar_t* text)
	{
		if (!text || !*text)
		{
			return {};
		}

		int const len = ::WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
		if (len <= 0)
		{
			return {};
		}

		std::string result(static_cast<size_t>(len) - 1, '\0'); // len includes the null terminator.
		::WideCharToMultiByte(CP_UTF8, 0, text, -1, result.data(), len, nullptr, nullptr);
		return result;
	}

	void Serialize_capture(void* /*This*/, const wchar_t* Text, void* /*Category*/)
	{
		if (g_captureFile && Text)
		{
			(*g_captureFile) << WideToUtf8(Text) << "\n";
			g_capturedLines++;
		}
	}

	void Flush_noop(void* /*This*/) {}
	void Destructor_noop(void* /*This*/) {}

	FakeOutputDeviceVTable g_fakeVTable{ &Destructor_noop, &Serialize_capture, &Flush_noop };

	// ! Initialization
	// ========================================

	void InitializeConfigDumpGlobals(::LESDK::Initializer& Init)
	{
		GConfig_location = Init.ResolveTyped<void*>(LEASI_RVA(0x17CD5B8));
		CHECK_RESOLVED(GConfig_location);

		DumpConfigToArchive_fn = Init.ResolveTyped<t_DumpConfigToArchive>(LEASI_RVA(0x91ED0));
		CHECK_RESOLVED(DumpConfigToArchive_fn);
	}

	// ! Dump entry point
	// ========================================

	void DumpRuntimeConfig()
	{
		if (!GConfig_location || !DumpConfigToArchive_fn)
		{
			LEASI_ERROR("ConfigDumper: globals were not resolved at startup, cannot dump config");
			return;
		}

		void* const gConfigInstance = *GConfig_location;
		if (!gConfigInstance)
		{
			LEASI_ERROR("ConfigDumper: GConfig is null (config system not initialized yet)");
			return;
		}

		std::error_code fsError;
		std::filesystem::create_directories("Logs", fsError);

		constexpr auto outPath = "Logs/RuntimeConfigDump.txt";
		std::ofstream out(outPath, std::ios::binary | std::ios::trunc);
		if (!out.is_open())
		{
			LEASI_ERROR("ConfigDumper: failed to open {} for writing", outPath);
			return;
		}

		out << "# Runtime config dump, captured via GConfig->DumpConfigToArchive() -- the exact same\n";
		out << "# code path the engine's own \"CONFIGHASH\" console command uses. This is the engine's\n";
		out << "# own text verbatim: nothing below has been reformatted, sorted, or deduplicated.\n";
		out << "#\n";
		out << "# Each property line the engine prints carries an inline \"(ACTION)\" annotation\n";
		out << "# (SET/CLEAR/ADD/ADDUNIQUE/REMOVEALL) between the '=' and the value -- that is the\n";
		out << "# engine's own parse-action tag for that entry, not part of the stored value text.\n";
		out << "#\n";
		out << "# Each top-level file is printed as one or more \"BINI Layer\"/\"File:\" blocks -- one per\n";
		out << "# config source (basegame Coalesced.bin, then each mounted DLC). CONFIRMED at runtime:\n";
		out << "# each layer shows ONLY that source's own raw, unmerged contribution (e.g. a DLC's\n";
		out << "# ADDUNIQUE layer here still shows just that DLC's own entries, not the merged result).\n";
		out << "# This is the \"what was loaded individually\" view, equivalent to parsing each source's\n";
		out << "# ConfigDelta separately -- it is NOT the final resolved config a property lookup would\n";
		out << "# return. See DumpResolvedConfig() / the DUMPRESOLVEDCONFIG command for that.\n";
		out << "#\n";

		g_captureFile = &out;
		g_capturedLines = 0;

		FakeOutputDevice fakeDevice{ &g_fakeVTable, 0, 0 };
		DumpConfigToArchive_fn(gConfigInstance, &fakeDevice);

		g_captureFile = nullptr;

		out.flush();
		out.close();

		if (g_capturedLines == 0)
		{
			LEASI_ERROR("ConfigDumper: dump produced no output -- another ASI may be intercepting "
				"FOutputDeviceError::Logf without chaining to the original (DebugLogger's "
				"FErrorOutputDeviceLogf hook does this). Try again with that ASI unloaded.");
			return;
		}

		LEASI_INFO("ConfigDumper: wrote {} lines to {}", g_capturedLines, outPath);
	}

	// ! Resolved (merged) config dump
	// ========================================
	//
	// DumpRuntimeConfig() above proved that GConfig never holds a flat, already-merged copy of a
	// config file -- FConfigFile::Dump() only ever shows each DLC's own raw, unmerged layer. The
	// actual Add/AddUnique/Remove/Clear/Set merge across layers happens lazily, inside
	// UObject::LoadConfig(), when a class's default object loads its Config properties.
	//
	// Rather than reverse-engineer that merge routine's internal per-layer record layout (risky --
	// wrong struct assumptions there would silently corrupt memory), this calls the real
	// UObject::LoadConfig() on every loaded class's own default object, then reads the resulting
	// property values back out via the SDK's existing UClass/UProperty reflection. The merge itself
	// is entirely the engine's own code; we only serialize memory we already understand the layout
	// of afterward.
	//
	// RVA: UObject::LoadConfig RVA 0x128810, signature confirmed via decompilation:
	//   void LoadConfig(UObject* Obj, UClass* ConfigClass, wchar_t* InFileName,
	//                    unsigned long long PropagateFlags, UProperty* PropertyToLoad)
	// PropagateFlags=0 matches what UnrealScript's parameterless obj.LoadConfig() compiles to --
	// no recursion into other objects/subclasses, just this one (Obj, ConfigClass) pair.
	//
	// Known simplifications (documented rather than silently wrong):
	//  - Only classes currently loaded in memory (in GObjObjects) are covered -- this is inherent to
	//    a *runtime* dump: it reflects what's loaded right now, not every class that could ever load.
	//  - Struct field export does not skip default/zero-valued fields the way the engine's own
	//    ExportTextItem does, so a struct line here may show more fields than the equivalent source
	//    ini/coalesced entry did. The values themselves are read directly from memory, not guessed.
	//  - A TArray nested *inside* a struct field (not at the top property level) is not expanded.
	//  - Map/Delegate/Interface/Component-typed properties fall back to a "<unsupported:ClassName>"
	//    placeholder rather than being misread.
	//  - Per-object config (CLASS_PerObjectConfig) and localized (.int) properties are not handled --
	//    this only walks a class's own ConfigLink against its ClassDefaultObject.

	// EClassFlags::CLASS_Config -- not exposed by the SDK headers, confirmed via Ghidra's own
	// EClassFlags enum (a stock UE3 value, unchanged in this build).
	constexpr DWORD CLASS_Config = 0x4;

	using t_LoadConfig = void(UObject* Obj, UClass* ConfigClass, wchar_t* InFileName, unsigned long long PropagateFlags, UProperty* PropertyToLoad);

	t_LoadConfig* LoadConfig_fn = nullptr;

	struct FScriptArray
	{
		void* Data;
		int32_t ArrayNum;
		int32_t ArrayMax;
	};

	std::wstring FormatScalarText(UProperty* prop, uint8_t* valuePtr);

	std::wstring FormatStructFields(UStruct* structDef, uint8_t* base)
	{
		std::wstring result = L"(";
		bool first = true;
		for (UProperty* field = structDef->PropertyLink; field; field = field->PropertyLinkNext)
		{
			for (int dim = 0; dim < field->ArrayDim; dim++)
			{
				if (!first)
				{
					result += L", ";
				}
				first = false;

				uint8_t* fieldPtr = base + field->Offset + (static_cast<size_t>(dim) * field->ElementSize);
				result += field->GetName();
				if (field->ArrayDim > 1)
				{
					result += L"[" + std::to_wstring(dim) + L"]";
				}
				result += L"=";
				result += FormatScalarText(field, fieldPtr);
			}
		}
		result += L")";
		return result;
	}

	std::wstring FormatScalarText(UProperty* prop, uint8_t* valuePtr)
	{
		if (prop->IsA(UFloatProperty::StaticClass()))
		{
			wchar_t buf[64];
			swprintf_s(buf, L"%.6g", static_cast<double>(*reinterpret_cast<float*>(valuePtr)));
			return buf;
		}
		if (prop->IsA(UIntProperty::StaticClass()))
		{
			return std::to_wstring(*reinterpret_cast<int32_t*>(valuePtr));
		}
		if (prop->IsA(UBoolProperty::StaticClass()))
		{
			auto const* boolProp = static_cast<UBoolProperty*>(prop);
			uint32_t const raw = *reinterpret_cast<uint32_t*>(valuePtr);
			return (raw & boolProp->BitMask) ? L"True" : L"False";
		}
		if (prop->IsA(UByteProperty::StaticClass()))
		{
			auto const* byteProp = static_cast<UByteProperty*>(prop);
			uint8_t const byteVal = *valuePtr;
			if (byteProp->Enum && byteVal < byteProp->Enum->Names.Count())
			{
				return byteProp->Enum->Names(byteVal).ToString().Chars();
			}
			return std::to_wstring(static_cast<unsigned>(byteVal));
		}
		if (prop->IsA(UNameProperty::StaticClass()))
		{
			return reinterpret_cast<SFXName*>(valuePtr)->ToString().Chars();
		}
		if (prop->IsA(UStrProperty::StaticClass()))
		{
			auto const* value = reinterpret_cast<FString*>(valuePtr)->Chars();
			return value ? value : L"";
		}
		if (prop->IsA(UStructProperty::StaticClass()))
		{
			auto const* structProp = static_cast<UStructProperty*>(prop);
			return FormatStructFields(structProp->Struct, valuePtr);
		}
		if (prop->IsA(UObjectProperty::StaticClass()))
		{
			UObject* obj = *reinterpret_cast<UObject**>(valuePtr);
			return obj ? obj->GetFullName().Chars() : L"None";
		}

		return L"<unsupported:" + std::wstring(prop->Class ? prop->Class->GetName().Chars() : L"?") + L">";
	}

	void EmitProperty(std::ofstream& out, UProperty* prop, uint8_t* objBase)
	{
		std::wstring const keyName = prop->GetName().Chars();
		uint8_t* const fieldPtr = objBase + prop->Offset;

		if (prop->IsA(UArrayProperty::StaticClass()))
		{
			auto* arrProp = static_cast<UArrayProperty*>(prop);
			auto* arr = reinterpret_cast<FScriptArray*>(fieldPtr);
			for (int32_t i = 0; i < arr->ArrayNum; i++)
			{
				uint8_t* elemPtr = static_cast<uint8_t*>(arr->Data) + (static_cast<size_t>(i) * arrProp->Inner->ElementSize);
				std::wstring line = L"\t\t" + keyName + L"=" + FormatScalarText(arrProp->Inner, elemPtr) + L"\n";
				out << WideToUtf8(line.c_str());
			}
			return;
		}

		for (int dim = 0; dim < prop->ArrayDim; dim++)
		{
			uint8_t* elemPtr = fieldPtr + (static_cast<size_t>(dim) * prop->ElementSize);
			std::wstring line = L"\t\t" + keyName;
			if (prop->ArrayDim > 1)
			{
				line += L"[" + std::to_wstring(dim) + L"]";
			}
			line += L"=" + FormatScalarText(prop, elemPtr) + L"\n";
			out << WideToUtf8(line.c_str());
		}
	}

	void InitializeResolvedConfigGlobals(::LESDK::Initializer& Init)
	{
		LoadConfig_fn = Init.ResolveTyped<t_LoadConfig>(LEASI_RVA(0x128810));
		CHECK_RESOLVED(LoadConfig_fn);
	}

	void DumpResolvedConfig()
	{
		if (!LoadConfig_fn)
		{
			LEASI_ERROR("ConfigDumper: LoadConfig was not resolved at startup, cannot dump resolved config");
			return;
		}

		if (!UObject::GObjObjects)
		{
			LEASI_ERROR("ConfigDumper: GObjObjects is null, cannot enumerate classes");
			return;
		}

		std::error_code fsError;
		std::filesystem::create_directories("Logs", fsError);

		constexpr auto outPath = "Logs/ResolvedConfigDump.txt";
		std::ofstream out(outPath, std::ios::binary | std::ios::trunc);
		if (!out.is_open())
		{
			LEASI_ERROR("ConfigDumper: failed to open {} for writing", outPath);
			return;
		}

		out << "# Resolved config dump. Unlike RuntimeConfigDump.txt (the engine's raw per-layer\n";
		out << "# CONFIGHASH-equivalent output), this calls the real UObject::LoadConfig() per class\n";
		out << "# and reads the resulting property values back out of memory -- the actual merged\n";
		out << "# result a lookup would return, produced entirely by the engine's own merge code.\n";
		out << "# See the comment above DumpResolvedConfig() in ConfigDump.cpp for known simplifications\n";
		out << "# (struct default-field elision is not replicated, nested arrays-in-structs are not\n";
		out << "# expanded, only classes currently loaded are covered, Map/Delegate/Interface/Component\n";
		out << "# properties are not decoded).\n";
		out << "#\n";

		size_t classCount = 0;
		size_t propertyCount = 0;

		for (UObject* obj : *UObject::GObjObjects)
		{
			if (!obj || !obj->IsA(UClass::StaticClass()))
			{
				continue;
			}

			auto* cls = static_cast<UClass*>(obj);
			if ((cls->ClassFlags & CLASS_Config) == 0 || !cls->ClassDefaultObject)
			{
				continue;
			}

			// Walk from the root-most Config ancestor down to this class, loading and then
			// printing each ancestor's own declared ConfigLink properties (ConfigLink only ever
			// contains properties declared directly on that class, not inherited ones).
			std::vector<UClass*> configChain;
			for (UClass* ancestor = cls; ancestor; ancestor = static_cast<UClass*>(ancestor->SuperField))
			{
				if ((ancestor->ClassFlags & CLASS_Config) != 0 && ancestor->ConfigLink)
				{
					configChain.push_back(ancestor);
				}
			}
			if (configChain.empty())
			{
				continue;
			}
			std::reverse(configChain.begin(), configChain.end());

			classCount++;
			std::wstring const header = L"### " + std::wstring(cls->GetFullName().Chars()) + L" (config=" + std::wstring(cls->ClassConfigName.ToString().Chars()) + L")\n";
			out << WideToUtf8(header.c_str());

			for (UClass* ancestor : configChain)
			{
				LoadConfig_fn(cls->ClassDefaultObject, ancestor, nullptr, 0, nullptr);

				std::wstring const sectionLine = L"\t[" + std::wstring(ancestor->GetFullName().Chars()) + L"]\n";
				out << WideToUtf8(sectionLine.c_str());
				for (UProperty* prop = ancestor->ConfigLink; prop; prop = prop->ConfigLinkNext)
				{
					EmitProperty(out, prop, reinterpret_cast<uint8_t*>(cls->ClassDefaultObject));
					propertyCount++;
				}
			}
			out << "\n";
		}

		out.flush();
		out.close();

		LEASI_INFO("ConfigDumper: wrote resolved config for {} classes ({} properties) to {}", classCount, propertyCount, outPath);
	}
}
