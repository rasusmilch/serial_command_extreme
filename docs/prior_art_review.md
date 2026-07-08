# Prior Art Review

## Purpose

This document records the prior-art findings used to shape `serial_command_extreme` before implementation begins.

The project intent is a reusable bounded embedded serial command library: a C-first core that can tokenize complete command lines, match nested command paths, validate typed arguments, dispatch callbacks, and generate operator-facing help/manpages from command metadata.

This document is not an API specification and does not approve copying any reference implementation wholesale. It identifies what to borrow, what to avoid, and what must be verified in the first architecture plan and implementation tasks.

## Inspection context

Last reviewed: 2026-07-07.

Primary project repository inspected:

```text
rasusmilch/serial_command_extreme
branch: main
head before this document: 452db44b9dc5cb9353708a60fec7469a2917e673
```

Project anchors inspected from the repository:

```text
docs/00_serial_console_library_design_intent.md
docs/01_serial_console_library_roadmap.md
docs/02_serial_console_library_implementation_guide.md
docs/03_serial_console_library_handoff.md
docs/code_documentation_policy.md
```

Uploaded archive inspected locally:

```text
/mnt/data/Archive.tar.gz
/mnt/data/sce_archive/
```

Reference-only implementations inspected from the archive:

```text
reference_only/AdvancedCLI
reference_only/StaticSerialCommands
reference_only/SimpleSerialShell
reference_only/SerialUI
reference_only/SerialCommandCoordinator
reference_only/SerialCommands
reference_only/SerialCommand
reference_only/SerialCommand_Advanced
reference_only/SerialCmd
reference_only/ParseCommands
reference_only/SerialConfigCommand
reference_only/CommandCatcher
reference_only/tinyCommand
reference_only/cmdArduino
reference_only/PT100_Mesh_Datalogger
```

Key PT100 files inspected for architecture precedent:

```text
reference_only/PT100_Mesh_Datalogger/main/console_registry.h
reference_only/PT100_Mesh_Datalogger/main/console_registry.c
reference_only/PT100_Mesh_Datalogger/main/console_help.h
reference_only/PT100_Mesh_Datalogger/main/console_help.c
reference_only/PT100_Mesh_Datalogger/main/console_alerts.c
```

Unverified items:

- No implementation source exists yet in the standalone repository.
- No license compatibility analysis has been completed for copying code. Treat all reference code as design inspiration only until licenses are reviewed.
- No build, CI, formatter, static-analysis, or unit-test commands exist yet for the standalone repository.
- No Arduino, ESP-IDF, AS7331, or hardware integration source exists yet in the standalone repository.
- The GitHub connector reported repository code-search indexing unavailable during prior inspection; discovery relied on known paths, repository metadata, and the uploaded archive.

## Executive conclusion

No reviewed library is a clean direct fit.

The closest conceptual match is a combination of:

- AdvancedCLI for typed arguments, validators, aliases, subcommands, help generation, and fixed capacity configuration.
- StaticSerialCommands for static embedded registration, typed/ranged argument constraints, quoted strings, subcommands, and program-memory awareness.
- SimpleSerialShell for simple command callbacks, host-callable execution, and `argc`/`argv` ergonomics.
- SerialUI only for menu-like discovery ideas, not for its stateful UI model.
- PT100 Mesh for the in-house registry/help/manpage architecture and operator-facing help depth.

The new library should implement its own bounded C-first core instead of adopting an existing library. The core should stay narrow: tokenizer, command descriptor model, path matcher, typed validator, dispatch, output callback, and generated help. Adapters and examples can be C++/Arduino-specific, but the core must not depend on Arduino `String`, STL containers, `std::function`, heap allocation, hidden menu state, or platform APIs.

## Fit criteria used for review

The desired library must support:

- Complete-line commands suitable for automation and logs.
- Static or caller-provided bounded storage.
- No heap allocation in the core.
- No Arduino `String` in the core.
- No STL containers in the core.
- No menu libraries or hidden stateful command mode.
- C-first core with adapters for Arduino, ESP-IDF, and host.
- Nested literal command paths such as `settings wifi set ssid <ssid>`.
- Typed positional arguments: none, signed integer, unsigned integer, float, boolean, enum, bounded string, and secret string.
- Numeric bounds, string length bounds, enum validation, and secret redaction.
- Callback dispatch with explicit status codes.
- Generated help/manpages from the same metadata used by parsing.
- Host tests for tokenizer, parser, matching, validation, dispatch, help, and redaction.

