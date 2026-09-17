#include <spdlog/details/windows_include.h>
#include <spdlog/sinks/basic_file_sink.h>
#include "Common/Base.hpp"
#include "LESDK/Headers.hpp"
#include "Common/DefaultLogger.hpp"
#include "Common/ProxyConsoleLease.hpp"

namespace Common
{
	// Static variables to cache logger parameters
	static std::string _cachedGameName;
	static std::string _cachedLogBaseName;
	static std::shared_ptr<ProxyConsoleLease> _consoleLease;

	static void SetupDefaultLoggerInternal(const char* gameName, const char* logBaseName, bool forceShowConsole, ::LESDK::Initializer* context);

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
		SetupDefaultLoggerInternal(gameName, logBaseName, forceShowConsole, nullptr);
	}

	void SetupDefaultLogger(const char* gameName, const char* logBaseName, bool forceShowConsole, ::LESDK::Initializer& context) {
		SetupDefaultLoggerInternal(gameName, logBaseName, forceShowConsole, &context);
	}

	static void SetupDefaultLoggerInternal(const char* gameName, const char* logBaseName, bool forceShowConsole, ::LESDK::Initializer* context) {
		// Cache parameters for later use by RestoreLoggingPattern()
		_cachedGameName = gameName;
		_cachedLogBaseName = logBaseName;

		auto DefaultLogger = spdlog::default_logger();
		DefaultLogger->sinks().clear();
		_consoleLease.reset();

#ifdef _DEBUG
		forceShowConsole = true;
#endif
		if (forceShowConsole) {
			if (context != nullptr)
			{
				_consoleLease = ProxyConsoleLease::Create(*context, logBaseName);
				if (_consoleLease != nullptr)
				{
					auto ConsoleSink = MakeProxyConsoleSink(_consoleLease);
					ConsoleSink->set_level(spdlog::level::trace);
					DefaultLogger->sinks().push_back(std::move(ConsoleSink));
				}
				else
				{
					OutputDebugStringA("Common logger: console output requested but SPI v4 broker setup failed.\n");
				}
			}
			else
			{
				OutputDebugStringA("Common logger: console output requested without an SPI initializer.\n");
			}
		}
		try {
			auto FileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("Logs/" + std::string(logBaseName) + ".log", true);
#ifdef _DEBUG
			FileSink->set_level(spdlog::level::trace);
#endif
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
        if (args) {
            for (int i = 0; i < numArgs; i++) {
                // wcscmp returns 0 when strings are equal, so check for == 0
                if (wcscmp(args[i], L"-trace-log-level") == 0) {
                    LEASI_INFO("-trace-log-level enabled!");
                    DefaultLogger->set_level(spdlog::level::trace);
                    break; // no need to keep scanning
                }
            }
            LocalFree(args);
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
			auto DefaultLogger = spdlog::default_logger();
			if (DefaultLogger)
			{
				DefaultLogger->flush();
				DefaultLogger->sinks().clear();
				spdlog::drop(DefaultLogger->name());
			}
			_consoleLease.reset();
		}
		catch (...) {
			// Do nothing
		}
	}
}
