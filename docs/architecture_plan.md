# Architecture Plan

## Purpose

This file is the read-only architecture plan for `serial_command_extreme` before implementation begins.

It converts the existing design anchors, prior-art review, test strategy, README, and current repository scaffold into an implementation contract for the first code tasks. It does not add source code and does not claim that any implementation exists yet.

The planned product is a reusable bounded embedded serial command library: a C-first core that tokenizes complete command lines, matches nested literal command paths, validates typed arguments, dispatches handlers, and generates operator-facing help/manpages from the same command metadata used by parsing.

## Current inspection context

Last updated: 2026-07-07.

Repository inspected:

```text
Repository: rasusmilch/serial_command_extreme
Branch: main
Current head inspected before this file: ce2ae8566f8dbde8af14f6e574f571bc64dc53cc
Code-search indexing: unavailable through the connector during inspection
Inspection mode: direct path-based file reads and known commit/file inspection
```

Current repository status:

```text
Stage: anchor/planning/scaffold
Implementation source: not added yet
Build scaffold: CMakeLists.txt exists
Core source directory: src/README.md only
Test directory: test/README.md only
Examples directory: examples/README.md only
Arduino adapter: not added yet
ESP-IDF adapter: not added yet
CI: not added yet
License: not finalized
```

Files inspected from GitHub before creating this plan:

```text
README.md
CMakeLists.txt
src/README.md
test/README.md
examples/README.md
docs/00_serial_console_library_design_intent.md
docs/01_serial_console_library_roadmap.md
docs/02_serial_console_library_implementation_guide.md
docs/03_serial_console_library_handoff.md
docs/code_documentation_policy.md
docs/prior_art_review.md
docs/test_strategy.md
```

Expected file before this change:

```text
docs/architecture_plan.md
```

Status before this change:

```text
missing
```

Reference implementation context:

The prior-art review records inspection of the uploaded archive and reference implementations including AdvancedCLI, StaticSerialCommands, SimpleSerialShell, SerialUI, SerialCommandCoordinator, SerialCommand variants, ParseCommands, SerialConfigCommand, CommandCatcher, tinyCommand, cmdArduino, and PT100 Mesh console code. This plan relies on that review for prior-art conclusions and does not approve copying reference code wholesale.

Unverified items:

- No current implementation source exists to compile or test.
- No CI exists yet.
- No host test runner exists yet.
- No hardware, Arduino, ESP-IDF, AS7331, or PT100 integration has been validated in this standalone repository.
- License compatibility for copying reference code has not been reviewed; treat reference code as design inspiration only.

## Architectural conclusion

Proceed with an original C-first bounded core.

Do not adopt any reviewed command library as the direct base. The core should selectively borrow concepts:

- AdvancedCLI: typed argument seriousness, validators, capacity diagnostics, help generated from metadata, host tests.
- StaticSerialCommands: static embedded registration, typed/ranged argument constraints, quoted strings, PROGMEM awareness as a later adapter concern.
- SimpleSerialShell: direct `execute(line)` ergonomics and simple host-callable behavior.
- SerialUI: discoverability through help/group browsing, not stateful menu navigation.
- PT100 Mesh: registry-driven command metadata and operator-facing manpage-style help.

The core should stay deliberately narrow:

```text
tokenizer
command descriptor model
registry validation
longest-path matcher
typed argument validator
dispatcher
output callback layer
generated help/manpage renderer
host test harness
forbidden-pattern checks
```

Defer shell-like and UI-like features:

```text
history
line editing
completion
aliases
named options/flags
runtime command discovery
authentication workflows
persistent settings storage
JSON output
scripting
pipes/redirection
stateful menus
```

## GO / NO-GO decision

### GO

Start implementation after this plan is reviewed and accepted.

Recommended first implementation task: create the minimal C core skeleton and host test harness without implementing the full parser yet. That task should establish file layout, build/test commands, status enum, string-view type, output capture test helper, and one smoke test.

### NO-GO

Do not start by adding Arduino examples, ESP-IDF adapters, AS7331 integration, command history, line editing, aliases, JSON, or persistent settings.

Do not start by copying AdvancedCLI, StaticSerialCommands, SimpleSerialShell, SerialUI, or PT100 code into this repo.

