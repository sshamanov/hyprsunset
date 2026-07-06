# hyprsunset

Blue-light filter application for Hyprland (Wayland compositor). Sets a Color Transformation Matrix (CTM) on all outputs via the `hyprland-ctm-control-v1` Wayland protocol.

**Version**: 0.3.3  
**License**: BSD 3-Clause  
**Requires**: hyprland >= 0.45.0

---

## Project overview

Single-binary C++26 application. Connects to a Wayland compositor, finds all outputs, and applies a color transformation matrix to reduce blue light. Supports time-based profile switching (e.g., warmer at night), runtime IPC control, and systemd user-unit autostart.

---

## File map

| File | Role |
|---|---|
| `src/main.cpp` | Entry point: CLI argument parsing, bootstrap |
| `src/Hyprsunset.hpp` | Core class declaration, data structures (`SOutput`, `SState`, `SSunsetProfile`), shared pointer aliases (`UP`/`SP`/`WP`) |
| `src/Hyprsunset.cpp` | Core logic: Wayland connection, CTM calculation (Kelvin→matrix), event loop (poll+thread), profile scheduling, signal handling |
| `src/ConfigManager.hpp` / `.cpp` | Configuration file loading via `hyprlang`. Parses `max-gamma` and `profile` blocks. |
| `src/IPCSocket.hpp` / `.cpp` | Unix domain socket IPC server. Accepts text commands at runtime (`gamma`, `temperature`, `identity`, `reset`, `profile`). |
| `src/helpers/Log.hpp` | Logging (`Debug::log`) and assertion macro (`RASSERT`). Log filtering controlled by `Debug::trace` global. |
| `src/meson.build` | Alternative Meson build definition (minimal, no protocol codegen) |
| `CMakeLists.txt` | Primary build system |
| `flake.nix` / `nix/*.nix` | Nix flake packaging |
| `systemd/hyprsunset.service.in` | systemd user unit template |
| `protocols/` | Generated Wayland protocol bindings (`.cpp`/`.hpp`), git-ignored |
| `VERSION` | Plain-text version string, read by CMake |
| `.clang-format` | LLVM-based formatting with 4-space indent, 180-column limit |

---

## Build systems

### CMake (primary)

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Key CMake details:
- **C++26** standard (`CMAKE_CXX_STANDARD 26`)
- Compile flags: `-Wall -Wextra` with several `-Wno-*` suppressions
- Debug builds add `-pg -no-pie -fno-builtin` for profiling
- Checks for `std::chrono::zoned_time` support; adds `-fexperimental-library` if missing (needed on clang/libc++/musl)
- Compile-time defines: `HYPRSUNSET_VERSION`, `GIT_COMMIT_HASH`, `GIT_BRANCH`, `GIT_COMMIT_MESSAGE`, `GIT_DIRTY`
- Protocol codegen via `hyprwayland-scanner` at build time; outputs `protocols/hyprland-ctm-control-v1.{cpp,hpp}` and `protocols/wayland.{cpp,hpp}`
- Installs binary + systemd user unit file

### Meson (alternative, `src/meson.build`)

Minimal fallback. Does NOT include protocol codegen — the generated protocol files must already exist in `protocols/`. Uses `run_command('sh', ...)` to glob `*.cpp` files.

### Nix

```bash
nix build
```

Uses `gcc15Stdenv`. Overlay `hyprsunset-with-deps` composes overlays from all hyprwm dependencies. Version string includes date + short rev.

---

## Dependencies

| Dependency | Min version | Purpose |
|---|---|---|
| wayland-client | — | Wayland client protocol |
| wayland-cursor | — | Cursor support (linked but usage unclear) |
| wayland-protocols | — | Standard protocol XMLs |
| wayland-scanner | — | `wayland.xml` for enum generation |
| hyprutils | 0.2.3 | `Mat3x3`, `CUniquePointer`/`CSharedPointer`/`CWeakPointer`, `Path::findConfig` |
| hyprlang | — | Config file parsing with special categories |
| hyprwayland-scanner | 0.4.0 | Build-time Wayland protocol codegen |
| hyprland-protocols | 0.4.0 | `hyprland-ctm-control-v1.xml` protocol definition |
| pthread | — | Threading |
| librt | — | `timerfd_create` |

