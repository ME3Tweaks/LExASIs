#include "Common/Base.hpp"
#include "Hooks.hpp"
#include "DebugLogger/VerboseLogger.hpp"

namespace DebugLogger {
#define VL_STRINGIFY_IMPL(x) #x
#define VL_STRINGIFY(x) VL_STRINGIFY_IMPL(x)

	// VerboseLogger RVAs

#if defined(SDK_TARGET_LE1)
#define VL_ACCESSED_NONE_RVA					LEASI_RVA(0x12575a)
#define VL_DYNAMIC_ARRAY_INSTANCE_VARIABLE_RVA	LEASI_RVA(0x122865)
#define VL_STATIC_ARRAY_RVA						LEASI_RVA(0x1223ed)
#define VL_FLOAT_DIVIDE_FLOAT_RVA				LEASI_RVA(0x117835)
#define VL_FLOAT_DIVIDE_ASSIGN_FLOAT_RVA		LEASI_RVA(0x1181ae)
#define VL_ROTATOR_DIVIDE_ASSIGN_FLOAT_RVA		LEASI_RVA(0x128b71)
#define VL_ROTATOR_DIVIDE_FLOAT_RVA				LEASI_RVA(0x128dc0)
#define VL_VECTOR_DIVIDE_ASSIGN_FLOAT_RVA		LEASI_RVA(0x12a0d1) // Unsure about this one. It's in undefined function so I don't know the name
#define VL_VECTOR_DIVIDE_FLOAT_RVA				LEASI_RVA(0x1133a0)
#define VL_INT_DIVIDE_INT_RVA					LEASI_RVA(0x1160da)
#define VL_FLOAT_MODULO_FLOAT_RVA				LEASI_RVA(0x117915)
#define VL_INT_MODULO_INT_RVA					LEASI_RVA(0x1161ba)
#define VL_SQRT_OF_NEGATIVE_NUMBER_RVA			LEASI_RVA(0x118a44)
#elif defined(SDK_TARGET_LE2)
#define VL_ACCESSED_NONE_RVA					LEASI_RVA(0xcdeca)
#define VL_DYNAMIC_ARRAY_INSTANCE_VARIABLE_RVA	LEASI_RVA(0xcaf15)
#define VL_STATIC_ARRAY_RVA						LEASI_RVA(0xcaa9d)
#define VL_FLOAT_DIVIDE_FLOAT_RVA				LEASI_RVA(0xc03d5)
#define VL_FLOAT_DIVIDE_ASSIGN_FLOAT_RVA		LEASI_RVA(0xc0d4e)
#define VL_ROTATOR_DIVIDE_ASSIGN_FLOAT_RVA		LEASI_RVA(0xd12e1)
#define VL_ROTATOR_DIVIDE_FLOAT_RVA				LEASI_RVA(0xd1530)
#define VL_VECTOR_DIVIDE_ASSIGN_FLOAT_RVA		LEASI_RVA(0xd2841) // Unsure about this one. It's in undefined function so I don't know the name
#define VL_VECTOR_DIVIDE_FLOAT_RVA				LEASI_RVA(0xd3360)
#define VL_INT_DIVIDE_INT_RVA					LEASI_RVA(0xbec7a)
#define VL_FLOAT_MODULO_FLOAT_RVA				LEASI_RVA(0xc04b5)
#define VL_INT_MODULO_INT_RVA					LEASI_RVA(0xd3360)
#define VL_SQRT_OF_NEGATIVE_NUMBER_RVA			LEASI_RVA(0xc15e4)
#elif defined(SDK_TARGET_LE3)
#define VL_ACCESSED_NONE_RVA					LEASI_RVA(0xe9d4a)
#define VL_DYNAMIC_ARRAY_INSTANCE_VARIABLE_RVA	LEASI_RVA(0xe69b5)
#define VL_STATIC_ARRAY_RVA						LEASI_RVA(0xe653d)
#define VL_FLOAT_DIVIDE_FLOAT_RVA				LEASI_RVA(0xdb665)
#define VL_FLOAT_DIVIDE_ASSIGN_FLOAT_RVA		LEASI_RVA(0xdbfde)
#define VL_ROTATOR_DIVIDE_ASSIGN_FLOAT_RVA		LEASI_RVA(0xed1e1)
#define VL_ROTATOR_DIVIDE_FLOAT_RVA				LEASI_RVA(0xed430)
#define VL_VECTOR_DIVIDE_ASSIGN_FLOAT_RVA		LEASI_RVA(0xee741)
#define VL_VECTOR_DIVIDE_FLOAT_RVA				LEASI_RVA(0xef260)
#define VL_INT_DIVIDE_INT_RVA					LEASI_RVA(0xf1f9a)
#define VL_FLOAT_MODULO_FLOAT_RVA				LEASI_RVA(0xdb745)
#define VL_INT_MODULO_INT_RVA					LEASI_RVA(0xf207a)
#define VL_SQRT_OF_NEGATIVE_NUMBER_RVA			LEASI_RVA(0xdc874)
#endif