Do not implement a menu system that changes the meaning of later input based on hidden state.

Do not allow parser behavior and help text to drift into separate hand-maintained systems.

## Selected implementation baseline

### Language baseline

Use C99 for the platform-independent core.

Rationale:

- The existing CMake scaffold already declares a C project and exposes C99 features.
- C99 is sufficient for fixed-width types, `stdbool.h`, static descriptors, and host tests.
- C99 keeps the core portable to Arduino-adjacent and ESP-IDF-adjacent environments while avoiding C++ runtime expectations.

C++ is allowed only for adapters, examples, or optional test/helper code when useful.

### Build baseline

Use CMake + CTest as the primary host build/test path.

The existing root `CMakeLists.txt` should remain host-oriented. Firmware-specific build files belong in adapter or integration directories later.

Target future commands:

```text
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

The first implementation task should make those commands meaningful for host tests.

### Test baseline

Use a small self-contained C test harness initially.

Do not introduce an external test framework unless there is a strong reason. The early test harness can be plain C with assertions and a small runner. This keeps Codex/CI friction low and avoids dependency issues.

A later task may replace or supplement it with Unity/CMocka/etc. if justified.

### Registration baseline

MVP uses static `const bsc_command_t[]` descriptor tables.

Do not implement runtime registration in the MVP. Runtime registration can be added later using caller-provided bounded storage if a real use case appears.

### Descriptor ergonomics baseline

Start with raw C structs and small helper macros only where they remove error-prone boilerplate without hiding behavior.

Do not build a macro-heavy DSL in the MVP. The descriptor model must remain inspectable by reviewers and Codex.

### Command matching baseline

Use linear scan over a bounded command array.

Rationale:

- Command counts are bounded and initially small.
- Linear scan is easy to test and reason about.
- Faster lookup can be deferred until there is evidence it is needed.

### Case sensitivity baseline

Recommend case-insensitive command path and enum matching by default.

Rationale:

- Operators frequently type commands manually.
- Prior art supports case-sensitivity options.
- Help and examples can still display canonical lowercase command names.

Open approval item: whether to expose compile-time options for case-sensitive matching in MVP or defer that configurability.

### Optional positional arguments baseline

Do not support optional positional arguments in MVP.

Rationale:

- They create ambiguity in path matching and help output.
- They increase test complexity.
- Required positional arguments plus explicit alternate commands are easier to validate.

### Aliases baseline

Do not support aliases in MVP.

Rationale:

- Canonical command paths must be stable first.
- Aliases complicate help, command listing, duplicate detection, and tests.
- They can be added later as explicit metadata after path matching is proven.

### Access baseline

Include an access enum and optional access-check callback in the descriptor model, but keep behavior minimal.

MVP should support:

```text
BSC_ACCESS_NORMAL
BSC_ACCESS_ADVANCED
BSC_ACCESS_FACTORY
BSC_ACCESS_HIDDEN
BSC_ACCESS_LOCKED
```

MVP behavior recommendation:

- `NORMAL` and `ADVANCED`: visible in help and executable unless application access callback denies them.
- `FACTORY`: visible only when help policy/access allows; executable only when access callback allows.
- `HIDDEN`: not listed in broad help; can still be resolved by exact path if access allows.
- `LOCKED`: always returns access denied unless the application access callback explicitly allows it.

Open approval item: exact default visibility for `ADVANCED`, `FACTORY`, and `HIDDEN`.

### Output ownership baseline

Core owns no platform output.

Use a callback abstraction:

```c
typedef void (*bsc_write_fn_t)(void *user, const char *text);
```

The core may provide bounded helper functions for writing strings, lines, status, and formatted chunks. Any formatted output helper must use a bounded buffer with a compile-time capacity.

### Final result output baseline

Recommend that handlers return `bsc_status_t`, and the console may optionally emit auto-result output when `auto_result_enabled` is true.

MVP should not force every command to print `OK:` or `ERR:` itself. Provide helpers, but keep policy explicit.

Open approval item: whether auto-result output is enabled by default in examples.

## Repository layout plan

Final target layout for the first implementation wave:

```text
README.md
LICENSE                       # still pending license decision
CMakeLists.txt
.clang-format
.editorconfig
.gitignore
docs/
  00_serial_console_library_design_intent.md
  01_serial_console_library_roadmap.md
  02_serial_console_library_implementation_guide.md
  03_serial_console_library_handoff.md
  architecture_plan.md
  code_documentation_policy.md
  prior_art_review.md
  test_strategy.md
