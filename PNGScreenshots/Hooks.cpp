#include <chrono>
#include "Common/Base.hpp"
#include "PNGScreenshots/Hooks.hpp"
#include "LESDK/Headers.hpp"
#include "fpng/fpng.h" // Yes cpp
#include <filesystem>
#include <string>

namespace PNGScreenshots
{
	bool fpngInitialized = false;
	int cachedScreenshotIndex = 1;
	int maxScreenshotIndex = 99999;

	t_AppCreateBitmap* appCreateBitmap_orig = nullptr;

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
		fpng::fpng_encode_image_to_wfile(path.c_str(), newImageData.data(), width, height, 3, 0U); // 3bpp, no flags
	}
}
