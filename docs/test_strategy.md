# Test Strategy

## Purpose

This file is the testing-policy anchor for `serial_command_extreme`.

The project is intended to be a reusable bounded embedded serial command library. The core should be testable on a normal host machine so Codex, reviewers, and maintainers can validate most behavior without repeatedly compiling and flashing firmware.

The test strategy is host-first by design. Firmware and hardware testing remain necessary for adapters and integrations, but they should not be required to validate the platform-independent parser, registry, argument validation, dispatch, output, redaction, and generated help behavior.

## Current status

Last updated: 2026-07-07.

Current repository status during this policy pass:

```text
Repository: rasusmilch/serial_command_extreme
Branch: main
Current pre-change head inspected: dc75a9f0f5111376b628a3bd9e0a9fc36e8265a2
Stage: bounded core implementation through output-neutral complete-line console orchestration
Implementation source: C99 core modules for tokenizer, registry, matcher, typed parser, selected-command dispatch/access enforcement, and complete-line console orchestration
Build system: CMake builds the core library and host tests
Tests: Host tests cover foundational helpers, tokenizer, registry, matcher, typed parser, dispatch/access enforcement, and complete-line console orchestration
Examples: not added yet
Arduino adapter: not added yet
ESP-IDF adapter: not added yet
```

Implementation code and host tests now exist. This file remains the testing-policy anchor and distinguishes current host coverage from future help, adapter, golden-output, and hardware validation.

## Source and anchor context inspected

Repository files inspected before creating this document:

```text
README.md
docs/00_serial_console_library_design_intent.md
docs/01_serial_console_library_roadmap.md
docs/02_serial_console_library_implementation_guide.md
docs/03_serial_console_library_handoff.md
docs/code_documentation_policy.md
docs/prior_art_review.md
```

Expected file before this change:

```text
docs/test_strategy.md
```

Status before this change:

```text
missing
```

The uploaded reference archive from prior planning was also available as context:

```text
/mnt/data/Archive.tar.gz
/mnt/data/sce_archive/
```

The GitHub connector reported repository code-search indexing unavailable during inspection, so direct path-based file reads were used.

## Testing goals

The main testing goal is to make Codex and human reviewers able to validate core behavior automatically without target hardware.

Target outcomes after the core exists:

```text
90%+ of core behavior host-tested.
100% of parser rejection/status cases host-tested.
100% of public example command descriptors covered by host tests.
100% of representative generated help/manpage output covered by golden tests.
0 hardware required for core PR validation.
Hardware required only for adapters, platform-specific builds, and integration pilots.
```

These are targets, not current measured coverage.

## Test architecture overview

Testing should be layered.

```text
Layer 1: host unit tests for pure core behavior
Layer 2: host integration tests using example descriptor tables
Layer 3: golden-output tests for help/manpages/errors
Layer 4: static architecture and forbidden-pattern checks
Layer 5: compile-only adapter checks where toolchains are available
Layer 6: hardware validation for platform adapters and real integrations
```

The core should be designed so Layers 1 through 4 can run on a standard host in CI and in Codex without flashing firmware.

## Layer 1 — Host unit tests

Host unit tests validate individual core modules with no Arduino, ESP-IDF, UART, RTOS, or hardware dependency.

Required areas:

- String-view helpers.
- Case-sensitive and case-insensitive comparison helpers, if both modes exist.
- Bounded line handling.
- Tokenizer behavior.
- Command registry validation.
- Command path matching.
- Typed argument parsing.
- Numeric range validation.
- String length validation.
- Enum validation.
- Secret argument redaction.
- Access-check behavior.
- Dispatch callback behavior.
- Status/error-code mapping.
- Output callback capture.

The unit-test harness should be usable from a normal shell command. Prefer a minimal host C test runner with no external dependency unless the architecture plan approves a specific framework.

A future command should look similar to one of these, depending on the selected build system:

