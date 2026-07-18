# Bounded Serial Console Library — Implementation Guide

## 1. Naming used in this guide

The repository and project are Serial Command Extreme. `BSC` is the established public C API prefix used by the platform-independent core.

Current public symbols include:

```c
bsc_console_t
bsc_command_t
bsc_arg_def_t
bsc_parsed_args_t
bsc_execute_line()
bsc_execute_line_with_builtins()
bsc_help_catalog_t
```

## 2. Current repository layout

The current repository uses this host-first core layout:

```text
README.md
CHANGELOG.md
CMakeLists.txt
docs/
  00_serial_console_library_design_intent.md
  01_serial_console_library_roadmap.md
  02_serial_console_library_implementation_guide.md
  03_serial_console_library_handoff.md        # historical handoff
  architecture_plan.md
  code_documentation_policy.md
  prior_art_review.md                         # reference material
  test_strategy.md
src/
  README.md
  bsc_args.c / bsc_args.h
  bsc_config.h                                # authoritative configuration defaults
  bsc_console.c / bsc_console.h
  bsc_dispatch.c / bsc_dispatch.h
  bsc_help.c / bsc_help.h
  bsc_help_catalog.c                          # Task 11C-1 structural catalog validation
  bsc_help_extended.c                         # Task 11C-2A pure flat-topic lookup
  internal/bsc_help_internal.c / .h           # shared help visibility and validated exact-path lookup
  bsc_matcher.c / bsc_matcher.h
  bsc_output.c / bsc_output.h
  bsc_registry.c / bsc_registry.h
  bsc_status.h                                # authoritative append-only status enum
  bsc_tokenizer.c / bsc_tokenizer.h
  bsc_types.h
test/
  CMakeLists.txt
  README.md
  test_main.c
  test_bsc_args.c
  test_bsc_console.c
  test_bsc_console_builtins.c
  test_bsc_dispatch.c
  test_bsc_help.c
  test_bsc_help_catalog.c
  test_bsc_matcher.c
  test_bsc_registry.c
  test_bsc_tokenizer.c
  test_bsc_types.c
  golden/
tools/
  check_forbidden_patterns.py
```

Adapter directories and runnable example applications remain future work. Documentation examples may show API usage, but Phase 4 runnable applications are separate from the Task 11C core catalog work.

## 3. Configuration header

`src/bsc_config.h` is authoritative for configuration defaults. Applications may override supported macros before including the public headers. Current configuration macros are:

- `BSC_MAX_COMMANDS` — maximum descriptor count accepted by registry validation.
- `BSC_MAX_ENUM_CHOICES` — maximum choices per enum argument.
- `BSC_MAX_LINE_LEN` — maximum complete input line length and Task 11C presentation-example line length.
- `BSC_MAX_TOKENS` — maximum tokens produced by the tokenizer.
- `BSC_MAX_TOKEN_LEN` — maximum token length, including descriptor path tokens and flat topic identifiers.
- `BSC_MAX_PATH_TOKENS` — maximum tokens in one command descriptor path.
- `BSC_MAX_ARGS` — maximum positional arguments per executable command and parsed values per dispatch.
- `BSC_ENABLE_FLOAT` — enables or disables float argument parsing at compile time.
- `BSC_MAX_FLOAT_FRACTION_DIGITS` — generated-help compact-float precision limit.
- `BSC_MAX_HELP_TEXT_LEN` — maximum generated-help prose and extended-help prose length.
- `BSC_MAX_HELP_TOPICS` — maximum flat topic records in an extended-help catalog; zero disables nonempty topic metadata.
- `BSC_MAX_HELP_TEXT_ITEMS` — maximum notes or warnings per target or topic; zero disables nonempty lists.
- `BSC_MAX_HELP_EXAMPLES` — maximum presentation examples per target or topic; zero disables nonempty example lists.
- `BSC_MAX_HELP_RELATED` — maximum related descriptor references per target or topic; zero disables nonempty related lists.
- `BSC_OUTPUT_CHUNK_LEN` — maximum internal output chunk used by generated-help rendering helpers.

