#include "Common/ME3TweaksLogger.hpp"
#include <format>
#include <spdlog/sinks/basic_file_sink.h>


namespace Common
{

	ME3TweaksLogger::ME3TweaksLogger(const std::string& logger_name, const LogOutput outputType, const spdlog::filename_t& filename)
		: ME3TweaksLogger(logger_name, outputType, filename, nullptr, logger_name.c_str())
	{
	}

	ME3TweaksLogger::ME3TweaksLogger(const std::string& logger_name, const LogOutput outputType, const spdlog::filename_t& filename,
		::LESDK::Initializer& context, const char* ownerName)
		: ME3TweaksLogger(logger_name, outputType, filename, &context, ownerName)
	{
	}

	ME3TweaksLogger::ME3TweaksLogger(const std::string& logger_name, const LogOutput outputType, const spdlog::filename_t& filename,
		::LESDK::Initializer* context, const char* ownerName)
		: m_startTime(std::chrono::steady_clock::now())
	{
		m_logger = std::make_shared<spdlog::logger>(logger_name);

		if (outputType & LogOutput::OutputToConsole)
		{
			if (context != nullptr)
			{
				m_consoleLease = ProxyConsoleLease::Create(*context, ownerName != nullptr ? ownerName : logger_name.c_str());
				if (m_consoleLease != nullptr)
					m_logger->sinks().push_back(MakeProxyConsoleSink(m_consoleLease));
				else
					OutputDebugStringA("ME3TweaksLogger: SPI v4 console broker setup failed.\n");
			}
			else
			{
				OutputDebugStringA("ME3TweaksLogger: console output requested without an SPI initializer.\n");
			}
		}

		if (outputType & LogOutput::OutputToFile)
		{
			try {
				m_logger->sinks().push_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>("Logs/" + filename, true));
			}
			catch (const spdlog::spdlog_ex& ex)
			{
				m_logger->error("File log init failed: {}", ex.what());
			}
		}

		m_logger->set_level(spdlog::level::trace);
		m_logger->set_pattern("%v");

		m_logger->trace("[{} log initialized]", logger_name);
	}

	std::string ME3TweaksLogger::GetBootTimeString() const
	{
		auto now = std::chrono::steady_clock::now();
		auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_startTime);

		auto total_ms = elapsed.count();
		auto seconds = total_ms / 1000;
		auto milliseconds = total_ms % 1000;

		return std::format("{:04}.{:03}", seconds, milliseconds);
	}

} // namespace Common