	// This is not in a header due to the use of the macros.
#pragma region ScriptErrorVerboseLogger

	void VerboseLoggerInternal(const FFrame* stack)
	{
		const auto funcOrStateFullPath = stack->Node->GetFullPath();
		const auto thisFullPath = stack->Object->GetFullPath();
		const long long scriptOffset = stack->Code - stack->Node->Script.GetData();

		// Convert the following line's forma
		// 	logger.writeToLog(string_format("Error in '%s' on '%s' at %i (0x%X) bytes into the bytecode", funcOrStateFullPath.c_str(), thisFullPath, scriptOffset, scriptOffset), true, true);
		LEASI_ERROR("Error in '{}' on '{}' at {} (0x{:X}) bytes into the bytecode", funcOrStateFullPath, thisFullPath, scriptOffset, scriptOffset);
		LEASI_INFO("Values of arguments and locals:");
		/*PropertyLogger propLogger;
		propLogger.PrintPropertyValues(stack->Locals, stack->Node, stack->OutParms);
		LEASI_INFO(propLogger.GetString(), false, true);*/
	}

	bool PatchMemory(const void* patch, const SIZE_T patchSize, void* patchLocation)
	{
		//make the memory we're going to patch writeable
		DWORD  oldProtect;
		if (!VirtualProtect(patchLocation, patchSize, PAGE_EXECUTE_READWRITE, &oldProtect))
			return false;

		//overwrite with our patch
		memcpy(patchLocation, patch, patchSize);

		//restore the memory's old protection level
		VirtualProtect(patchLocation, patchSize, oldProtect, &oldProtect);
		FlushInstructionCache(GetCurrentProcess(), patchLocation, patchSize);
		return true;
	}

	void AccessedNoneVerboseLogger(const FFrame* stack)
	{
		VerboseLoggerInternal(stack);
	}

	void AttachAccessedNoneVerboseLogger(void* AccessedNone_location)
	{
		static bool isPatched;
		if (isPatched == true)
		{
			return;
		}
		BYTE patch[] = {
							   0x48, 0x89, 0xF9, //MOV RCX, RDI //Move the FFrame pointer into the 1st argument register
							   0x49, 0xBB, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //MOV R11, 0xFFFFFFFFFFFFFFFF //put address of AccessedNoneVerboseLogger into R11 (actual address filled in at runtime) 
							   0x41, 0xFF, 0xD3,  //CALL R11 //Call AccessedNoneVerboseLogger

							   //remaining bytes are NOPs of various sizes: https://stackoverflow.com/questions/43991155/what-does-nop-dword-ptr-raxrax-x64-assembly-instruction-do/50594130#50594130
							   0x66, 0x90 };

		//place the absolute address of AccessedNoneVerboseLogger into the patch
		void* funcPtr = AccessedNoneVerboseLogger;
		memcpy(patch + 5, &funcPtr, sizeof(funcPtr));

		isPatched = PatchMemory(patch, sizeof(patch), AccessedNone_location);
		if (!isPatched)
		{
			LEASI_ERROR("FAILED TO ATTACH VERBOSE ACCESSED NONE LOGGER!");
		}
	}

	void ArrayOOBLocalVerboseLogger(const FFrame* stack, void* addressOfThisFunction, wchar_t* formatString, wchar_t* arrayPropName, const int index, const int arrayLen)
	{
		LEASI_UNUSED(addressOfThisFunction);
		auto logString = FString::Printf(formatString, arrayPropName, index, arrayLen);
		LEASI_ERROR(L"appLogf: {}", logString);
		VerboseLoggerInternal(stack);
	}

	void ArrayOOBWithObjectNameVerboseLogger(const FFrame* stack, void* addressOfThisFunction, wchar_t* formatString, wchar_t* objName, wchar_t* arrayPropName, const int index, const int arrayLen)
	{
		LEASI_UNUSED(addressOfThisFunction);

		auto logString = FString::Printf(formatString, objName, arrayPropName, index, arrayLen);
		LEASI_ERROR(L"appLogf: {}", logString);
		VerboseLoggerInternal(stack);
	}

