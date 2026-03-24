#include "CoverClient/EditorPipe.hpp"

#include <cstdio>

namespace CoverClient
{
	static HANDLE   s_EditorPipe = INVALID_HANDLE_VALUE;
	static OVERLAPPED s_Overlapped{};
	static char     s_WriteBuf[128];

	static HANDLE GetEditorPipe()
	{
		if (s_EditorPipe == nullptr || s_EditorPipe == INVALID_HANDLE_VALUE)
		{
			s_EditorPipe = CreateFileW(
				L"\\\\.\\pipe\\pathfindingnetworkeditor" SDK_TARGET_NAME_W,
				GENERIC_WRITE, FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
				FILE_FLAG_OVERLAPPED, nullptr);
		}
		return s_EditorPipe;
	}

	static void SendLine(const char* buf, DWORD len)
	{
		HANDLE pipe = GetEditorPipe();
		if (pipe == INVALID_HANDLE_VALUE || pipe == nullptr)
			return;

		if (len > sizeof(s_WriteBuf))
			return;

		// Cancel any prior pending write so the static buffer and OVERLAPPED
		// are safe to reuse. For the tiny payloads sent here this is normally
		// a no-op because the kernel will have drained the pipe buffer already.
		CancelIo(pipe);

		memcpy(s_WriteBuf, buf, len);
		ZeroMemory(&s_Overlapped, sizeof(s_Overlapped));

		if (!WriteFile(pipe, s_WriteBuf, len, nullptr, &s_Overlapped))
		{
			if (GetLastError() != ERROR_IO_PENDING)
			{
				CloseHandle(s_EditorPipe);
				s_EditorPipe = INVALID_HANDLE_VALUE;
			}
		}
		// Fire-and-forget: do not wait for the write to complete.
	}

	static float s_LastPlayerX = 0.f, s_LastPlayerY = 0.f, s_LastPlayerZ = 0.f;
	static int   s_LastPlayerPitch = 0, s_LastPlayerYaw = 0, s_LastPlayerRoll = 0;
	static bool  s_PlayerSent = false;

	static float s_LastCamX = 0.f, s_LastCamY = 0.f, s_LastCamZ = 0.f;
	static int   s_LastCamPitch = 0, s_LastCamYaw = 0, s_LastCamRoll = 0;
	static float s_LastCamFOV = 0.f;
	static bool  s_CamSent = false;

	void UpdateEditorPlayerData(const FVector& location, const FRotator& rotation)
	{
		if (s_PlayerSent &&
			location.X == s_LastPlayerX && location.Y == s_LastPlayerY && location.Z == s_LastPlayerZ &&
			rotation.Pitch == s_LastPlayerPitch && rotation.Yaw == s_LastPlayerYaw && rotation.Roll == s_LastPlayerRoll)
		{
			return;
		}

		char buf[128];
		int len = snprintf(buf, sizeof(buf), "PLAYER %g %g %g %d %d %d\n",
			location.X, location.Y, location.Z,
			rotation.Pitch, rotation.Yaw, rotation.Roll);
		if (len > 0)
			SendLine(buf, static_cast<DWORD>(len));

		s_LastPlayerX = location.X;
		s_LastPlayerY = location.Y;
		s_LastPlayerZ = location.Z;
		s_LastPlayerPitch = rotation.Pitch;
		s_LastPlayerYaw = rotation.Yaw;
		s_LastPlayerRoll = rotation.Roll;
		s_PlayerSent = true;
	}

	void UpdateEditorCameraData(const FVector& location, const FRotator& rotation, float fov)
	{
		if (s_CamSent &&
			location.X == s_LastCamX && location.Y == s_LastCamY && location.Z == s_LastCamZ &&
			rotation.Pitch == s_LastCamPitch && rotation.Yaw == s_LastCamYaw && rotation.Roll == s_LastCamRoll &&
			fov == s_LastCamFOV)
		{
			return;
		}

		char buf[128];
		int len = snprintf(buf, sizeof(buf), "CAMERA %g %g %g %d %d %d %g\n",
			location.X, location.Y, location.Z,
			rotation.Pitch, rotation.Yaw, rotation.Roll, fov);
		if (len > 0)
			SendLine(buf, static_cast<DWORD>(len));

		s_LastCamX = location.X;
		s_LastCamY = location.Y;
		s_LastCamZ = location.Z;
		s_LastCamPitch = rotation.Pitch;
		s_LastCamYaw = rotation.Yaw;
		s_LastCamRoll = rotation.Roll;
		s_LastCamFOV = fov;
		s_CamSent = true;
	}
}