**Headers** (key includes): `<chrono>`, `<thread>`, `<mutex>`, `<condition_variable>`, `<sys/poll.h>`, `<sys/timerfd.h>`, `<sys/socket.h>`, `<sys/un.h>`, `<signal.h>`, `<unistd.h>`, `<wayland-client.h>`

---

## Architecture patterns

### Singleton/global pattern

Three global `inline std::unique_ptr` instances declared in headers and defined via `inline`:

```cpp
// Hyprsunset.hpp
inline std::unique_ptr<CHyprsunset> g_pHyprsunset;

// ConfigManager.hpp
inline UP<CConfigManager> g_pConfigManager;

// IPCSocket.hpp
inline std::unique_ptr<CIPCSocket> g_pIPCSocket;
```

Created in `main()` and `CHyprsunset::init()` respectively. Code accesses these globals directly — no dependency injection.

### Shared pointer aliases

```cpp
#define UP CUniquePointer
#define SP CSharedPointer
#define WP CWeakPointer
```

These are from `hyprutils`. Used extensively for Wayland proxy objects (`SP<CCWlOutput>`, `SP<CCHyprlandCtmControlManagerV1>`, etc.).

### Event loop (two-thread design) — `CHyprsunset::startEventLoop()`

This is the most complex part of the codebase. Recent fix (commit `5eac3a9`) addressed a high-CPU busy-loop. The design:

1. **Poll thread**: Runs `poll()` on Wayland fd + timerfd. Calls `wl_display_prepare_read()` before polling to avoid races. When events arrive, sets `shouldProcess = true` and notifies the main loop condition variable.

2. **Main loop thread**: Waits on `loopSignal` condition variable. When woken, dispatches Wayland events via `wl_display_dispatch_pending()`, then either:
   - `reload()` if profile was scheduled
   - `tick()` (IPC processing) otherwise

**Signal flow**: `loopSignal` (condition variable) + `loopRequestMutex` (protects `shouldProcess` + `isScheduled`) + `loopMutex` (for `wait_for`). The poll thread takes `loopRequestMutex`, sets flags, notifies `loopSignal`. The main loop holds `loopMutex` during `wait_for`, then takes `loopRequestMutex` to read/reset flags.

### Profile scheduling — `CHyprsunset::schedule()`

Detached thread that:
1. Finds the next profile time
2. Sleeps in 1-minute increments for the bulk of the wait
3. Fine-sleeps via `sleep_until()` for the final minute
4. Updates `KELVIN`/`GAMMA`/`identity`, sets `isScheduled = true`, notifies main loop

Uses `std::chrono::zoned_time` for timezone-aware scheduling. After suspend/resume the 1-minute polling loop catches up.

### CTM calculation — `calculateMatrix()`

```
Kelvin temperature → RGB vector (Tanner Helland algorithm) → diagonal matrix
                                                               × gamma scalar
                                                               = final 3×3 CTM
```

If `identity` is set, uses `Mat3x3::identity()` instead. If `kelvinSet` is true (CLI override), suppresses "(default)" suffix in logging.

The CTM is applied to every output via `CCHyprlandCtmControlManagerV1::sendSetCtmForOutput()`, then committed via `sendCommit()`.

### IPC protocol

Unix domain socket at `$XDG_RUNTIME_DIR/hypr/$HYPRLAND_INSTANCE_SIGNATURE/.hyprsunset.sock` (or `$XDG_RUNTIME_DIR/hypr/.hyprsunset.sock` without HIS).

Text-based commands (one per connection):