	void DivideBy0VerboseLogger(const FFrame* stack, void* addressOfThisFunction, void* undefined)
	{
		LEASI_UNUSED_2(addressOfThisFunction, undefined);
		LEASI_ERROR("appLogf: Divide by zero");
		VerboseLoggerInternal(stack);
	}

	void ModuloBy0VerboseLogger(const FFrame* stack, void* addressOfThisFunction, void* undefined)
	{
		LEASI_UNUSED_2(addressOfThisFunction, undefined);
		LEASI_ERROR("appLogf: Module by zero");
		VerboseLoggerInternal(stack);
	}

	void SqrtOfNegativeNumberVerboseLogger(const FFrame* stack, void* addressOfThisFunction, void* undefined)
	{
		LEASI_UNUSED_2(addressOfThisFunction, undefined);
		LEASI_ERROR("appLogf: Attempt to take Sqrt() of negative number - returning 0.");
		VerboseLoggerInternal(stack);
	}

	void AttachArrayOOBVerboseLogger(void* funcPtr, void* patchLocation, const char* name)
	{
		BYTE patch[] = {
							   0x48, 0xBA, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //MOV RDX, 0xFFFFFFFFFFFFFFFF //put address of logging function into RDX (actual address filled in at runtime) 
							   0x49, 0x8B, 0xCD, //MOV RCX, R13 //Move the FFrame pointer into the 1st argument register
							   0xFF, 0xD2,  //CALL RDX //Call logging function
		};

		//place the absolute address of logging function into the patch
		memcpy(patch + 2, &funcPtr, sizeof(funcPtr));

		const bool isPatched = PatchMemory(patch, sizeof(patch), patchLocation);
		if (!isPatched)
		{
			LEASI_WARN("Failed to attach {}", name);
		}
	}

	void AttachMathVerboseLogger(void* funcPtr, void* patchLocation, const char* name)
	{
		BYTE patch[] = {
							   0x48, 0xBA, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //MOV RDX, 0xFFFFFFFFFFFFFFFF //put address of logging function into RDX (actual address filled in at runtime) 
							   0x48, 0x8B, 0xCB, //MOV RCX, RBX //Move the FFrame pointer into the 1st argument register
							   0xFF, 0xD2,  //CALL RDX //Call logging function
		};

		//place the absolute address of logging function into the patch
		memcpy(patch + 2, &funcPtr, sizeof(funcPtr));

		const bool isPatched = PatchMemory(patch, sizeof(patch), patchLocation);
		if (!isPatched)
		{
			LEASI_WARN("Failed to attach {}", name);
		}
	}
#pragma endregion ScriptErrorVerboseLogger


