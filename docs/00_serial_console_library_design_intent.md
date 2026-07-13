# Bounded Serial Console Library — Design Intent Anchor

## 1. Project intent

Create a reusable embedded serial console library for small firmware projects. The library should let firmware register commands, nested command paths, typed arguments, validation rules, dispatch callbacks, and complete operator-facing help/manpages from one bounded metadata model.

The console is not only a `help` system. Help is a built-in consumer of the same command registry used for parsing and dispatch. The primary product is a command subsystem that can support commands such as:

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

The library should be generic enough to reuse across Arduino, ESP-IDF, and small host-test builds, while staying bounded and predictable enough for microcontrollers.

## 2. Why this library exists

Existing command libraries in the reviewed archive are useful references, but none fit the desired shape without compromise.

AdvancedCLI is the richest reference: typed arguments, aliases, validators, subcommands, help output, and fixed capacity configuration. It should influence the schema and validation model. It is not a clean direct fit because some builds use `std::function`, the command tree/subcommand model is not the exact reusable manpage/namespace model desired here, and its API is not a deliberately plain-C portable core.

StaticSerialCommands is the strongest small embedded reference: static tables, typed int/float/string arguments, ranges, quoted strings, program-memory awareness, and subcommands. It should influence static registration and range validation. It is not a complete fit because help is shallow, types are limited, and the macro style may become hard to scale across projects.

SimpleSerialShell is a useful reference for classic `argc`/`argv` command handlers and simple serial shell behavior. It should influence the callback shape and testability, but it lacks typed argument schema and generated help.

SerialUI is a useful reference for menu-style concepts and interactive prompting, but it is not the right base for this library. It is too large, stateful, and menu/UI-oriented for an automation-friendly command console.

The PT100 Mesh project is the best in-house architecture reference. It already separates command registration from help/manpage metadata. Its entries include command name, summary, synopsis, description, optional help body, optional topic-help callback, function pointer, and argtable pointer. Its help flow supports index help, command manpages, and command-topic pages. This library should preserve that spirit but make it platform-independent, bounded, and reusable.

## 3. Design principles

The library must be deterministic and bounded. Do not use heap allocation, `String`, STL containers, exceptions, RTTI, hidden recursion, unbounded stack buffers, or runtime-discovered command storage. All input line length, token count, command count, command depth, argument count, string length, and output scratch buffers must have compile-time caps.

The library must be metadata-driven. A command's syntax, accepted arguments, bounds, access level, help text, examples, and dispatch callback should be registered together. Do not duplicate command syntax in one parser and separately in a help string unless unavoidable.

The library should support explicit command namespaces rather than a hidden stateful menu. `settings wifi set ssid Shop_AP` is preferred over entering a `settings` menu, then a `wifi` menu, then typing `set ssid`. The namespace may be displayed like a menu for discovery, but the command line should remain complete, loggable, scriptable, and testable.

The library should provide operator help by default. Every command leaf should have at least a brief summary, synopsis, detailed description, argument description, and examples. Group nodes should have summaries and list their children. Help should be consistent and generated from metadata.

The library should be usable with both direct command functions and structured typed arguments. The lowest-level handler can receive a context pointer and a parsed argument view. For advanced cases, a command can use a custom parser/handler while still providing metadata and help.

The library should be host-testable. The parser, matcher, argument validator, and help renderer should compile and test on a host without Arduino or ESP-IDF.

## 4. Primary constraints

Hard constraints:

- C core first, with optional Arduino/C++ adapters.
- No Arduino `String` in the core.
- No heap allocation in the core.
- No STL containers in the core.
- No menu libraries.
- No dynamic command discovery.
- No unbounded recursion.
- No unbounded printf buffers.
- All buffers and arrays must be compile-time or caller-provided and size-bounded.
- All parsing should be non-blocking at the input layer; dispatch may be synchronous unless a command explicitly defers work.
- Command output must be stable enough for serial logs and simple host automation.

Allowed implementation tools:

- `char` arrays and string views.
- `const` metadata tables.
- Function pointers.
- Caller-provided context pointers.
- Compile-time config macros.
- Platform output callbacks.
- Optional platform adapters for Arduino `Stream`, ESP-IDF console/UART, and host stdio.

## 5. Non-goals

This library is not a general terminal UI, not a curses-style menu, not a shell with pipes and redirection, not a scripting language, not a line editor first, and not a configuration database.

The library should not own project settings storage. It may provide typed setters/getters and callbacks, but persistence belongs to the application.

The library should not force one hardware platform. Arduino and ESP-IDF adapters are separate from the core.

The library should not require interactive menu state for normal operation. Commands should be complete lines.

