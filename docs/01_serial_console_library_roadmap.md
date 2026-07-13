# Bounded Serial Console Library — Roadmap

## Phase 0 — Repository and anchor setup

Goal: create a standalone reusable library repo with stable anchors before code generation.

Deliverables:

- `docs/design_intent.md`
- `docs/roadmap.md`
- `docs/implementation_guide.md`
- `docs/api_reference.md` placeholder
- `docs/integration_arduino.md` placeholder
- `docs/integration_esp_idf.md` placeholder
- `docs/test_plan.md` placeholder
- `examples/` folder
- `src/` folder
- `test/` folder
- license decision

Acceptance:

- Anchors clearly state no heap, no `String`, no STL in core.
- Anchors define static capacities and command metadata design.
- Anchors identify prior art reviewed and borrowed concepts.

## Phase 1 — Read-only architecture planning

Goal: have Codex inspect the uploaded command-library archive and PT100 reference code, then produce a detailed implementation plan without writing code.

Inputs:

- This document set.
- Uploaded command-library archive.
- PT100 Mesh GitHub repo reference.
- Initial empty or skeleton library repository.

Required analysis:

- AdvancedCLI capabilities and caveats.
- StaticSerialCommands capabilities and caveats.
- SimpleSerialShell capabilities and caveats.
- SerialUI menu concepts and reasons not to adopt it directly.
- PT100 registry/help/manpage architecture.

Output:

- Implementation plan with file layout, types, parser algorithm, helper macros, tests, examples, and non-goals.

Acceptance:

- No code changes.
- Clear GO/NO-GO recommendations for MVP scope.
- Identifies ambiguous decisions needing user approval.

## Phase 2 — Core MVP parser, registry, dispatch, and console orchestration

Goal: implement the platform-independent core stages before generated help.

Core files:

```text
src/bsc_config.h
src/bsc_types.h
src/bsc_tokenizer.h
src/bsc_tokenizer.c
src/bsc_registry.h
src/bsc_registry.c
src/bsc_args.h
src/bsc_args.c
src/bsc_matcher.h
src/bsc_matcher.c
src/bsc_dispatch.h
src/bsc_dispatch.c
src/bsc_console.h
src/bsc_console.c
```

Minimum functionality:

- Lightweight console configuration plus caller-owned execution workspace supplied per complete-line execution.
- Static command descriptor tables.
- Optional bounded runtime registry if approved.
- Tokenization with quotes and escaping.
- Longest-path command matching.
- Positional argument schema validation and selected-command dispatch.
- Types: none, int, uint, float, bool, enum, string, secret.
- Numeric bounds.
- String length bounds.
- Enum choice lists.
- Handler callbacks.
- Output callback passed to handlers without automatic console output.
- Explicit-length complete-line execution through tokenizer, matcher, and selected-command dispatch.
- Deterministic status codes.

Generated `help`, `help <path>`, and `commands` built-ins move to Phase 3 with the help/manpage system.

Acceptance:

- Host tests pass.
- No dynamic allocation in core.
- No Arduino/ESP-IDF includes in core.
- No `String` or STL in core.
- Parser returns clear errors for line too long, token too long, too many tokens, unknown command, ambiguous command, missing arg, extra arg, invalid type, range violation, enum violation, and string too long.

## Phase 3 — Generated help/manpage system

Goal: implement rich help using command metadata.

Minimum help sections:

```text
NAME
SYNOPSIS
DESCRIPTION
ARGUMENTS
VALID VALUES
NOTES
EXAMPLES
RELATED
```

Requirements:

- `help` lists top-level commands/groups.
- `help <group>` lists group children.
- `help <leaf>` prints manpage.
- `help <path>` supports nested paths.
- Sensitive arguments are redacted in examples/status helpers when marked secret.
- Public commands missing required help fields should be caught by validation tests.

Acceptance:

- Help output is stable and testable in golden files.
- All example commands in the repo have useful manpages.
- Help output does not require heap allocation.

## Phase 4 — Example applications