src/
  README.md
  bsc_config.h
  bsc_status.h
  bsc_string_view.h
  bsc_string_view.c
  bsc_output.h
  bsc_output.c
  bsc_types.h
  bsc_tokenizer.h
  bsc_tokenizer.c
  bsc_args.h
  bsc_args.c
  bsc_registry.h
  bsc_registry.c
  bsc_matcher.h
  bsc_matcher.c
  bsc_dispatch.h
  bsc_dispatch.c
  bsc_help.h
  bsc_help.c
  bsc_console.h
  bsc_console.c
test/
  README.md
  CMakeLists.txt
  test_main.c
  test_bsc_string_view.c
  test_bsc_output.c
  test_bsc_tokenizer.c
  test_bsc_args.c
  test_bsc_registry.c
  test_bsc_matcher.c
  test_bsc_dispatch.c
  test_bsc_help.c
  test_bsc_console.c
  fixtures/
    bsc_test_commands.h
    bsc_test_commands.c
  golden/
    help_index.txt
    help_gain.txt
    help_settings_wifi.txt
    help_settings_wifi_set_password.txt
    commands.txt
examples/
  README.md
  host_basic/
  host_sensor_settings/
adapters/
  arduino/
  esp_idf/
tools/
  check_forbidden_patterns.py
```

Notes:

- `bsc_status.h` is separated from `bsc_types.h` so status codes are easy to include in all modules.
- `bsc_string_view.*` is separated because string views are foundational to tokenizer, matcher, args, and tests.
- `bsc_matcher.*` is separated from `bsc_registry.*` to keep command-table validation distinct from runtime path matching.
- `bsc_dispatch.*` should coordinate matcher + args + handler, but not own line buffering.
- `bsc_console.*` should be the high-level public entry point that copies/normalizes input into the bounded line buffer and calls tokenizer/matcher/dispatch/help.
- `adapters/` can be created later; do not add adapter code before the host-tested core exists.

## Module responsibilities

### `bsc_config.h`

Defines conservative compile-time defaults and allows project overrides.

Recommended initial values:

```c
#ifndef BSC_MAX_COMMANDS
#define BSC_MAX_COMMANDS 64u
#endif

#ifndef BSC_MAX_PATH_TOKENS
#define BSC_MAX_PATH_TOKENS 6u
#endif

#ifndef BSC_MAX_ARGS
#define BSC_MAX_ARGS 8u
#endif

#ifndef BSC_MAX_TOKENS
#define BSC_MAX_TOKENS 24u
#endif

#ifndef BSC_MAX_LINE_LEN
#define BSC_MAX_LINE_LEN 160u
#endif

#ifndef BSC_MAX_TOKEN_LEN
#define BSC_MAX_TOKEN_LEN 64u
#endif

#ifndef BSC_MAX_ENUM_CHOICES
#define BSC_MAX_ENUM_CHOICES 16u
#endif

