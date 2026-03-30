#include <fstream>
#include <string>
#include <cstdint>
#include "Hooks.hpp"
#include "Common/Objects.hpp"
#include "Common/Utils.hpp"
#include <LESDK/Common/Math.hpp>

namespace ConsoleExtension
{
	// ! Constants
	// ========================================

	constexpr auto savedCamsFileName = "savedCams";
	constexpr auto savedCamsLength = 100;

	// ! Global variables
	// ========================================

	FTPOV cachedPOV;
	FTPOV povToLoad;
	bool shouldSetCamPOV = false;
	FTPOV savedPOVs[savedCamsLength];

	// ! Helper functions
	// ========================================

	void writeCamArray(const std::string& file_name, FTPOV* data)
	{
		std::ofstream out;
		out.open(file_name, std::ios::binary);
		out.write(reinterpret_cast<char*>(data), sizeof(FTPOV) * savedCamsLength);
		out.close();
	}

	void readCamArray(const std::string& file_name, FTPOV* data)
	{
		std::ifstream in;
		in.open(file_name, std::ios::binary);
		if (in.good())
		{
			in.read(reinterpret_cast<char*>(data), savedCamsLength * sizeof(FTPOV));
		}
		in.close();
	}

	bool IsCmd(wchar_t** cmd, const wchar_t* check)
	{
		const size_t len = wcslen(check);
		if (_wcsnicmp(*cmd, check, len) == 0)
		{
			*cmd += len;
			return true;
		}
		return false;
	}

	void CauseRemoteEvent(ABioPlayerController* player, SFXName eventName)
	{
		if (player)
		{
			Common::CauseRemoteEvent(player, eventName);
		}
	}

	// ! UEngine::Exec hook
	// ========================================

	t_UEngine_Exec* UEngine_Exec_orig = nullptr;
	unsigned UEngine_Exec_hook(UEngine* Context, wchar_t* cmd, void* unk)
	{
		const auto seps = L" \t";

		// Try the original handler first
		if (UEngine_Exec_orig(Context, cmd, unk))
		{
			return TRUE;
		}

		// Handle our custom commands
		if (IsCmd(&cmd, L"savecam"))
		{
			wchar_t* context = nullptr;
			const wchar_t* const token = wcstok_s(cmd, seps, &context);
			if (token != nullptr)
			{
				if (const int i = _wtoi(token); i >= 0 && i < savedCamsLength)
				{
					savedPOVs[i] = cachedPOV;
					writeCamArray(savedCamsFileName, savedPOVs);
					return TRUE;
				}
			}
		}
		else if (IsCmd(&cmd, L"loadcam"))
		{
			wchar_t* context = nullptr;
			const wchar_t* const token = wcstok_s(cmd, seps, &context);
			if (token != nullptr)
			{
				if (const int i = _wtoi(token); i >= 0 && i < savedCamsLength)
				{
					readCamArray(savedCamsFileName, savedPOVs);
					povToLoad = savedPOVs[i];
					shouldSetCamPOV = true;
					return TRUE;
				}
			}
		}
		else if (IsCmd(&cmd, L"remoteevent") || IsCmd(&cmd, L"re"))
		{
			wchar_t* context = nullptr;
			const wchar_t* const token = wcstok_s(cmd, seps, &context);
			if (token != nullptr)
			{
				if (const auto player = ::Common::FindFirstObject<ABioPlayerController>())
				{
					SFXName eventName{ token, 0, true };
					CauseRemoteEvent(player, eventName);
					return TRUE;
				}
			}
		}
#ifdef SDK_TARGET_LE3
		//LE1 and LE2 have these commands built in
		else if (IsCmd(&cmd, L"streamlevelin"))
		{
			wchar_t* context = nullptr;
			const wchar_t* const token = wcstok_s(cmd, seps, &context);
			if (token != nullptr)
			{
				if (const auto cheatMan = ::Common::FindFirstObject<UBioCheatManager>())
				{
					SFXName levelName{ token, 0, true };
					cheatMan->StreamLevelIn(levelName);
					return TRUE;
				}
			}
		}
		else if (IsCmd(&cmd, L"streamlevelout"))
		{
			wchar_t* context = nullptr;
			const wchar_t* const token = wcstok_s(cmd, seps, &context);
			if (token != nullptr)
			{
				if (const auto cheatMan = ::Common::FindFirstObject<UBioCheatManager>())
				{
					SFXName levelName{ token, 0, true };
					cheatMan->StreamLevelOut(levelName);
					return TRUE;
				}
			}
		}
		else if (IsCmd(&cmd, L"onlyloadlevel"))
		{
			wchar_t* context = nullptr;
			const wchar_t* const token = wcstok_s(cmd, seps, &context);
			if (token != nullptr)
			{
				if (const auto cheatMan = ::Common::FindFirstObject<UBioCheatManager>())
				{
					SFXName levelName{ token, 0, true };
					cheatMan->OnlyLoadLevel(levelName);
					return TRUE;
				}
			}
		}
		else if (IsCmd(&cmd, L"ortho"))
		{
			wchar_t* context = nullptr;
			const wchar_t* const token = wcstok_s(cmd, seps, &context);
			if (token != nullptr)
			{
				orthoWidth = static_cast<float>(_wtof(token));
				orthoEnabled = true;
			}
			else
			{
				orthoEnabled = !orthoEnabled;
			}
			LEASI_INFO("Orthographic projection {}, width={}", orthoEnabled ? "ON" : "OFF", orthoWidth);
			return TRUE;
		}
#endif

		return FALSE;
	}

