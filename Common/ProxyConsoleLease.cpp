#include "Common/ProxyConsoleLease.hpp"

#include <algorithm>
#include <cstring>
#include <limits>
#include <string>

#include <spdlog/details/log_msg.h>
#include <spdlog/sinks/base_sink.h>

namespace Common
{
    namespace
    {
        constexpr size_t MaxWriteChunk = 0x7ffff000u;
        constexpr std::string_view AnsiReset = "\x1b[m";

        // All leases can refer to the same console screen buffer.  Serialize
        // the complete write, including temporary attribute changes, so one
        // ASI cannot interleave text while another ASI has its colors applied.
        std::mutex& ConsoleWriteMutex() noexcept
        {
            static std::mutex mutex;
            return mutex;
        }

        WORD ConsoleColorForLevel(spdlog::level::level_enum const level) noexcept
        {
            // Keep this mapping identical to spdlog::sinks::wincolor_sink.
            switch (level)
            {
            case spdlog::level::trace:
                return FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
            case spdlog::level::debug:
                return FOREGROUND_GREEN | FOREGROUND_BLUE;
            case spdlog::level::info:
                return FOREGROUND_GREEN;
            case spdlog::level::warn:
                return FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
            case spdlog::level::err:
                return FOREGROUND_RED | FOREGROUND_INTENSITY;
            case spdlog::level::critical:
                return BACKGROUND_RED | FOREGROUND_RED | FOREGROUND_GREEN |
                    FOREGROUND_BLUE | FOREGROUND_INTENSITY;
            default:
                return 0;
            }
        }

        std::string_view AnsiColorForLevel(spdlog::level::level_enum const level) noexcept
        {
            switch (level)
            {
            case spdlog::level::trace:
                return "\x1b[37m"; // white
            case spdlog::level::debug:
                return "\x1b[36m"; // cyan
            case spdlog::level::info:
                return "\x1b[32m"; // green
            case spdlog::level::warn:
                return "\x1b[33m\x1b[1m"; // bold yellow
            case spdlog::level::err:
                return "\x1b[31m\x1b[1m"; // bold red
            case spdlog::level::critical:
                return "\x1b[1m\x1b[41m"; // bold white on red
            default:
                return {};
            }
        }

        bool WriteFileBytes(HANDLE const output, std::string_view const bytes) noexcept
        {
            size_t offset = 0;
            while (offset < bytes.size())
            {
                DWORD const chunkLength = static_cast<DWORD>(std::min<size_t>(bytes.size() - offset, MaxWriteChunk));
                DWORD written = 0;
                if (WriteFile(output, bytes.data() + offset, chunkLength, &written, nullptr) == FALSE || written != chunkLength)
                    return false;
                offset += written;
            }

            return true;
        }