#ifndef BSC_OUTPUT_CHUNK_LEN
#define BSC_OUTPUT_CHUNK_LEN 160u
#endif
```

Feature flags:

```c
BSC_ENABLE_FLOAT
BSC_ENABLE_QUOTED_STRINGS
BSC_ENABLE_BUILTIN_HELP
BSC_CASE_INSENSITIVE_COMMANDS
BSC_CASE_INSENSITIVE_ENUMS
```

### `bsc_status.h`

Defines explicit status codes. Do not collapse parser failures into generic false.

Recommended MVP status set:

```c
typedef enum {
  BSC_STATUS_OK = 0,
  BSC_STATUS_NO_INPUT,
  BSC_STATUS_LINE_TOO_LONG,
  BSC_STATUS_TOKEN_TOO_LONG,
  BSC_STATUS_TOO_MANY_TOKENS,
  BSC_STATUS_UNTERMINATED_QUOTE,
  BSC_STATUS_UNKNOWN_COMMAND,
  BSC_STATUS_AMBIGUOUS_COMMAND,
  BSC_STATUS_GROUP_REQUIRES_SUBCOMMAND,
  BSC_STATUS_MISSING_ARGUMENT,
  BSC_STATUS_EXTRA_ARGUMENT,
  BSC_STATUS_INVALID_ARGUMENT_TYPE,
  BSC_STATUS_ARGUMENT_OUT_OF_RANGE,
  BSC_STATUS_ARGUMENT_TOO_LONG,
  BSC_STATUS_INVALID_ENUM_VALUE,
  BSC_STATUS_ACCESS_DENIED,
  BSC_STATUS_OUTPUT_TRUNCATED,
  BSC_STATUS_APP_ERROR,
  BSC_STATUS_INTERNAL_ERROR
} bsc_status_t;
```

`BSC_STATUS_GROUP_REQUIRES_SUBCOMMAND` is added to distinguish a known group prefix from an unknown command.

### `bsc_string_view.h/.c`

Defines immutable string views and comparison helpers.

Responsibilities:

- Store pointer + length without copying.
- Compare view-to-cstr exactly.
- Compare view-to-cstr case-insensitively.
- Check length and empty state.
- Copy view into bounded destination when application persistence is needed.
- Avoid assuming tokens are null-terminated.

### `bsc_output.h/.c`

Defines output callback abstraction and bounded output helpers.

Responsibilities:

- Write string chunks through callback.
- Write newline helpers.
- Optionally write bounded formatted output.
- Return explicit status on truncation if formatting exceeds `BSC_OUTPUT_CHUNK_LEN`.
- Never own UART, Serial, stdout, file descriptors, or RTOS objects.

### `bsc_types.h`

Defines public descriptor and parsed argument types:

- `bsc_arg_type_t`
- `bsc_enum_choice_t`
- `bsc_arg_def_t`
- `bsc_arg_value_t`
- `bsc_parsed_args_t`
- `bsc_node_type_t`
- `bsc_access_level_t`
- `bsc_command_t`
- callback typedefs
- maybe `bsc_console_options_t`

Keep this header public and Doxygen-documented.

### `bsc_tokenizer.h/.c`

Turns a bounded mutable line buffer into token views.

Responsibilities:

- Trim leading/trailing whitespace.
- Split on spaces and tabs outside quotes.
- Support quoted strings when enabled.
- Support `\"` and `\\` inside quotes.
- Reject unterminated quotes.
- Reject too many tokens.
- Reject token too long.
- Return empty input as `BSC_STATUS_NO_INPUT`.
- Avoid heap allocation.
- Avoid unbounded recursion.

Implementation recommendation:

Use an in-place state machine over `char line_buffer[]`. Store tokens as `bsc_str_view_t`. For quoted/unescaped strings, it is acceptable to compact escapes in-place inside `line_buffer` so the token view points to the unescaped content.

Tokenizer states:

```text
idle
bare_token
quoted_token
quoted_escape
```

### `bsc_args.h/.c`

Parses remaining tokens according to a command's argument schema.

Responsibilities:

- Validate exact required argument count for MVP.
- Reject missing and extra arguments.
- Parse signed int with full-token consumption.
- Parse unsigned int with full-token consumption and no sign.
- Parse float with full-token consumption and NaN/inf rejection when possible.
- Parse bool accepted forms: `on/off`, `true/false`, `yes/no`, `1/0`.
- Parse enum choices, case-insensitively by default.
- Validate string and secret byte lengths.
- Store string/secret as views into the line buffer.
- Store enum as index into the descriptor's choice array.

Do not use `atoi()` or `atof()` without end-pointer validation. Prefer `strtol`, `strtoul`, and `strtof`/`strtod` with end-pointer and range checks.

### `bsc_registry.h/.c`

Validates static descriptor tables.

Responsibilities:

- Check command count within limits.
- Check every path has `path_len > 0` and `path_len <= BSC_MAX_PATH_TOKENS`.
- Check no null path token in the active path range.
- Check node type validity.
- Check executable commands have handlers unless they are recognized built-ins.
- Check argument count within limits.
- Check enum choice count within limits.
- Check required help fields for public commands if validation mode asks for strict help.
- Detect exact duplicate paths.
- Detect duplicate built-ins if core built-ins and app descriptors collide.

