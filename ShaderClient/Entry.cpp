
#include "Common/Base.hpp"
#include "Common/DefaultLogger.hpp"
#include "ShaderClient/Entry.hpp"
#include "ShaderClient/Hooks.hpp"
#include "ShaderClient/Helpers.hpp"
#include "ShaderClient/SharedVersion.h"

SPI_PLUGINSIDE_SUPPORT(SDK_TARGET_NAME_W ASI_NAME_NO_SPACE_W, DEVELOPER_W, L"" VERSION_STRING_W, SPI_GAME_SDK_TARGET, SPI_VERSION_ANY);
SPI_PLUGINSIDE_POSTLOAD;
SPI_PLUGINSIDE_ASYNCATTACH;


SPI_IMPLEMENT_ATTACH
{
	::LESDK::Initializer Init{ InterfacePtr, SDK_TARGET_NAME_A ASI_NAME_NO_SPACE_A };
	Common::SetupDefaultLogger(SDK_TARGET_NAME_A, ASI_NAME_NO_SPACE_A, true);
	::ShaderClient::InitializeGlobals(Init);
	::ShaderClient::InitializeHooks(Init);
	LEASI_INFO("Welcome to Milan Shader Client");


	LEASI_INFO("Size of FArchive: {}", sizeof(FArchive));
	LEASI_INFO("Size of FMemoryArchive: {}", sizeof(FMemoryArchiveBase));

	return true;
}

SPI_IMPLEMENT_DETACH
{
	LEASI_UNUSED(InterfacePtr);
	return true;
}


