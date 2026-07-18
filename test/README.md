# Test Directory

This directory is reserved for host-first tests.

An initial self-contained C host test harness exists and runs through CTest. When Python3 is available, CTest also runs the forbidden-pattern checker against `src/`. The core library should remain testable on a normal host machine without Arduino, ESP-IDF, UART hardware, or firmware flashing.

## Purpose

The current host tests cover foundational status, string-view, and output helpers; static descriptor type initialization; bounded tokenizer behavior; registry descriptor validation; longest-path matcher behavior; typed positional argument parsing and exact operator diagnostics; selected-command dispatch/access enforcement; output-neutral complete-line console orchestration; pure generated-help validation, lookup, rendering, and golden fixtures; optional complete-line help/commands built-in routing; compact float enabled/disabled behavior; all legal compact-float fractional precision settings from 1 through 6; compact magnitude endpoint tests; and the forbidden-pattern source check. Future tests in this directory should continue proving the behavior of the platform-independent core:

- Tokenization.
- Quoted strings and escapes.
- Strict input rejection.
- Command registry validation.
- Longest-path command matching.
- Typed argument parsing and validation.
- Numeric ranges.
- Enum values.
- String length bounds.
- Secret redaction.
- Access checks.
- Handler dispatch.
- Output/error formatting.
- Generated help/manpage output.

## Current and future layout

The current host test files are:

```text
test/
  README.md
  test_main.c
  test_bsc_types.c
  test_bsc_tokenizer.c
  test_bsc_registry.c
  test_bsc_matcher.c
  test_bsc_args.c
  test_bsc_dispatch.c
  test_bsc_console.c
  test_bsc_help.c
  test_bsc_help_catalog.c
  test_bsc_help_extended.c
  golden/
```

The harness uses `test/test_main.c` plus module-specific test runner sources and CTest without a third-party framework. CTest registers `sce_host_tests` for host behavior tests and `sce_forbidden_patterns` for the static guard when Python3 is found by CMake. `test_bsc_args.c` is the current typed-parser suite; `test_bsc_dispatch.c` covers selected-command access enforcement, parser integration, handler invocation, output-pointer passing, handler status propagation, and caller-owned dispatch storage. `test_bsc_console.c` covers registry-backed console initialization, caller-owned execution workspace, explicit-length complete-line execution, result phases, output-neutral behavior, cleanup, and same-workspace recursion rejection. `test_bsc_console_builtins.c` covers the separate built-in-aware complete-line API, exact help routing, commands routing, collision metadata, visibility options, output failures, cleanup, recursion, and secret non-disclosure. Additional parser cases should be added to the parser suite when defects or supported behavior require them. Future dispatch, access, orchestration, broader redaction, extended-help, integration, and golden-output tests may add files such as:

```text
test/
  test_dispatch.c
  test_bsc_help_extended.c
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

Exact future names may change as dispatch, access, orchestration, redaction, help, integration, and additional parser-regression tests are added.

## Required testing posture

Core PR validation should not require target hardware. Hardware validation belongs to adapter and integration tasks.

Future receipts should separate:

```text
Host tests
Golden-output tests
Static/forbidden-pattern checks
Adapter compile checks
Hardware validation
Unverified items
```

Do not claim hardware validation unless hardware was actually used and evidence is provided. Catalog-aware extended generated-help tests now cover current core output; adapter compile and hardware tests remain deferred.

## Golden tests

Generated help index, command-list output, ordinary group pages, ordinary executable-command pages, catalog-aware extended command/group pages, pure topic pages, and no-metadata compatibility pages have byte-exact LF golden-output tests. Future redacted echo/status output and representative error messages should add golden-output tests once formatting is approved.

Golden files should live under:

```text
test/golden/
```

Intentional output changes must update the golden files and explain the change in the final receipt.

## Static checks

The current CTest path includes a first-pass forbidden-pattern check for the core when Python3 is available. The checker is a guard against known bad patterns, not a full C parser. At minimum, checks should guard against:

- Heap allocation in core source.
- Arduino `String` in core source.
- STL containers in core source.
- Arduino or ESP-IDF includes in core source.
- Unbounded `sprintf`.
- `atoi`/`atof` without end-pointer validation.
- Hidden stateful menu behavior.

See `docs/test_strategy.md` for the canonical test policy.


`test_bsc_help_catalog.c` covers Task 11C-1 extended-help catalog structural validation, including zero-count pointer policy, exact descriptor-pointer membership, flat topics, deterministic example validation, related-command rules, visibility independence, and callback non-invocation. `test_bsc_help_extended.c` covers Task 11C-2 pure flat-topic lookup, topic-specific statuses, parent visibility inheritance, result clearing, validation precedence, catalog-aware command/group rendering, pure topic-page rendering, extended golden fixtures, no-metadata compatibility, visibility filtering, and exhaustive short-write termination.