This module should not execute commands.

### `bsc_matcher.h/.c`

Finds the best matching command path from token views.

Responsibilities:

- Linear scan over bounded descriptor array.
- Compare path tokens against input tokens.
- Prefer longest exact path match.
- Distinguish unknown command from known group requiring a subcommand.
- Detect ambiguous duplicate/compatible matches if registry validation did not prevent them.
- Return matched command pointer and count of consumed path tokens.
- Avoid recursive descent.

Important behavior:

`settings wifi set ssid Shop_AP` must match path `settings wifi set ssid` with remaining arg `Shop_AP`, not a shorter `settings wifi` command with extra strings.

### `bsc_dispatch.h/.c`

Coordinates access checks, argument parsing, and handler invocation for a matched executable command.

Responsibilities:

- Reject group-only commands unless a group handler policy is explicitly supported.
- Check access level and optional access callback.
- Parse remaining tokens into `bsc_parsed_args_t`.
- Call handler exactly once on valid command.
- Do not call handler on parse, validation, matching, or access failure.
- Propagate handler return status.
- Support optional auto-result output policy.

### `bsc_help.h/.c`

Renders generated help/manpage output from command descriptors.

Responsibilities:

- Render `help` top-level command/group index.
- Render `commands` canonical command list.
- Render `help <group>` child listing.
- Render `help <leaf>` manpage.
- Render nested path help.
- Display argument names, types, ranges, enum values, notes, examples, and related commands.
- Redact secret examples in any helper-generated echo/status paths.
- Respect access/visibility policy.
- Use bounded output helpers only.

Minimum manpage sections:

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

Sections with no content may be omitted if output policy says so, but golden tests must lock the chosen behavior.

### `bsc_console.h/.c`

High-level public entry point.

Responsibilities:

- Own or receive console runtime context.
- Copy a complete line into `line_buffer` with bounds checking.
- Call tokenizer.
- Route built-ins such as `help` and `commands`.
- Match command paths.
- Dispatch executable commands.
- Provide deterministic status/error output when configured.

Recommended public API sketch:

```c
bsc_status_t bsc_console_init(bsc_console_t *console,
                              const bsc_command_t *commands,
                              uint8_t command_count,
                              void *app_ctx,
                              bsc_output_t output);

bsc_status_t bsc_execute_line(bsc_console_t *console, const char *line);

const char *bsc_status_name(bsc_status_t status);
```

Exact API may change during implementation, but the high-level `bsc_execute_line()` concept should remain.

## Lifetime and ownership rules

These rules must be documented in headers and enforced by tests where practical:

- Command descriptor arrays are caller-owned and must outlive the console context.
- Descriptor strings are caller-owned/static and must outlive the console context.
- `bsc_console_t` is caller-owned.
- The console owns its bounded `line_buffer`.
- Tokens are views into `line_buffer`.
- Parsed string/secret arguments are views into `line_buffer`.
- Parsed string/secret views are valid only during dispatch and until the next line-buffer mutation.
- If the application needs to keep a string/secret, the handler must copy it into bounded application storage.
- Output callback user pointer is caller-owned and opaque to the core.
- No core function may allocate heap memory.

## Built-in command policy

MVP built-ins:

```text
help
commands
```

Implementation recommendation:

- Treat `help` and `commands` as normal descriptors where possible.
- `help` may require special dispatch because it accepts a variable number of path tokens.
- Built-ins must be testable through `bsc_execute_line()` like any other command.
- App command names should not be allowed to collide with core built-ins unless there is an explicit override policy.

Deferred built-ins:

```text
version
about
```

Do not add them in MVP unless needed by examples.

## Error and status output policy

Parser/validation errors should be concise and deterministic.

Recommended examples:

```text
ERR: unknown command
ERR: missing argument: value
ERR: invalid argument type: hz expects float
ERR: argument out of range: hz must be 0.100..50.000
ERR: invalid value: output must be raw|scaled|both
ERR: string too long: ssid max 32
ERR: access denied
```

