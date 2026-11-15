# SPI v4 console broker

The proxy's SPI v4 interface provides shared console ownership without giving
individual ASIs permission to tear down the process console.

## Consumer contract

Consumers that need console output must declare `SPI_VERSION_V4` as their
minimum SPI version. They acquire a named `SPIConsoleLease`, open a private
caller-owned `CONOUT$` handle, and release the handle before releasing the
lease. The shared `Common::ProxyConsoleLease` bridge and its spdlog sink are the
preferred implementation for ASI logging.

The broker copies the owner name, uses nonzero opaque monotonic lease tokens,
and treats API failures as ordinary `false` returns. It allocates a console
only when the process has none. Releasing the final lease does not call
`FreeConsole`; the proxy's console is intentionally sticky for the process
lifetime so another injected module cannot lose its output when one ASI
detaches.

The v3 interface prefix remains unchanged, and v3 consumers continue to load.
They do not receive the broker operations and must not be treated as brokered
console users.

## Lifecycle and compatibility

The proxy performs ASI attach work on tracked workers outside `DllMain`, keeps
an attach-success ledger for reverse-order detach, and exposes
`LEBinkProxyShutdown` for hosts that explicitly unload the proxy. A host should
call that entry point before `FreeLibrary`; process termination uses the
loader-safe detach path.

Legacy non-broker ASIs remain an unavoidable compatibility boundary: an older
ASI that calls `FreeConsole`, redirects CRT streams, or changes shared standard
handles can still disrupt brokered output. Such ASIs should be migrated to SPI
v4 rather than relying on process-global stdout/stderr state.