## Summary matrix

| Reference | Strong ideas to borrow | Do not adopt directly | Fit |
| --- | --- | --- | --- |
| AdvancedCLI | Typed args, validators, aliases, subcommands, help depth, fixed caps, host/native examples, overflow diagnostics | C++ object model, optional `std::function`, command-tree API shape, static scratch buffers, not plain-C portable core | Strong reference, not base |
| StaticSerialCommands | Static tables, typed constraints, ranges, quoted strings, subcommands, PROGMEM awareness | Macro-heavy descriptor style, limited type/help model, Arduino coupling | Strong embedded reference, not base |
| SimpleSerialShell | Simple callbacks, host-callable `execute`, `argc`/`argv` mental model, alternate tokenizer hook | Arduino `String`, shallow help, no typed schema, no rich validation/manpages | Useful ergonomics only |
| SerialUI | Group/menu browsing, help/navigation concepts, interactive examples | Stateful menu hierarchy, authentication/UI bulk, not scriptable complete-line-first | Borrow discovery ideas only |
| PT100 Mesh console | Registry entries with metadata + function pointer, separate help registry fed by command metadata, manpage sections, subtopic help | ESP-IDF/argtable coupling, one-project command behavior, manual subcommand parsing in handlers | Best in-house architecture precedent |
| SerialCommand variants | Simple serial command dispatch, small examples | Token parsing mostly left to handlers, limited metadata/help, Arduino style | Background only |
| ParseCommands | Lightweight parse-command style | Not enough schema/help/dispatch architecture | Background only |
| SerialConfigCommand | Configuration-command examples | Too narrow and project-specific | Background only |
| CommandCatcher | Listener/catcher concept and Doxygen-generated docs | Not a typed bounded registry/manpage system | Background only |
| tinyCommand | Very small command dispatch idea | Too minimal for typed arguments/help/manpages | Background only |
| cmdArduino | Simple Arduino command examples | Arduino-centric and too minimal | Background only |

## AdvancedCLI

### Relevant files inspected

```text
reference_only/AdvancedCLI/src/AdvancedCLI.h
reference_only/AdvancedCLI/src/internal/AdvancedCLI.h
reference_only/AdvancedCLI/src/internal/AdvancedCLI.cpp
reference_only/AdvancedCLI/src/internal/acli-argument.h
reference_only/AdvancedCLI/src/internal/acli-command.h
reference_only/AdvancedCLI/src/internal/acli-config.h
reference_only/AdvancedCLI/src/internal/acli-utils.h
reference_only/AdvancedCLI/test/test_acli.cpp
reference_only/AdvancedCLI/examples/*
```

### Capabilities observed

AdvancedCLI is the richest library in the archive.

It supports:

- Commands and subcommands.
- Aliases.
- Required arguments and positional arguments.
- Flags and named arguments.
- Validators and invalid-value callbacks.
- Help output with configurable depth.
- Native/host examples and tests.
- Compile-time capacity controls such as command count, total argument slots, name length, value length, description length, aliases, tokens, and input length.
- Overflow diagnostics for command and argument pool capacity.
- Case-sensitivity controls.
- Tokenization with bounded token arrays.

Its design makes a serious attempt to be capacity-aware. It has command arrays and pooled argument definitions instead of unbounded dynamic storage. The implementation records attempted registration counts so users can detect capacity loss instead of silently assuming all descriptors registered.

### Useful ideas to borrow

Borrow these concepts:

- Typed argument descriptors are first-class metadata, not handler-only parsing.
- Validators should be attached to argument metadata.
- Capacity limits should be explicit and inspectable.
- Registration overflow should produce a detectable status or diagnostic.
- Help output should be generated from command/argument metadata.
- Native/host tests and examples should exist from the beginning.
- Aliases are useful, but they should be deferred until the canonical path matcher is stable.
- Case-insensitive command matching should be supported or at least considered explicitly.

