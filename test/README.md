# Test Directory

This directory is reserved for host-first tests.

An initial self-contained C host test harness exists and runs through CTest. The core library should remain testable on a normal host machine without Arduino, ESP-IDF, UART hardware, or firmware flashing.

## Purpose

The current Phase 2A tests cover skeleton status, string-view, and output behavior. Future tests in this directory should prove the behavior of the platform-independent core:

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

## Expected future layout

The test strategy currently recommends a layout similar to:

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

The initial harness uses `test/test_main.c` and CTest without a third-party framework. Exact future names may change as parser, registry, dispatch, and help tests are added.

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

Do not claim hardware validation unless hardware was actually used and evidence is provided. Full parser, golden-output, adapter compile, and hardware tests remain deferred after the Phase 2A skeleton.

## Golden tests

Generated help, command-list output, redacted echo/status output, and representative error messages should have golden-output tests once formatting is approved.

Golden files should live under:

```text
test/golden/
```

Intentional output changes must update the golden files and explain the change in the final receipt.

## Static checks

Future test tooling should include forbidden-pattern checks for the core. At minimum, checks should guard against:

- Heap allocation in core source.
- Arduino `String` in core source.
- STL containers in core source.
- Arduino or ESP-IDF includes in core source.
- Unbounded `sprintf`.
- `atoi`/`atof` without end-pointer validation.
- Hidden stateful menu behavior.

See `docs/test_strategy.md` for the canonical test policy.
