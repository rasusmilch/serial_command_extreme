# Bounded Serial Console Library — Implementation Guide

## 1. Naming used in this guide

Working name: `BSC` — Bounded Serial Console.

This name is not final. It is short enough for C symbols and distinguishes the core from project-specific consoles.

Example prefixes:

```c
bsc_console_t
bsc_command_t
bsc_arg_def_t
bsc_parsed_args_t
bsc_execute_line()
```

## 2. Recommended repository layout

```text
bounded-serial-console/
  README.md
  LICENSE
  docs/
    design_intent.md
    roadmap.md
    implementation_guide.md
    api_reference.md
    test_plan.md
    integration_arduino.md
    integration_esp_idf.md
  src/
    bsc_config.h
    bsc_types.h
    bsc_tokenizer.h
    bsc_tokenizer.c
    bsc_registry.h
    bsc_registry.c
    bsc_parser.h
    bsc_parser.c
    bsc_help.h
    bsc_help.c
    bsc_console.h
    bsc_console.c
  adapters/
    arduino/
      BscArduinoConsole.h
      BscArduinoConsole.cpp
    esp_idf/
      bsc_esp_idf_adapter.h
      bsc_esp_idf_adapter.c
  examples/
    host_basic/
    host_sensor_settings/
    arduino_basic/
    arduino_sensor_console/
  test/
    test_tokenizer.c
    test_parser.c
    test_args.c
    test_help.c
    test_dispatch.c
    golden/
```

## 3. Configuration header

`bsc_config.h` should provide conservative defaults and allow project overrides.

```c
#ifndef BSC_CONFIG_H
#define BSC_CONFIG_H

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

#ifndef BSC_ENABLE_FLOAT
#define BSC_ENABLE_FLOAT 1
#endif

#ifndef BSC_ENABLE_QUOTED_STRINGS
#define BSC_ENABLE_QUOTED_STRINGS 1
#endif

#ifndef BSC_ENABLE_BUILTIN_HELP
#define BSC_ENABLE_BUILTIN_HELP 1
#endif

#endif
```

## 4. Core status codes

Use explicit status codes. Do not collapse parser failures into a generic false.

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
  BSC_STATUS_MISSING_ARGUMENT,
  BSC_STATUS_EXTRA_ARGUMENT,
  BSC_STATUS_INVALID_ARGUMENT_TYPE,
  BSC_STATUS_ARGUMENT_OUT_OF_RANGE,
  BSC_STATUS_ARGUMENT_TOO_LONG,
  BSC_STATUS_INVALID_ENUM_VALUE,
  BSC_STATUS_ACCESS_DENIED,
  BSC_STATUS_APP_ERROR,
  BSC_STATUS_INTERNAL_ERROR
} bsc_status_t;
```

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

The console orchestration layer is output-neutral. It does not automatically write command echo, help, parser errors, matcher errors, access errors, `OK`, `ERR`, or final-result text. It only forwards the configured output wrapper to handlers through selected-command dispatch. Future diagnostic rendering, redacted echo, and automatic final-result formatting require separate approval and tests.

## 6. Token representation

The tokenizer produces explicit-length `bsc_string_view_t` token views into caller-owned mutable line storage. In the high-level console path, those token views point into `bsc_console_workspace_t.line_buffer`, not into `bsc_console_t`.

The workspace owns the bounded token array used by complete-line execution:

```c
bsc_string_view_t tokens[BSC_MAX_TOKENS];
size_t token_count;
```

Token views are valid only during the active synchronous execution and are cleared before `bsc_execute_line()` returns. String comparison helpers support exact and ASCII case-insensitive matching without copying token text.

## 7. Argument type definitions

```c
typedef enum {
  BSC_ARG_NONE = 0,
  BSC_ARG_INT,
  BSC_ARG_UINT,
  BSC_ARG_FLOAT,
  BSC_ARG_BOOL,
  BSC_ARG_ENUM,
  BSC_ARG_STRING,
  BSC_ARG_SECRET
} bsc_arg_type_t;

typedef struct {
  const char* value;
  const char* help;
} bsc_enum_choice_t;

typedef struct {
  const char* name;
  bsc_arg_type_t type;
  bool required;
  int32_t min_i;
  int32_t max_i;
  uint32_t min_u;
  uint32_t max_u;
  float min_f;
  float max_f;
  uint16_t min_len;
  uint16_t max_len;
  const bsc_enum_choice_t* enum_choices;
  uint8_t enum_choice_count;
  const char* help;
} bsc_arg_def_t;
```

For MVP, avoid optional positional arguments unless there is a specific accepted use case. Optional positional arguments create ambiguity and complicate help/testing.

## 8. Parsed argument representation

```c
typedef enum {
  BSC_PARSED_NONE = 0,
  BSC_PARSED_INT,
  BSC_PARSED_UINT,
  BSC_PARSED_FLOAT,
  BSC_PARSED_BOOL,
  BSC_PARSED_ENUM,
  BSC_PARSED_STRING,
  BSC_PARSED_SECRET
} bsc_parsed_type_t;