### Caveats and rejection reasons

Do not adopt AdvancedCLI directly as the core.

Reasons:

- It is a C++ library, not a deliberately plain-C portable core.
- It can use `std::function` depending on configuration and target support. That conflicts with the desired core rule of no STL containers or C++ runtime dependencies.
- Its command/subcommand object API is not identical to the desired namespace/manpage descriptor model.
- It supports named args, flags, persistent args, and richer CLI features that are useful but would add scope before the MVP parser/registry is proven.
- Some implementation storage is static inside parse/output paths. The new library should be explicit about console-owned or caller-owned storage and reentrancy expectations.
- It is not shaped around C structs that can be consumed directly from Arduino, ESP-IDF, and host builds.

### Design implication for this project

AdvancedCLI should influence the descriptor schema, validation model, help generation, and tests. It should not decide the implementation language, object model, registration API, or MVP feature set.

For MVP, prefer static `const bsc_command_t[]` descriptors and typed positional arguments. Defer flags, named arguments, aliases, persistent parent arguments, and default values unless a specific embedded use case requires them.

## StaticSerialCommands

### Relevant files inspected

```text
reference_only/StaticSerialCommands/src/StaticSerialCommands.h
reference_only/StaticSerialCommands/src/StaticSerialCommands.cpp
reference_only/StaticSerialCommands/src/Arg.h
reference_only/StaticSerialCommands/src/Command.h
reference_only/StaticSerialCommands/src/CommandBuilder.h
reference_only/StaticSerialCommands/src/Parse.h
reference_only/StaticSerialCommands/examples/*
```

### Capabilities observed

StaticSerialCommands is the strongest small embedded reference.

It uses static command definitions and argument constraints. The argument model includes types such as integer, float, and string. It supports range-like constraints and uses macros/templates to define argument metadata. It also shows subcommand handling and program-memory-friendly patterns relevant to Arduino-class targets.

The examples demonstrate simple commands, commands with arguments, and subcommands.

### Useful ideas to borrow

Borrow these concepts:

- Static registration is the right default for embedded command tables.
- Argument constraints should live with command descriptors.
- Numeric ranges should be declared as metadata, not ad-hoc in handlers.
- Quoted string handling is needed for Wi-Fi SSIDs, passwords, labels, and user-visible settings.
- Subcommands should be represented structurally instead of parsed entirely inside one handler.
- Program-memory awareness may matter for AVR/Arduino adapters later, but should not contaminate the core.

### Caveats and rejection reasons

Do not adopt StaticSerialCommands directly.

Reasons:

- The macro-heavy definition style may become hard to scale for a reusable library with rich manpages, access levels, redaction metadata, and future custom validators.
- The help model is too shallow for operator-facing manpages.
- The type model is narrower than the desired `int`, `uint`, `float`, `bool`, `enum`, `string`, `secret`, and `none` model.
- It is Arduino-oriented, while the new core must be host-testable and platform-independent.
- It does not provide the full registry/help/manpage structure desired for nested command paths.

### Design implication for this project

Use C structs as the primary descriptor format. Convenience macros can be added later after the struct model is stable and tested. Do not let macro ergonomics hide ownership, bounds, redaction, access, or help requirements.

## SimpleSerialShell

### Relevant files inspected

```text
reference_only/SimpleSerialShell/src/SimpleSerialShell.h
reference_only/SimpleSerialShell/src/SimpleSerialShell.cpp
reference_only/SimpleSerialShell/README.md
reference_only/SimpleSerialShell/CONFIGURATION.md
reference_only/SimpleSerialShell/examples/*
reference_only/SimpleSerialShell/extras/tests/*
```

### Capabilities observed

SimpleSerialShell provides a classic shell-style command model. Commands use a function signature similar to:

```text
int command(int argc, char **argv)
```

It supports adding commands, executing command strings, help output, alternate tokenizer hooks, and examples. It is easy to understand and useful as a mental model for host tests and handler ergonomics.

### Useful ideas to borrow

Borrow these concepts:

- A host-callable `execute("command line")` style entry point is valuable for tests.
- `argc`/`argv` remains a useful internal mental model even if public handlers receive typed parsed arguments.
- Alternate tokenizer behavior is useful to consider, but should not be in MVP unless needed.
- Simple command examples help prove API ergonomics.

### Caveats and rejection reasons

Do not adopt SimpleSerialShell directly.

Reasons:

- It uses Arduino `String` internally in places, which violates the core constraints.
- It does not provide a typed argument schema.
- It does not generate rich manpage-style help from command metadata.
- Token parsing and validation remain mostly the handler's burden.
- It does not support the desired nested path model with typed remaining arguments.

### Design implication for this project

The new library should provide a simple test-friendly function such as `bsc_execute_line()` or equivalent. Internally, token views can resemble `argv`, but handlers should receive typed validated arguments by default.

## SerialUI

### Relevant files inspected

```text
reference_only/SerialUI/README.md
reference_only/SerialUI/src/*
reference_only/SerialUI/examples/*
```

### Capabilities observed

SerialUI is a serial user-interface/menu framework. It supports menus, submenus, command items, help, navigation, authentication examples, widgets, and multiple communication-channel abstractions.

The README describes a hierarchy of menus and submenus with built-in navigation commands such as help, home, and up. It is designed for interactive operation through a serial channel.

### Useful ideas to borrow

Borrow these concepts only at the discovery/help level:

- Groups can be displayed like menus to improve operator discoverability.
- Help can list child commands under a namespace.
- Operators benefit from structured browsing when command sets become large.

### Caveats and rejection reasons

Do not adopt SerialUI directly.

Reasons:

- It is a stateful UI/menu system, not a complete-line command dispatcher.
- It changes the meaning of future input based on current menu state.
- It is too large and broad for the desired core.
- Authentication, widgets, menu navigation, and channel abstractions are scope creep for the MVP.
- The new library must stay automation-friendly: commands should be complete, loggable, copy/pasteable lines.

### Design implication for this project

Represent groups as command-path namespaces, not hidden interactive state. For example, support:

```text
help settings wifi
settings wifi set ssid "Shop AP"
```

Do not require:

```text
settings
wifi
set ssid Shop AP
```

The help renderer can show namespaces in a menu-like way, but the command parser should remain complete-line-first.

## PT100 Mesh console

### Relevant files inspected

```text
reference_only/PT100_Mesh_Datalogger/main/console_registry.h
reference_only/PT100_Mesh_Datalogger/main/console_registry.c
reference_only/PT100_Mesh_Datalogger/main/console_help.h
reference_only/PT100_Mesh_Datalogger/main/console_help.c
reference_only/PT100_Mesh_Datalogger/main/console_alerts.c
```

### Capabilities observed

PT100 Mesh is the best in-house architectural precedent.

Its registry entry includes command metadata and callback fields, including command name, summary, synopsis, description/help body, topic-help callback, function pointer, and argtable pointer. Its help module keeps a help registry and can print an index, a command manpage, and topic/subcommand help. The alert command demonstrates rich subtopic metadata with summary, synopsis, details, parameters/options, examples, and related usage.

### Useful ideas to borrow

Borrow these concepts:

- Keep command metadata and help metadata connected.
- Help should be generated from the same registration data used for parsing/dispatch.
- Help should support index help, group/command help, and leaf/subtopic help.
- Manpage-style sections are useful: name, synopsis, description, arguments/options, notes, examples, related commands.
- Command registration should include a function pointer and enough metadata for operator-facing documentation.
- A separate help renderer can consume the command registry rather than embedding help printing inside every handler.

### Caveats and rejection reasons

Do not copy PT100 architecture directly into the new core.

Reasons:

- PT100 is ESP-IDF-specific.
- It uses argtable3-style metadata, which should not become a dependency of the new portable core.
- It represents some subcommands as manual handler parsing rather than a generic nested literal path model.
- It is project-specific and includes PT100 domain behavior that must not leak into the standalone serial command library.

### Design implication for this project

The new library should preserve the PT100 spirit but generalize the model:

- Use `path[]` and `path_len` instead of a single command string plus manual subcommand parsing.
- Use generic typed argument descriptors instead of argtable3.
- Use a platform-independent output callback instead of `printf`/ESP-IDF assumptions.
- Keep help/manpage generation as a first-class consumer of descriptors.

