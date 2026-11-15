#include <chrono>
#include "Common/Base.hpp"
#include "Common/Objects.hpp"
#include "ImageScanner/Hooks.hpp"
#include "LESDK/Headers.hpp"
#include <filesystem>
#include <string>

namespace ImageScanner
{
	bool bCaptureInProgress = false;
	int numCaptures = 2000;
	int currentCaptureNum = 0;
	int captureSpacing = 20; // Configure as needed
	int rendersUntilNextCapture = -1;
	int rendersBetweenCaptures = 2; // Configure as needed

	HANDLE hPipe = INVALID_HANDLE_VALUE;
	bool bSendHeaders = false;

	bool ConnectToPipe() {
		hPipe = CreateFileW(
			L"\\\\.\\pipe\\slitscanhost",
			GENERIC_WRITE,
			0,
			nullptr,
			OPEN_EXISTING,
			0,
			nullptr
		);
		return hPipe != INVALID_HANDLE_VALUE;
	}

	void ClosePipe() {
		if (hPipe != INVALID_HANDLE_VALUE) {
			CloseHandle(hPipe);
			hPipe = INVALID_HANDLE_VALUE;
		}
	}

	bool WritePipe(const void* data, DWORD size) {
		DWORD bytesWritten = 0;
		return WriteFile(hPipe, data, size, &bytesWritten, nullptr) && bytesWritten == size;
	}

	template<typename T>
	bool WritePipeValue(T value) {
		return WritePipe(&value, sizeof(T));
	}

	void RunConsoleCommand(const FString& command) {
		auto playerController = ::Common::FindFirstObject<APlayerController>();
		if (playerController) {
			playerController->ConsoleCommand(command, 0);
		}
	}

	void StartScan() {
		currentCaptureNum = 0;
		bSendHeaders = true;

		if (!ConnectToPipe()) {
			LEASI_ERROR(L"Failed to connect to slitscanhost pipe");
			return;
		}

		LEASI_INFO(L"Connected to slitscanhost pipe");
		bCaptureInProgress = true;
	}

	void CorrectCamera() {
		auto playerController = ::Common::FindFirstObject<APlayerController>();
		if (playerController && playerController->Pawn) {
			auto loc = playerController->location;
			// Allow camera to point downwards.
			playerController->Pawn->ViewPitchMax = 16535;
			playerController->Pawn->ViewPitchMin = -16535;
			// Point the camera directly down with no X/Y rot.
			FRotator downRot{ -16535,0,0 };
			playerController->SetRotation(downRot);
		}
	}

	t_UGameEngine_Exec* UGameEngine_Exec_orig = nullptr;
	DWORD UGameEngine_Exec_hook(UGameEngine* const Context, WCHAR const* const Command, void* const Archive)
	{
		if (std::wstring_view const CommandView{ Command };
			CommandView.starts_with(L"is."))
		{
			FString const CommandCopy{ Command };
			if (CommandCopy.Contains(L"is.startscan"))
			{
				LEASI_INFO(L"Beginning image scan");
				//FloodFillPathfindingNetwork();
				StartScan();
			}
			else if (CommandCopy.Contains(L"is.correctcamera")) {
				CorrectCamera();
			}
		}

		return UGameEngine_Exec_orig(Context, Command, Archive);
	}

	t_UObject_ProcessEvent* UObject_ProcessEvent_orig = nullptr;
	void UObject_ProcessEvent_hook(UObject* Context, UFunction* Function, void* Parms, void* Result)
	{
		auto funcFullName = Function->GetFullName();

		if (funcFullName.Equals(L"Function SFXGame.BioHUD.PostRender"))
		{
			if (bCaptureInProgress) {
				if (rendersUntilNextCapture > 0) {
					rendersUntilNextCapture--;

					if (rendersUntilNextCapture == 1) {
						auto playerController = ::Common::FindFirstObject<APlayerController>();
						if (playerController && playerController->Pawn) {
							auto loc = playerController->location;
							playerController->Pawn->ViewPitchMax = 16535;
							playerController->Pawn->ViewPitchMin = -16535;
							playerController->SetLocation({ loc.X + 10, loc.Y, loc.Z });
						}
					}
				}
				else {
					// Perform capture
					RunConsoleCommand(L"screenshot");
					if (currentCaptureNum++ >= numCaptures) {
						LEASI_INFO(L"Completed image scan");
						bCaptureInProgress = false;
					}
					rendersUntilNextCapture = rendersBetweenCaptures;
				}
			}
		}

		UObject_ProcessEvent_orig(Context, Function, Parms, Result);
	}

	t_AppCreateBitmap* appCreateBitmap_orig = nullptr;
	// data is an array of FColor structs (BGRA) of length width * height
	void appCreateBitmap_hook(wchar_t* inputBaseName, int width, int height, FColor* data, void* fileManager)
	{
		LEASI_UNUSED_2(inputBaseName, fileManager);

		if (hPipe == INVALID_HANDLE_VALUE) return;

		if (bSendHeaders) {
			bSendHeaders = false;

			// START_CAPTURE: command byte, width, height, camera movement units
			WritePipeValue(static_cast<unsigned char>(CaptureCommands::START_CAPTURE));
			WritePipeValue(width);
			WritePipeValue(height);
			WritePipeValue(captureSpacing);

			// CAPTURE_COUNT: command byte, number of captures
			WritePipeValue(static_cast<unsigned char>(CaptureCommands::CAPTURE_COUNT));
			WritePipeValue(numCaptures);
		}

		// Get camera position
		int camX = 0, camY = 0;
		auto playerController = ::Common::FindFirstObject<APlayerController>();
		if (playerController) {
			camX = static_cast<int>(playerController->location.X);
			camY = static_cast<int>(playerController->location.Y);
		}

		// CAPTURE: command byte, X, Y, byte count, pixel data
		int byteCount = width * height * static_cast<int>(sizeof(FColor));
		WritePipeValue(static_cast<unsigned char>(CaptureCommands::CAPTURE));
		WritePipeValue(camX);
		WritePipeValue(camY);
		WritePipeValue(byteCount);
		WritePipe(data, static_cast<DWORD>(byteCount));

		// Check if this is the last capture
		if (currentCaptureNum >= numCaptures) {
			WritePipeValue(static_cast<unsigned char>(CaptureCommands::CAPTURE_END));
			ClosePipe();
		}
	}
}