        bool WriteConsoleUtf8(HANDLE const output, std::string_view const bytes) noexcept
        {
            try
            {
                size_t offset = 0;
                while (offset < bytes.size())
                {
                    size_t chunkLength = std::min<size_t>(bytes.size() - offset,
                        static_cast<size_t>(std::numeric_limits<int>::max()));

                    // Do not split a UTF-8 sequence when a very large log message
                    // crosses the MultiByteToWideChar int-sized API boundary.
                    if (offset + chunkLength < bytes.size())
                    {
                        while (chunkLength > 0 &&
                            (static_cast<unsigned char>(bytes[offset + chunkLength]) & 0xc0u) == 0x80u)
                        {
                            --chunkLength;
                        }
                    }
                    if (chunkLength == 0)
                        return false;

                    int const sourceLength = static_cast<int>(chunkLength);
                    int const wideLength = MultiByteToWideChar(
                        CP_UTF8, 0, bytes.data() + offset, sourceLength, nullptr, 0);
                    if (wideLength <= 0)
                        return false;

                    std::wstring wide(static_cast<size_t>(wideLength), L'\0');
                    int const convertedLength = MultiByteToWideChar(
                        CP_UTF8, 0, bytes.data() + offset, sourceLength, wide.data(), wideLength);
                    if (convertedLength != wideLength)
                        return false;

                    size_t wideOffset = 0;
                    while (wideOffset < wide.size())
                    {
                        DWORD const wideChunkLength = static_cast<DWORD>(std::min<size_t>(
                            wide.size() - wideOffset, MaxWriteChunk));
                        DWORD written = 0;
                        BOOL const writeSucceeded = WriteConsoleW(
                            output, wide.data() + wideOffset, wideChunkLength, &written, nullptr);
                        if (writeSucceeded == FALSE || written != wideChunkLength)
                        {
                            // If the console rejected the write before emitting
                            // any UTF-16 code units, replay the untouched UTF-8
                            // remainder through the byte-oriented path. Do not
                            // replay after a partial write: the UTF-16/UTF-8
                            // boundary is no longer safely recoverable and doing
                            // so could duplicate console output.
                            if (written == 0)
                                return WriteFileBytes(output, bytes.substr(offset));
                            return false;
                        }
                        wideOffset += written;
                    }

                    offset += chunkLength;
                }

                return true;
            }
            catch (...)
            {
                // The sink is noexcept: a failed conversion/allocation must not
                // unwind through spdlog or the SPI callback boundary.
                return false;
            }
        }
    }

    std::shared_ptr<ProxyConsoleLease> ProxyConsoleLease::Create(::LESDK::Initializer& context, char const* ownerName) noexcept
    {
        ISharedProxyInterface* const proxy = context.ProxyInstance;
        if (proxy == nullptr)
        {
            OutputDebugStringA("SPI v4 console broker unavailable: initializer has no proxy instance.\n");
            return nullptr;
        }

        unsigned long proxyVersion = 0;
        if (proxy->GetVersion(&proxyVersion) != SPIReturn::Success || proxyVersion < SPI_VERSION_V4)
        {
            OutputDebugStringA("SPI v4 console broker unavailable: proxy is older than SPI v4.\n");
            return nullptr;
        }

        std::wstring ownerWide;
        try
        {
            ownerWide = L"<unnamed>";
            if (ownerName != nullptr && *ownerName != '\0')
            {
                int const ownerLength = MultiByteToWideChar(CP_UTF8, 0, ownerName, -1, nullptr, 0);
                if (ownerLength > 1)
                {
                    ownerWide.resize(static_cast<size_t>(ownerLength));
                    if (MultiByteToWideChar(CP_UTF8, 0, ownerName, -1, ownerWide.data(), ownerLength) > 0)
                        ownerWide.pop_back();
                }
            }
        }
        catch (...)
        {
            OutputDebugStringA("SPI v4 console broker failed to prepare the owner name.\n");
            return nullptr;
        }

        SPIConsoleLease lease = 0;
        if (!proxy->AcquireConsoleLease(ownerWide.c_str(), &lease) || lease == 0)
        {
            OutputDebugStringA("SPI v4 console broker failed to acquire a lease.\n");
            return nullptr;
        }

        HANDLE output = INVALID_HANDLE_VALUE;
        if (!proxy->OpenConsoleOutput(lease, &output) || output == nullptr || output == INVALID_HANDLE_VALUE)
        {
            proxy->ReleaseConsoleLease(lease);
            OutputDebugStringA("SPI v4 console broker failed to open CONOUT$.\n");
            return nullptr;
        }

        try
        {
            return std::shared_ptr<ProxyConsoleLease>(new ProxyConsoleLease(proxy, lease, output));
        }
        catch (...)
        {
            CloseHandle(output);
            proxy->ReleaseConsoleLease(lease);
            return nullptr;
        }
    }