Do not copy a partial configuration sketch into application code; include `src/bsc_config.h` through the public headers and override only the documented macros that the application needs to tune.

## 4. Status source

`src/bsc_status.h` is authoritative for the append-only public `bsc_status_t` enum. It includes success, input/tokenizer failures, matcher failures, registry/descriptor failures, parser failures such as `BSC_STATUS_ARGUMENT_TOO_SHORT`, access denial, application errors, output truncation, recursion/initialization failures, and `BSC_STATUS_INTERNAL_ERROR` for invalid API usage or internal consistency failures.

Do not collapse parser, matcher, output, registry, or API failures into a generic false. New public statuses must be append-only and must not change existing numeric meanings.

## 5. Output abstraction

The core does not know about `Serial`, UART, stdout, files, RTOS objects, or platform locks. Output is represented by a small callback wrapper:

```c
typedef size_t (*bsc_write_fn_t)(void *user, const char *data, size_t length);

typedef struct bsc_output {
  bsc_write_fn_t write;
  void *user;
} bsc_output_t;
```

The current helpers write C strings and C strings followed by `\n` through this callback and report short writes as `BSC_STATUS_OUTPUT_TRUNCATED`. The complete-line console copies the wrapper by value during initialization when one is provided, but the callback and user pointer remain borrowed.

The console orchestration layer is output-neutral. `bsc_execute_line()` performs application execution and only forwards the configured output wrapper to handlers through selected-command dispatch. `bsc_execute_line_with_builtins()` intentionally emits only the explicitly requested pure generated-help renderer output for `help`, exact-path `help <path>`, and `commands`. Neither API automatically writes command echo, parser errors, matcher errors, access errors, prompts, `OK`, `ERR`, or final-result text. Future diagnostic rendering, redacted echo, and automatic final-result formatting require separate approval and tests.

## 6. Token representation

The tokenizer produces explicit-length `bsc_string_view_t` token views into caller-owned mutable line storage. In the high-level console path, those token views point into `bsc_console_workspace_t.line_buffer`, not into `bsc_console_t`.

The workspace owns the bounded token array used by complete-line execution:

```c
bsc_string_view_t tokens[BSC_MAX_TOKENS];
size_t token_count;
```

Token views are valid only during the active synchronous execution and are cleared before `bsc_execute_line()` returns. String comparison helpers support exact and ASCII case-insensitive matching without copying token text.

## 7. Argument type definitions

The current public argument schema is defined in `src/bsc_types.h`. Argument descriptors use these active field names:

```c
typedef struct bsc_enum_choice {
  const char *name;
  int32_t value;
  const char *help;
} bsc_enum_choice_t;

typedef struct bsc_arg_def {
  const char *name;
  bsc_arg_type_t type;
  int32_t min_int;
  int32_t max_int;
  uint32_t min_uint;
  uint32_t max_uint;
  float min_float;
  float max_float;
  size_t min_length;
  size_t max_length;
  const bsc_enum_choice_t *enum_choices;
  size_t enum_choice_count;
  const char *help;
} bsc_arg_def_t;
```

All current positional arguments are required by position. Optional positional arguments are not part of the implemented core. `BSC_ARG_NONE` exists as an enum value but registry validation rejects it as an active argument descriptor.

## 8. Parsed argument representation

The current parsed-argument representation is defined in `src/bsc_args.h`. Parsed values reuse `bsc_arg_type_t` as the runtime tag and store type-specific data in the `data` union:

```c
typedef struct bsc_arg_value {
  bsc_arg_type_t type;
  union {
    int32_t int_value;
    uint32_t uint_value;
    float float_value;
    bool bool_value;
    int32_t enum_value;
    bsc_string_view_t text_value;
  } data;
} bsc_arg_value_t;

typedef struct bsc_parsed_args {
  bsc_arg_value_t values[BSC_MAX_ARGS];
  size_t count;
} bsc_parsed_args_t;
```

String and secret arguments are borrowed views into the active token storage. In the complete-line console path they point into `bsc_console_workspace_t.line_buffer`. They are valid only during synchronous dispatch/handler execution; applications that need persistent values must copy them into bounded application-owned storage. Runtime secret values must not be emitted by generated help, diagnostics, examples, or result metadata.