```text
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

or:

```text
make test
```

or:

```text
python tools/run_tests.py
```

Do not require Arduino IDE, PlatformIO, ESP-IDF, USB serial devices, or real boards to run core tests.

## Layer 2 — Host integration tests

Host integration tests should use real descriptor tables and fake application contexts. They execute complete command lines through the public bsc_execute_line() entry point that adapters can call after accumulating a line and stripping line endings.

Example test shape:

```c
bsc_status_t status = bsc_execute_line(
    &console,
    &workspace,
    "settings wifi set password \"example password\",
    strlen("settings wifi set password \"example password\"),
    &result);
```

Then assert:

```text
status == BSC_STATUS_OK
matched command path == settings wifi set password
handler was called once
parsed argument count == 1
parsed argument type == secret string
argument length was accepted
output/log/echo captured no raw secret value
```

Integration tests must favor complete-line command execution over direct internal calls. Internal calls are useful for unit tests, but complete-line tests prove that tokenizer, matcher, validator, dispatcher, and output behavior work together.

## Layer 3 — Golden-output tests

Generated help and error output must be stable enough for serial logs, operator support, and simple automation. Representative output should be tested byte-for-byte with golden files once formatting is approved.

Required golden-output areas:

- `help` top-level index.
- `commands` output.
- `help <group>` child listing.
- `help <leaf>` manpage.
- `help settings wifi`-style namespace help.
- `help settings wifi set password`-style nested leaf help.
- Parser/validation error messages for common rejection cases.
- Secret redaction in echoed commands and status/helper output.

Golden files should live under:

```text
test/golden/
```

Golden tests should fail when output changes unexpectedly. If a task intentionally changes output, it must update the golden files and explain the output change in the final receipt.

## Layer 4 — Static architecture and forbidden-pattern checks

Static checks should prevent architectural drift.

The core must not contain:

- `malloc`, `calloc`, `realloc`, or `free`.
- `new` or `delete`.
- Arduino `String`.
- STL containers.
- Arduino includes in core files.
- ESP-IDF includes in core files.
- Unbounded recursion.
- Unbounded `sprintf`.
- `strcpy` or `strcat` without destination-size enforcement.
- `atoi` or `atof` without end-pointer validation.
- Hidden command state that changes the meaning of the next input line.
- Commands whose help text contradicts their parser schema.

The first implementation should include an automated check script if practical, for example:

```text
tools/check_forbidden_patterns.py
```

The check should distinguish core source from adapters. Arduino and ESP-IDF includes are expected in adapter directories, but not in `src/` core files.

Static checks must not be the only validation. They are guards against known bad patterns, not proof of correctness.

## Layer 5 — Compile-only adapter checks

Platform adapters should be tested separately from the core.

Required adapter checks once adapters exist:

- Arduino adapter compiles with the intended Arduino-compatible toolchain, if available.
- ESP-IDF adapter compiles with the intended ESP-IDF-compatible toolchain, if available.
- Host stdio adapter compiles and runs under host tests.
- Fake-stream or fake-UART tests exercise adapter buffering without hardware where possible.

Adapter compile checks should not be required for pure core changes unless the changed API affects adapters.

If toolchains are unavailable in Codex or CI, final receipts must state:

```text
Arduino adapter compile: not run, toolchain unavailable
ESP-IDF adapter compile: not run, toolchain unavailable
Hardware validation: not run
```

Do not claim adapter behavior is hardware-validated from compile-only tests.

## Layer 6 — Hardware validation

Hardware validation is still necessary for real platform behavior, but it should be narrow.

Hardware is required to verify:

- Real UART electrical/driver behavior.
- USB serial terminal behavior.
- Board-specific Arduino or ESP-IDF build quirks.
- Target stack, static RAM, flash, and linker behavior.
- Timing behavior under actual target clock and scheduler.
- Interrupt or RTOS interactions.
- Watchdog/reset behavior.
- Sleep/wake behavior, if ever added.
- AS7331, PT100, or other real integration behavior.
- Any command that changes real hardware state.

Hardware validation must be reported separately from host validation. A task must not say "validated" without specifying the validation level.

Preferred wording:

```text
Host core tests passed.
Adapter compile checks passed.
Hardware validation was not run.
```

Not acceptable:

```text
Fully validated.
```

unless hardware tests were actually performed and logs/observations are provided.

## Core behavior test matrix

The core test suite must cover these categories once implemented.

### Input trimming and empty input

- Empty string.
- Whitespace-only string.
- Leading whitespace.
- Trailing whitespace.
- Mixed spaces and tabs.
- Input line exactly at maximum length.
- Input line over maximum length.

### Tokenizer

Accept:

```text
status
settings wifi status
settings wifi set ssid Shop_AP
settings wifi set ssid "Shop AP"
settings wifi set password "pass with spaces"
settings wifi set password "quote: \""
settings wifi set password "backslash: \\"
```

Reject:

```text
"unterminated
<line longer than BSC_MAX_LINE_LEN>
<token longer than BSC_MAX_TOKEN_LEN>
<more than BSC_MAX_TOKENS tokens>
```

Additional tokenizer cases:

- Empty quoted string when schema allows length 0.
- Empty quoted string rejected when schema requires minimum length 1.
- Escape at end of quoted string.
- Backslash outside quotes, if supported or explicitly rejected.
- Consecutive delimiters.
- Tab delimiters.

Buffer/workspace policy cases and review checks:

- Tokenizer tests must prove token views can represent non-null-terminated slices.
- Tokenizer tests must prove quoted and escaped tokens are represented by views into the mutable input line after in-place compaction, not by copied strings.
- Tests or code inspection/static checks must verify tokenizer source does not introduce heap allocation, a separate tokenizer-owned text buffer, per-token text buffers, hidden static scratch, or large stack-local token/text arrays.
- Future parser tests must prove string and secret parsed arguments remain borrowed views into the active line buffer until dispatch and are not copied into parser-owned text buffers.
- Future adapter tests or plans must account for direct fill into the console/workspace line buffer where practical; any required RX staging buffer must have documented owner, capacity, lifetime, and copy boundary.
- Receipts for tokenizer, parser, console, matcher, dispatch, and adapter tasks must report buffer owners and any large local arrays.
- `sce_forbidden_patterns` remains a useful guard for heap/platform/API drift, but it is not proof of stack/local-buffer discipline or no-copy workspace discipline.
- Non-ASCII input policy, once defined.

### Command matching

- Unknown top-level command.
- Unknown child under known group.
- Group-only command with no default handler.
- Executable group command, if approved.
- Leaf command with no arguments.
- Nested leaf command.
- Longest-path match.
- Ambiguous match.
- Short input that is a valid group prefix.
- Extra literal token that should not be treated as an argument.
- Case-insensitive path matching, if default.
- Case-sensitive mode, if supported.

### Argument validation

For each type, test valid values, invalid values, boundary values, and error status.

Required types:

- none.
- signed integer.
- unsigned integer.
- float.
- boolean.
- enum.
- bounded string.
- secret string.

Required checks:

- Missing required argument.
- Extra argument.
- Invalid signed integer syntax.
- Invalid unsigned integer syntax, including negative input.
- Float syntax and full-token consumption.
- Float rejection of NaN/inf if the standard library would otherwise accept them.
- Numeric min, max, below-min, and above-max.
- Boolean accepted forms: `on/off`, `true/false`, `yes/no`, `1/0` if approved.
- Canonical boolean output as `on/off`.
- Enum exact choices.
- Enum case-insensitive matching, if approved.
- String minimum length.
- String maximum length.
- Secret same validation as string plus redaction.

Do not use `atoi()` or `atof()` without end-pointer validation.

### Dispatch

- Handler called exactly once on valid command.
- Handler not called on parser failure.
- Handler not called on validation failure.
- Handler not called when access check denies command.
- Handler return status is propagated or mapped as specified.
- Application context pointer is passed unchanged.
- Command descriptor pointer is passed unchanged.
- Parsed string views are valid during callback.
- Parsed string views are documented as invalid after the next execution or line-buffer mutation.

### Output and error formatting

- Parser errors are concise and deterministic.
- Validation errors name the argument where useful.
- Range errors include allowed range where useful.
- Enum errors include allowed values where useful.
- Secret values are redacted in echo/status/log helper output.
- Auto-result output behavior is tested if enabled.
- No error output contradicts command descriptors.

### Help/manpages

- Every public command has useful help metadata.
- `help` lists top-level commands/groups.
- `commands` lists commands as specified.
- `help <group>` lists children.
- `help <leaf>` prints manpage sections.
- `help <path>` supports nested paths.
- `NAME`, `SYNOPSIS`, `DESCRIPTION`, `ARGUMENTS`, `VALID VALUES`, `NOTES`, `EXAMPLES`, and `RELATED` sections appear where applicable.
- Secret arguments are described without leaking values.
- Public commands missing required help fields are caught by validation tests.

### Access/security levels

- Normal commands visible and executable.
- Advanced/factory/hidden/locked visibility rules, once finalized.
- Access-check callback receives correct context.
- Access denied prevents handler dispatch.
- Hidden commands do not appear in help index unless policy allows.
- Locked commands return deterministic error status.

### Capacity and bounds

- Maximum command count handling.
- Maximum path depth handling.
- Maximum argument count handling.
- Maximum enum choice count handling.
- Maximum token count handling.
- Maximum token length handling.
- Maximum output chunk behavior.
- Help rendering under bounded output buffers.
- Registration overflow diagnostics if bounded runtime registration is approved.

## Example command suites for tests

Use small representative descriptor tables for tests.

### Basic status suite

Commands:

```text
status
reset_stats
help status
```

Purpose:

- No-argument commands.
- Basic dispatch.
- Basic help/manpage behavior.

### Sensor settings suite

Commands:

```text
gain <enum>
time <enum>
rate <float>
output <raw|scaled|both>
status
```

Purpose:

- Enum validation.
- Float validation.
- Range errors.
- Help text explaining operator tradeoffs.

### Wi-Fi settings suite

Commands:

```text
settings wifi status
settings wifi set ssid <string[1..32]>
settings wifi set password <secret[8..64]>
settings wifi clear
```

Purpose:

- Nested command paths.
- Group help.
- Quoted strings.
- String bounds.
- Secret redaction.

### Factory locked suite

Commands:

```text
status
factory status
factory cal clear
```

Purpose:

- Access levels.
- Hidden/factory/locked visibility.
- Access-denied behavior.
- Handler not called on denied access.

## Test file layout

Recommended future layout:

```text
test/
  README.md
  test_main.c
  test_tokenizer.c
  test_parser.c
  test_args.c
  test_dispatch.c
  test_help.c
  test_redaction.c
  test_access.c
  test_capacity.c
  fixtures/
    basic_commands.c
    sensor_commands.c
    wifi_commands.c
    factory_commands.c
  golden/
    help_index.txt
    help_gain.txt
    help_settings_wifi.txt
    help_settings_wifi_set_password.txt
    commands.txt
    error_invalid_gain.txt
```

If the architecture plan selects a different test framework, keep the same conceptual coverage.

## Tooling recommendations

The first implementation should prefer boring, portable tooling.

Recommended default:

- CMake + CTest, or a simple Makefile, for host builds.
- A self-contained C test harness if that reduces dependency friction.
- Optional Python helper scripts for static checks and golden update/compare behavior.
- GitHub Actions for host tests after the build system exists.

Avoid tool choices that require flashing firmware to run core tests.

Avoid making PlatformIO or Arduino IDE the primary core test runner. They can be useful for adapter examples, but the core should test with a normal C compiler.

## CI expectations

Once implementation begins, CI should run at least:

```text
configure host build
build host tests
run host tests
run golden-output tests
run forbidden-pattern checks
```

Later CI jobs can add:

```text
compile Arduino examples, if toolchain available
compile ESP-IDF adapter/example, if toolchain available
run docs/link checks, if tooling exists
run formatting/static-analysis checks, if configured
```

CI should publish clear failure output. Golden-output diffs should be easy to read.

## Codex task requirements

Every implementation task must state exactly what was tested and what remains unverified.

Codex must not claim tests passed unless it ran the command or was given output by the user.

Implementation prompts should include this acceptance language:

```text
Testing acceptance criteria:
- Read docs/test_strategy.md before editing source.
- Add or update host tests for every new or changed parser, tokenizer, registry, validation, dispatch, output, redaction, help, or access behavior.
- For tokenizer/parser/console/adapter tasks, report buffer owners, intentional copy boundaries, and any local automatic arrays over the documentation-policy threshold.
- Include negative tests for malformed input and strict rejection cases.
- Include golden-output updates for intentional help/manpage/error-output changes.
- Run the available host test command and report the exact command and result.
- Run forbidden-pattern/static checks when available and report the exact command and result.
- Compile adapters only when the task touches adapter-facing APIs or adapter source and the toolchain is available.
- Do not claim hardware validation unless hardware was actually used and evidence is provided.
- In the final receipt, separate Host tests, Static checks, Adapter compile checks, Hardware tests, and Unverified items.
```

## Final receipt format

Future Codex receipts should use this structure:

```text
Files changed:
- <path>: <summary>

Commands run:
- <command>
  Result: <passed/failed/skipped>
  Notes: <important output or reason skipped>

Host tests:
- <passed/failed/skipped with details>

Golden-output tests:
- <passed/failed/skipped with details>

Static/forbidden-pattern checks:
- <passed/failed/skipped with details>

Adapter compile checks:
- Arduino: <passed/failed/skipped with reason>
- ESP-IDF: <passed/failed/skipped with reason>
- Host adapter: <passed/failed/skipped with reason>

Hardware validation:
- <not run unless actually run>

Unverified items:
- <anything not proven>
```

Receipts must make skipped checks visible. Silent skips are not acceptable.

## Documentation-only change testing

For Markdown-only tasks like this file:

- Confirm no source files were changed.
- Confirm the intended Markdown file exists after the change.
- No compile/test run is required unless the task also changes source, build files, links, generated docs, or examples.

For future documentation-only source passes:

- Python docstring-only changes should use AST equivalence after removing docstrings where practical.
- C/C++ comment-only changes should use compile, preprocess, or token-equivalence checks where practical.
- Documentation-only source tasks must still run available tests if the touched source participates in tests.

## Hardware validation boundary

The project should minimize firmware flashing during core development, but it cannot eliminate hardware testing entirely.

Host tests can validate:

- Tokenization.
- Matching.
- Typed validation.
- Redaction.
- Help generation.
- Output formatting.
- Handler dispatch behavior.
- Access-denied behavior.
- Core capacity rejection.

Host tests cannot fully validate:

- UART electrical behavior.
- USB serial driver behavior.
- Arduino `Stream` behavior on every board.
- ESP-IDF UART or console behavior on real targets.
- Real stack/heap/flash/linker measurements under every target compiler.
- RTOS scheduling, ISR timing, DMA, watchdog, reset, or sleep behavior.
- Real AS7331/PT100/hardware command side effects.

Any task that touches platform adapter or integration code must state which of these remain unverified.

## Open testing decisions

The architecture plan should resolve or explicitly defer these decisions:

1. Host test framework: self-contained harness, CTest, Unity, CMocka, or another option.
2. Build system: CMake, Make, Meson, or another option.
3. Golden-output update workflow: manual update, tool-assisted update, or no auto-update.
4. Static forbidden-pattern check implementation language.
5. Whether CI is added during the first implementation task or immediately after the core skeleton exists.
6. Whether coverage tooling is required or deferred.
7. Whether Arduino compile checks use Arduino CLI, PlatformIO, or another tool.
8. Whether ESP-IDF compile checks are part of normal CI or manual validation only.
9. Whether memory-map/size reporting is required for adapter builds.
10. Whether fuzz/property-style tests are added for tokenizer/parser after deterministic unit tests are stable.

## Suggested phased backlog

### Phase T0 — Test infrastructure plan

Goal: decide build system, test runner, static check style, and CI shape before implementation.

Acceptance:

- No source implementation required.
- Commands for future host tests are specified.
- Adapter compile policy is specified.
- Hardware validation boundary is explicit.

### Phase T1 — Minimal host test harness

Goal: create a host build that can compile and run a failing/passing smoke test before core implementation expands.

Acceptance:

- One host test command exists.
- CI or local command can run without target hardware.
- Test receipt format is proven.

### Phase T2 — Core parser/tokenizer tests

Goal: cover line trimming, tokenization, quotes, escapes, and strict rejection cases.

Acceptance:

- Required tokenizer accept/reject cases pass.
- Boundary tests for line/token/token-count limits pass.
- Tokenizer/parser buffer-policy tests or review checks prove views point into the active mutable line buffer and no extra tokenizer/parser text buffers were introduced.
- No hardware required.

### Phase T3 — Registry, matching, and argument validation tests

Goal: validate descriptor tables, path matching, typed arguments, ranges, enum values, and error statuses.

Acceptance:

- Representative command suites execute complete-line tests.
- Unknown, ambiguous, missing, extra, invalid-type, range, enum, and string-length failures are covered.

### Phase T4 — Dispatch, access, output, and redaction tests

Goal: validate handler invocation, access denial, output capture, and secret redaction.

Acceptance:

- Handlers are not called on parse/validation/access failure.
- Secrets do not appear in echo/status/log helper output.
- Output status lines are deterministic.

### Phase T5 — Help/manpage golden tests

Goal: validate generated help output from metadata.

Acceptance:

- Top-level, group, leaf, nested leaf, and command-list outputs have golden tests.
- Missing help metadata for public commands is caught.

### Phase T6 — Adapter compile and fake-I/O tests

Goal: validate adapters without requiring hardware.

Acceptance:

- Arduino adapter compile check exists if toolchain available.
- ESP-IDF adapter compile check exists if toolchain available.
- Fake stream/UART tests exercise input buffering where practical.

### Phase T7 — Hardware smoke validation

Goal: run minimal real-hardware checks only after host behavior is stable.

Acceptance:

- Hardware target, board, toolchain, command log, and observed output are recorded.
- Hardware checks are not used as a substitute for host core tests.

## Final rule

A core change is not ready just because it compiles for firmware. A core change is ready when host tests prove the parser/registry/validation/dispatch/help behavior and the receipt clearly states what adapter and hardware validation did or did not run.

## Generated-help golden-output policy

The pure help core has byte-exact golden fixtures under `test/golden/`. CMake copies that directory to the test binary directory and provides the copied path to `sce_host_tests` with a private compile definition, so tests do not depend on the process working directory. Golden files are opened in binary mode and compared byte-for-byte with LF-only output; tests fail on CRLF, whitespace changes, missing final LF, reordered entries, or changed section headings.

Generated-help tests cover separate help metadata validation, static visibility filtering, exact descriptor-path lookup, descriptor-order rendering, short-write propagation, invalid-metadata no-output behavior, secret non-disclosure, and compact-float formatting. Future console built-ins, extended sections, and subtopics must add or update golden fixtures when their output grammar is approved.
