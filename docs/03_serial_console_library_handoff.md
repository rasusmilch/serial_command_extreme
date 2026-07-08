# Bounded Serial Console Library — Handoff for New Chat

## Context

The user wants a reusable embedded serial console library, not a one-off help command system. The new library should support command registration, nested command paths, typed argument validation, callbacks/function pointers, bounded storage, and generated manpage-style help.

The user will upload the same archive of Arduino/embedded command libraries to the new chat and put this work into a GitHub repository. After the library is implemented, the user plans to return to the AS7331 UV console project and integrate the new library there.

## User constraints

- No heap allocation in the core.
- No Arduino `String` in the core.
- No STL containers in the core.
- No menu libraries.
- Minimal and bounded stack use.
- C core preferred, with adapters for Arduino/ESP-IDF/host.
- All capacities bounded by compile-time configuration or caller-provided storage.
- Commands must have operator-facing help/manpages.
- Every command should be registered with metadata, argument schema, and callback.
- Typed arguments should include integer, unsigned integer, float, string, boolean, enum, and none; secret/password strings should be supported or planned.
- Nested command paths should be supported, such as `settings wifi set ssid <ssid>`.
- Group/menu-like browsing should exist through command namespaces and help output, not through hidden stateful menu mode.

## Prior art that must be inspected

Inspect the uploaded archive, especially:

- AdvancedCLI
- StaticSerialCommands
- SimpleSerialShell
- SerialUI
- SerialCommandCoordinator
- SerialCommands / SerialCommand / SerialCommand_Advanced / SerialCmd
- ParseCommands
- SerialConfigCommand
- CommandCatcher
- tinyCommand
- cmdArduino

Inspect PT100 Mesh console code:

- `main/console_registry.h`
- `main/console_registry.c`
- `main/console_help.h`
- `main/console_help.c`
- `main/console_alerts.c`

Key PT100 concepts to borrow:

- Registry entries contain command metadata and function pointer.
- Help registry is separate but fed by the same command metadata.
- Help supports index, command manpage, and subtopic manpages.
- Alert command demonstrates subtopic metadata with summary, synopsis, details, parameters, and examples.

## Important correction from prior discussion

Do not treat this as a `help` feature. Help is a built-in consumer of the registry. The real product is a reusable command parser/dispatcher subsystem.

## Desired MVP

Implement a standalone bounded serial console core with:

- Static command descriptor array.
- Nested command path matching.
- Tokenizer with quotes and bounded token views.
- Typed positional argument schemas.
- Numeric range validation.
- String length validation.
- Enum choice validation.
- Secret argument redaction.
- Callback dispatch.
- Built-in `help`, `help <path>`, and `commands` output.
- Host tests.
- Arduino adapter.
- Examples.

## Recommended first task in the new chat

Use this exact task text:

```text
You are ChatGPT acting as the senior embedded architect for a reusable bounded serial console library.

Read the uploaded design intent, roadmap, and implementation guide. Inspect the uploaded command-library archive and the PT100 Mesh console reference code. Produce a read-only implementation plan for a standalone bounded serial console library. Do not write code yet.

The plan must include:
1. Prior-art findings from AdvancedCLI, StaticSerialCommands, SimpleSerialShell, SerialUI, and PT100 Mesh.
2. Recommended file layout.
3. Core data structures.
4. Parser/tokenizer algorithm.
5. Command descriptor schema.
6. Typed argument validation strategy.
7. Help/manpage rendering strategy.
8. Error/status code design.
9. Arduino adapter design.
10. Host test plan.
11. Example commands to implement.
12. Decisions requiring user approval.

Constraints:
- No heap allocation in the core.
- No Arduino String in the core.
- No STL containers in the core.
- No menu libraries.
- Static or caller-provided bounded storage only.
- Function pointers/callbacks are allowed.
- Keep help generated from command metadata.
- Treat command namespaces as menu-like for discovery, but keep complete line commands for automation.

Return the plan in one copy/paste-ready fenced text block.
```

## Suggested implementation sequence after plan approval

1. Core tokenizer and string-view utilities.
2. Static command descriptor model.
3. Command path matching.
4. Typed argument parsing/validation.
5. Dispatch callbacks and output callback.
6. Built-in help index/group/leaf manpages.
7. Host tests and golden help-output tests.
8. Arduino adapter.
9. Examples.
10. AS7331 integration pilot in a separate later task.

## Integration target after standalone library exists

AS7331 UV console candidate commands:

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

Operator-level help should explain why settings exist:

- `gain`: sensitivity versus saturation risk.
- `time`: weak-signal resolution versus speed and saturation risk.
- `clock`: advanced timing/control setting.
- `divider`: advanced conversion-time scaling.
- `output`: raw versus scaled estimates.
- `smooth`/`alpha`: EWMA smoothing behavior.
- `rate`: stream cadence and conversion-time limits.
- `channels`: output mask only, not sensor acquisition mask.
- `ready`: optional hardware wait pin.
- `flags`: status flags and WARN/OK interpretation.

## Final note

The library should be designed as a reusable project first. Do not let AS7331-specific behavior leak into the core. AS7331 should be an integration example, not the architecture source.