namespace ShaderClient
{
	void InitializeGlobals(::LESDK::Initializer& Init)
	{
		Common::InitializeRequiredGlobals(Init);
	}

#define HOOK_PATTERN_POSTHOOK(NAME, SIG, POSTHOOK) \
		do { \
	auto const NAME##_target = Init.ResolveTyped<SIG>(::LESDK::Address::FromPostHook(POSTHOOK)); \
	CHECK_RESOLVED(NAME##_target); \
	NAME##_orig = (SIG*)Init.InstallHook(#NAME, NAME##_target, NAME##_hook); \
	CHECK_RESOLVED(NAME##_orig); \
		} while (0);

#define HOOK_RVA(NAME, SIG, RVA) \
		do { \
	auto const NAME##_target = Init.ResolveTyped<SIG>(RVA); \
	CHECK_RESOLVED(NAME##_target); \
	NAME##_orig = (SIG*)Init.InstallHook(#NAME, NAME##_target, NAME##_hook); \
	CHECK_RESOLVED(NAME##_orig); \
		} while (0);

	void InitializeHooks(::LESDK::Initializer& Init)
	{
		// All of these addresses are for LE3 currently.

		// INIT SHADER MAP - used to trigger hlsl compile when shader fails to load
		HOOK_PATTERN_POSTHOOK(InitShaderMap, tInitShaderMap, "54 41 55 41 56 41 57 48 81 ec a0 00 00 00 48 c7 44 24 40 fe ff ff ff 49 89 5b 10 49 89 6b 18 49 89 73 20 45 8b e1 45 8b f0 48 8b ea");
		
		// FMaterialCompile - used to inspect result of shader map compile and actually generate shader cache
		HOOK_PATTERN_POSTHOOK(FMaterialCompile, tFMaterialCompile, "41 54 41 55 41 56 41 57 48 8d ac 24 38 f8 ff ff 48 81 ec c8 08 00 00 48 c7 45 f8 fe ff ff ff");

		HOOK_PATTERN_POSTHOOK(FMaterialShaderMap_Compile, tCompileShader, "50 48 c7 44 24 30 fe ff ff ff 48 89 5c 24 60 48 89 74 24 68 48 8b fa 48 8b 02 48 8d 54 24 38 48 8b cf ff 90 b8 01 00 00 90");

		//INIT_FIND_PATTERN_POSTHOOK(InitShaderMap, /*"4c 8b dc 57 41*/ "54 41 55 41 56 41 57 48 81 ec a0 00 00 00 48 c7 44 24 40 fe ff ff ff 49 89 5b 10 49 89 6b 18 49 89 73 20 45 8b e1 45 8b f0 48 8b ea")
		//INIT_HOOK_PATTERN(InitShaderMap);

		// FMaterialCompile - used to inspect result of shader map compile and actually generate shader cache
		//INIT_FIND_PATTERN_POSTHOOK(FMaterialCompile, /*"40 55 53 56 57*/ "41 54 41 55 41 56 41 57 48 8d ac 24 38 f8 ff ff 48 81 ec c8 08 00 00 48 c7 45 f8 fe ff ff ff")
		//INIT_HOOK_PATTERN(FMaterialCompile);

		//FMaterialShaderMap_Compile = Init.ResolveTyped<tCompileShader>(LEASI_RVA(0x2a7a30));
		//INIT_FIND_PATTERN_POSTHOOK(FMaterialShaderMap_Compile, /*"40 57 48 83 ec*/ "50 48 c7 44 24 30 fe ff ff ff 48 89 5c 24 60 48 89 74 24 68 48 8b fa 48 8b 02 48 8d 54 24 38 48 8b cf ff 90 b8 01 00 00 90");
		//INIT_HOOK_PATTERN(FMaterialShaderMap_Compile);

		// Hooked this way to allow hook sharing
		HOOK_PATTERN_POSTHOOK(GetTlkString, tGetTlkString,/*"89 54 24 10 4c*/ "8b dc 57 41 54 41 55 41 56 41 57 48 83 ec 60 49 c7 43 a8 fe ff ff ff");

		HOOK_RVA(ShowLoadMessage, tShowLoadMessage, LEASI_RVA(0xc23830))

		HOOK_RVA(UObject_ProcessInternal, t_UObject_ProcessInternal, BUILTIN_PROCESSINTERNAL_RVA);
		HOOK_RVA(PlayLoadingMovie, tPlayLoadingMovie, LEASI_RVA(0xbd9a70));
#if defined(NOTDEFINED)

		// uncomment to make _setSomething_hook break. The game crashes in here after a few iterations
		//INIT_FIND_PATTERN_POSTHOOK(_setSomething, /*"48 89 5c 24 10*/ "48 89 6c 24 18 57 48 83 ec 20 48 8b d9 48 8b fa 8b 49 34 bd 01 00 00 00 85 c9 7e 1d 4c 63 4b 30 44 89 0a 48 8b 13 4f 8d 04 89 42 8b 04 82 89 43 30 8d 41 ff 89 43 34 eb 53 44 8b 43 0c 48 89 74 24 30 8b 73 08 8d 56 01 89 53 08 41 3b d0 7e 24 41 b9 14 00 00 00 48 8b cb e8 fd ad db ff");
		//INIT_HOOK_PATTERN(_setSomething);

		//INIT_FIND_PATTERN_POSTHOOK(FindFirstBit, /*"4c 8b 59 08 45 */ "33 d2 4c 63 01 41 8b d2 49 8b 43 10 48 85 c0 49 0f 44 d3 48 0b d0 48 8d 05 96 29 ff 00 4c 0f 44 d0 4c 0b d2 43 8b 14 82 23 51 10 75 32");
		//INIT_HOOK_PATTERN(FindFirstBit);


		// HLSL Generator - used to capture hlsl to file
		//INIT_FIND_PATTERN_POSTHOOK(FHLSLTranslator_Translate, /*"48 8b c4 55 41*/ "54 41 55 41 56 41 57 48 8d a8 08 ff ff ff 48 81 ec d0 01 00 00 48 c7 45 a8 fe ff ff ff 48 89 58 10 48 89 70 18 48 89 78 20 48 8b 05 b3 28 50 01 48 33 c4 48 89 85 c0 00 00 00 48 8b f1 c7 41 08 01 00 00 00")
		//INIT_HOOK_PATTERN(FHLSLTranslator_Translate);

#endif

		// VTables
		StringArchiveProxyVTableStart = Init.ResolveTyped<void>(LEASI_RVA(0x10dfbf0));
		MemoryArchiveWriterVTableStart = Init.ResolveTyped<void>(LEASI_RVA(0x107fac8));

		// Functions
		FMaterial_GetFriendlyName = Init.ResolveTyped<tFGetMaterialFriendlyName>(LEASI_RVA(0x2BDEC0));
		FHLSL_GetMaterialCode = Init.ResolveTyped<tGetMaterialCode>(LEASI_RVA(0x2BE1F0));
		GetShaderTypeList = Init.ResolveTyped<tGetShaderTypeList>(LEASI_RVA(0x387540));
		GetVertexFactoryTypeList = Init.ResolveTyped<tGetVertexFactoryTypeList>(LEASI_RVA(0x8d4830));
		SerializeUShaderCache = Init.ResolveTyped<tSerializeUShaderCache>(LEASI_RVA(0x376840));
		SerializeMaterialResource = Init.ResolveTyped<tSerializeMaterialResource>(LEASI_RVA(0x2c56a0));
		// SerializeFUniformExpressionSet = Init.ResolveTyped<tSerializeFUniformExpressionSet>(LEASI_RVA(0x2C66B0));
		static_construct_object = Init.ResolveTyped<tStaticConstructObject>(BUILTIN_STATICCONSTRUCTOBJECT_RVA);
		FStaticParameterSet::StaticParameterSetEqualRef = Init.ResolveTyped<FStaticParameterSet::tStaticParameterSetEqual>(LEASI_RVA(0x2A46A0));
		CreateArchiveProxy = Init.ResolveTyped<tCreateArchiveProxy>(LEASI_RVA(0x235EE0));
		// FString::StrHash = Init.ResolveTyped<FString::tappStrHash>(LEASI_RVA(0x9F710));

		// for debugging purposes
		// ByteOrderSerialize = Init.ResolveTyped<tByteOrderSerialize>(LEASI_RVA(0x84930));

		// Globals
		GFileManager = Init.ResolveTyped<FFileManager*>(LEASI_RVA(0x17cd5d0)); 
		GObjTransientPackage = Init.ResolveTyped<UPackage*>(LEASI_RVA(0x1887DA0));
		GIsSeekFreePCConsole = Init.ResolveTyped<int>(LEASI_RVA(0x17DF724));
		GError = Init.ResolveTyped<FArchive*>(LEASI_RVA(0x17CD570));
		GCookingTarget = Init.ResolveTyped<ECookingTarget>(LEASI_RVA(0x17cd580));

		LoadCompiler();
	}
}
