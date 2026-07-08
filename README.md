# Serial Command Extreme

Serial Command Extreme is a planned reusable bounded embedded serial command library.

The project goal is a small, predictable command parser/dispatcher core for firmware projects. Firmware should be able to register commands, nested command paths, typed argument schemas, validation rules, callbacks, access metadata, and operator-facing help/manpages from one bounded metadata model.

This repository now has an initial Phase 2A C99 core skeleton and host test harness. The full parser, command registry, examples, and platform adapters have not been added yet.

## Intended use

The library is intended for small firmware projects that need a serial console with complete, scriptable, loggable commands such as:

```text
status
flags
gain 2048
output both
settings wifi status
settings wifi set ssid "Shop AP"
settings wifi set password "example password"
factory cal clear
help
help gain
help settings wifi
help settings wifi set password
```

`help` is not the product. Help is a built-in consumer of the same command registry used by parsing and dispatch. The primary product is the bounded command subsystem.

## Design goals

The core library should provide:

- Static or caller-provided command storage.
- Nested literal command paths.
- Bounded tokenization with quoted strings and escapes.
- String-view token handling where practical.
- Typed positional arguments.
- Argument types for none, signed integer, unsigned integer, float, boolean, enum, bounded string, and secret string.
- Numeric bounds, string length bounds, and enum validation.
- Secret/password argument redaction.
- Explicit status/error codes.
- Callback dispatch through function pointers.
- Platform-independent output callbacks.
- Generated help/manpage output from command metadata.
- Host-testable parser, matcher, validator, dispatcher, and help renderer.
- Arduino and ESP-IDF adapters outside the core.

## Hard constraints

The core must remain deterministic, bounded, and portable.

Hard constraints for the core:

- C-first implementation.
- No heap allocation.
- No Arduino `String`.
- No STL containers.
- No menu-library dependency.
- No platform API dependency.
- No hidden stateful menu mode.
- No unbounded recursion.
- No unbounded `printf`/formatting buffers.
- All major capacities must be compile-time configured or caller-provided.
- Input and output behavior must be stable enough for serial logs and simple host automation.

Adapters may use platform types such as Arduino `Stream`, but those types must not leak into the core.

## Non-goals

This project is not intended to become:

- A general terminal UI.
- A curses-style menu system.
- A shell with pipes, redirection, variables, or scripting.
- A line editor first.
- A command history implementation first.
- A persistent configuration database.
- An Arduino-only convenience wrapper.

Discovery can be menu-like through namespaces and help output, but normal operation should use complete command lines.

## Current repository status

Current status:

```text
Stage: Phase 2A core skeleton
Implementation source: initial skeleton added
Build system: CMake scaffold builds the core library and host tests
Tests: initial CTest host smoke tests added
Examples: not added yet
Arduino adapter: not added yet
ESP-IDF adapter: not added yet
License: not finalized in this README
```

Do not treat this repository as an installable or usable command parser yet; only the initial skeleton APIs and host smoke tests exist.

## Documentation anchors

The current source of truth is the documentation in `docs/`:

```text
docs/00_serial_console_library_design_intent.md
docs/01_serial_console_library_roadmap.md
docs/02_serial_console_library_implementation_guide.md
docs/03_serial_console_library_handoff.md
docs/code_documentation_policy.md
docs/prior_art_review.md
```

Read these before implementation work:

- `docs/00_serial_console_library_design_intent.md` explains the product intent, constraints, non-goals, and core conceptual model.
- `docs/01_serial_console_library_roadmap.md` defines the phase order from anchors through MVP parser, help, examples, adapters, and AS7331 integration pilot.
- `docs/02_serial_console_library_implementation_guide.md` sketches the recommended file layout, configuration caps, status codes, descriptor structures, tokenizer policy, parser algorithm, examples, and tests.
- `docs/03_serial_console_library_handoff.md` preserves prior context and the recommended first Codex planning task.
- `docs/code_documentation_policy.md` defines how future source, tests, APIs, adapters, and hardware-facing code must be documented.
- `docs/prior_art_review.md` records what was inspected from existing command libraries and what should be borrowed or rejected.

## Prior-art conclusion

No reviewed command library is a clean direct base for this project.