| Command | Args | Effect |
|---|---|---|
| `gamma` | _none_ | Query current gamma % |
| `gamma <N>` | Absolute % (0–max) | Set gamma |
| `gamma +N` / `-N` | Relative % | Adjust gamma (clamped) |
| `temperature` | _none_ | Query current K |
| `temperature <N>` | 1000–20000 K | Set temperature, clears identity |
| `temperature +N` / `-N` | Relative K | Adjust temperature (clamped) |
| `identity` | _none_ | Enable identity matrix |
| `identity get` | — | Query identity state |
| `identity true` / `false` | — | Set identity |
| `reset` | _none_ | Reload current profile |
| `reset temperature` / `gamma` / `identity` | — | Reset single value to profile default |
| `profile` | — | Print current profile details |
| anything else | — | Reply: "invalid command" |

Return value `true` from `mainThreadParseRequest()` means the caller should `reload()` (CTMs need recalculating). Return value `false` means reply was already written with no state change.

### Logging

```cpp
Debug::log(level, format_string, args...);
```

Levels: `NONE` (always shown), `LOG`, `WARN`, `ERR`, `CRIT`, `INFO`, `TRACE`.  
Levels `LOG`/`INFO`/`TRACE` are suppressed **unless** `Debug::trace` is true (set by `--verbose` CLI flag).  
Output goes to `std::cout` with `[LEVEL]` prefix; uses `std::endl` for systemd compatibility.

The `RASSERT` macro logs at CRIT and calls `std::abort()`.

### Config format (hyprlang)

Config file searched via `Hyprutils::Path::findConfig("hyprsunset")` (resolves `$XDG_CONFIG_HOME/hypr/hyprsunset.conf` or similar). Override with `-c`/`--config`.

```hyprlang
max-gamma = 150  # percentage, default 100

profile {
    time = 22:00
    temperature = 3500
    gamma = 0.9
    identity = 1  # 0 = false, 1 = true
}

profile {
    time = 06:00
    temperature = 6000
    gamma = 1.0
    identity = 0
}
```

- `max-gamma`: integer percentage, stored as `Hyprlang::INT`, divided by 100 for internal float. Default 100 (i.e., 1.0×).
- `profile` blocks: anonymous-key special category. Each block identified by its content key. All four properties required or `RASSERT` fires.
- Profiles are sorted by time. The "current" profile is the last one whose time ≤ now. If now is before all profiles, the last profile of the day applies.

### Wayland protocol binding