## 9. Command descriptor

The current descriptor model is the public `bsc_command_t` in `src/bsc_types.h`. It uses borrowed path arrays rather than array-embedded path storage, and hidden is a metadata flag rather than an access level. Current command fields are `summary` and `description`; there are no `brief`, stored `synopsis`, `notes`, `examples`, `related`, warning, or subtopic fields in the active schema. Synopsis and valid-value text are generated by `src/bsc_help.c` from the same path and argument descriptors used by parsing.

Task 11C-1 adds an optional borrowed `bsc_help_catalog_t` in `src/bsc_help.h` for structural validation of extended help metadata. The catalog wraps one authoritative command table and stores notes, warnings, presentation examples, related descriptor pointers, and flat single-token topic records outside `bsc_command_t`. Catalog validation is structural only and calls registry validation rather than visibility-dependent help validation. Task 11C-2A adds `bsc_help_find_topic()` for pure flat-topic lookup through the catalog authority; extended command/group rendering and pure topic-page rendering remain future Task 11C-2B work.

Current access levels are `BSC_ACCESS_NORMAL`, `BSC_ACCESS_ADVANCED`, `BSC_ACCESS_FACTORY`, and `BSC_ACCESS_LOCKED`. `BSC_COMMAND_FLAG_HIDDEN` is separate visibility metadata.

Handlers use `bsc_command_handler_t` and receive application context, the selected descriptor, parsed arguments, and optional output. Execution access callbacks use `bsc_command_access_fn_t` and are invoked only by dispatch; the pure help module never invokes handlers or access callbacks.

Path tokens should be literals. For example, `settings wifi set ssid` is a descriptor path, and the actual SSID is a positional argument. All current positional arguments are required; optional positional arguments are not part of the implemented core.

## 10. Console configuration and execution workspace

The implemented console model separates lightweight configuration from execution storage. `bsc_console_t` is initialized from `bsc_console_config_t`, validates the static descriptor table once, then remains read-only during execution:

```c
typedef struct bsc_console_config {
  const bsc_command_t *commands;
  size_t command_count;
  void *app_context;
  const bsc_output_t *output;
} bsc_console_config_t;

typedef struct bsc_console {
  const bsc_command_t *commands;
  size_t command_count;
  void *app_context;
  bsc_output_t output;
  bool has_output;
  bool initialized;
} bsc_console_t;
```

`bsc_console_t` owns no line buffer, token array, parsed-argument array, matcher result, diagnostic storage, mutex, or execution-active state. It borrows descriptor metadata and application context for its lifetime. When configured, it stores a by-value copy of the output wrapper while the callback and user pointer remain borrowed.

Complete-line execution receives caller-owned workspace per call:

```c
typedef struct bsc_console_workspace {
  char line_buffer[BSC_MAX_LINE_LEN + 1u];
  bsc_string_view_t tokens[BSC_MAX_TOKENS];
  size_t token_count;
  bsc_match_result_t match_result;
  bsc_parsed_args_t parsed_args;
  bsc_arg_parse_error_t parse_error;
  bool execution_active;
} bsc_console_workspace_t;
```

The workspace may be static, global, embedded in an adapter/application object, or otherwise explicitly placed by the caller. Its non-atomic active guard rejects ordinary same-workspace recursion, but it is not a mutex and does not provide cross-task synchronization. Runtime registration can be added later using a separately approved bounded model.

## 11. Complete-line execution algorithm

`bsc_execute_line()` accepts an explicit byte span and length. A normal C string is submitted with a length that excludes its terminating NUL. The current high-level sequence is:

1. Clear optional non-secret result metadata.
2. Validate the console, workspace, initialized state, input pointer, and input length.
3. Reject overlength input.
4. Reject active reuse of the same workspace before modifying workspace contents.
5. Reject embedded NUL bytes before tokenization.
6. Mark the workspace active and clear transient metadata.
7. Return `BSC_STATUS_NO_INPUT` for zero-length input.
8. Copy exactly the submitted bytes into `workspace->line_buffer` with `memmove()` and place a trailing NUL outside the tokenizer span.
9. Call `bsc_tokenize_line()` on the workspace line buffer.
10. Call `bsc_match_command()` over the initialized descriptor table.
11. Derive the remaining-token argument slice from `bsc_match_result_t`.
12. Call `bsc_dispatch_command()` with the already selected executable command.
13. Copy parser diagnostics into `bsc_console_result_t` only when dispatch populated them.
14. Clear line, token, match, parsed-argument, and parser-diagnostic workspace state before returning.

The tokenizer, matcher, typed parser, access enforcement, dispatch, and handler invocation remain separate stages. Dispatch begins with an already selected executable command and does not perform matching. The console does not automatically print final results.

## 12. Built-in commands

The pure generated-help APIs are implemented in `src/bsc_help.h` and `src/bsc_help.c`: callers can validate help metadata, resolve exact descriptor paths, and render top-level indexes, command lists, group pages, and executable-command pages through `bsc_output_t`. The original `bsc_execute_line()` API remains application-only and executes the configured descriptor table with no hard-coded built-in route. Applications that want complete-line help can call `bsc_execute_line_with_builtins()`, which recognizes `help`, exact-path `help <path>`, and `commands` after tokenization and before application matching.

The built-in-aware API preserves output-neutral orchestration: it emits only the selected pure renderer output, uses only the output wrapper copied into `bsc_console_t`, and adds no prompts, fallback diagnostics, echo, or final OK/ERR text. Built-in names are reserved only for the invoked built-in in this API; application descriptors beginning with that built-in name return `BSC_STATUS_INVALID_DESCRIPTOR` with borrowed collision metadata and do not run the matcher, parser, access callbacks, dispatcher, or handlers. The static help visibility behavior remains separate from execution access: normal and advanced descriptors are visible by default, while factory, locked, and hidden descriptors require explicit `bsc_help_options_t`, copied by value per call.

## 13. Example: sensor gain command

Command behavior through the current dispatcher remains application-defined:

```text
gain 2048
```

The same descriptor metadata can be rendered directly by calling `bsc_help_render_path()` with the path token `gain`, or through `bsc_execute_line_with_builtins()` with a complete line such as `help gain`. The original `bsc_execute_line()` path remains application-only for callers that need ordinary commands named `help` or `commands`.

Representative current-schema descriptor:

```c
static const char *const k_gain_path[] = {"gain"};

static const bsc_enum_choice_t k_gain_choices[] = {
  {.name = "1", .value = 1, .help = "Lowest sensitivity, highest saturation margin."},
  {.name = "2", .value = 2, .help = "Low sensitivity."},
  {.name = "4", .value = 4, .help = "Low sensitivity."},
  {.name = "8", .value = 8, .help = "Moderate sensitivity."},
  {.name = "16", .value = 16, .help = "Moderate sensitivity."},
  {.name = "32", .value = 32, .help = "Moderate sensitivity."},
  {.name = "64", .value = 64, .help = "High sensitivity."},
  {.name = "128", .value = 128, .help = "High sensitivity."},
  {.name = "256", .value = 256, .help = "Very high sensitivity."},
  {.name = "512", .value = 512, .help = "Very high sensitivity."},
  {.name = "1024", .value = 1024, .help = "Near maximum sensitivity."},
  {.name = "2048", .value = 2048, .help = "Maximum sensitivity, high saturation risk."},
};

static const bsc_arg_def_t k_gain_args[] = {
  {
    .name = "value",
    .type = BSC_ARG_ENUM,
    .min_int = 0,
    .max_int = 0,
    .min_uint = 0u,
    .max_uint = 0u,
    .min_float = 0.0f,
    .max_float = 0.0f,
    .min_length = 0u,
    .max_length = 0u,
    .enum_choices = k_gain_choices,
    .enum_choice_count = sizeof(k_gain_choices) / sizeof(k_gain_choices[0]),
    .help = "Sensor gain multiplier. Higher values make weak signals easier to see but saturate sooner."
  }
};

static bsc_status_t handle_gain(void *app_context,
                                const bsc_command_t *command,
                                const bsc_parsed_args_t *args,
                                bsc_output_t *output) {
  app_config_t *cfg = (app_config_t *)app_context;
  (void)command;
  (void)output;
  cfg->gain = gain_value_from_choice(args->values[0].data.enum_value);
  return BSC_STATUS_OK;
}

static const bsc_command_t k_commands[] = {
  {
    .path = k_gain_path,
    .path_len = sizeof(k_gain_path) / sizeof(k_gain_path[0]),
    .node_type = BSC_NODE_COMMAND,
    .args = k_gain_args,
    .arg_count = sizeof(k_gain_args) / sizeof(k_gain_args[0]),
    .handler = handle_gain,
    .command_context = NULL,
    .access = BSC_ACCESS_NORMAL,
    .flags = BSC_COMMAND_FLAG_NONE,
    .access_fn = NULL,
    .summary = "Set sensor gain",
    .description = "Changes sensor sensitivity. Use higher gain for weak signals and lower gain when readings saturate."
  },
};
```

