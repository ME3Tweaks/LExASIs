# SPI v4 Console Broker - Implementation Plan

## Purpose

Implement a proxy-owned console service for these ASIs:

- `MilanHost` in `B:\UserProfile\source\repos\ME3Tweaks\MilanClientServer`
- `DebugLogger`, `ShaderClient`, `KismetLogger`, and `TextureOverride` in `B:\UserProfile\source\repos\LExASIs`
- `LEBinkProxy` in `B:\UserProfile\source\repos\ME3Tweaks\LEBinkProxy`

`LEASIMods` is explicitly out of scope. Do not inspect, modify, build, or rely on it.

The preferred design is an SPI v4 console broker, not an ad-hoc exported function discovered with `GetProcAddress`. The existing shared-proxy interface already supplies version negotiation, lifetime context, and a stable dependency direction from ASI to proxy.

## Problem statement

The process owns one Windows console, while many injected ASI DLLs may independently decide to allocate, redirect, or free it. That causes two distinct failures:

1. An ASI can call `FreeConsole` while another ASI still writes to the console. Output disappears or reaches a stale stream.
2. Directly sharing CRT `stdout` state across these DLLs is unsafe. The current proxy is built with `/MT` or `/MTd`, whereas the named ASIs are built with `/MD` or `/MDd`. Redirecting the proxy CRT's `stdout` does not redirect the ASI CRT's `stdout`.

The broker must therefore give each participating ASI a caller-owned Windows `HANDLE` for `CONOUT$`, rather than a `FILE*`, `stdout`, or a promise that another module has redirected standard streams.

## Required behavior

- LEBinkProxy is the only migrated component allowed to allocate a process console.
- Each participating ASI acquires a named console lease after it has obtained SPI v4.
- The ASI writes through a handle or sink it owns; it must not use `LESDK::InitializeConsole`, `LESDK::TerminateConsole`, `AllocConsole`, `FreeConsole`, `freopen`, or a shared CRT standard stream for broker output.
- Releasing a lease means that ASI no longer needs its output handle. It does **not** close the process console in the initial implementation.
- The initial console-lifetime policy is **sticky until process termination**. This is intentional: an unknown legacy ASI can still use the global console and cannot be accurately counted by the broker.
- A short-lived ASI must be able to release its lease and close its own output handle without affecting long-running ASIs.
- Existing SPI v3 ASIs remain loadable. Only the five named ASIs are upgraded to require SPI v4.
- Normal process termination must not wait for workers, destroy logging state, or perform module callbacks under loader lock.

## Non-goals

- Do not attempt to make arbitrary legacy ASIs safe if they directly call `FreeConsole`.
- Do not automatically call `FreeConsole` when the last known v4 lease is released.
- Do not pass CRT streams, `FILE*`, `std::ostream*`, spdlog objects, heap ownership, or STL containers across DLL boundaries.
- Do not turn the broker into a general logging transport. It supplies console ownership and output handles; each ASI retains its own logger, formatting, level, and files.

## SPI v4 contract

### Versioning

In LEBinkProxy's canonical SPI header:

```cpp
#define SPI_VERSION_V3 3
#define SPI_VERSION_V4 4
#define SPI_VERSION_ANY SPI_VERSION_V3
#define SPI_VERSION_LATEST SPI_VERSION_V4
```

Set LEBinkProxy's own `ASI_SPI_VERSION` to `SPI_VERSION_LATEST`. Each migrated ASI declares `SPI_VERSION_V4` as its minimum version. Leave `SPI_VERSION_ANY` at v3 so existing v3 ASIs continue to negotiate as before.

Find the canonical installed/shared copy of this header used by LExSDK and the five ASI projects. Update that public copy and LEBinkProxy together; do not create a second, divergent interface definition.

### ASI release-version bump

This is a behavior-changing compatibility release for every migrated ASI. Bump the ASI's published DLL version as part of the same change; do not leave the existing version while changing its minimum SPI requirement to v4.

- In each LExASIs project, update that project's `SharedVersion.h`: `DebugLogger/SharedVersion.h`, `ShaderClient/SharedVersion.h`, `KismetLogger/SharedVersion.h`, and `TextureOverride/SharedVersion.h`.
- In MilanHost, update `MilanHost/Version/SharedVersion.h`.
- Increment the appropriate release component according to the repository's existing versioning convention. The agent must inspect the current macros and nearby release history before choosing the exact value; do not invent a different version scheme.
- Keep the string and comma-separated resource forms synchronized. In particular, update the macros that produce the human-readable version and `APP_VERSION_RC` (or the equivalent numeric macro).
- Do not edit the RC file's `FILEVERSION` or `PRODUCTVERSION` literals directly: the named `.rc` files already include `SharedVersion.h` and consume `APP_VERSION_RC`. The header is the single source of truth.
- After building, inspect the actual DLL version resource (for example with PowerShell `(Get-Item <dll>).VersionInfo`) and verify it matches the updated header. Also verify the ASI's in-code version display/log value, where present, agrees with the resource.