Secret echo/status example:

```text
> settings wifi set password ********
OK: settings wifi password set
```

Golden tests should lock representative output after the initial format is approved.

## Host test plan

The first implementation wave must create the host test foundation before adding many features.

Recommended initial test layout:

```text
test/CMakeLists.txt
test/test_main.c
test/test_bsc_string_view.c
test/test_bsc_output.c
test/test_bsc_tokenizer.c
test/test_bsc_args.c
test/test_bsc_registry.c
test/test_bsc_matcher.c
test/test_bsc_dispatch.c
test/test_bsc_help.c
test/test_bsc_console.c
test/fixtures/bsc_test_commands.h
test/fixtures/bsc_test_commands.c
test/golden/
```

Recommended first test milestone:

```text
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Initial smoke test should prove:

- CMake configures.
- Host tests build.
- Test runner executes.
- A known passing test reports success.
- A command fixture can be linked without platform dependencies.

Core coverage required before MVP can be considered complete:

- Empty/whitespace input.
- Basic tokens.
- Quoted strings.
- Escapes.
- Unterminated quote rejection.
- Line too long rejection.
- Token too long rejection.
- Too many tokens rejection.
- Unknown command.
- Group-only command.
- Nested leaf command.
- Longest-path matching.
- Missing argument.
- Extra argument.
- Each typed argument parse success/failure.
- Numeric range validation.
- Enum validation.
- String length validation.
- Secret redaction.
- Access denied path.
- Handler not called on validation failure.
- Handler return status.
- Help index output.
- Help group output.
- Help leaf/manpage output.

## Static checks plan

Add a forbidden-pattern checker early, preferably:

```text
tools/check_forbidden_patterns.py
```

The checker must distinguish core source from adapters.

Core forbidden patterns:

```text
malloc
calloc
realloc
free
new
delete
Arduino String
#include <Arduino.h>
#include "Arduino.h"
ESP-IDF includes in src/
std:: containers in src/
unbounded sprintf
strcpy/strcat without size enforcement
atoi/atof without end-pointer validation
```

This check is a guard, not proof of correctness. It must run alongside host tests.

## Example plan

Add host examples after the core can execute commands in tests.

Initial host example set:

```text
examples/host_basic/
  status
  reset_stats
  help
  help status

examples/host_sensor_settings/
  gain <enum>
  time <enum>
  rate <float>
  output <enum>
  status

examples/host_wifi_settings/
  settings wifi status
  settings wifi set ssid <string[1..32]>
  settings wifi set password <secret[8..64]>
  settings wifi clear
