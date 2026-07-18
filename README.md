# Serial Command Extreme

Serial Command Extreme is a reusable bounded embedded serial command library under active development.

The project goal is a small, predictable command parser/dispatcher core for firmware projects. Firmware should be able to define commands, nested command paths, typed argument schemas, validation rules, callbacks, access metadata, and operator-facing help/manpages from one bounded metadata model.

This repository currently includes the foundational C99 core, bounded tokenizer, static command descriptor types, registry descriptor validation, longest-path matcher, typed runtime positional argument parser with structured diagnostics, selected-command dispatch with access enforcement, output-neutral complete-line console orchestration with caller-owned execution workspace, a pure bounded generated-help core, optional complete-line `help`/`commands` built-in routing, optional extended-help catalog schema, visibility-independent structural validation, and pure flat-topic lookup, host tests, golden help-output fixtures, and forbidden-pattern source checks. It is not yet a complete installable serial command parser/dispatcher: extended help rendering, pure topic-page rendering, catalog-aware console grammar, examples, and platform adapters remain future work.

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
Stage: Foundational core, bounded tokenizer, static registry validation, longest-path matcher, typed positional argument parser, selected-command dispatch/access enforcement, output-neutral complete-line console orchestration, pure generated help, optional help/commands built-ins, Task 11C-1 extended-help catalog structural validation, and Task 11C-2A pure flat-topic lookup implemented
Implementation source: C99 core modules for config, status, string views, output, console configuration/workspace/result orchestration, descriptors, tokenizer, registry validation, matcher, typed argument parsing, selected-command dispatch/access enforcement, and internal compact float parsing
Build system: CMake builds the core library and host tests
Tests: Host coverage for foundational helpers, descriptor types, tokenizer, registry validation, matcher, typed argument parsing, operator diagnostics, selected-command dispatch/access enforcement, complete-line console orchestration, generated help, extended-help catalog validation, compact float enabled/disabled behavior, and forbidden-pattern checks
Typed argument parser: implemented for signed integer, unsigned integer, compact decimal float when enabled, boolean, enum, bounded string, and bounded secret
Selected-command dispatch and access enforcement: implemented
Output-neutral complete-line console orchestration: implemented
Generated help/manpages: pure metadata validation, exact path lookup, top-level index, complete command list, group pages, executable command pages, optional complete-line `help`/`commands` routing, catalog structural validation, and pure flat-topic lookup implemented; extended section rendering, pure topic-page rendering, and catalog-aware console grammar remain future work
Examples: not added yet
Arduino adapter: not added yet
ESP-IDF adapter: not added yet
License: not finalized in this README
```

Do not treat this repository as a complete installable command parser/dispatcher yet. The current core foundation can tokenize input, validate static descriptors, match command paths, parse typed positional arguments, enforce access for selected commands, dispatch handlers, and render pure generated help through explicit APIs, but it still lacks extended help sections, examples, and adapters.


## Optional complete-line help built-ins

The pure generated-help APIs remain directly callable for applications that want to render help outside the console pipeline. Applications that want complete-line operator built-ins can call `bsc_execute_line_with_builtins()` instead of `bsc_execute_line()`. The original `bsc_execute_line()` API remains application-only and still permits ordinary application commands named `help` or `commands`.

The built-in-aware API recognizes `help`, `help <exact command or group path>`, and `commands` with ASCII case-insensitive built-in names after the normal tokenizer has processed quotes and escapes. Help path lookup is exact: tokens after `help` are never treated as command arguments and never use longest-prefix dispatch matching. `commands` accepts no extra tokens.

Built-ins use only the output wrapper configured in `bsc_console_init()`. They add no prompts, echo, automatic diagnostics, or final OK/ERR text. If no output is configured, valid help rendering reaches the existing pure renderer and returns its `BSC_STATUS_INTERNAL_ERROR`. Help visibility is supplied with optional `bsc_help_options_t`; NULL uses the existing defaults. Built-in names are reserved only in `bsc_execute_line_with_builtins()` for the invoked built-in, and a colliding application descriptor returns `BSC_STATUS_INVALID_DESCRIPTOR` with non-secret collision metadata.

## Compact float arguments

When `BSC_ENABLE_FLOAT` is enabled, runtime float arguments use a compact
operator grammar rather than the C library's general floating-point grammar.
Accepted input is an optional leading minus, one or more ASCII digits, and an
optional decimal point followed by one through `BSC_MAX_FLOAT_FRACTION_DIGITS`
ASCII digits. Scientific notation, leading plus signs, NaN, infinity,
hexadecimal floats, whitespace, and embedded NUL bytes are not operator inputs.

Compact decimal input is limited to the inclusive domain
`-1000000000.0` through `1000000000.0`. Registry validation rejects float
descriptor bounds outside that domain so operator range diagnostics remain
truthful. Exact decimal accounting, larger domains, or values that need
scientific notation should be modeled as scaled integer arguments or another
application-level representation.

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

## Target core shape

The current core uses static, borrowed descriptor metadata with caller-owned execution storage:

```text
bsc_console_t
  references the validated command registry
  carries application context and a by-value output callback wrapper
  remains read-only during execution