The design should selectively borrow:

- AdvancedCLI's typed validation seriousness.
- StaticSerialCommands' static embedded registration discipline.
- SimpleSerialShell's direct execution and host-test ergonomics.
- SerialUI's browsing/discovery ideas without stateful menu mode.
- PT100 Mesh's registry-driven operator help/manpage model.

The implementation should be original and shaped around this project's bounded C-first core, not copied wholesale from any reference implementation.

## Planned core shape

The implementation guide currently expects a core similar to:

```text
bsc_console_t
  references the command registry
  stores bounded line/input state
  carries application context and output callback

bsc_command_t
  literal path tokens
  node type: group or executable command
  access level
  synopsis, description, examples, related help
  typed argument descriptors
  handler callback
  optional access check
  optional user data

bsc_arg_def_t
  argument name
  type
  required flag
  numeric bounds
  string length bounds
  enum choices
  help text
  secret/redaction behavior
```

Expected execution flow:

```text
trim input
bounded tokenize
match longest command path
validate access
parse and validate arguments
dispatch callback
render deterministic result or error
```

Exact names and layout may change after the read-only architecture plan, but the constraints should not be weakened without explicit approval.

## Planned repository layout

The implementation guide recommends this general layout:

```text
README.md
LICENSE
docs/
src/
adapters/
  arduino/
  esp_idf/
examples/
  host_basic/
  host_sensor_settings/
  arduino_basic/
  arduino_sensor_console/
test/
  golden/
```

The planned core source files include tokenizer, registry, parser, dispatch, help, and console modules. Host tests should cover tokenization, command matching, typed argument parsing, validation failures, secret redaction, dispatch, and generated help output.

## Example command descriptor intent

A future command descriptor should be able to express behavior like this without duplicating parser and help logic:

```text
path:        settings wifi set password
argument:    password, secret string, 8..64 bytes
help:        Set Wi-Fi password
handler:     handle_wifi_set_password
access:      normal
```

From that metadata, the library should know how to:

- Match `settings wifi set password "example password"`.
- Validate exactly one secret string argument.
- Reject too-short or too-long values.
- Redact the value in echo/status/log paths.
- Generate `help settings wifi set password`.
- Dispatch the handler with a typed parsed argument view.

## Development workflow expectation

For nontrivial work, use this sequence:

```text
Plan -> Review -> Execute -> Validate
```

Before implementation begins, create a read-only architecture plan from the anchors and prior-art review. Do not start by writing parser code, Arduino wrappers, or examples until the architecture plan is approved.

Future Codex tasks should:

- Inspect the current repository head before making decisions.
- Read the design intent, roadmap, implementation guide, handoff, documentation policy, and prior-art review.
- State exactly what source and anchors were inspected.
- Preserve the hard constraints.
- Keep source changes narrow.
- Include tests for behavior, rejection cases, and redaction.
- Report files changed, commands run, command results, and unverified items.

## Documentation standard

Future source-code tasks must follow `docs/code_documentation_policy.md`.

In short:

- Public C headers, structs, enums, callbacks, macros that behave like APIs, and nontrivial functions need Doxygen-compatible comments.
- C++ adapter classes and methods need Doxygen-compatible comments where they define ownership, lifetime, blocking behavior, allocation policy, or side effects.
- Python tooling needs PEP 257-compatible docstrings, with Google-style sections where helpful.
- Inline comments should explain why, not narrate syntax.
- Hardware, timing, redaction, ownership, lifetime, strict rejection, and regression-protection comments must not be removed without an accurate replacement.

## Next recommended task

The next useful task is a read-only architecture plan.

That plan should inspect the uploaded reference archive and current repository anchors, then define:

- Recommended file layout.
- Core data structures.
- Tokenizer algorithm.
- Command descriptor schema.
- Longest-path matching rules.
- Typed argument validation strategy.
- Help/manpage renderer strategy.
- Error/status code design.
- Arduino adapter design.
- Host test plan.
- Example commands.
- Open decisions requiring approval.

No code should be written in that task.

## License

License selection is not finalized in this README. Do not copy code from reference libraries into this repository until license compatibility has been reviewed and explicitly approved.
