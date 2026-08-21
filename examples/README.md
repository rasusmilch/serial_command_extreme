# Examples Directory

This directory contains host-runnable examples that demonstrate the library without weakening the core architecture.

## Implemented: `basic_status`

`basic_status/` provides the first Phase 4 example. Its shared example-local module owns one static registry with `status` and `reset_stats`, while `main.c` supplies a real console, caller-owned workspace, stdout callback, and a fixed built-in-aware execution sequence. The console deliberately configures `help_catalog == NULL`; ordinary descriptor metadata generates `help status`.

The deterministic demonstration seeds `commands_processed=7` and `errors=2`. `status` reports both counters without mutation, and `reset_stats` clears both to zero. Build and run it with:

```sh
cmake -S . -B build -DSCE_BUILD_EXAMPLES=ON
cmake --build build
./build/examples/basic_status
```

The shared module is linked into `test/sce_basic_status_tests`, so integration coverage executes the actual descriptors rather than copies. It uses only the public library API and fixed caller-owned storage; it introduces no transport adapter or line accumulator.

## Example goals

Examples should show realistic use of the bounded command registry and generated help system while staying small enough to review.

Examples should demonstrate:

- Static command descriptor tables.
- Complete-line command execution.
- Typed argument schemas.
- Generated `help`, `help <path>`, and `commands` output.
- Secret argument redaction.
- Host execution without hardware.
- Arduino adapter usage after the adapter exists.
- Sensor/settings-style command organization.

## Remaining planned examples

The implementation guide currently suggests examples similar to:

```text
examples/host_sensor_settings/
examples/arduino_basic/
examples/arduino_sensor_console/
```

Exact names may change after the read-only architecture plan.

## Host examples

Host examples should compile and run on a normal development machine. They should not require firmware flashing.

Suggested host examples:

```text
status
help
gain 2048
settings wifi status
settings wifi set ssid "Shop AP"
settings wifi set password "example password"
help settings wifi set password
```

Host examples should be suitable for smoke tests and documentation snippets.

## Arduino and ESP-IDF examples

Platform examples belong in this directory or in adapter-specific subdirectories, but platform-specific code must not leak into `src/`.

Arduino examples may use Arduino `Stream` through an adapter. ESP-IDF examples may use ESP-IDF UART/console APIs through an adapter. The core must remain independent of both.

## Example acceptance rules

Example tasks should include:

- Build or compile checks when the relevant toolchain is available.
- A statement when hardware validation was not run.
- Clear separation between host examples and hardware examples.
- No raw secret values in logs or expected output.
- Links back to the command descriptors or generated help behavior being demonstrated.

Examples should not become the only tests. Behavior must be covered by tests under `test/`.