	// ! UObject::ProcessEvent hook
	// ========================================

#ifdef SDK_TARGET_LE3
	constexpr auto PLAYERTICK_FUNCNAME = L"Function SFXGame.BioPlayerController.PlayerTick";
#else
	constexpr auto PLAYERTICK_FUNCNAME = L"Function Engine.PlayerController.PlayerTick";
#endif

	t_UObject_ProcessEvent* UObject_ProcessEvent_orig = nullptr;
	void UObject_ProcessEvent_hook(UObject* Context, UFunction* Function, void* Parms, void* Result)
	{
		auto funcFullName = Function->GetFullName();

		if (Context->IsA(APlayerController::StaticClass()) && funcFullName.Equals(PLAYERTICK_FUNCNAME))
		{
			const auto playerController = static_cast<APlayerController*>(Context);
			if (playerController->PlayerCamera)
			{
				cachedPOV = playerController->PlayerCamera->CameraCache.POV;
			}
		}
		else if (shouldSetCamPOV && funcFullName.Equals(L"Function Engine.Camera.UpdateCamera"))
		{
			if (const auto camera = static_cast<ACamera*>(Context))
			{
				if (camera->PCOwner && camera->PCOwner->IsA(AActor::StaticClass()))
				{
					shouldSetCamPOV = false;
					const auto actor = static_cast<AActor*>(camera->PCOwner);
					actor->LOCATION = povToLoad.LOCATION;
					actor->Rotation = povToLoad.Rotation;
				}
			}
		}

		UObject_ProcessEvent_orig(Context, Function, Parms, Result);
	}

#ifdef SDK_TARGET_LE3
	// ! StaticConstructObject for LE3
	// ========================================

	t_StaticConstructObject* StaticConstructObject = nullptr;

	// ! Orthographic projection state
	// ========================================

	bool orthoEnabled = false;
	float orthoWidth = 2048.0f;

	// ! ULocalPlayer::CalcSceneView hook
	// ========================================

	t_CalcSceneView* CalcSceneView_orig = nullptr;

