#include <spdlog/details/windows_include.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include "Common/Base.hpp"
#include "LESDK/Headers.hpp"
#include "Common/DefaultLogger.hpp"

namespace Common
{
	// Static variables to cache logger parameters
	static std::string _cachedGameName;
	static std::string _cachedLogBaseName;

	// Removes the prefix of all logging.
	void RemoveLoggingPattern() {
		auto DefaultLogger = spdlog::default_logger();
		DefaultLogger->set_pattern(std::string("%v%$"));
	}

	// Sets the default pattern of [DateStamp] (Logger name) Message
	void RestoreLoggingPattern() {
		auto DefaultLogger = spdlog::default_logger();
		DefaultLogger->set_pattern(std::string("%^[%H:%M:%S.%e %l] (") + _cachedGameName + _cachedLogBaseName + ") %v%$");
	}

	// Sets the logging pattern messages are formatted with
	void SetLoggingPattern(std::string pattern) {
		auto DefaultLogger = spdlog::default_logger();
		DefaultLogger->set_pattern(pattern);
	}

	void SetupDefaultLogger(const char* gameName, const char* logBaseName)
	{
		SetupDefaultLogger(gameName, logBaseName, false);
	}

	void SetupDefaultLogger(const char* gameName, const char* logBaseName, bool forceShowConsole) {
		// Cache parameters for later use by RestoreLoggingPattern()
		_cachedGameName = gameName;
		_cachedLogBaseName = logBaseName;

		auto DefaultLogger = spdlog::default_logger();
		DefaultLogger->sinks().clear();

#ifdef _DEBUG
		forceShowConsole = true;
#endif
		if (forceShowConsole) {
			::LESDK::InitializeConsole();
			auto ConsoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
			ConsoleSink->set_level(spdlog::level::trace);
			ConsoleSink->set_color(spdlog::level::trace, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
			ConsoleSink->set_color(spdlog::level::debug, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
			ConsoleSink->set_color(spdlog::level::info, FOREGROUND_BLUE | FOREGROUND_INTENSITY);
			DefaultLogger->sinks().push_back(std::move(ConsoleSink));
		}
		try {
			auto FileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("Logs/" + std::string(logBaseName) + ".log", true);
			FileSink->set_level(spdlog::level::trace);
			DefaultLogger->sinks().push_back(std::move(FileSink));
		}
		catch (const spdlog::spdlog_ex& ex)
		{
			LEASI_ERROR("File log init failed: {}", ex.what());
		}

		DefaultLogger->set_level(spdlog::level::info);
		RestoreLoggingPattern(); // Sets the default pattern

#ifndef _DEBUG
		// Check command line to see if we should turn on lower level logging
		// This is so we can have more logging in release builds for users.
		int numArgs;
		auto args = CommandLineToArgvW(GetCommandLineW(), &numArgs);
		for (int i = 0; i < numArgs; i++) {
			if (wcscmp(args[i], L"-to-trace")) {
				DefaultLogger->set_level(spdlog::level::trace);
			}
		}
#else
		DefaultLogger->set_level(spdlog::level::trace);
#endif

		spdlog::flush_on(spdlog::level::warn);
		spdlog::flush_every(std::chrono::seconds(5));
	}

	void ShutdownLogger() {
		// Flush and release file logger
		try {
			if (spdlog::default_logger())
			{
				spdlog::default_logger()->flush();
				spdlog::default_logger().reset();
				spdlog::drop(_cachedLogBaseName);
			}
		}
		catch (...) {
			// Do nothing
		}

		::LESDK::TerminateConsole();
	}
}