Goal: prove the library with representative embedded commands.

Required examples:

1. `basic_status`
   - `status`
   - `reset_stats`
   - `help status`

2. `sensor_settings`
   - `gain <enum>`
   - `time <enum>`
   - `rate <float>`
   - `output <enum>`
   - `status`

3. `wifi_settings`
   - `settings wifi status`
   - `settings wifi set ssid <string[1..32]>`
   - `settings wifi set password <secret[8..64]>`
   - `settings wifi clear`

4. `factory_locked`
   - normal commands
   - advanced commands
   - locked/factory command requiring access callback

Acceptance:

- Examples compile in host build.
- Arduino example compiles if Arduino tooling is available.
- Examples demonstrate command groups, typed arguments, help, and redaction.

## Phase 5 — Arduino adapter

Goal: provide an Arduino-friendly adapter while keeping the core free of Arduino types.

Files:

```text
adapters/arduino/BscArduinoConsole.h
adapters/arduino/BscArduinoConsole.cpp
examples/arduino_basic/arduino_basic.ino
examples/arduino_sensor_console/arduino_sensor_console.ino
```

Requirements:

- Accept a `Stream&` for input/output.
- Non-blocking `poll()`.
- Echo policy configurable.
- Bounded input line buffer.
- No Arduino `String` in adapter internals unless explicitly approved. Prefer `char[]`.
- Optional `PROGMEM` support deferred unless needed.

Acceptance:

- Arduino example compiles for ESP32 if tooling is available.
- Adapter does not allocate heap.
- Adapter can execute host-fed commands in tests if possible.

## Phase 6 — ESP-IDF adapter

Goal: provide an ESP-IDF integration that does not make the core depend on ESP-IDF.

Potential approaches:

- Adapter that reads UART lines and calls the core directly.
- Optional bridge to `esp_console_cmd_register()` for projects that already use ESP-IDF console.

Acceptance:

- PT100-like command metadata can map to the generic library.
- Existing PT100 command registry concepts are representable.
- No forced ESP-IDF dependency in core.

## Phase 7 — AS7331 integration pilot

Goal: integrate the library into `as7331-uv-console` after the standalone library is proven.

Candidate AS7331 command definitions:

```text
status
single
start
stop
scan
id
header
flags
gain <value>
time <ms>
clock <khz>
divider off
divider <value>
output <raw|scaled|both>
smooth <on|off>
alpha <value>
rate <hz>
channels all
channels none
channels <uva|uvb|uvc> <on|off>
ready off
ready pin <gpio>
ready mode <pp|od>
```

Acceptance:

- Existing hardware-validated command behavior is preserved.
- Detailed help exists for every command.
- Operator-level `help gain`, `help time`, and `help output` explain why/when to change settings.
- Existing fixed-width measurement rows remain intact.

## Phase 8 — Hardening and optional features

Deferred until MVP is stable:

- Completion/hints.
- Command history.
- Optional prompt editing.
- Authentication/unlock workflows.
- Persistent command aliases.
- Multi-transport support.
- Machine-readable command schema export.
- JSON output mode.
- Localization.

## Cross-phase test requirements

Every phase that changes code must include:

- Unit tests for tokenizer.
- Unit tests for parser and matcher.
- Unit tests for typed validation.
- Unit tests for help output.
- Unit tests for examples.
- Negative tests for malformed input.
- Tests for too-long line/token/argument.
- Tests confirming no sensitive value leaks in redacted output.
- Static inspection for heap allocation and forbidden types.

## Suggested first Codex task in the new chat

Use this task only after the new repo exists and the same command-library archive has been uploaded.

```text
Read the serial console library design intent, roadmap, and implementation guide. Inspect the uploaded command-library archive and the PT100 Mesh console reference. Produce a read-only implementation plan for a standalone bounded serial console library. Do not write code yet. Identify file layout, core data structures, parser/tokenizer algorithm, command descriptor schema, typed argument validation, help/manpage rendering, test strategy, examples, and open decisions requiring user approval.
```