	// FSceneView field offsets (LE3)
	namespace SV
	{
		constexpr ptrdiff_t ViewMatrix                         = 0x90;
		constexpr ptrdiff_t ProjectionMatrix                   = 0xD0;
		constexpr ptrdiff_t TranslatedViewMatrix               = 0x190;
		constexpr ptrdiff_t TranslatedViewProjectionMatrix     = 0x1D0;
		constexpr ptrdiff_t InvTranslatedViewProjectionMatrix  = 0x210;
		constexpr ptrdiff_t PreViewTranslation                 = 0x250; // FVector (0x0C) + float (0x04) padding
		constexpr ptrdiff_t ViewProjectionMatrix               = 0x260;
		constexpr ptrdiff_t InvProjectionMatrix                = 0x2A0;
		constexpr ptrdiff_t InvViewProjectionMatrix            = 0x2E0;
		constexpr ptrdiff_t ViewOrigin                         = 0x320; // FVector4 (FPlane)
	}

	template<typename T>
	T& At(void* base, ptrdiff_t off) { return *reinterpret_cast<T*>(static_cast<uint8_t*>(base) + off); }

	// General 4x4 matrix inverse via cofactor expansion
	FMatrix MatrixInverse(const FMatrix& m)
	{
		typedef float A[4][4];
		const A& s = reinterpret_cast<const A&>(m);

		float s0 = s[0][0] * s[1][1] - s[1][0] * s[0][1];
		float s1 = s[0][0] * s[1][2] - s[1][0] * s[0][2];
		float s2 = s[0][0] * s[1][3] - s[1][0] * s[0][3];
		float s3 = s[0][1] * s[1][2] - s[1][1] * s[0][2];
		float s4 = s[0][1] * s[1][3] - s[1][1] * s[0][3];
		float s5 = s[0][2] * s[1][3] - s[1][2] * s[0][3];

		float c5 = s[2][2] * s[3][3] - s[3][2] * s[2][3];
		float c4 = s[2][1] * s[3][3] - s[3][1] * s[2][3];
		float c3 = s[2][1] * s[3][2] - s[3][1] * s[2][2];
		float c2 = s[2][0] * s[3][3] - s[3][0] * s[2][3];
		float c1 = s[2][0] * s[3][2] - s[3][0] * s[2][2];
		float c0 = s[2][0] * s[3][1] - s[3][0] * s[2][1];

		float det = s0 * c5 - s1 * c4 + s2 * c3 + s3 * c2 - s4 * c1 + s5 * c0;
		if (det == 0.0f) return IdentityMatrix;
		float invDet = 1.0f / det;

		FMatrix r;
		A& d = reinterpret_cast<A&>(r);

		d[0][0] = ( s[1][1] * c5 - s[1][2] * c4 + s[1][3] * c3) * invDet;
		d[0][1] = (-s[0][1] * c5 + s[0][2] * c4 - s[0][3] * c3) * invDet;
		d[0][2] = ( s[3][1] * s5 - s[3][2] * s4 + s[3][3] * s3) * invDet;
		d[0][3] = (-s[2][1] * s5 + s[2][2] * s4 - s[2][3] * s3) * invDet;

		d[1][0] = (-s[1][0] * c5 + s[1][2] * c2 - s[1][3] * c1) * invDet;
		d[1][1] = ( s[0][0] * c5 - s[0][2] * c2 + s[0][3] * c1) * invDet;
		d[1][2] = (-s[3][0] * s5 + s[3][2] * s2 - s[3][3] * s1) * invDet;
		d[1][3] = ( s[2][0] * s5 - s[2][2] * s2 + s[2][3] * s1) * invDet;

		d[2][0] = ( s[1][0] * c4 - s[1][1] * c2 + s[1][3] * c0) * invDet;
		d[2][1] = (-s[0][0] * c4 + s[0][1] * c2 - s[0][3] * c0) * invDet;
		d[2][2] = ( s[3][0] * s4 - s[3][1] * s2 + s[3][3] * s0) * invDet;
		d[2][3] = (-s[2][0] * s4 + s[2][1] * s2 - s[2][3] * s0) * invDet;

		d[3][0] = (-s[1][0] * c3 + s[1][1] * c1 - s[1][2] * c0) * invDet;
		d[3][1] = ( s[0][0] * c3 - s[0][1] * c1 + s[0][2] * c0) * invDet;
		d[3][2] = (-s[3][0] * s3 + s[3][1] * s1 - s[3][2] * s0) * invDet;
		d[3][3] = ( s[2][0] * s3 - s[2][1] * s1 + s[2][2] * s0) * invDet;

		return r;
	}