Generated code in `protocols/` (git-ignored). Two protocol files generated at build time:
- `wayland.{cpp,hpp}` — core Wayland protocol enums (from `wayland-scanner`'s `wayland.xml`)
- `hyprland-ctm-control-v1.{cpp,hpp}` — Hyprland-specific CTM control protocol

The CTM protocol version negotiation tries v2 first; if available, `setBlocked()` callback is registered for duplicate-detection. Output protocol is `wl_output` v3.

`globalRemove` handler erases outputs from the vector when they disappear.

---

## CLI reference

```
hyprsunset [options]

-t, --temperature <K>   Set temperature (1000–20000 K)
-g, --gamma <%>         Set gamma as percentage (0–max)
    --gamma_max <%>     Set maximum gamma percentage (default 100, max 200)
-i, --identity          Use identity matrix (no color change)
-c, --config <path>     Config file path (auto-detected otherwise)
    --verbose           Enable trace/LOG/INFO output
-v, --version           Print version
-h, --help              Print help
```

CLI values override config file. `--temperature` also clears identity mode and sets `kelvinSet = true`.

---

## systemd integration

Template at `systemd/hyprsunset.service.in`, `@CMAKE_INSTALL_PREFIX@` substituted at configure time.

- Type: `simple`
- Part of `graphical-session.target`
- Condition: `WAYLAND_DISPLAY` must be set
- Restart: `on-failure`

---

## CI/CD

Single workflow (`.github/workflows/nix-build.yml`):
- Triggers on push, PR, manual dispatch
- Skips PRs from forks (trust boundary)
- Delegates to `hyprwm/actions/.github/workflows/nix.yml`
- Runs `nix build --print-build-logs --keep-going`

---

## Signal handling

`SIGTERM` and `SIGINT` call `handleExitSignal()` → `CHyprsunset::terminate()`, which sets `shouldProcess = true`, notifies `loopSignal`, and sets `m_bTerminate = true`.

Cleanup sequence in `startEventLoop()`:
1. Clear outputs, reset Wayland proxies
2. Fire a near-future timerfd to unblock poll
3. Join the poll thread
4. Disconnect Wayland display, close timerfd

---

## Code conventions

- **C++26** with `std::chrono::zoned_time`, `std::format`, `std::erase_if`
- **Type aliases**: `UP` = `CUniquePointer`, `SP` = `CSharedPointer`, `WP` = `CWeakPointer` (from hyprutils)
- **Indentation**: 4 spaces, LLVM-based clang-format, 180 column limit
- **Naming**: PascalCase classes (`CHyprsunset`), camelCase methods, UPPER_CASE config values (`KELVIN`, `GAMMA`), `m_` prefix for non-public members in CIPCSocket but NOT in CHyprsunset (inconsistency)
- **Globals**: `g_` prefix (`g_pHyprsunset`, `g_pConfigManager`, `g_pIPCSocket`)
- **Mutex naming**: `m_sEventLoopInternals` with `loopRequestMutex`/`loopMutex` (snake_case for mutex members)
- **Error handling**: Return 0/1 int from init functions, `RASSERT` for fatal, early return for argument validation
- **No exceptions** in normal flow — `try`/`catch` only for parsing user input and config values
- **`inline` globals** in headers — define-once, shared across translation units

---

## Common modification guides

### Adding a new CLI argument
1. Parse in `main.cpp` loop (`for (int i = 1; i < argc; ++i)`)
2. Add corresponding member to `CHyprsunset` in `Hyprsunset.hpp`
3. Add to `printHelp()`

### Adding a new config option
1. Register in `CConfigManager::init()` via `m_config.addConfigValue()` or `m_config.addSpecialConfigValue()`
2. Add getter method to `CConfigManager`
3. Call getter from `CHyprsunset::loadCurrentProfile()` or `main()`

### Adding a new IPC command
1. Add parsing block in `CIPCSocket::mainThreadParseRequest()` (around line 93)
2. Follow existing pattern: check `copy.find("command") == 0`, parse args, set `m_szReply`
3. Return `true` if CTM needs recalculation, `false` otherwise

### Adding a new profile field
1. Add field to `SSunsetProfile` in `Hyprsunset.hpp`
2. Register in `CConfigManager::init()` via `addSpecialConfigValue("profile", ...)`
3. Read in `CConfigManager::getSunsetProfiles()` loop
4. Apply in `CHyprsunset::loadCurrentProfile()`
5. Add reset support in `IPCSocket.cpp` (in `reset` handler)
6. Add display in `profile` IPC handler

### Modifying the CTM calculation
- Core algorithm in `calculateMatrix()` (Hyprsunset.cpp:76)
- Kelvin→RGB conversion in `matrixForKelvin()` (Hyprsunset.cpp:44)
- The matrix is diagonal (R, G, B on diagonal) multiplied by scalar gamma
- Matrix type is `Hyprutils::Math::Mat3x3` (9-element float array)
- Values applied as `wl_fixed` via `wl_fixed_from_double()`

### Changing the event loop
The two-thread design is fragile — recent fixes targeted CPU busy-loop and threading races. Key interactions:
- `loopRequestMutex` protects the shared flags between poll thread and main loop
- `loopMutex` guards the main loop's condition variable wait
- `timerfd` is used only to unblock `poll()` on shutdown
- The schedule thread directly modifies `KELVIN`/`GAMMA`/`identity` (held behind `loopRequestMutex` during flag set)

### Dependency updates
- CMake: update `pkg_check_modules` version constraints in `CMakeLists.txt`
- Nix: update `flake.nix` inputs, run `nix flake lock --update-input <name>`
- New hyprwm dependency: follow pattern in `flake.nix` (add input with `follows` for nixpkgs/systems, add overlay in `hyprsunset-with-deps`)