## SerialCommandCoordinator

### Relevant files inspected

```text
reference_only/SerialCommandCoordinator/SerialCommandCoordinator.h
reference_only/SerialCommandCoordinator/README.md
reference_only/SerialCommandCoordinator/examples/*
reference_only/SerialCommandCoordinator/tests/*
```

### Capabilities observed

SerialCommandCoordinator is a lightweight coordination layer for serial command handling. It provides simple registration and examples, with integration-oriented tests.

### Useful ideas to borrow

- Keep examples small and integration-oriented.
- Test command execution paths, not only tokenizer utilities.
- Simple registration ergonomics matter.

### Caveats and rejection reasons

- It does not provide the desired typed argument schema and rich generated help model.
- It does not solve the bounded metadata/manpage problem.
- It is not the right core architecture for nested path matching.

## SerialCommands / SerialCommand / SerialCommand_Advanced / SerialCmd

### Relevant files inspected

```text
reference_only/SerialCommands/src/*
reference_only/SerialCommand/src/*
reference_only/SerialCommand_Advanced/src/*
reference_only/SerialCmd/src/*
reference_only/*/examples/*
```

### Capabilities observed

These libraries generally demonstrate classic Arduino serial-command dispatch: register commands, read serial input, split tokens, call handlers, and provide examples.

Some variants support argument access or more advanced command parsing than the simplest implementations. They are useful references for API ergonomics and small examples.

### Useful ideas to borrow

- Simple command registration examples should exist.
- Small examples are important for adoption.
- The library should make common `status`, `set`, and `get` style commands easy.

### Caveats and rejection reasons

- Most parsing responsibility remains in the handler.
- Metadata and help are too limited for the desired manpage model.
- Arduino-specific assumptions are common.
- Nested command paths and typed validation are not first-class enough.

### Design implication for this project

These libraries confirm the minimum baseline users expect, but the new library must go beyond them: typed descriptors, bounded validation, generated help, and host tests are not optional extras.

## ParseCommands

### Relevant files inspected

```text
reference_only/ParseCommands/src/ParseCommands.h
reference_only/ParseCommands/src/ParseCommands.cpp
reference_only/ParseCommands/README.md
reference_only/ParseCommands/examples/*
```

### Capabilities observed

ParseCommands is a lightweight parser-style Arduino library. It is useful as background for basic command parsing and examples.

### Useful ideas to borrow

- Keep simple command execution cheap.
- Do not overcomplicate the operator-facing syntax.

### Caveats and rejection reasons

- It is not a full command registry/help/manpage architecture.
- It does not provide the desired typed schema, nested paths, and redaction model.

## SerialConfigCommand

### Relevant files inspected

```text
reference_only/SerialConfigCommand/src/*
reference_only/SerialConfigCommand/README.md
reference_only/SerialConfigCommand/examples/*
reference_only/SerialConfigCommand/extras/*
```

### Capabilities observed

SerialConfigCommand is narrower and configuration-command-oriented. It demonstrates serial interaction for reading or setting values.

### Useful ideas to borrow

- Configuration commands need clear operator syntax.
- Examples should show setting and reading values.

### Caveats and rejection reasons

- It is too narrow to serve as the generic parser/dispatcher library.
- It does not provide a sufficiently general nested command-path or manpage model.

## CommandCatcher

### Relevant files inspected

```text
reference_only/CommandCatcher/src/*
reference_only/CommandCatcher/docs/*
reference_only/CommandCatcher/examples/*
```

### Capabilities observed

CommandCatcher demonstrates a listener/catcher style and includes generated Doxygen documentation.

### Useful ideas to borrow

- Doxygen-generated public API documentation is useful for a reusable library.
- Listener/callback concepts may inform adapter or dispatch design.

### Caveats and rejection reasons

- It is not the desired typed bounded registry/manpage system.
- It does not solve nested command paths or typed argument schema generation.

## tinyCommand

### Relevant files inspected

```text
reference_only/tinyCommand/src/tinyCommand.hpp
reference_only/tinyCommand/README.md
reference_only/tinyCommand/examples/*
```

### Capabilities observed

