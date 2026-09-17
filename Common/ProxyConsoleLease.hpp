#pragma once

#include <Windows.h>

#include <memory>
#include <mutex>
#include <string>
#include <string_view>

#include <LESDK/Init.hpp>
#include <spdlog/common.h>
#include <SPI.h>
#include <spdlog/sinks/sink.h>

namespace Common
{
    /**
     * Owns one SPI v4 console lease and its caller-owned CONOUT$ handle.
     * The proxy pointer is borrowed from LESDK::Initializer and is never
     * retained after the lease is released.
     */
    class ProxyConsoleLease final
    {
    public:
        static std::shared_ptr<ProxyConsoleLease> Create(::LESDK::Initializer& context, char const* ownerName) noexcept;

        ~ProxyConsoleLease() noexcept;

        ProxyConsoleLease(ProxyConsoleLease const&) = delete;
        ProxyConsoleLease& operator=(ProxyConsoleLease const&) = delete;

        [[nodiscard]] bool Write(std::string_view bytes) noexcept;
        [[nodiscard]] bool WriteColorized(std::string_view bytes,
            std::size_t colorRangeStart, std::size_t colorRangeEnd,
            spdlog::level::level_enum level) noexcept;
        void Flush() noexcept;

    private:
        ProxyConsoleLease(ISharedProxyInterface* proxy, SPIConsoleLease lease, HANDLE output) noexcept;

        ISharedProxyInterface* proxy_{};
        SPIConsoleLease lease_{};
        HANDLE output_{ INVALID_HANDLE_VALUE };
        bool virtualTerminalProcessing_{};
        std::mutex writeMutex_;
    };

    /** Create a spdlog sink that keeps the bridge alive until the logger drops it. */
    std::shared_ptr<spdlog::sinks::sink> MakeProxyConsoleSink(std::shared_ptr<ProxyConsoleLease> lease);
}