typedef struct {
  bsc_parsed_type_t type;
  union {
    int32_t i;
    uint32_t u;
    float f;
    bool b;
    bsc_string_view_t s;
    uint8_t enum_index;
  } value;
} bsc_arg_value_t;

typedef struct {
  bsc_arg_value_t values[BSC_MAX_ARGS];
  uint8_t count;
} bsc_parsed_args_t;
```

String and secret arguments are borrowed views into the active token storage. In the complete-line console path they point into `bsc_console_workspace_t.line_buffer`. They are valid only during synchronous dispatch/handler execution; applications that need persistent values must copy them into bounded application-owned storage.

## 9. Command descriptor

```c
struct bsc_command_s;

typedef bsc_status_t (*bsc_handler_fn_t)(
    void* app_ctx,
    const struct bsc_command_s* command,
    const bsc_parsed_args_t* args,
    bsc_output_t* out);

typedef bsc_status_t (*bsc_access_fn_t)(
    void* app_ctx,
    const struct bsc_command_s* command);

typedef enum {
  BSC_NODE_GROUP = 0,
  BSC_NODE_COMMAND
} bsc_node_type_t;

typedef enum {
  BSC_ACCESS_NORMAL = 0,
  BSC_ACCESS_ADVANCED,
  BSC_ACCESS_FACTORY,
  BSC_ACCESS_HIDDEN,
  BSC_ACCESS_LOCKED
} bsc_access_level_t;

typedef struct bsc_command_s {
  const char* path[BSC_MAX_PATH_TOKENS];
  uint8_t path_len;
  bsc_node_type_t node_type;
  bsc_access_level_t access;

  const char* brief;
  const char* synopsis;
  const char* description;
  const char* notes;
  const char* examples;
  const char* related;

  const bsc_arg_def_t* args;
  uint8_t arg_count;

  bsc_handler_fn_t handler;
  bsc_access_fn_t access_check;
  void* user_data;
} bsc_command_t;
```

Path tokens should be literals. For example, `settings wifi set ssid` is a path, and the actual SSID is an argument.

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

Generated help/manpages and built-in commands remain future work. The current complete-line console executes only the configured application descriptor table and contains no hard-coded `help`, `commands`, `version`, or `about` behavior.

Future built-ins may be represented as descriptors supplied by the library/application or as a clearly documented pre-dispatch route after tokenization. Collision policy, visibility filtering, variable path-token handling, and access behavior must be approved with that future feature.

## 13. Example: sensor gain command

Command behavior:

```text
gain 2048
help gain
```

Descriptor sketch:

```c
static const bsc_enum_choice_t k_gain_choices[] = {
  { "1", "lowest sensitivity, highest saturation margin" },
  { "2", "low sensitivity" },
  { "4", "low sensitivity" },
  { "8", "moderate sensitivity" },
  { "16", "moderate sensitivity" },
  { "32", "moderate sensitivity" },
  { "64", "high sensitivity" },
  { "128", "high sensitivity" },
  { "256", "very high sensitivity" },
  { "512", "very high sensitivity" },
  { "1024", "near maximum sensitivity" },
  { "2048", "maximum sensitivity, high saturation risk" },
};

static const bsc_arg_def_t k_gain_args[] = {
  {
    .name = "value",
    .type = BSC_ARG_ENUM,
    .required = true,
    .enum_choices = k_gain_choices,
    .enum_choice_count = sizeof(k_gain_choices) / sizeof(k_gain_choices[0]),
    .help = "Sensor gain multiplier. Higher values make weak signals easier to see but saturate sooner."
  }
};

static bsc_status_t handle_gain(void* app_ctx,
                                const bsc_command_t* command,
                                const bsc_parsed_args_t* args,
                                bsc_output_t* out) {
  (void)command;
  app_config_t* cfg = (app_config_t*)app_ctx;
  const uint8_t choice = args->values[0].value.enum_index;
  cfg->gain = gain_value_from_choice(choice);
  bsc_out_printf(out, "OK: gain=%ux\n", cfg->gain);
  return BSC_STATUS_OK;
}

static const bsc_command_t k_commands[] = {
  {
    .path = { "gain" },
    .path_len = 1,
    .node_type = BSC_NODE_COMMAND,
    .access = BSC_ACCESS_NORMAL,
    .brief = "Set sensor gain",
    .synopsis = "gain <1|2|4|8|16|32|64|128|256|512|1024|2048>",
    .description = "Changes sensor sensitivity. Use higher gain for weak signals and lower gain when readings saturate or show WARN/overflow flags.",
    .notes = "Changing gain changes raw counts and scaled estimates. Do not compare readings across different gain settings unless the workflow explicitly accounts for it.",
    .examples = "  gain 2048\n  gain 2",
    .args = k_gain_args,
    .arg_count = 1,
    .handler = handle_gain,
  },
};
```

## 14. Example: Wi-Fi settings namespace

Commands:

```text
settings wifi status
settings wifi set ssid "Shop AP"
settings wifi set password "example password"
settings wifi clear
```

Design notes:

- `settings` and `settings wifi` are group nodes.
- `settings wifi set ssid` and `settings wifi set password` are executable leaf commands.
- Password uses `BSC_ARG_SECRET`; echo and status should redact it.
- SSID and password string lengths are explicit bounds.

Argument descriptors:

```c
static const bsc_arg_def_t k_wifi_ssid_args[] = {
  {
    .name = "ssid",
    .type = BSC_ARG_STRING,
    .required = true,
    .min_len = 1,
    .max_len = 32,
    .help = "Wi-Fi network name. Quote the value if it contains spaces."
  }
};