    ProxyConsoleLease::ProxyConsoleLease(ISharedProxyInterface* const proxy, SPIConsoleLease const lease, HANDLE const output) noexcept
        : proxy_{ proxy }
        , lease_{ lease }
        , output_{ output }
    {
        DWORD consoleMode = 0;
        if (GetConsoleMode(output_, &consoleMode) != FALSE)
        {
            // VT processing is a property of the console output buffer. Do not
            // restore it on release: another ASI may still be using that buffer.
            virtualTerminalProcessing_ = (SetConsoleMode(
                output_, consoleMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING) != FALSE);
        }
    }

    ProxyConsoleLease::~ProxyConsoleLease() noexcept
    {
        std::lock_guard<std::mutex> lock{ writeMutex_ };
        if (output_ != nullptr && output_ != INVALID_HANDLE_VALUE)
        {
            FlushFileBuffers(output_);
            CloseHandle(output_);
            output_ = INVALID_HANDLE_VALUE;
        }

        if (proxy_ != nullptr && lease_ != 0)
            proxy_->ReleaseConsoleLease(lease_);

        lease_ = 0;
        proxy_ = nullptr;
    }

    bool ProxyConsoleLease::Write(std::string_view const bytes) noexcept
    {
        if (bytes.empty())
            return true;

        std::lock_guard<std::mutex> lock{ writeMutex_ };
        std::lock_guard<std::mutex> consoleLock{ ConsoleWriteMutex() };
        if (output_ == nullptr || output_ == INVALID_HANDLE_VALUE)
            return false;

        DWORD consoleMode = 0;
        if (GetConsoleMode(output_, &consoleMode) != FALSE)
        {
            // WriteConsoleW preserves UTF-8 text after conversion while the
            // console host interprets spdlog's ANSI color sequences when VT is
            // available. If VT setup was rejected, retain plain console output.
            return WriteConsoleUtf8(output_, bytes);
        }

        // CONOUT$ is normally a console handle, but retain byte-oriented output
        // for redirected pipes/files and for test or host-provided handles.
        return WriteFileBytes(output_, bytes);
    }