TinyCommand is intentionally small. It demonstrates the lowest-complexity end of serial command dispatch.

### Useful ideas to borrow

- The core should remain small and easy to reason about.
- Avoid adding a terminal UI or scripting features to the MVP.

### Caveats and rejection reasons

- It is too minimal for typed arguments, generated help, nested paths, redaction, and host tests.

## cmdArduino

### Relevant files inspected

```text
reference_only/cmdArduino/cmdArduino.h
reference_only/cmdArduino/cmdArduino.cpp
reference_only/cmdArduino/README.md
reference_only/cmdArduino/examples/*
```

### Capabilities observed

cmdArduino demonstrates simple Arduino command callbacks and examples.

### Useful ideas to borrow

- Keep beginner examples approachable.
- Provide simple LED/status/PWM-style examples for Arduino users after the core exists.

### Caveats and rejection reasons

- It is Arduino-centric.
- It does not provide the desired typed metadata/help/validation architecture.
- It should not influence the core design beyond adapter/example ergonomics.

## Cross-cutting findings

### 1. Do not duplicate syntax between parser and help

Several simple libraries leave syntax and validation in handlers. That makes help output drift from actual accepted input. `serial_command_extreme` must keep path, argument schema, bounds, enum choices, access level, and help text in one descriptor model.

### 2. Nested literal paths are better than ad-hoc subcommand parsing

The desired command model should treat `settings wifi set ssid` as literal path tokens and `<ssid>` as an argument. This keeps matching, help, and tests deterministic.

Avoid designing a generic `settings` handler that manually parses all remaining tokens. That repeats the weakness of simpler command libraries and prevents consistent generated help.

### 3. Bounded does not only mean no heap

A bounded core must define and enforce limits for:

- Input line length.
- Token count.
- Token length.
- Command count.
- Command path depth.
- Argument count.
- Enum choice count.
- Output chunk size.
- Help rendering scratch buffers.

Too-long input must fail safely with explicit status codes.

### 4. Handler callbacks should not receive unvalidated strings by default

The core should validate arguments before dispatch. Handlers should receive typed parsed values where possible. A custom parser or raw-token hook may be added later for unusual commands, but it should be an explicit exception.

### 5. Secret values need first-class redaction

Secret/password arguments must be declared in metadata. Echo, status helpers, examples, logs, and test failure output must not leak secret values. Redaction should not be left to every handler independently.

### 6. Help must be a built-in registry consumer

The help system should not be an afterthought. It should consume the same descriptors as the parser and dispatcher.

Required help modes:

```text
help
help <path>
commands
```

Likely output modes:

- Top-level index.
- Group/namespace child listing.
- Leaf command manpage.
- Argument/value details.
- Examples and related commands.

### 7. Complete-line operation is mandatory

The core must not require stateful menu navigation. Command namespaces may be displayed like menus, but the command line should remain complete, testable, loggable, and scriptable.

### 8. Host tests should be designed before hardware adapters

The parser, tokenizer, matcher, validator, dispatcher, help renderer, and redaction behavior should compile and run on host before Arduino/ESP-IDF adapters are added.

### 9. Adapters must not leak into the core

Arduino `Stream`, ESP-IDF UART/console APIs, stdio, and any future transport belong in adapters. The core should communicate through plain C structs, string views, callbacks, and explicit status codes.

### 10. Comments/docs should protect behavior, not decorate syntax

The code documentation policy should be enforced from the first implementation task. Public headers should document ownership, storage limits, lifetime, status codes, blocking behavior, thread/ISR safety, and redaction rules.

## Recommended borrowed architecture

The MVP should use this model:

```text
bsc_console_t
  owns or references bounded runtime context
  references const command descriptor array
  stores line buffer and output callback

bsc_command_t
  path tokens
  node type: group or executable command
  access level
  synopsis/description/examples/related
  typed argument descriptor array
  callback
  optional access check
  optional user data

bsc_arg_def_t
  name
  type
  required flag
  numeric bounds
  string length bounds
  enum choices
  help text
  redaction behavior for secret fields

bsc_execute_line()
  trim
  tokenize
  match longest command path
  validate access
  validate typed arguments
  dispatch callback
  render deterministic errors
```