```

Arduino examples should wait until the Arduino adapter exists and the core is stable.

## Adapter plan

Adapters are not part of MVP core task 1.

### Arduino adapter

Future location:

```text
adapters/arduino/BscArduinoConsole.h
adapters/arduino/BscArduinoConsole.cpp
```

Responsibilities:

- Borrow an Arduino `Stream` or compatible object.
- Accumulate input lines in bounded storage.
- Call `bsc_execute_line()` for complete lines.
- Write output through the core output callback.
- Avoid Arduino `String` internally unless a later explicit decision approves it.
- Document lifetime: borrowed `Stream` and `bsc_console_t` must outlive adapter.

### ESP-IDF adapter

Future location:

```text
adapters/esp_idf/bsc_esp_idf_adapter.h
adapters/esp_idf/bsc_esp_idf_adapter.c
```

Responsibilities:

- Bridge ESP-IDF UART/console input to complete command lines.
- Keep RTOS/blocking behavior explicit.
- Keep ESP-IDF includes out of `src/`.
- Document task/ISR/thread assumptions.

### Host adapter

A host stdio adapter may be useful for examples and manual testing, but it should not be required by core unit tests.

## Documentation policy integration

Implementation tasks must follow `docs/code_documentation_policy.md`.

Public C headers must document:

- Ownership.
- Buffer sizes.
- Units and ranges.
- Status codes.
- Blocking behavior.
- Thread/ISR safety.
- Lifetime rules.
- Redaction rules.

Inline comments should explain non-obvious parser, memory, redaction, or regression-protection behavior. Do not comment trivial syntax.

## Golden help/manpage policy

Help output is a public product surface.

Before Phase 3 is complete:

- Representative help/manpage output must be stored under `test/golden/`.
- Golden diffs must be visible in failing tests.
- Public commands missing required help metadata should fail descriptor validation tests.
- Secret values must not appear in generated status/echo/helper output.

## First implementation task recommendation

The next Codex execute task should be narrow and infrastructure-first.

Recommended task title:

```text
Phase 2A — Core Skeleton and Host Test Harness
```

Scope:

```text
Add C99 core skeleton files.
Add test/CMakeLists.txt and a minimal self-contained C test runner.
Add bsc_config.h, bsc_status.h, bsc_string_view.h/.c, bsc_output.h/.c, and minimal bsc_console.h/.c stubs sufficient for host smoke tests.
Add tools/check_forbidden_patterns.py if practical.
Make cmake/build/ctest run successfully.
Do not implement full tokenizer, parser, help, Arduino adapter, ESP-IDF adapter, or examples yet.
```

Acceptance:

```text
- CMake configures from a clean build directory.
- Host tests build and run through CTest.
- At least one smoke test validates bsc_status_name() or equivalent.
- At least one string-view helper test exists if string-view code is added.
- Forbidden-pattern check exists or is explicitly deferred with reason.
- No heap allocation in core.
- No Arduino String or Arduino/ESP-IDF includes in core.
- Public headers have Doxygen-compatible comments per docs/code_documentation_policy.md.
- Final receipt separates Host tests, Static checks, Adapter compile checks, Hardware validation, and Unverified items.
```

## Suggested implementation sequence

### Phase 2A — Core skeleton and host test harness

Goal: make host build/test infrastructure real.

Files likely added:

```text
src/bsc_config.h
src/bsc_status.h
src/bsc_string_view.h
src/bsc_string_view.c
src/bsc_output.h
src/bsc_output.c
src/bsc_console.h
src/bsc_console.c
test/CMakeLists.txt
test/test_main.c
test/test_bsc_string_view.c
test/test_bsc_output.c
tools/check_forbidden_patterns.py
```

### Phase 2B — Tokenizer

Goal: bounded tokenizer with quotes/escapes and rejection tests.

### Phase 2C — Descriptor types, registry validation, and matcher

Goal: static command descriptor model, table validation, and longest-path matching.

### Phase 2D — Typed argument parser

Goal: int/uint/float/bool/enum/string/secret validation with full-token parsing and negative tests.

### Phase 2E — Dispatch and access checks

Goal: handler callback dispatch, access denial, app context propagation, and handler return status.

### Phase 3A — Help renderer foundation

Goal: `help`, `help <path>`, and `commands` output with golden tests.

### Phase 4A — Host examples

Goal: `host_basic`, `host_sensor_settings`, and `host_wifi_settings` examples.

### Phase 5A — Arduino adapter plan/implementation

Goal: add bounded Arduino `Stream` adapter after core behavior is stable.

## Open decisions requiring approval

These should be accepted, changed, or explicitly deferred before full MVP implementation.

1. Confirm C99 as the core language standard.
2. Confirm CMake + CTest as the primary host build/test path.
3. Confirm self-contained C test runner for initial tests.
4. Confirm static descriptor tables only for MVP; defer runtime registration.
5. Confirm no aliases in MVP.
6. Confirm no optional positional arguments in MVP.
7. Confirm case-insensitive command and enum matching by default.
8. Confirm exact visibility defaults for `ADVANCED`, `FACTORY`, `HIDDEN`, and `LOCKED` commands.
9. Confirm whether auto-result output is enabled by default in examples.
10. Confirm whether `bsc_out_printf()` is included in MVP or deferred in favor of write/write-line helpers only.
11. Confirm whether `BSC_STATUS_GROUP_REQUIRES_SUBCOMMAND` should be added beyond the existing implementation-guide status list.
12. Confirm license before copying any reference code or accepting code that appears derived from a third-party implementation.

## Final rule

The core must be boring, bounded, and heavily host-tested.

A source task is not acceptable merely because firmware might compile later. It must prove host behavior for the parser/registry/validator/dispatcher/help surface and explicitly state what was not verified on adapters or hardware.