	void InstallVerboseLoggerHooks(::LESDK::Initializer& Init) {
		// Patterns in comments are from LE3.
		//INIT_FIND_PATTERN(AccessedNone_location, "48 8b 0d 5f b9 6e 01 48 85 c9 74 06 48 8b 01 ff 50 20")
		const auto accessedNone_location = Init.ResolveTyped<void*>(VL_ACCESSED_NONE_RVA);
		AttachAccessedNoneVerboseLogger(accessedNone_location);

#define ATTACH_VERBOSE_LOGGER2(TYPE, LOGFUNCTION, SIGNATURE, ADDRESS) \
		{  \
			auto const attach##TYPE##VerboseLogger_location = Init.ResolveTyped<SIGNATURE>(ADDRESS); \
			CHECK_RESOLVED(attach##TYPE##VerboseLogger_location); \
			Attach##TYPE##VerboseLogger(LOGFUNCTION, attach##TYPE##VerboseLogger_location, #LOGFUNCTION " at " #ADDRESS); \
		} 

		//dynamic array, instance variable
		//ATTACH_VERBOSE_LOGGER(ArrayOOB, ArrayOOBWithObjectNameVerboseLogger, "48 8b 15 8c 0b 6c 01 49 8b cd e8 9c c7 02 00 90")
		ATTACH_VERBOSE_LOGGER2(ArrayOOB, ArrayOOBWithObjectNameVerboseLogger, tArrayOOBWithObjectName, VL_DYNAMIC_ARRAY_INSTANCE_VARIABLE_RVA)

			//static array
			//ATTACH_VERBOSE_LOGGER(ArrayOOB, ArrayOOBWithObjectNameVerboseLogger, "48 8b 15 04 10 6c 01 49 8b cd e8 14 cc 02 00 90")
			ATTACH_VERBOSE_LOGGER2(ArrayOOB, ArrayOOBWithObjectNameVerboseLogger, tArrayOOBWithObjectName, VL_STATIC_ARRAY_RVA)

			// float / float
			// ATTACH_VERBOSE_LOGGER(Math, DivideBy0VerboseLogger, "4c 8d 05 c4 fa fa 00 48 8b cb e8 ec 7a 03 00")
			ATTACH_VERBOSE_LOGGER2(Math, DivideBy0VerboseLogger, tStackAddrUndef, VL_FLOAT_DIVIDE_FLOAT_RVA)

			// float /= float
			// ATTACH_VERBOSE_LOGGER(Math, DivideBy0VerboseLogger, "4c 8d 05 4b f1 fa 00 48 8b cb e8 73 71 03 00")
			ATTACH_VERBOSE_LOGGER2(Math, DivideBy0VerboseLogger, tStackAddrUndef, VL_FLOAT_DIVIDE_ASSIGN_FLOAT_RVA)


			// rotator /= float
			//ATTACH_VERBOSE_LOGGER(Math, DivideBy0VerboseLogger, "4c 8d 05 48 df f9 00 48 8b cb e8 70 5f 02 00")
			ATTACH_VERBOSE_LOGGER2(Math, DivideBy0VerboseLogger, tStackAddrUndef, VL_ROTATOR_DIVIDE_ASSIGN_FLOAT_RVA)

			// rotator / float
			//ATTACH_VERBOSE_LOGGER(Math, DivideBy0VerboseLogger, "4c 8d 05 f9 dc f9 00 48 8b cb e8 21 5d 02 00")
			ATTACH_VERBOSE_LOGGER2(Math, DivideBy0VerboseLogger, tStackAddrUndef, VL_ROTATOR_DIVIDE_FLOAT_RVA)

			// vector /= float
			//ATTACH_VERBOSE_LOGGER(Math, DivideBy0VerboseLogger, "4c 8d 05 e8 c9 f9 00 48 8b cb e8 10 4a 02 00")
			ATTACH_VERBOSE_LOGGER2(Math, DivideBy0VerboseLogger, tStackAddrUndef, VL_VECTOR_DIVIDE_ASSIGN_FLOAT_RVA)

			// vector / float
			//ATTACH_VERBOSE_LOGGER(Math, DivideBy0VerboseLogger, "4c 8d 05 c9 be f9 00 48 8b cb e8 f1 3e 02 00")
			ATTACH_VERBOSE_LOGGER2(Math, DivideBy0VerboseLogger, tStackAddrUndef, VL_VECTOR_DIVIDE_FLOAT_RVA)

			// int / int
			//ATTACH_VERBOSE_LOGGER(Math, DivideBy0VerboseLogger, "4c 8d 05 8f 91 f9 00 48 8b cb e8 b7 11 02 00")
			ATTACH_VERBOSE_LOGGER2(Math, DivideBy0VerboseLogger, tStackAddrUndef, VL_INT_DIVIDE_INT_RVA)

			// float % float
			//ATTACH_VERBOSE_LOGGER(Math, ModuloBy0VerboseLogger, "4c 8d 05 04 fa fa 00 48 8b cb e8 0c 7a 03 00")
			ATTACH_VERBOSE_LOGGER2(Math, ModuloBy0VerboseLogger, tStackAddrUndef, VL_FLOAT_MODULO_FLOAT_RVA)

			// int % int
			//ATTACH_VERBOSE_LOGGER(Math, ModuloBy0VerboseLogger, "4c 8d 05 cf 90 f9 00 48 8b cb e8 d7 10 02 00")
			ATTACH_VERBOSE_LOGGER2(Math, ModuloBy0VerboseLogger, tStackAddrUndef, VL_INT_MODULO_INT_RVA)

			// square root of negative number
			//ATTACH_VERBOSE_LOGGER(Math, SqrtOfNegativeNumberVerboseLogger, "4c 8d 05 05 e9 fa 00 48 8b cb e8 dd 68 03 00")
			ATTACH_VERBOSE_LOGGER2(Math, SqrtOfNegativeNumberVerboseLogger, tStackAddrUndef, VL_SQRT_OF_NEGATIVE_NUMBER_RVA)

	}

#undef ATTACH_VERBOSE_LOGGER
#undef ATTACH_VERBOSE_LOGGER2
#undef VL_STRINGIFY_IMPL
#undef VL_STRINGIFY
}