bsc_console_workspace_t
  stores bounded line/input, token, match, parsed-argument, and diagnostic state
  is caller-owned and supplied per complete-line execution

bsc_command_t
  borrowed literal path tokens
  node type: group or executable command
  borrowed typed argument descriptor array
  handler callback
  command context
  access level
  command flags
  optional access callback
  optional summary
  optional description

bsc_arg_def_t
  argument name
  argument type
  numeric bounds
  string or secret length bounds
  enum choices
  optional argument help
```

All current positional arguments are required by position; there is no current `required`
field and no optional-argument syntax. Generated help synopsis text is derived from the
descriptor path and argument metadata rather than from a stored synopsis field. Notes,
warnings, presentation examples, related descriptor references, and flat topic metadata are
represented outside `bsc_command_t` by the optional borrowed `bsc_help_catalog_t`; catalog
structural validation and pure flat-topic lookup are implemented, while extended rendering,
pure topic-page rendering, and catalog-aware console grammar remain future work.

Expected execution flow:

```text
adapter strips line endings
copy explicit byte span into caller-owned workspace
reject embedded NUL
bounded tokenize
match longest command path
validate access
parse and validate arguments
dispatch callback
return primary status plus optional non-secret result
```

Some future orchestration, dispatch, help, adapter, and example names may still change, but the approved constraints should not be weakened without explicit approval.

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

The current core source files include tokenizer, registry validation, matcher, typed argument parser, selected-command dispatch/access enforcement, output-neutral complete-line console orchestration, pure generated-help validation/lookup/rendering, extended-help catalog structural validation, pure flat-topic lookup, internal compact-float parser, descriptor, status, string-view, and output modules. Optional console help built-ins are implemented through `bsc_execute_line_with_builtins()`; planned future modules still include extended help rendering, pure topic pages, catalog-aware console grammar, adapters, and examples. Host tests currently cover foundational helpers, tokenization, registry validation, descriptor types, matcher behavior, typed argument parsing, selected-command dispatch/access enforcement, complete-line console orchestration, generated-help validation/rendering/golden output, exact operator diagnostics, compact-float enabled/disabled behavior, all fractional precision values from 1 through 6, secret non-disclosure behavior, focused extended-help catalog structural validation, pure flat-topic lookup, capacity overrides, and forbidden-pattern checks; future tests should add broader redaction, adapter, and integration coverage.

## Example command descriptor intent

The current descriptor metadata supports command matching, typed argument parsing, selected-command access enforcement, handler dispatch, and complete-line console orchestration. It is also intended to support future broader redaction and help modules:

```text
path:        settings wifi set password
argument:    password, secret string, 8..64 bytes
help:        Set Wi-Fi password
handler:     handle_wifi_set_password
access:      normal
```

From that metadata, the current reusable core can already:

- Match the `settings wifi set password` path through the complete-line console pipeline.
- Parse and length-check the secret argument through the selected-command dispatcher after access approval.
- Dispatch the handler with caller-owned workspace parsed arguments during synchronous execution.
- Preserve the secret as a typed borrowed string view only during active handler execution.
- Return primary statuses and optional non-secret structured result metadata without echoing the secret value.

Still-future integration must:

- Redact secrets in echo, status, logs, history, or other presentation layers outside the parser.
- Route console-level `help settings wifi set password` through the optional `bsc_execute_line_with_builtins()` integration layer when built-ins are desired.

## Development workflow expectation

For nontrivial work, use this sequence:

```text
Plan -> Review -> Execute -> Validate
```

The read-only architecture planning milestone established the implementation direction, and the pure generated-help foundation is now implemented and host-tested. Future implementation should continue from the current tokenizer, registry-validation, matcher, typed-argument-parser, selected-command dispatch/access, complete-line console orchestration, and pure help-rendering foundation; the remaining help work is extended rendering, pure topic pages, and catalog-aware console grammar, and adapters or examples should not begin ahead of approved remaining core work.

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

After independent validation of the optional console built-in routing stage for `help` and `commands`, the next useful implementation task is Task 11C-2 extended rendering and pure topic APIs, or another approved roadmap item with focused host tests and bounded-memory documentation. Do not skip directly to adapters or examples before the remaining core behavior is implemented and validated.

## License

License selection is not finalized in this README. Do not copy code from reference libraries into this repository until license compatibility has been reviewed and explicitly approved.