static const bsc_arg_def_t k_wifi_password_args[] = {
  {
    .name = "password",
    .type = BSC_ARG_SECRET,
    .required = true,
    .min_len = 8,
    .max_len = 64,
    .help = "Wi-Fi password. Echo/status output must redact this value."
  }
};
```

Command descriptors:

```c
static const bsc_command_t k_commands[] = {
  {
    .path = { "settings" },
    .path_len = 1,
    .node_type = BSC_NODE_GROUP,
    .brief = "Device settings namespace",
    .synopsis = "settings <subsystem> ...",
    .description = "Groups runtime and persistent configuration commands.",
  },
  {
    .path = { "settings", "wifi" },
    .path_len = 2,
    .node_type = BSC_NODE_GROUP,
    .brief = "Wi-Fi settings namespace",
    .synopsis = "settings wifi <status|set|clear> ...",
    .description = "Inspect or update Wi-Fi connection settings.",
  },
  {
    .path = { "settings", "wifi", "status" },
    .path_len = 3,
    .node_type = BSC_NODE_COMMAND,
    .brief = "Show Wi-Fi settings status",
    .synopsis = "settings wifi status",
    .description = "Shows configured SSID and whether a password is stored. The password value is never printed.",
    .examples = "  settings wifi status",
    .handler = handle_wifi_status,
  },
  {
    .path = { "settings", "wifi", "set", "ssid" },
    .path_len = 4,
    .node_type = BSC_NODE_COMMAND,
    .brief = "Set Wi-Fi SSID",
    .synopsis = "settings wifi set ssid <ssid>",
    .description = "Sets the Wi-Fi network name. Quote SSIDs that contain spaces.",
    .examples = "  settings wifi set ssid Shop_AP\n  settings wifi set ssid \"Shop AP\"",
    .args = k_wifi_ssid_args,
    .arg_count = 1,
    .handler = handle_wifi_set_ssid,
  },
  {
    .path = { "settings", "wifi", "set", "password" },
    .path_len = 4,
    .node_type = BSC_NODE_COMMAND,
    .brief = "Set Wi-Fi password",
    .synopsis = "settings wifi set password <password>",
    .description = "Sets the Wi-Fi password. This is a sensitive field and must be redacted from echo and status output.",
    .examples = "  settings wifi set password \"example password\"",
    .args = k_wifi_password_args,
    .arg_count = 1,
    .handler = handle_wifi_set_password,
  },
};
```

## 15. Example help output

`help settings wifi set password`:

```text
NAME
  settings wifi set password - Set Wi-Fi password

SYNOPSIS
  settings wifi set password <password>

DESCRIPTION
  Sets the Wi-Fi password. This is a sensitive field and must be redacted from echo and status output.

ARGUMENTS
  password  secret[8..64]
            Wi-Fi password. Echo/status output must redact this value.

EXAMPLES
  settings wifi set password "example password"

RELATED
  settings wifi status
  settings wifi set ssid
  settings wifi clear
```

`help settings wifi`:

```text
NAME
  settings wifi - Wi-Fi settings namespace

SYNOPSIS
  settings wifi <status|set|clear> ...

DESCRIPTION
  Inspect or update Wi-Fi connection settings.

COMMANDS
  status        Show Wi-Fi settings status
  set ssid      Set Wi-Fi SSID
  set password  Set Wi-Fi password
  clear         Clear Wi-Fi settings
```

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
- Bool: accept `on/off`, `true/false`, `yes/no`, `1/0`; canonical output should use `on/off`.
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
- Help index output once generated help is implemented.
- Help group output once generated help is implemented.
- Help leaf/manpage output once generated help is implemented.
- Access-denied path.
- Handler return status.

Golden tests should compare exact help output for representative commands.

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

- When generated help is implemented, does every public command have a useful manpage?
- Does every argument have a type, bounds, and help text?
- Are all capacities compile-time bounded?
- Are too-long inputs rejected safely?
- Are secret values avoided in core-generated diagnostics, and are future presentation layers responsible for redaction?
- Can the parser run in host tests?
- Is the Arduino adapter separate from the core?
- Are platform-specific dependencies isolated?
- Are examples representative of real projects?
- Is there a clear migration path for AS7331?