The exact names are not final, but the shape should remain close to the implementation guide unless the architecture plan identifies a better reasoned alternative.

## Recommended MVP scope

Implement first:

- Static command descriptor arrays.
- Bounded tokenizer with quotes and escapes.
- String-view tokens.
- Longest-path command matching.
- Group nodes and executable leaf nodes.
- Typed positional argument validation.
- `int`, `uint`, `float`, `bool`, `enum`, `string`, `secret`, and `none` argument types.
- Numeric bounds, string length bounds, and enum value validation.
- Explicit status codes.
- Output callback abstraction.
- Built-in `help`, `help <path>`, and `commands`.
- Host unit tests and golden help-output tests.
- Minimal Arduino adapter after the host-tested core exists.
- Examples for status, sensor settings, Wi-Fi settings, and locked/factory access.

Defer:

- Command history.
- Line editing.
- Completion/hints.
- Aliases unless architecture plan approves a minimal canonical alias model.
- Authentication/unlock workflows beyond access-check callbacks.
- JSON output.
- Persistent settings storage.
- Named options/flags.
- Optional positional arguments.
- Runtime command registration unless explicitly approved.
- Scripting, pipes, variables, or shell-like expansion.

## GO / NO-GO recommendations

### GO

Proceed with a standalone implementation plan for a new C-first bounded core.

Use the reference implementations only for inspected patterns and test ideas. The new implementation should be original and shaped by the design anchors.

### NO-GO

Do not adopt AdvancedCLI, StaticSerialCommands, SerialUI, SimpleSerialShell, or any SerialCommand variant as the project base.

Do not start by wrapping Arduino `Serial` or `String`. That would bias the architecture toward one platform and weaken host testing.

Do not start by building a menu UI. That conflicts with complete-line automation and predictable logs.

Do not let help output be hand-written separately from parser schema. That causes drift.

## Decisions still requiring explicit approval

These should be resolved in the read-only architecture plan before implementation:

1. C standard: C99 or C11.
2. Whether descriptor definitions start as raw C structs only, or include optional convenience macros from the beginning.
3. Whether runtime registration is deferred entirely, or whether bounded caller-provided registration is included in MVP.
4. Whether aliases exist in MVP or are deferred.
5. Whether enum and command-path matching are case-insensitive by default.
6. Whether optional positional arguments are prohibited in MVP.
7. Whether the library owns final `OK:`/`ERR:` formatting or only provides helper hooks.
8. Whether help output must be byte-for-byte stable in golden tests from the first implementation task.
9. Whether AVR `PROGMEM` support is deferred to adapters or considered in descriptor layout now.
10. Whether custom validators and custom parsers exist in MVP or are deferred until core typed arguments are stable.

## Required use in future Codex tasks

Future planning and execute tasks must read this file alongside:

```text
docs/00_serial_console_library_design_intent.md
docs/01_serial_console_library_roadmap.md
docs/02_serial_console_library_implementation_guide.md
docs/03_serial_console_library_handoff.md
docs/code_documentation_policy.md
```

Implementation prompts should restate this acceptance language:

```text
Prior-art acceptance criteria:
- Read docs/prior_art_review.md before proposing or editing implementation code.
- Borrow concepts selectively; do not copy a reference implementation wholesale.
- Preserve the C-first bounded core architecture: no heap allocation, no Arduino String, no STL containers, no menu-library dependency, and no platform API dependency in the core.
- Use nested literal command paths and typed argument metadata rather than handler-only parsing.
- Keep generated help/manpages tied to the same descriptors used by parser and dispatcher.
- State any deviation from this prior-art review explicitly in the final receipt, including why the deviation was necessary.
```

## Final note

The strongest direction is not to choose the "best" existing serial command library. The strongest direction is to build a small, boring, testable core that combines the right proven ideas:

- AdvancedCLI's typed validation seriousness.
- StaticSerialCommands' embedded static-registration discipline.
- SimpleSerialShell's direct execution/test ergonomics.
- SerialUI's discovery/help browsing concept without stateful menu mode.
- PT100 Mesh's registry-driven operator manpage model.

That combination matches the project intent better than any single reviewed implementation.