### ABI rules

Append new virtual functions to the **end** of `ISharedProxyInterface`. Do not reorder, remove, or change v3 slots. v3 clients must remain binary-compatible with v4 proxy instances because they only use the existing prefix of the vtable.

Use ABI-simple Windows types. A suitable initial API is:

```cpp
using SPIConsoleLease = std::uint64_t;

virtual bool AcquireConsoleLease(
    wchar_t const* ownerName,
    SPIConsoleLease* outLease) = 0;

virtual bool OpenConsoleOutput(
    SPIConsoleLease lease,
    HANDLE* outOutputHandle) = 0;

virtual void ReleaseConsoleLease(SPIConsoleLease lease) = 0;
```

Contract details:

- `ownerName` is diagnostic metadata only. Copy it while holding broker state; never retain a caller pointer.
- `AcquireConsoleLease` returns a nonzero opaque token only on success.
- `OpenConsoleOutput` validates the token and returns a newly opened, caller-owned `HANDLE` to `CONOUT$`. The caller must call `CloseHandle` after its writer/sink stops.
- `ReleaseConsoleLease` is idempotent and tolerates an invalid/already-released token. It does not close output handles and does not detach the process console.
- The output writer may use `WriteConsoleW` when possible and must fall back to `WriteFile` for redirected/non-console handles. It must not close a borrowed handle.
- API methods must return failure rather than throw through the ABI.

Use a distinct lease type rather than exposing a raw reference count. It enables diagnostics, idempotent release, and a later policy change without altering the ABI.

## LEBinkProxy implementation

### Add a broker module

Add a focused `ConsoleBroker` implementation under the proxy's utility/SPI area. Its protected state should include:

- a mutex;
- a monotonically increasing, nonzero lease ID;
- `lease ID -> copied owner name` records for diagnostics;
- whether the proxy allocated the current console; and
- optional counters/logging for successful acquires, opens, releases, and invalid tokens.

`AcquireConsoleLease` should:

1. Lock broker state.
2. Detect an existing process console with `GetConsoleWindow`, `GetStdHandle`, or `GetConsoleMode` as appropriate; do not assume the proxy created it.
3. If no console is attached, call `AllocConsole`. Record that the proxy created it only if this call succeeds.
4. Allocate and record the lease token, then return it.

`OpenConsoleOutput` should open `CONOUT$` with `CreateFileW`, requesting write access and compatible sharing flags. It returns that fresh handle to the caller. Do not use `freopen_s`, `SetStdHandle`, or CRT stream rebinding in this path.

`ReleaseConsoleLease` should erase its record. For this version, it must not call `FreeConsole`, even if the map is empty and even if the proxy originally allocated the console.

If a debug-only proxy console is retained for proxy diagnostics, route it through this broker or make it explicitly coexist without mutating another module's CRT streams. Do not leave a second console ownership implementation in `utils/io.h` that can independently call `FreeConsole`.

### Proxy shutdown correctness

Preserve and finish the existing ASI-loader lifecycle work while adding the broker:

- Keep `DLL_PROCESS_DETACH` with non-null `lpReserved` as the process-termination fast path: do not invoke ASI detach callbacks or wait for workers there.
- Track proxy async-attach worker handles outside `DllMain`; do not create an untracked thread that can race manual unload.
- Do not wait, acquire arbitrary locks, or call ASI code directly from `DllMain`.
- In the loader, record successful `PostLoad` invocations in an attachment ledger. On explicit unload, detach from that ledger in reverse successful-attachment order. Do not derive detach order merely from registration order, because preload and postload occur in separate passes.
- Ensure a failed `PostLoad` never receives `OnDetach`.
- Keep callback state per ASI instance, not in static function-local caches shared by all ASIs.

The broker itself needs no `FreeConsole` teardown. It can release maps/mutexes only in safe explicit-shutdown paths, and process termination may intentionally let the OS reclaim them.

## Shared LExASIs logging migration

### Add a local RAII bridge in `Common`

Add a small `Common` component, for example `ProxyConsoleLease` plus a custom spdlog sink. It should hold only:

- `ISharedProxyInterface*` borrowed from `LESDK::Initializer`;
- an SPI console lease token;
- a caller-owned output `HANDLE`; and
- synchronization required by its own logger writes.

Acquisition flow:

1. Require `Init.ProxyInstance` and SPI v4 before creating the bridge.
2. Call `AcquireConsoleLease` with the ASI name.
3. Call `OpenConsoleOutput` for that lease.
4. Construct the local sink/writer around the returned handle.

Destruction flow:

1. Flush and destroy the logger/sink so no writes can still use the handle.
2. `CloseHandle` the output handle.
3. Release the lease.

The sink must write UTF-8 logger bytes safely. Prefer a `WriteFile` implementation; `CONOUT$` accepts byte writes and avoids cross-CRT formatting. If wide console output is desired, perform explicit UTF-8-to-UTF-16 conversion and use `WriteConsoleW` only when `GetConsoleMode` confirms a console handle.

### Update shared logger APIs

Refactor `Common::SetupDefaultLogger` so console output can be obtained through an `LESDK::Initializer` (or a small explicit SPI/owner argument), not through `LESDK::InitializeConsole` plus `stdout_color_sink_mt`.

- Keep the existing file sink and log naming behavior.
- Preserve the existing policy: `forceShowConsole` decides whether console output is requested; TextureOverride's normal release path remains file-only.
- Replace `stdout_color_sink_mt` with the broker-handle sink when console output is requested.
- Make `Common::ShutdownLogger` idempotently flush/drop the logger before releasing its bridge.
- Update `ME3TweaksLogger` similarly, because KismetLogger uses its `OutputToConsole` option.
- Do not retain raw `HANDLE` values in static state after logger destruction.

Add unit-level tests if the current build supports them; otherwise isolate the handle-writer behind a testable function and document its manual verification.

## Per-ASI migration

### DebugLogger

- Change its minimum SPI version from `SPI_VERSION_ANY` to `SPI_VERSION_V4`.
- Pass its `LESDK::Initializer`/proxy context and owner name into the shared logger setup.
- Continue using its own `Common::ShutdownLogger` on detach, but ensure attach failure after lease acquisition tears down the logger and releases the lease.
- Remove any dependence on default `stdout` redirection.

### ShaderClient

- Change the minimum SPI version to v4.
- Use the shared broker-backed default logger for its forced console output.
- Add `Common::ShutdownLogger` to `SpiOnDetach`; currently it does not symmetrically clean up the logger.
- Do not change shader compilation, hook ordering, or cache behavior as part of this migration.

### KismetLogger

- Change the minimum SPI version to v4.
- Remove direct `LESDK::InitializeConsole` and `LESDK::TerminateConsole` calls.
- Update `ME3TweaksLogger(OutputToConsole)` to acquire and own the broker handle via the Kismet initializer.
- On detach, shut down/destroy the Kismet logger before its broker handle and lease are released.
- Preserve file logging and Kismet hook behavior.

### TextureOverride

- Change the minimum SPI version to v4.
- Route debug/forced console output through the shared broker-backed default logger.
- Preserve the normal release behavior: no console request unless its established debug/force policy asks for one.
- Keep its current `ShutdownLogger` path and verify it releases the bridge after dropping the logger.

### MilanHost

- Change its minimum SPI version to v4.
- Make `COutput::Initialize` retain sufficient context to acquire a broker lease; change `InitConsole` as needed so it receives that context rather than calling LESDK console helpers directly.
- Replace `LESDK::InitializeConsole`, `LESDK::TerminateConsole`, and the local `bConsoleOwned` decision with the broker lease lifecycle.
- Replace `stdout`/`stderr`-based console logging in `MilanHost/Base/Logger.cpp` with a Milan-owned handle writer/sink supplied by `COutput`. Never depend on the proxy's CRT streams.
- Preserve the existing serialized/asynchronous logger behavior. Ensure all async logging has stopped and its console writer has been destroyed before closing the handle and releasing the lease.
- Keep the prior fix that does not perform a full logger teardown from the console-control handler. That handler may request shutdown, but normal MilanHost lifetime code owns orderly logger destruction.
- Keep log styling as Milan-owned metadata/formatting. Do not pass spdlog instances or CRT output streams through SPI.

## Compatibility and failure policy