	void* CalcSceneView_hook(ULocalPlayer* thisPlayer, void* ViewFamily, FVector& ViewLocation,
	                         FRotator& ViewRotation, void* Viewport, void* ViewDrawer)
	{
		void* sceneView = CalcSceneView_orig(thisPlayer, ViewFamily, ViewLocation, ViewRotation, Viewport, ViewDrawer);
		if (!sceneView || !orthoEnabled)
			return sceneView;

		auto& projMatrix  = At<FMatrix>(sceneView, SV::ProjectionMatrix);
		auto& viewMatrix  = At<FMatrix>(sceneView, SV::ViewMatrix);

		// Derive aspect ratio from the perspective matrix before overwriting
		float aspectRatio = (projMatrix.XPlane.X != 0.0f)
			? (projMatrix.YPlane.Y / projMatrix.XPlane.X)
			: 1.0f;

		// Build orthographic projection matrix
		float halfWidth  = orthoWidth / 2.0f;
		float halfHeight = halfWidth / aspectRatio;
		constexpr float HALF_WORLD_MAX = 524288.0f;
		constexpr float zScale = 0.5f / HALF_WORLD_MAX;

		FMatrix ortho{};
		ortho.XPlane = { 1.0f / halfWidth, 0.0f, 0.0f, 0.0f };
		ortho.YPlane = { 0.0f, 1.0f / halfHeight, 0.0f, 0.0f };
		ortho.ZPlane = { 0.0f, 0.0f, zScale, 0.0f };
		ortho.WPlane = { 0.0f, 0.0f, 0.5f, 1.0f };

		// Overwrite ProjectionMatrix
		projMatrix = ortho;

		// Recompute derived matrices
		FMatrix invProj       = MatrixInverse(ortho);
		FMatrix invViewMatrix = MatrixInverse(viewMatrix);
		FMatrix viewProj      = viewMatrix * ortho;
		FMatrix invViewProj   = invProj * invViewMatrix;  // Scene.cpp:195

		At<FMatrix>(sceneView, SV::ViewProjectionMatrix)   = viewProj;
		At<FMatrix>(sceneView, SV::InvProjectionMatrix)     = invProj;
		At<FMatrix>(sceneView, SV::InvViewProjectionMatrix) = invViewProj;

		// For ortho: ViewOrigin = InvViewMatrix * normalize(0,0,-1), W=0
		// PreViewTranslation = (0,0,0)
		FVector viewDir{};
		{
			// InvViewMatrix.TransformNormal(FVector(0,0,-1)).SafeNormal()
			// TransformNormal multiplies by the upper-left 3x3
			float dx = -invViewMatrix.ZPlane.X;
			float dy = -invViewMatrix.ZPlane.Y;
			float dz = -invViewMatrix.ZPlane.Z;
			float len = sqrtf(dx * dx + dy * dy + dz * dz);
			if (len > 1e-8f) { dx /= len; dy /= len; dz /= len; }
			viewDir = { dx, dy, dz };
		}
		At<FPlane>(sceneView, SV::ViewOrigin) = { viewDir.X, viewDir.Y, viewDir.Z, 0.0f };
		At<FVector>(sceneView, SV::PreViewTranslation) = { 0.0f, 0.0f, 0.0f };

		// TranslatedViewMatrix = FTranslationMatrix(-PreViewTranslation) * ViewMatrix
		// With PreViewTranslation=(0,0,0), this is just ViewMatrix
		At<FMatrix>(sceneView, SV::TranslatedViewMatrix) = viewMatrix;

		FMatrix transViewProj = viewMatrix * ortho;
		FMatrix invTransVP    = MatrixInverse(transViewProj);

		At<FMatrix>(sceneView, SV::TranslatedViewProjectionMatrix)    = transViewProj;
		At<FMatrix>(sceneView, SV::InvTranslatedViewProjectionMatrix) = invTransVP;

		return sceneView;
	}
#endif
}