## 6. Core conceptual model

A console contains a fixed registry of command nodes. A node may be a group or an executable command.

A command path is a sequence of literal tokens:

```text
settings wifi set ssid
```

The node at that path can define arguments that follow the path:

```text
settings wifi set ssid <ssid:string[1..32]>
settings wifi set password <password:secret[8..64]>
gain <value:enum{1,2,4,8,16,32,64,128,256,512,1024,2048}>
rate <hz:float[0.1..50.0]>
status
```

A node can have child nodes. A group node may not execute by itself, or it may execute a default action such as printing status/listing children. A leaf node usually has a handler callback.

The parser should match the longest command path first, then parse the remaining tokens as arguments according to the leaf's schema.

## 7. Required command capabilities

The library should support:

- Fixed command paths of configurable depth.
- Parent/group nodes.
- Executable leaf commands.
- Optional executable group commands.
- Typed positional arguments.
- Literal subcommands as path nodes, not ad-hoc handler parsing.
- Optional aliases.
- Optional access levels such as normal, advanced, factory, hidden, or locked.
- Per-command context pointer or global application context pointer.
- Optional custom validator callback.
- Optional custom parser callback for unusual commands.
- Optional completion/hints later, not in MVP.

## 8. Required argument types

Minimum argument types:

- none
- literal/path token
- signed integer
- unsigned integer
- float
- boolean
- enum/string choice
- bounded text string
- secret/password text string

Argument metadata must include:

- name
- type
- required/optional
- minimum/maximum for numeric types
- maximum length for strings
- allowed values for enums
- whether value is sensitive and must be redacted in echo/status/help examples
- help text

Potential future types:

- MAC address
- IPv4 address
- duration in milliseconds/seconds/minutes
- temperature with unit suffix
- hexadecimal integer
- fixed-size byte array

These should be implemented as validators or application-defined custom types after the core is stable.

## 9. Help and documentation model

Help is generated from command metadata.

Required commands:

```text
help
help <path>
help <path> <subtopic>
commands
```

`help` should list top-level commands/groups with one-line summaries.

`help settings` should show the `settings` group description and children.

`help settings wifi` should show Wi-Fi group description and children.

`help settings wifi set ssid` should show a full manpage with name, synopsis, description, arguments, accepted values/bounds, examples, warnings, and related commands.

Manpage sections should be predictable:

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

Every executable command must have enough metadata to produce a useful manpage. The library should fail validation or return an error if a public command lacks a required help field.

## 10. Output policy

The library should provide output helpers but not impose a project-specific style. Current complete-line console orchestration is output-neutral: it returns statuses and optional non-secret result metadata and forwards configured output to handlers, but it does not emit echo, help, error, `OK`, `ERR`, or final-result text itself. Future presentation layers may choose conventions such as:

```text
> gain 2048
OK: gain=2048x

> gain 3
ERR: gain: invalid value
```

Stable automation-friendly lines are preferred. Human explanations may be printed before final `OK:`/`ERR:` if appropriate, but the final result line should be deterministic.

Sensitive values should be redacted in echo, status, and logs:

```text
> settings wifi set password ********
OK: settings wifi password set
```

The library separates command echo and presentation policy from dispatch. Applications or future adapters may enable echo, disable echo, or redact sensitive tokens outside the current output-neutral core execution path.

## 11. Memory model

The core must use caller-provided storage. Default caps should be configurable:

```c
#define BSC_MAX_COMMANDS       64
#define BSC_MAX_PATH_TOKENS     6
#define BSC_MAX_ARGS            8
#define BSC_MAX_TOKENS         24
#define BSC_MAX_LINE_LEN      160
#define BSC_MAX_TOKEN_LEN      64
#define BSC_MAX_OUTPUT_CHUNK  128
#define BSC_MAX_ENUM_CHOICES   16
```

No handler should receive pointers into temporary stack data after dispatch returns. In the implemented complete-line console path, token views and parsed string/secret views borrow the caller-owned workspace line buffer and are valid only during synchronous dispatch/handler execution. If a command needs to persist data, the application must copy it into its own bounded storage.

## 12. Parser policy

The tokenizer should support:

- ASCII command input.
- Space/tab delimiters.
- Double-quoted strings.
- Backslash escaping inside quoted strings, at minimum `\"` and `\\`.
- Empty quoted strings only if an argument schema allows length 0.
- Clear error reporting for unterminated quotes, too many tokens, token too long, line too long, unknown command, ambiguous command, missing argument, extra argument, invalid type, out-of-range value, and invalid enum.

The parser should not modify command metadata. It may tokenize by modifying the line buffer or by storing bounded token views.