All current positional arguments are required by position. The current schema has no stored synopsis, notes, examples, or related-command fields; generated help derives SYNOPSIS and VALID VALUES from the path and argument metadata above.

## 14. Example: Wi-Fi settings namespace

Commands:

```text
settings wifi status
settings wifi set ssid "Shop AP"
settings wifi set password "example password"
settings wifi clear
```

Design notes:

- `settings`, `settings wifi`, and any other visible proper path prefixes are explicit group nodes when generated help is enabled.
- `settings wifi set ssid` and `settings wifi set password` are executable leaf commands.
- Password uses `BSC_ARG_SECRET`; generated help shows a placeholder and length bounds, never a runtime value.
- SSID and password string lengths are explicit `min_length` and `max_length` byte bounds.
- Generated help uses `summary`, `description`, argument `help`, and enum-choice `help`; it does not use stored synopsis, notes, examples, or related fields.

Representative current-style descriptors use borrowed path arrays and current field names from `src/bsc_types.h`. The active schema has `summary` and `description`; generated help creates synopsis and valid-value output from descriptor metadata rather than stored synopsis text.

## 15. Example help output

The pure generated-help renderer emits only the sections implemented by the current `src/bsc_help.*` API. It does not emit NOTES, WARNINGS, EXAMPLES, RELATED, or subtopics yet.

`bsc_help_render_path()` for `settings wifi set password` may produce:

```text
NAME
  settings wifi set password - Set Wi-Fi password

SYNOPSIS
  settings wifi set password <password>

DESCRIPTION
  Sets the Wi-Fi password. This is a sensitive field and must be redacted by future echo, history, or log layers.

ARGUMENTS
  password - Wi-Fi password.

VALID VALUES
  password: secret, 8..64 bytes
```

`bsc_help_render_path()` for the group `settings wifi` may produce:

```text
NAME
  settings wifi - Wi-Fi settings namespace

DESCRIPTION
  Inspect or update Wi-Fi connection settings.

COMMANDS
  settings wifi status - Show Wi-Fi status
  settings wifi set - Set Wi-Fi fields
  settings wifi clear - Clear Wi-Fi settings
```

Group DESCRIPTION is omitted when the group descriptor has no description, and COMMANDS is omitted when no immediate children are visible under the selected static help options.

## 16. Tokenizer requirements

The tokenizer must pass these cases:

```text
status
settings wifi status
settings wifi set ssid Shop_AP
settings wifi set ssid "Shop AP"
settings wifi set password "pass with spaces"
settings wifi set password "quote: \""
settings wifi set password "backslash: \\"
```

It must reject:

```text
"unterminated
<line longer than BSC_MAX_LINE_LEN>
<token longer than BSC_MAX_TOKEN_LEN>
<more than BSC_MAX_TOKENS tokens>
```

## 17. Validation requirements

Typed parsing rules:

- Int: optional leading `-`, decimal digits, full-token consumption required.
- UInt: decimal digits only, no sign, full-token consumption required.
- Float: decimal syntax, full-token consumption required; reject NaN/inf if standard library makes them possible.
- Bool: accept `on/off`, `true/false`, and `1/0`; generated valid-value output preserves the parser-supported order `on | off | true | false | 1 | 0`.
- Enum: match allowed values case-insensitively by default.
- String: bounded length in bytes, preserve content after unescaping.
- Secret: same as string but marked sensitive for redaction.

Do not use `atoi()`/`atof()` without end-pointer validation.

## 18. Command matching examples

Given commands:

```text
settings
settings wifi
settings wifi set ssid <ssid>
settings wifi set password <password>
settings wifi status
settings display brightness <percent>
```

Input:

```text
settings wifi set ssid Shop_AP
```

Match path:

```text
settings wifi set ssid
```

Remaining args:

```text
Shop_AP
```

Input:

```text
settings wifi set
```

If input exactly matches a group descriptor, the current matcher reports `BSC_STATUS_GROUP_REQUIRES_SUBCOMMAND`. Future help integration may render child usage for that status.

Input:

```text
settings bluetooth status
```

Return unknown command and, if possible, list nearby children under `settings`.

## 19. Diagnostic and presentation recommendations

The current console orchestration layer is output-neutral. It returns a primary status and optional non-secret result metadata; it does not render these example messages itself:

```text
ERR: unknown command
ERR: missing argument: value
ERR: invalid argument type: hz expects float
ERR: argument out of range: hz must be 0.100..50.000
ERR: invalid value: output must be raw|scaled|both
ERR: string too long: ssid max 32
ERR: access denied
```

Future adapter or diagnostic-rendering work may choose stable wording and golden tests for operator messages. Command echo, redacted echo, automatic errors, and automatic success/failure result lines are presentation policy, not current console execution behavior.

## 20. Test plan

Host unit tests must cover:

- Empty line.
- Whitespace-only line.
- Basic tokenization.
- Quoted strings.
- Escapes.
- Unterminated quotes.
- Too-long line.
- Too-long token.
- Too many tokens.
- Unknown command.
- Group-only command.
- Leaf command.
- Nested command.
- Missing argument.
- Extra argument.
- Each typed argument parse success/failure.
- Range validation.
- Enum validation.
- String length validation.
- Secret redaction.
- Complete-line console initialization and execution.
- Workspace cleanup and same-workspace recursion rejection.
- Top-level generated-help index output.
- Complete generated command-list output.
- Generated group output, including optional DESCRIPTION and COMMANDS omission.
- Generated executable-command output, including NAME, SYNOPSIS, DESCRIPTION, ARGUMENTS, and VALID VALUES.
- Exact LF-only golden output for representative generated-help pages.
- Help visibility filtering, hidden/factory/locked omission, and optional-section omission behavior.
- Help metadata validation failures and renderer no-output behavior.
- Output short-write failures at help-rendering callback boundaries.
- Access-denied path.
- Handler return status.

Golden tests compare exact help output for representative commands, while focused C literals cover narrow one-off regressions that do not need fixture files.

## 21. Forbidden implementation patterns

Do not use:

- `malloc`, `calloc`, `realloc`, `free`, `new`, `delete` in the core.
- Arduino `String` in the core.
- STL containers in the core.
- Unbounded recursion.
- Unbounded `sprintf`.
- `strcpy`/`strcat` without destination-size enforcement.
- `atoi`/`atof` without validation.
- Hidden command state that changes the meaning of the next line.
- Commands whose help text contradicts their parser schema.

## 22. Review checklist

Before merging any implementation:

- Does every visible public command intended for generated help have a useful summary and executable description?
- Does every argument have a type and bounds, with optional help text where operator-facing context is needed?
- Are all capacities compile-time bounded?
- Are too-long inputs rejected safely?
- Are secret values avoided in core-generated diagnostics, and are future presentation layers responsible for redaction?
- Can the parser run in host tests?
- Is the Arduino adapter separate from the core?
- Are platform-specific dependencies isolated?
- Are examples representative of real projects?
- Is there a clear migration path for AS7331?