- A v3 ASI continues to load against the v4 proxy. It cannot call the new methods.
- A named v4 ASI must reject an older proxy through the existing minimum-version check, with a clear ASI log/status diagnostic where possible.
- The broker cannot prevent a legacy, non-participating ASI from calling `FreeConsole`; document this limitation prominently in proxy diagnostics and release notes.
- Failure to acquire/open broker output must not crash the game. The ASI may continue with file logging where that is already supported; it should record an actionable failure message through its normal non-console diagnostic path.
- Never silently fall back to `LESDK::InitializeConsole` in a migrated v4 ASI, as that recreates split ownership.

## Implementation order for agents

1. Locate the canonical shared SPI public header and the proxy implementation of `ISharedProxyInterface`; map every distributed header copy before editing.
2. Add SPI v4 constants and append the three methods. Build a v3 compatibility sample/ASI before changing consumers.
3. Implement `ConsoleBroker` and wire it through the proxy interface. Add internal diagnostics without CRT stream rebinding.
4. Complete proxy lifecycle safety: tracked async attach, successful-attachment ledger, reverse detach, and no heavy process-termination work in `DllMain`.
5. Add the Common RAII lease and handle-backed sink; migrate `SetupDefaultLogger`, `ShutdownLogger`, and `ME3TweaksLogger`.
6. Migrate DebugLogger, ShaderClient, KismetLogger, and TextureOverride one at a time, building after each.
7. Migrate MilanHost's separate output/logger pipeline last; keep the change tightly limited to console ownership and output transport.
8. Bump every migrated ASI's `SharedVersion.h` as described above, then build and inspect the resulting DLL version resources.
9. Run static checks, builds, and game-runtime scenarios below. Fix regressions before proceeding to the next ASI.
10. Update developer documentation with the v4 requirement and the sticky-lifetime rationale.

## Validation plan

### Static verification

For the five named ASIs and shared `Common` code, confirm there are no remaining direct calls to:

```text
AllocConsole
FreeConsole
LESDK::InitializeConsole
LESDK::TerminateConsole
freopen
freopen_s
stdout_color_sink_mt
```

Exceptions require a written justification and must not be in a migrated ASI's console lifecycle. Confirm the proxy is the sole migration-path owner of console allocation.

Confirm every new SPI method is appended after the v3 methods, every named ASI requires v4, and all other ASIs still use their pre-existing minimum version.

### Build verification

- Build LEBinkProxy in Debug and Release configurations.
- Build each named LExASIs target for every supported game/configuration it already supports.
- Build MilanHost with its normal solution/configuration.
- Inspect the built version resource for each of the five ASI DLLs and confirm it matches that project's updated `SharedVersion.h`/`APP_VERSION_RC` values.
- Treat a missing native Visual C++ toolchain as an environment limitation, not a code pass. Record exact commands and outputs.

### Runtime matrix

Use a disposable game installation/profile and preserve per-module log files. Test at minimum:

| Scenario | Expected result |
| --- | --- |
| DebugLogger only | Console appears; DebugLogger output is visible; game exit completes without manually closing the console. |
| MilanHost only | MilanHost output is visible; its shutdown completes cleanly. |
| MilanHost then DebugLogger | Both outputs are visible; neither steals the other logger's stream. |
| DebugLogger then MilanHost | Same result with reversed ASI discovery/load order. |
| ShaderClient plus DebugLogger | Both console streams remain visible; ShaderClient detach drops its logger. |
| KismetLogger plus another console user | Kismet logs without allocating/freeing the global console. |
| TextureOverride release behavior | File logging remains correct; no unwanted console when not requested. |
| All five named ASIs | Output remains available throughout play; normal game exit terminates the process. |
| Short-lived v4 test ASI | It acquires, writes, releases, and finishes while a long-lived ASI continues to log. |
| Mixed v3/v4 ASIs | v4 users work; no claim is made that a legacy v3 ASI calling `FreeConsole` is safe. |
| Explicit proxy unload, if supported | No callback is issued to an ASI whose attach did not complete; no worker race or deadlock. |

For every successful scenario, verify both visible output and final log files. A console window remaining open after normal process termination is a failure unless a debugger or an explicitly documented launcher owns it.

## Acceptance criteria

- The five named ASIs use SPI v4 broker leases for all console output.
- No migrated ASI directly owns the process console or assumes another DLL's CRT `stdout` state.
- Multiple named ASIs can produce visible output in either load order.
- A participant can finish early and release its own resources without disrupting other console users.
- Normal game exit completes without requiring the user to close the console window manually.
- Existing v3 ASIs remain compatible with LEBinkProxy v4.
- Each migrated ASI has an incremented published DLL version whose resource comes from its `SharedVersion.h`.
- The final implementation and release notes explicitly state that legacy non-broker ASIs can still break the global console by directly calling `FreeConsole`.