## 13. Dispatch policy

A handler receives:

- application context pointer
- matched command descriptor pointer
- parsed argument view
- optional output callback wrapper

A handler returns an explicit status code, not just boolean. Example:

```c
typedef enum {
  BSC_OK = 0,
  BSC_ERR_UNKNOWN,
  BSC_ERR_AMBIGUOUS,
  BSC_ERR_USAGE,
  BSC_ERR_ARG_TYPE,
  BSC_ERR_ARG_RANGE,
  BSC_ERR_ARG_ENUM,
  BSC_ERR_ARG_LENGTH,
  BSC_ERR_LOCKED,
  BSC_ERR_INTERNAL,
  BSC_ERR_APP
} bsc_status_t;
```

The current console does not print final `OK:`/`ERR:` lines or automatic parser/validation errors. Applications and future presentation helpers own that policy.

## 14. Access/security levels

The library should support optional access metadata:

- normal: visible and executable by default
- advanced: visible by default or shown with `help advanced`
- factory: hidden unless enabled/unlocked
- hidden: not shown in help index but can exist for testing or automation
- locked: requires application-provided unlock check

The core should not implement passwords or security policy directly. It should call an application access-check callback with command metadata and user/session state.

## 15. Platform adapters

The core should be portable C.

Adapters can provide:

- Arduino `Stream` input/output.
- ESP-IDF UART/console input/output.
- Host stdio test harness.
- Optional line editing/history wrappers later.

Platform-specific storage optimizations, such as AVR `PROGMEM`, should be adapter/macros, not core assumptions.

## 16. Borrowed design notes

From PT100 Mesh:

- Keep command metadata and help metadata together.
- Support `help`, `help <command>`, and `help <command> <topic/path>`.
- Use manpage-style sections.
- Keep command registration separate from help rendering.
- Use function pointers and optional custom help bodies where generic metadata is not enough.

From AdvancedCLI:

- Typed arguments and validation should be first-class.
- Aliases and subcommands are useful.
- Fixed-size capacity configuration is useful.
- Tests and examples should be part of the library from the beginning.

From StaticSerialCommands:

- Static registration and program-memory-friendly metadata are useful.
- Argument ranges and types should be declared with the command.
- Subcommands should be represented structurally, not only parsed inside a handler.

From SimpleSerialShell:

- Host-callable `execute("command line")` is valuable for tests.
- `argc`/`argv` style is a useful mental model, even if typed parsed arguments are the public handler interface.

From SerialUI:

- Menu/group navigation ideas are useful for help browsing.
- Do not adopt a hidden stateful UI model as the primary command interface.

## 17. Open design questions

1. Should the first implementation be pure C99, C11, or C++ with a C-compatible core?
2. Should command metadata be defined entirely in C structs, or should convenience macros generate descriptors?
3. Should command registration be compile-time static only for MVP, or should bounded runtime registration be included in MVP?
4. Should the library own echo/OK/ERR formatting, or should it expose hooks and let the application format final output?
5. Should quoted strings be required in MVP? Recommendation: yes, because Wi-Fi SSIDs/passwords need them.
6. Should optional arguments exist in MVP? Recommendation: avoid optional positional arguments initially unless the use case is strong.
7. Should bool accept only `on/off`, or also `true/false`, `1/0`, `yes/no`? Recommendation: accept all common forms, document canonical output as `on/off`.
8. Should enum matching be case-sensitive? Recommendation: command paths and enum values should be case-insensitive by default, with an option for case-sensitive strings.
9. Should `help` output be generated entirely from metadata, or should commands be able to append custom sections? Recommendation: both.
10. Should every public command be required to have examples? Recommendation: yes.

## 18. Initial recommendation

Build the reusable library as its own repository, not inside AS7331. Continue with a host-tested C core before adapters. Use AS7331 as the first integration pilot only after the standalone parser/registry/help system works.

The current core implements static descriptor validation, nested path matching, typed positional arguments, selected-command dispatch, and output-neutral complete-line console orchestration with caller-owned workspace storage. Generated help/manpages remain the next core phase. Defer completion, history, authentication, persistent settings, interactive prompts, examples, and adapters until the core behavior is validated.

## Generated help staging

The implemented first help milestone is a pure platform-independent renderer: it validates help prose and emitted identifiers separately from ordinary registry validation, performs exact metadata-path lookup, and renders initial NAME, SYNOPSIS, DESCRIPTION, ARGUMENTS, VALID VALUES, and COMMANDS sections through `bsc_output_t`. Console-level `help` and `commands` built-ins, extended metadata sections, subtopics, examples, and adapters remain future staged work rather than abandoned product intent.
