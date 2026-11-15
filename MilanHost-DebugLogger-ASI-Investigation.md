# LE3 MilanHost and DebugLogger ASI Investigation

## Executive summary

The LE3 behavior has two independent root causes and one shared teardown design problem.

The load-order-dependent DebugLogger behavior is caused by `LEBinkProxy` caching each ASI's preload and attach-mode decision in function-local `static` variables. Those variables are shared by every `AsiPluginLoadInfo` instance, so the first ASI examined determines how later ASIs are classified. A DebugLogger plugin loaded after MilanHost can therefore be skipped during preload and incorrectly attached during postload, with its asynchronous attach mode also misclassified.

The process-exit problem is caused by a blocking MilanHost shutdown path being reached from `LEBinkProxy`'s `DllMain(DLL_PROCESS_DETACH)`. MilanHost waits for its asynchronous logger thread to finish before the proxy reaches its own console teardown. If the logger flush or thread join blocks, the console remains visible and the process remains alive.

Both projects also treat the process console as if they independently own it. The proxy opens and closes the console, while MilanHost initializes and terminates a console through the shared LESDK code. This makes the final output stream and teardown behavior dependent on which ASI callback runs first.

## Scope

This report covers the source currently present in:

- `B:\UserProfile\source\repos\ME3Tweaks\LEBinkProxy`
- `B:\UserProfile\source\repos\ME3Tweaks\MilanClientServer`

The analysis is specific to the LE3 SPI and ASI lifecycle implemented by those repositories.

## Observed symptoms

1. With MilanHost and DebugLogger enabled together, selecting Exit leaves the console window open and the game process does not finish until the console is manually closed.
2. When DebugLogger is loaded after MilanHost, DebugLogger produces no console output.
3. Changing ASI filename or load order changes the behavior.

The third observation is especially important because it points to shared loader state rather than a deterministic pattern-scan failure inside one plugin.

## Finding one ASI phase decisions are shared across plugins

`LEBinkProxy/src/modules/asi_loader.h` defines `AsiPluginLoadInfo::ShouldPreload()`, `ShouldPostload()`, and `ShouldSpawnThread()` as member functions, but each function contains a function-local `static bool ranCall`.

Relevant locations:

- `LEBinkProxy/src/modules/asi_loader.h:82-103` - `ShouldPreload()`
- `LEBinkProxy/src/modules/asi_loader.h:104-125` - `ShouldPostload()`
- `LEBinkProxy/src/modules/asi_loader.h:126-146` - `ShouldSpawnThread()`

A function-local static is created once per function for the entire process. It is not created once per `AsiPluginLoadInfo` object. The cached result is therefore shared by all loaded ASIs.

For example, if MilanHost is the first plugin evaluated and reports postload:

1. MilanHost's `SpiShouldPreload()` result is queried and cached as false.
2. A later DebugLogger instance sees `ranCall == true` and does not query its own `SpiShouldPreload()` export.
3. DebugLogger's own `shouldPreloadFetched_` member remains at its default false value.
4. DebugLogger is skipped by `PreLoad()`.
5. During `PostLoad()`, the later instance appears to be a postload plugin because `ShouldPostload()` returns `!false`.

The same issue affects the attach-mode decision. If the first plugin is sequential, a later asynchronous plugin can be treated as sequential because its per-instance cached value was never populated.

This makes the result depend on the sorted filename order used by `AsiLoaderModule::Activate()`.

### Consequence

DebugLogger can run at the wrong lifecycle point or not run at its intended point at all. That is sufficient to explain why changing its position relative to MilanHost changes whether any DebugLogger console output appears.

### Required correction

The preload and asynchronous-attach results must be cached per `AsiPluginLoadInfo` instance. Each instance needs its own resolved flag in addition to its cached Boolean value, because false is a valid result.

`ShouldPostload()` should derive its answer from that same instance's resolved preload result rather than maintaining a separate process-wide cache.

## Finding two teardown blocks before console cleanup

The proxy performs substantial shutdown work directly from `DllMain`:

- `LEBinkProxy/src/dllmain.cpp:305-321` defines `OnDetach()`.
- `LEBinkProxy/src/dllmain.cpp:323-335` calls `OnDetach()` for `DLL_PROCESS_DETACH`.

`OnDetach()` first calls `AsiLoaderModule::Deactivate()` at `LEBinkProxy/src/dllmain.cpp:309-310`. `Deactivate()` invokes every registered ASI's `SpiOnDetach()` callback at `LEBinkProxy/src/modules/asi_loader.h:439-452`.

MilanHost's detach callback enters `SingleDetachImpl()` at `MilanHost/Shell/Entry.cpp:89-132`. Its final component shutdown calls `COutput::Terminate()` at `MilanHost/Shell/Entry.cpp:119-126`.

`COutput::Terminate()` then calls the asynchronous logger cleanup at `MilanHost/Components/COutput.cpp:109-125`. `AsyncMilanLogger::Cleanup()` performs a flush and then waits for the processing thread with `ProcessThread.join()` at `MilanHost/Base/Logger.cpp:363-380`.

The proxy does not reach `Utils::TeardownOutput()` until after all ASI detach callbacks return, at `LEBinkProxy/src/dllmain.cpp:316-317`.

The effective sequence is therefore:

```text
process shutdown
  -> bink2w64 DllMain DLL_PROCESS_DETACH
    -> LEBinkProxy OnDetach
      -> ASI detach callbacks
        -> MilanHost SingleDetachImpl
          -> AsyncMilanLogger Flush and join
      -> proxy console teardown
```

If the flush wait or logger thread join does not complete, the proxy's console teardown is never reached. That explains why the console remains visible at the same time that the process remains alive.

### Existing logger change

The current working tree contains a MilanHost change that holds `WriteMutex` while updating the exit and flush predicates and notifying the condition variables. This addresses a real lost-notification race in the logger implementation.

The relevant code is in `MilanHost/Base/Logger.cpp:363-379` and `MilanHost/Base/Logger.cpp:437-446`.

That change is useful, but it does not make a blocking logger join safe inside `DllMain`. It also requires repeated runtime close-cycle testing with the actual built ASI.

## Finding three console ownership is not centralized

The proxy opens a console during synchronous attach:

- `LEBinkProxy/src/dllmain.cpp:233-241`
- `LEBinkProxy/src/utils/io.h:25-38`

The proxy's `OpenConsole()` calls `AllocConsole()` and redirects `stdout` and `stderr`. Its `CloseConsole()` calls `FreeConsole()` at `LEBinkProxy/src/utils/io.h:39-43`.

MilanHost independently initializes and terminates the console:

- `MilanHost/Components/COutput.cpp:183-187` calls `LESDK::InitializeConsole()`.
- `MilanHost/Components/COutput.cpp:123-125` calls `LESDK::TerminateConsole()`.
- `ThirdParty/LExSDKv2/Src/LESDK/Common/Common.cpp:386-400` implements those operations with unconditional `AllocConsole()` and `FreeConsole()` calls.

Neither layer records whether it created the console or whether another ASI still needs it. As a result, one component can detach the shared console while another component is still flushing output. Reopening `stdout` from multiple DLLs also makes the C runtime stream state order-dependent.

The loader compounds this by detaching plugins in forward registration order at `LEBinkProxy/src/modules/asi_loader.h:443-452`. Resource ownership normally needs to unwind in reverse attachment order.

## Recommended remediation

### Priority one fix the ASI loader state

Change `AsiPluginLoadInfo` so that each plugin independently resolves and caches:

- its preload or postload declaration;
- its sequential or asynchronous attach declaration;
- whether its attach callback actually ran and succeeded.

Remove all function-local `static bool ranCall` variables from the three query methods.

### Priority two centralize console ownership

Choose one owner for the process console. The least ambiguous arrangement is for LEBinkProxy to own console creation and destruction, while ASIs only write to the streams provided by the proxy.

At minimum:

- check whether a console is already attached before calling `AllocConsole()`;
- record whether the current component created the console;
- call `FreeConsole()` only by the component that owns it;
- do not let an ASI tear down a console still used by another ASI;
- unregister any console-control handler during normal cleanup.

### Priority three move blocking shutdown out of DllMain

Do not perform ASI callbacks, condition-variable waits, thread joins, hook teardown, or complex allocation work from `DLL_PROCESS_DETACH`.

Use an explicit shutdown path that runs on a normal worker or game-controlled thread. For process termination, the proxy can avoid unnecessary cleanup because the operating system is already tearing down the process. A separate safe path is still needed if the proxy is explicitly unloaded while the game continues running.

### Priority four make attach and detach ordering explicit

The loader currently creates asynchronous attach threads with `CreateThread()` but does not retain their handles. It therefore cannot prove that an asynchronous attach has completed before shutdown begins.

The loader should retain those thread handles, wait for them before detaching, and detach successfully attached plugins in reverse attachment order.

### Priority five harden MilanHost cleanup

MilanHost should:

- keep the condition-variable locking correction;
- make logger cleanup idempotent;
- check `gp_mainLogger` before calling `Cleanup()`;
- avoid freeing the shared console from `COutput::Terminate()` if LEBinkProxy owns it;
- remove its console-control handler during cleanup;
- ensure no late hook callback can write through a logger after the logger has been destroyed.

## Validation plan

The following tests isolate the causes:

1. Build the loader change and run MilanHost alone. Confirm that the proxy and MilanHost both initialize and shut down without leaving the console attached.
2. Run MilanHost with the synchronous logger option `-synclogger`. If the hang disappears, the asynchronous logger cleanup path is involved.
3. Run MilanHost and DebugLogger with both filename orders. After the loader fix, both ASIs should retain their declared preload/postload and attach-mode behavior regardless of filename order.
4. Repeat several normal in-game exit cycles and record the final lines of `MilanHost.log` and the proxy log. A successful cycle must reach the final detach messages and terminate the process without manually closing the console.
5. Test a console close event separately from an in-game exit. These are different shutdown paths and should not both invoke full component teardown concurrently.
6. Test an explicit proxy unload, if supported, separately from process termination. The solution must preserve safe cleanup for unload while avoiding blocking work during process termination.

## Conclusion

The DebugLogger load-order issue is deterministic and should be fixed first in `LEBinkProxy` by removing the shared function-local decision caches. The exit hang is a teardown sequencing problem: a potentially blocking MilanHost logger shutdown is invoked through the proxy's `DllMain` detach path before console cleanup can run. Centralizing console ownership, tracking asynchronous attach threads, reversing detach order, and moving heavy shutdown out of `DllMain` are the changes needed for reliable combined ASI behavior.
