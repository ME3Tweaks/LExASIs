#include <chrono>
#include "Common/Base.hpp"
#include "PNGScreenshots/Hooks.hpp"
#include "LESDK/Headers.hpp"
#include "fpng/fpng.h" // Yes cpp
#include <algorithm>
#include <cwctype>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>
#include <windows.h>

namespace PNGScreenshots
{
	bool fpngInitialized = false;
	int cachedScreenshotIndex = 1;
	int maxScreenshotIndex = 99999;

	t_AppCreateBitmap* appCreateBitmap_orig = nullptr;

	bool ShouldCopyImagesToClipboard()
	{
		static bool checked = false;
		static bool enabled = false;
		if (checked)
		{
			return enabled;
		}

		checked = true;
		std::wstring commandLine = GetCommandLineW();
		std::wstring currentToken;
		bool inQuotes = false;

		for (wchar_t ch : commandLine)
		{
			if (ch == L'"')
			{
				inQuotes = !inQuotes;
				continue;
			}

			if (!inQuotes && (ch == L' ' || ch == L'\t' || ch == L'\n' || ch == L'\r'))
			{
				if (!currentToken.empty())
				{
					std::transform(currentToken.begin(), currentToken.end(), currentToken.begin(), [](wchar_t value) {
						return static_cast<wchar_t>(std::towlower(value));
					});
					if (currentToken == L"-copyimages")
					{
						enabled = true;
						break;
					}
					currentToken.clear();
				}
			}
			else
			{
				currentToken.push_back(ch);
			}
		}

		if (!currentToken.empty())
		{
			std::transform(currentToken.begin(), currentToken.end(), currentToken.begin(), [](wchar_t value) {
				return static_cast<wchar_t>(std::towlower(value));
			});
			if (currentToken == L"-copyimages")
			{
				enabled = true;
			}
		}

		return enabled;
	}

	bool CopyImageToClipboard(int width, int height, const std::vector<unsigned char>& rgbData)
	{
		if (width <= 0 || height <= 0 || rgbData.empty())
		{
			return false;
		}

		const int bytesPerPixel = 4;
		const int rowBytes = ((width * bytesPerPixel) + 3) & ~3;
		const size_t pixelDataSize = static_cast<size_t>(rowBytes) * static_cast<size_t>(height);
		std::vector<unsigned char> dibData(pixelDataSize, 0);

		const auto rgbPixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);
		for (size_t i = 0; i < rgbPixelCount; ++i)
		{
			const size_t rgbOffset = i * 3;
			const size_t dibOffset = i * 4;
			if (rgbOffset + 2 >= rgbData.size())
			{
				break;
			}

			dibData[static_cast<size_t>(dibOffset)] = rgbData[rgbOffset + 2];
			dibData[static_cast<size_t>(dibOffset) + 1] = rgbData[rgbOffset + 1];
			dibData[static_cast<size_t>(dibOffset) + 2] = rgbData[rgbOffset];
			dibData[static_cast<size_t>(dibOffset) + 3] = 0xFF;
		}

		BITMAPINFOHEADER header{};
		header.biSize = sizeof(BITMAPINFOHEADER);
		header.biWidth = width;
		header.biHeight = -height;
		header.biPlanes = 1;
		header.biBitCount = 32;
		header.biCompression = BI_RGB;
		header.biSizeImage = static_cast<DWORD>(pixelDataSize);

		const size_t totalSize = sizeof(BITMAPINFOHEADER) + dibData.size();
		HGLOBAL clipboardData = GlobalAlloc(GMEM_MOVEABLE, totalSize);
		if (clipboardData == nullptr)
		{
			return false;
		}

		void* memory = GlobalLock(clipboardData);
		if (memory == nullptr)
		{
			GlobalFree(clipboardData);
			return false;
		}

		std::memcpy(memory, &header, sizeof(BITMAPINFOHEADER));
		std::memcpy(static_cast<unsigned char*>(memory) + sizeof(BITMAPINFOHEADER), dibData.data(), dibData.size());
		GlobalUnlock(clipboardData);

		bool clipboardOpened = false;
		for (int attempt = 0; attempt < 5 && !clipboardOpened; ++attempt)
		{
			clipboardOpened = OpenClipboard(nullptr);
			if (!clipboardOpened)
			{
				Sleep(10);
			}
		}

		if (!clipboardOpened)
		{
			GlobalFree(clipboardData);
			return false;
		}

		bool success = EmptyClipboard() && (SetClipboardData(CF_DIB, clipboardData) != nullptr);
		CloseClipboard();

		if (!success)
		{
			GlobalFree(clipboardData); // clipboard never took ownership
		}

		return success;
	}

	// data is an array of FColor structs (BGRA) of size width * height
	void appCreateBitmap_hook(wchar_t* inputBaseName, int width, int height, FColor* data, void* fileManager)
	{
		LEASI_UNUSED(fileManager);
		if (!fpngInitialized)
		{
			// Initialize the library before use
			fpng::fpng_init();
			fpngInitialized = true;
		}

		long byteCount = width * height * 3;

		// Color order needs swapped around for FPNG to access data
		// since nothing supports BRGA.
		std::vector<unsigned char> newImageData(byteCount);
		int pixelIndex = 0; // Which pixel we are one
		long totalCount = width * height; // how many pixels
		long position = 0; // Where to place into vector
		for (long i = 0; i < totalCount; i++)
		{
			auto color = &data[pixelIndex];
			newImageData[position] = color->R;
			newImageData[position + 1L] = color->G;
			newImageData[position + 2L] = color->B;

			pixelIndex++;
			position = pixelIndex * 3;
		}

        // Determine output filename.
        auto path = std::filesystem::path(inputBaseName);
        auto extension = path.extension();
        if (extension != ".png")
        {
            // Calculate a new name / output path inside a per-game subdirectory (e.g. "LE3")
            auto basePath = std::filesystem::path(inputBaseName);
            auto parentDir = basePath.parent_path(); // ScreenShots directory

            // Build subdirectory name like "LE3" using SDK_TARGET_GAME macro
            auto outDir = parentDir / SDK_TARGET_NAME_W;

            // Ensure directory exists (ignore errors)
            std::error_code ec;
            std::filesystem::create_directories(outDir, ec);

			if (std::filesystem::exists(outDir) == false) {
				// Cannot create output folder!!
				return;
			}

            std::filesystem::path outPath;
            std::wstring newFname;
            while (cachedScreenshotIndex < maxScreenshotIndex)
            {
                newFname = std::format(L"ScreenShot{:05}.png", cachedScreenshotIndex);
                cachedScreenshotIndex++; // Increment the cached index immediately.

				// Test the game-specific folder...
                outPath = outDir / newFname;

                // Check if file exists
                if (!std::filesystem::exists(outPath))
                {
                    // File doesn't exist. Use this one
                    path = outPath;
                    break;
                }
                // File exists, go to next one
            }

            if (cachedScreenshotIndex == maxScreenshotIndex)
                return; // Can't take any more screenshots
        }

		// Save the image data using the fpng library.
		// Returns false if can't write, we don't really care though.
		const bool saved = fpng::fpng_encode_image_to_wfile(path.c_str(), newImageData.data(), width, height, 3, 0U); // 3bpp, no flags
		if (saved && ShouldCopyImagesToClipboard())
		{
			CopyImageToClipboard(width, height, newImageData);
		}
	}
}
