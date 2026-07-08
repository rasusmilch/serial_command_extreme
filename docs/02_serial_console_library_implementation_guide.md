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

The core should not know about `Serial`, UART, or stdout.

```c
typedef void (*bsc_write_fn_t)(void* user, const char* text);

typedef struct {
  bsc_write_fn_t write;
  void* user;
} bsc_output_t;
```

Optionally add helper functions:

```c
void bsc_out_write(bsc_output_t* out, const char* text);
void bsc_out_writeln(bsc_output_t* out, const char* text);
void bsc_out_printf(bsc_output_t* out, const char* fmt, ...);
```

`bsc_out_printf()` must use a fixed local or caller-provided bounded buffer. Its maximum output chunk size must be configurable.

## 6. Token representation

The tokenizer should produce token views. Tokens may point into the console line buffer.

```c
typedef struct {
  const char* ptr;
  uint16_t len;
} bsc_string_view_t;

typedef struct {
  bsc_string_view_t tokens[BSC_MAX_TOKENS];
  uint8_t count;
} bsc_token_list_t;
```

String comparison helpers should support case-insensitive command matching without copying.

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

String and secret arguments should be views into the line buffer. If the application needs to persist a value, it must copy it into a bounded destination.

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

## 10. Console context

```c
typedef struct {
  const bsc_command_t* commands;
  uint8_t command_count;
  void* app_ctx;
  bsc_output_t out;
  bool echo_enabled;
  bool auto_result_enabled;
  char line_buffer[BSC_MAX_LINE_LEN + 1u];
} bsc_console_t;
```

For static registration MVP, `commands` points to a const array. Runtime registration can be added later using a caller-provided array of pointers or descriptors.

## 11. Parser algorithm

1. Receive a complete line.
2. Trim leading/trailing whitespace.
3. If empty, return `BSC_STATUS_NO_INPUT`.
4. Tokenize into bounded token list.
5. Find the best matching command:
   - Compare path tokens against input tokens.
   - Prefer the longest exact path match.
   - If multiple commands have the same path match, return ambiguous or choose the leaf with compatible argument count only if unambiguous.
6. Validate access level.
7. Parse remaining tokens according to argument schema.
8. Call handler.
9. Print final result if auto-result is enabled.

The matching algorithm should not require recursive descent. A simple linear scan over a bounded command array is acceptable for MVP. Faster lookup can be deferred.

## 12. Built-in commands

Built-ins should be normal command descriptors added by the library or application:

```text
help
commands
```

Optional later:

```text
version
about
```

`help` should be able to accept zero or more path tokens. This can be implemented as a special built-in because it needs variable-length path matching.

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

If no executable command exists at `settings wifi set`, return usage/help for children, not an unknown-command failure.

Input:

```text
settings bluetooth status
```

Return unknown command and, if possible, list nearby children under `settings`.

## 19. Error output recommendations

Parser/validation errors should be clear but concise:

```text
ERR: unknown command
ERR: missing argument: value
ERR: invalid argument type: hz expects float
ERR: argument out of range: hz must be 0.100..50.000
ERR: invalid value: output must be raw|scaled|both
ERR: string too long: ssid max 32
ERR: access denied
```

If command echo is enabled:

```text
> gain 3
ERR: invalid value: value must be 1|2|4|8|16|32|64|128|256|512|1024|2048
```

For secret values:

```text
> settings wifi set password ********
OK: settings wifi password set
```

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
- Help index output.
- Help group output.
- Help leaf/manpage output.
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

- Does every public command have a useful manpage?
- Does every argument have a type, bounds, and help text?
- Are all capacities compile-time bounded?
- Are too-long inputs rejected safely?
- Are secret values redacted?
- Can the parser run in host tests?
- Is the Arduino adapter separate from the core?
- Are platform-specific dependencies isolated?
- Are examples representative of real projects?
- Is there a clear migration path for AS7331?
