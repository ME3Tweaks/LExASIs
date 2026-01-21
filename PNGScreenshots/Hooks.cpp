#include <chrono>
#include "Common/Base.hpp"
#include "PNGScreenshots/Hooks.hpp"
#include "fpng/fpng.h" // Yes cpp
#include <filesystem>

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
			// Calculate a new name
			auto outPath = std::filesystem::path(inputBaseName);
			std::wstring newFname;
			while (cachedScreenshotIndex < maxScreenshotIndex)
			{
				newFname = std::format(L"PNG" SDK_TARGET_NAME_W L"ScreenShot%05i.png", cachedScreenshotIndex);
				cachedScreenshotIndex++; // Increment the cached index immediately.
				outPath.replace_filename(newFname);

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
		fpng::fpng_encode_image_to_wfile(path.c_str(), newImageData.data(), width, height, 3, 0U); // 3bpp, no flags
	}
}
