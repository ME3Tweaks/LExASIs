#pragma once
#include <LESDK/Headers.hpp>
#include "ShaderClient/MilanShaderCompiler.hpp"

#define PRINT_DEBUG_INFO false

namespace ShaderClient
{
	enum class ShaderHostCommand : BYTE
	{
		SaveShaderCache = 1,
		TerminateGame = 2, // Unused
		ShaderCompileFailed = 3,
		SaveMaterialBinary = 4,
		SaveMaterialHLSL = 5,
		SaveDebugInfo = 6,
		SaveTerrainMaterialResource = 7
	};
	extern std::wstring currentAction;
	extern bool SendDebugPackages;
	/// <summary>
	/// Capture compile jobs to debug packages without compiling them. Used to build the offline
	/// calibration corpus in a single pass; implies SendDebugPackages.
	/// </summary>
	extern bool CaptureOnly;
	void UpdateCurrentAction(std::wstring_view newAction);

	/// <summary>
	/// The static parameter set currently being compiled, or nullptr outside a compile window.
	/// </summary>
	/// <remarks>
	/// A MaterialInstance's static permutation is compiled through an FMaterialResource whose
	/// ->Material is the *parent* UMaterial, so every instance permutation would otherwise be
	/// captured under the parent's path and overwrite the parent's own jobs. The set is the engine's
	/// own identity for a shader map, so it is what the capture is keyed by.
	/// </remarks>
	extern FStaticParameterSet const* GCurrentStaticParameters;

	/// <summary>
	/// Scopes <see cref="GCurrentStaticParameters"/> to one call.
	/// </summary>
	/// <remarks>
	/// Restores the previous value rather than clearing to nullptr, because these nest:
	/// FMaterial::Compile covers a whole compile, and FMaterialShaderMap::Compile runs inside it once
	/// per shader map. A destructor that nulled the global would empty the *outer* scope the moment
	/// the first inner one finished.
	///
	/// That is not hypothetical -- it is how SaveMaterialBinary (called from FMaterialCompile_hook,
	/// after the inner shader map compiles have returned) came to send a permutation's material
	/// resource under the parent's bare path. ShaderHost then resolves that name to the parent
	/// UMaterial and overwrites the parent's own compiled resource with the instance's.
	/// </remarks>
	struct StaticParameterScope
	{
		explicit StaticParameterScope(FStaticParameterSet const* parameters)
			: previous(GCurrentStaticParameters)
		{
			GCurrentStaticParameters = parameters;
		}

		~StaticParameterScope() { GCurrentStaticParameters = previous; }

		StaticParameterScope(StaticParameterScope const&) = delete;
		StaticParameterScope& operator=(StaticParameterScope const&) = delete;

	private:
		FStaticParameterSet const* previous;
	};

	/// <summary>
	/// Returns "#&lt;16 hex&gt;" identifying a static permutation, or an empty string when the set
	/// does not override anything and therefore resolves to the parent's own shader map.
	/// </summary>
	std::wstring StaticPermutationSuffix(FStaticParameterSet const* parameters);

	/// <summary>
	/// Records that shader-compile work just happened. Resets the idle timer the auto-exit watchdog
	/// uses to decide the level has finished compiling.
	/// </summary>
	void NotifyCompileActivity();

	/// <summary>
	/// Starts the auto-exit watchdog: once compiling has started and then been idle for long enough,
	/// the game terminates itself with exit code 0.
	/// </summary>
	/// <remarks>
	/// The level's Kismet ShaderCompilerSequence exits on a scripted trigger, which is not tied to the
	/// compiler actually being finished -- the game can leave while payloads are still being written,
	/// and the host then fails the package with "ShaderClient's pipe did not drain". Exiting from here
	/// instead means the process only goes away after the last payload has been written to the pipe,
	/// because SendDebugInfoToMilanHost writes synchronously.
	/// </remarks>
	void StartCompileIdleWatchdog();
	void StopCompileIdleWatchdog();

	/// <summary>Idle seconds before the watchdog exits. 0 disables auto-exit entirely.</summary>
	extern int CompileIdleExitSeconds;

	void SendShaderCompileFailure(const std::string& message);
	void SendMaterialCompileFailure(FMaterialResource* material);
	void SaveShaderCache(UShaderCache* shaderCache);
	void SaveMaterialBinary(FMaterialResource* material);
	void SaveTerrainMaterialResource(FMaterialResource* material);
	void SaveMaterialHLSL(FMaterialResource* material, wchar_t* materialCode);

	bool CompileSingleShader(FMaterialResource* material, ShaderCompileCombo* scc, FShaderCompilerEnvironment& env, std::vector<FShader*>* shaderList);
	void CaptureShaderJob(FMaterialResource* material, ShaderCompileCombo* scc, FShaderCompilerEnvironment& env);
	void GetShaderTypesToCompile(FMaterial* Material, std::vector<ShaderCompileCombo>& outToCompile);
}