    bool ProxyConsoleLease::WriteColorized(std::string_view const bytes,
        size_t const colorRangeStart, size_t const colorRangeEnd,
        spdlog::level::level_enum const level) noexcept
    {
        if (bytes.empty())
            return true;

        std::lock_guard<std::mutex> lock{ writeMutex_ };
        std::lock_guard<std::mutex> consoleLock{ ConsoleWriteMutex() };
        if (output_ == nullptr || output_ == INVALID_HANDLE_VALUE)
            return false;

        DWORD consoleMode = 0;
        bool const isConsole = GetConsoleMode(output_, &consoleMode) != FALSE;

        // Pipes/files must receive the original bytes without terminal control
        // sequences.
        if (!isConsole)
            return WriteFileBytes(output_, bytes);
        if (colorRangeStart >= colorRangeEnd || colorRangeStart >= bytes.size())
            return WriteConsoleUtf8(output_, bytes);

        size_t const boundedEnd = std::min(colorRangeEnd, bytes.size());
        if (boundedEnd <= colorRangeStart)
            return WriteConsoleUtf8(output_, bytes);

        WORD const color = ConsoleColorForLevel(level);
        if (color == 0)
            return WriteConsoleUtf8(output_, bytes);

        CONSOLE_SCREEN_BUFFER_INFO bufferInfo{};
        if (GetConsoleScreenBufferInfo(output_, &bufferInfo) == FALSE)
        {
            bool const virtualTerminalEnabled = virtualTerminalProcessing_ &&
                (consoleMode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0;
            if (!virtualTerminalEnabled)
                return WriteConsoleUtf8(output_, bytes);

            std::string_view const ansiColor = AnsiColorForLevel(level);
            if (ansiColor.empty())
                return WriteConsoleUtf8(output_, bytes);

            return WriteConsoleUtf8(output_, bytes.substr(0, colorRangeStart)) &&
                WriteConsoleUtf8(output_, ansiColor) &&
                WriteConsoleUtf8(output_, bytes.substr(colorRangeStart, boundedEnd - colorRangeStart)) &&
                WriteConsoleUtf8(output_, AnsiReset) &&
                WriteConsoleUtf8(output_, bytes.substr(boundedEnd));
        }

        WORD const coloredAttributes = static_cast<WORD>(color | (bufferInfo.wAttributes & 0xfff0));

        // Prefer the native Windows console attributes. Some hosts accept
        // ENABLE_VIRTUAL_TERMINAL_PROCESSING but still render ANSI sequences
        // literally or without color. Probe the native path before writing so
        // the prefix is not accidentally emitted with the colored attributes.
        if (SetConsoleTextAttribute(output_, coloredAttributes) != FALSE)
        {
            (void)SetConsoleTextAttribute(output_, bufferInfo.wAttributes);

            bool const prefixWritten = WriteConsoleUtf8(output_, bytes.substr(0, colorRangeStart));
            bool const rangeColorSet = SetConsoleTextAttribute(output_, coloredAttributes) != FALSE;
            bool const rangeWritten = rangeColorSet &&
                WriteConsoleUtf8(output_, bytes.substr(colorRangeStart, boundedEnd - colorRangeStart));
            bool const attributesRestored = SetConsoleTextAttribute(output_, bufferInfo.wAttributes) != FALSE;
            bool const suffixWritten = WriteConsoleUtf8(output_, bytes.substr(boundedEnd));
            return prefixWritten && rangeWritten && attributesRestored && suffixWritten;
        }

        // If native attributes are unavailable, use ANSI/VT only when this
        // output buffer actually reports VT processing as enabled.
        bool const virtualTerminalEnabled = virtualTerminalProcessing_ &&
            (consoleMode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0;
        if (virtualTerminalEnabled)
        {
            std::string_view const ansiColor = AnsiColorForLevel(level);
            if (ansiColor.empty())
                return WriteConsoleUtf8(output_, bytes);

            return WriteConsoleUtf8(output_, bytes.substr(0, colorRangeStart)) &&
                WriteConsoleUtf8(output_, ansiColor) &&
                WriteConsoleUtf8(output_, bytes.substr(colorRangeStart, boundedEnd - colorRangeStart)) &&
                WriteConsoleUtf8(output_, AnsiReset) &&
                WriteConsoleUtf8(output_, bytes.substr(boundedEnd));
        }

        return WriteConsoleUtf8(output_, bytes);
    }

    void ProxyConsoleLease::Flush() noexcept
    {
        std::lock_guard<std::mutex> lock{ writeMutex_ };
        if (output_ != nullptr && output_ != INVALID_HANDLE_VALUE)
            FlushFileBuffers(output_);
    }

    class ProxyConsoleSink final : public spdlog::sinks::base_sink<std::mutex>
    {
    public:
        explicit ProxyConsoleSink(std::shared_ptr<ProxyConsoleLease> lease)
            : lease_{ std::move(lease) }
        {
        }

    protected:
        void sink_it_(spdlog::details::log_msg const& message) override
        {
            spdlog::memory_buf_t formatted;
            this->formatter_->format(message, formatted);
            if (lease_ != nullptr)
                (void)lease_->WriteColorized(std::string_view{ formatted.data(), formatted.size() },
                    message.color_range_start, message.color_range_end, message.level);
        }

        void flush_() override
        {
            if (lease_ != nullptr)
                lease_->Flush();
        }

    private:
        std::shared_ptr<ProxyConsoleLease> lease_;
    };

    std::shared_ptr<spdlog::sinks::sink> MakeProxyConsoleSink(std::shared_ptr<ProxyConsoleLease> lease)
    {
        if (lease == nullptr)
            return nullptr;
        return std::make_shared<ProxyConsoleSink>(std::move(lease));
    }
}
