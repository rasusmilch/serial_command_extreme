#include "bsc_console.h"

#include "bsc_config.h"

#include <stdio.h>
#include <string.h>

#define BUILTIN_ASSERT_TRUE(condition)                                                            \
  do {                                                                                             \
    if (!(condition)) {                                                                            \
      printf("FAIL: %s: %s:%d: %s\n", test_name, __FILE__, __LINE__, #condition);                 \
      return 1;                                                                                    \
    }                                                                                              \
  } while (0)
#define BUILTIN_ASSERT_STATUS(expected, actual) BUILTIN_ASSERT_TRUE((expected) == (actual))
#define BUILTIN_RUN_TEST(fn)                                                                       \
  do {                                                                                             \
    int result;                                                                                    \
    const char *test_name = #fn;                                                                   \
    result = fn(test_name);                                                                        \
    if (result != 0) {                                                                             \
      failures += 1;                                                                               \
    } else {                                                                                       \
      printf("PASS: %s\n", test_name);                                                            \
    }                                                                                              \
  } while (0)

typedef struct builtin_capture {
  char buffer[8192];
  size_t used;
  size_t limit;
  size_t fail_after;
} builtin_capture_t;

typedef struct builtin_fixture {
  int handler_calls;
  int access_calls;
  int nested;
  bsc_status_t handler_status;
  bsc_status_t nested_status;
  bsc_console_t *nested_console;
  bsc_console_workspace_t *nested_workspace;
  const char *secret_seen;
  size_t secret_len;
  int32_t int_value;
} builtin_fixture_t;


typedef struct builtin_recursive_output {
  builtin_capture_t capture;
  bsc_console_t *console;
  bsc_console_workspace_t *workspace;
  bsc_help_options_t *options_to_mutate;
  bsc_console_builtins_result_t nested_result;
  bsc_status_t nested_status;
  int recurse_once;
  int mutate_once;
} builtin_recursive_output_t;

static const char *const path_status[] = {"status"};
static const char *const path_settings[] = {"settings"};
static const char *const path_settings_wifi[] = {"settings", "wifi"};
static const char *const path_settings_wifi_set[] = {"settings", "wifi", "set"};
static const char *const path_settings_wifi_set_password[] = {"settings", "wifi", "set", "password"};
static const char *const path_factory[] = {"factory"};
static const char *const path_locked[] = {"locked"};
static const char *const path_hidden[] = {"hidden"};
static const char *const path_help[] = {"help"};
static const char *const path_HELP[] = {"HELP"};
static const char *const path_help_nested[] = {"help", "nested"};
static const char *const path_commands[] = {"commands"};
static const char *const path_Commands[] = {"Commands"};
static const char *const path_commands_nested[] = {"commands", "nested"};
static const char *const path_echo[] = {"echo"};
static const char *const path_set_int[] = {"set", "int"};
static const char *const path_set_group[] = {"set"};
static const char *const path_output[] = {"output"};
static const char *const path_secret_group[] = {"secret"};
static const char *const path_secret[] = {"secret", "set"};
static const char *const path_bad_parent_child[] = {"bad", "child"};
static const char *const path_bad_summary[] = {"badsummary"};
static const char *const path_bad_description[] = {"baddesc"};
static const char *const path_bad_control[] = {"badcontrol"};

static const bsc_arg_def_t secret_args[] = {
    {"password", BSC_ARG_SECRET, 0, 0, 0u, 0u, 0.0f, 0.0f, 3u, 16u, NULL, 0u, "Secret password"},
};

static const bsc_arg_def_t int_args[] = {
    {"value", BSC_ARG_INT, -5, 5, 0u, 0u, 0.0f, 0.0f, 0u, 0u, NULL, 0u, "Value"},
};

/** @brief Capture output bytes into bounded fixture storage and optionally force a short write. */
static size_t builtin_capture_write(void *user, const char *data, size_t length) {
  builtin_capture_t *capture = (builtin_capture_t *)user;
  size_t accepted;
  size_t available;
  if (capture == NULL || data == NULL) {
    return 0u;
  }
  available = capture->limit - capture->used;
  accepted = length < available ? length : available;
  if (capture->fail_after != 0u && capture->used + accepted > capture->fail_after) {
    accepted = capture->fail_after > capture->used ? capture->fail_after - capture->used : 0u;
  }
  if (accepted > 0u) {
    memcpy(&capture->buffer[capture->used], data, accepted);
    capture->used += accepted;
  }
  return accepted;
}

/** @brief Initialize capture storage without heap allocation. */
static void builtin_capture_init(builtin_capture_t *capture, size_t limit) {
  memset(capture->buffer, 0, sizeof(capture->buffer));
  capture->used = 0u;
  capture->limit = limit;
  capture->fail_after = 0u;
}

/** @brief Return true when captured bytes contain a literal needle. */
static int builtin_bytes_find(const char *haystack, size_t haystack_len, const char *needle) {
  size_t needle_len = strlen(needle);
  size_t index;
  if (needle_len == 0u) {
    return 1;
  }
  if (haystack_len < needle_len) {
    return 0;
  }
  for (index = 0u; index <= haystack_len - needle_len; ++index) {
    if (memcmp(&haystack[index], needle, needle_len) == 0) {
      return 1;
    }
  }
  return 0;
}


/** @brief Output callback that can recurse once and mutate caller help options during active rendering. */
static size_t builtin_recursive_write(void *user, const char *data, size_t length) {
  builtin_recursive_output_t *fixture = (builtin_recursive_output_t *)user;
  if (fixture == NULL) {
    return 0u;
  }
  if (fixture->recurse_once == 0 && fixture->console != NULL && fixture->workspace != NULL) {
    fixture->recurse_once = 1;
    fixture->nested_status = bsc_execute_line_with_builtins(fixture->console,
                                                            fixture->workspace,
                                                            NULL,
                                                            "commands",
                                                            strlen("commands"),
                                                            &fixture->nested_result);
  }
  if (fixture->mutate_once == 0 && fixture->options_to_mutate != NULL) {
    fixture->mutate_once = 1;
    fixture->options_to_mutate->include_factory = false;
    fixture->options_to_mutate->include_locked = false;
    fixture->options_to_mutate->include_hidden = false;
  }
  return builtin_capture_write(&fixture->capture, data, length);
}

/** @brief Access callback sentinel that records unwanted built-in access use. */
static bool builtin_access(void *app_context, const bsc_command_t *command, bsc_access_level_t required_access) {
  builtin_fixture_t *fixture = (builtin_fixture_t *)app_context;
  (void)command;
  (void)required_access;
  if (fixture != NULL) {
    fixture->access_calls += 1;
  }
  return true;
}

/** @brief Handler sentinel that records application dispatch and parsed secret views. */
static bsc_status_t builtin_handler(void *app_context,
                                    const bsc_command_t *command,
                                    const bsc_parsed_args_t *args,
                                    bsc_output_t *output) {
  builtin_fixture_t *fixture = (builtin_fixture_t *)app_context;
  (void)command;
  (void)output;
  if (fixture != NULL) {
    fixture->handler_calls += 1;
    if (args != NULL && args->count > 0u && args->values[0].type == BSC_ARG_SECRET) {
      fixture->secret_seen = args->values[0].data.text_value.data;
      fixture->secret_len = args->values[0].data.text_value.length;
    }
    if (args != NULL && args->count > 0u && args->values[0].type == BSC_ARG_INT) {
      fixture->int_value = args->values[0].data.int_value;
    }
    return fixture->handler_status;
  }
  return BSC_STATUS_OK;
}


/** @brief Handler that writes deterministic bytes so ordinary-route output can be compared. */
static bsc_status_t builtin_output_handler(void *app_context,
                                           const bsc_command_t *command,
                                           const bsc_parsed_args_t *args,
                                           bsc_output_t *output) {
  builtin_fixture_t *fixture = (builtin_fixture_t *)app_context;
  (void)command;
  (void)args;
  if (fixture != NULL) {
    fixture->handler_calls += 1;
  }
  return bsc_out_write(output, "abcdef");
}

/** @brief Handler that recursively invokes the built-in-aware API with fixture-selected workspace. */
static bsc_status_t builtin_nested_handler(void *app_context,
                                           const bsc_command_t *command,
                                           const bsc_parsed_args_t *args,
                                           bsc_output_t *output) {
  builtin_fixture_t *fixture = (builtin_fixture_t *)app_context;
  (void)command;
  (void)args;
  (void)output;
  if (fixture != NULL) {
    fixture->handler_calls += 1;
    fixture->nested_status = bsc_execute_line_with_builtins(fixture->nested_console,
                                                            fixture->nested_workspace,
                                                            NULL,
                                                            "help",
                                                            strlen("help"),
                                                            NULL);
    return fixture->handler_status;
  }
  return BSC_STATUS_OK;
}

static const bsc_command_t base_commands[] = {
    {path_status, 1u, BSC_NODE_COMMAND, NULL, 0u, builtin_handler, NULL, BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_NONE, builtin_access, "Status", "Show status."},
    {path_settings, 1u, BSC_NODE_GROUP, NULL, 0u, NULL, NULL, BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_NONE, builtin_access, "Settings", "Settings group."},
    {path_settings_wifi, 2u, BSC_NODE_GROUP, NULL, 0u, NULL, NULL, BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_NONE, builtin_access, "WiFi", "WiFi group."},
    {path_settings_wifi_set, 3u, BSC_NODE_GROUP, NULL, 0u, NULL, NULL, BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_NONE, builtin_access, "Set", "Set group."},
    {path_settings_wifi_set_password, 4u, BSC_NODE_COMMAND, secret_args, 1u, builtin_handler, NULL, BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_NONE, builtin_access, "Password", "Set password."},
    {path_factory, 1u, BSC_NODE_COMMAND, NULL, 0u, builtin_handler, NULL, BSC_ACCESS_FACTORY, BSC_COMMAND_FLAG_NONE, builtin_access, "Factory", "Factory command."},
    {path_locked, 1u, BSC_NODE_COMMAND, NULL, 0u, builtin_handler, NULL, BSC_ACCESS_LOCKED, BSC_COMMAND_FLAG_NONE, builtin_access, "Locked", "Locked command."},
    {path_hidden, 1u, BSC_NODE_COMMAND, NULL, 0u, builtin_handler, NULL, BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_HIDDEN, builtin_access, "Hidden", "Hidden command."},
    {path_echo, 1u, BSC_NODE_COMMAND, NULL, 0u, builtin_handler, NULL, BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_NONE, builtin_access, "Echo", "Echo command."},
    {path_set_group, 1u, BSC_NODE_GROUP, NULL, 0u, NULL, NULL, BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_NONE, builtin_access, "Set", "Set group."},
    {path_set_int, 2u, BSC_NODE_COMMAND, int_args, 1u, builtin_handler, NULL, BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_NONE, builtin_access, "Set int", "Set integer."},
    {path_output, 1u, BSC_NODE_COMMAND, NULL, 0u, builtin_output_handler, NULL, BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_NONE, builtin_access, "Output", "Output command."},
    {path_secret_group, 1u, BSC_NODE_GROUP, NULL, 0u, NULL, NULL, BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_NONE, builtin_access, "Secret", "Secret group."},
    {path_secret, 2u, BSC_NODE_COMMAND, secret_args, 1u, builtin_handler, NULL, BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_NONE, builtin_access, "Secret", "Set secret."},
};

/** @brief Initialize a console over supplied descriptors and optional output. */
static bsc_status_t builtin_init_console(bsc_console_t *console,
                                         builtin_fixture_t *fixture,
                                         const bsc_command_t *commands,
                                         size_t command_count,
                                         const bsc_output_t *output) {
  bsc_console_config_t config;
  config.commands = commands;
  config.command_count = command_count;
  config.app_context = fixture;
  config.output = output;
  return bsc_console_init(console, &config, NULL);
}

/** @brief Return true when workspace transient storage is inactive and cleared. */
static int builtin_workspace_clean(const bsc_console_workspace_t *workspace) {
  size_t index;
  if (workspace->execution_active || workspace->token_count != 0u || workspace->match_result.command != NULL ||
      workspace->match_result.group != NULL || workspace->parsed_args.count != 0u ||
      workspace->parse_error.reason != BSC_ARG_PARSE_ERROR_NONE) {
    return 0;
  }
  for (index = 0u; index < sizeof(workspace->line_buffer); ++index) {
    if (workspace->line_buffer[index] != '\0') {
      return 0;
    }
  }
  return 1;
}

/** @brief Verify built-in result clearing resets every public field. */
static int test_builtins_result_clear(const char *test_name) {
  bsc_console_builtins_result_t result;
  bsc_console_builtins_result_clear(NULL);
  result.phase = BSC_CONSOLE_PHASE_BUILTIN;
  result.builtin = BSC_CONSOLE_BUILTIN_COMMANDS;
  result.application_result.phase = BSC_CONSOLE_PHASE_DISPATCH;
  result.application_result.command = &base_commands[0];
  result.application_result.command_index = 3u;
  result.collision = &base_commands[1];
  result.collision_index = 1u;
  bsc_console_builtins_result_clear(&result);
  BUILTIN_ASSERT_TRUE(result.phase == BSC_CONSOLE_PHASE_NONE);
  BUILTIN_ASSERT_TRUE(result.builtin == BSC_CONSOLE_BUILTIN_NONE);
  BUILTIN_ASSERT_TRUE(result.application_result.phase == BSC_CONSOLE_PHASE_NONE);
  BUILTIN_ASSERT_TRUE(result.application_result.command == NULL);
  BUILTIN_ASSERT_TRUE(result.collision == NULL);
  BUILTIN_ASSERT_TRUE(result.collision_index == 0u);
  return 0;
}

/** @brief Verify the existing application-only API still dispatches colliding command names. */
static int test_existing_api_dispatches_help_and_commands(const char *test_name) {
  bsc_console_t console;
  bsc_console_workspace_t workspace;
  bsc_console_result_t result;
  builtin_fixture_t fixture;
  const bsc_command_t commands[] = {
      {path_help, 1u, BSC_NODE_COMMAND, NULL, 0u, builtin_handler, NULL, BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_NONE, NULL, "Help", "App help."},
      {path_commands, 1u, BSC_NODE_COMMAND, NULL, 0u, builtin_handler, NULL, BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_NONE, NULL, "Commands", "App commands."},
  };
  memset(&fixture, 0, sizeof(fixture));
  fixture.handler_status = BSC_STATUS_OK;
  BUILTIN_ASSERT_STATUS(BSC_STATUS_OK, builtin_init_console(&console, &fixture, commands, 2u, NULL));
  bsc_console_workspace_init(&workspace);
  BUILTIN_ASSERT_STATUS(BSC_STATUS_OK, bsc_execute_line(&console, &workspace, "help", 4u, &result));
  BUILTIN_ASSERT_TRUE(result.phase == BSC_CONSOLE_PHASE_DISPATCH);
  BUILTIN_ASSERT_TRUE(result.command == &commands[0]);
  BUILTIN_ASSERT_STATUS(BSC_STATUS_OK, bsc_execute_line(&console, &workspace, "commands", 8u, &result));
  BUILTIN_ASSERT_TRUE(result.command == &commands[1]);
  BUILTIN_ASSERT_TRUE(fixture.handler_calls == 2);
  BUILTIN_ASSERT_TRUE(builtin_workspace_clean(&workspace));
  return 0;
}

/** @brief Verify ordinary inputs through the new API match the existing application route. */
static int test_ordinary_route_equivalence(const char *test_name) {
  bsc_console_t console_a;
  bsc_console_t console_b;
  bsc_console_workspace_t workspace_a;
  bsc_console_workspace_t workspace_b;
  bsc_console_result_t app_result;
  bsc_console_builtins_result_t builtin_result;
  builtin_capture_t capture_a;
  builtin_capture_t capture_b;
  bsc_output_t output_a = {builtin_capture_write, &capture_a};
  bsc_output_t output_b = {builtin_capture_write, &capture_b};
  builtin_fixture_t fixture_a;
  builtin_fixture_t fixture_b;
  memset(&fixture_a, 0, sizeof(fixture_a));
  memset(&fixture_b, 0, sizeof(fixture_b));
  fixture_a.handler_status = BSC_STATUS_OK;
  fixture_b.handler_status = BSC_STATUS_OK;
  builtin_capture_init(&capture_a, sizeof(capture_a.buffer));
  builtin_capture_init(&capture_b, sizeof(capture_b.buffer));
  BUILTIN_ASSERT_STATUS(BSC_STATUS_OK, builtin_init_console(&console_a, &fixture_a, base_commands, sizeof(base_commands) / sizeof(base_commands[0]), &output_a));
  bsc_console_workspace_init(&workspace_a);
  bsc_console_workspace_init(&workspace_b);

  BUILTIN_ASSERT_STATUS(BSC_STATUS_OK, bsc_execute_line(&console_a, &workspace_a, "status", 6u, &app_result));
  BUILTIN_ASSERT_STATUS(BSC_STATUS_OK, builtin_init_console(&console_b, &fixture_b, base_commands, sizeof(base_commands) / sizeof(base_commands[0]), &output_b));
  BUILTIN_ASSERT_STATUS(BSC_STATUS_OK, bsc_execute_line_with_builtins(&console_b, &workspace_b, NULL, "status", 6u, &builtin_result));
  BUILTIN_ASSERT_TRUE(builtin_result.builtin == BSC_CONSOLE_BUILTIN_NONE);
  BUILTIN_ASSERT_TRUE(builtin_result.phase == app_result.phase);
  BUILTIN_ASSERT_TRUE(builtin_result.application_result.command == app_result.command);
  BUILTIN_ASSERT_TRUE(builtin_result.application_result.command_index == app_result.command_index);
  BUILTIN_ASSERT_TRUE(builtin_result.collision == NULL);
  BUILTIN_ASSERT_TRUE(fixture_a.handler_calls == 1 && fixture_b.handler_calls == 1);
  BUILTIN_ASSERT_TRUE(builtin_workspace_clean(&workspace_a));
  BUILTIN_ASSERT_TRUE(builtin_workspace_clean(&workspace_b));

  fixture_a.handler_calls = 0;
  fixture_b.handler_calls = 0;
  BUILTIN_ASSERT_STATUS(BSC_STATUS_GROUP_REQUIRES_SUBCOMMAND, bsc_execute_line(&console_a, &workspace_a, "set", 3u, &app_result));
  BUILTIN_ASSERT_STATUS(BSC_STATUS_GROUP_REQUIRES_SUBCOMMAND, bsc_execute_line_with_builtins(&console_b, &workspace_b, NULL, "set", 3u, &builtin_result));
  BUILTIN_ASSERT_TRUE(app_result.phase == BSC_CONSOLE_PHASE_MATCH);
  BUILTIN_ASSERT_TRUE(builtin_result.phase == BSC_CONSOLE_PHASE_MATCH);
  BUILTIN_ASSERT_TRUE(builtin_result.application_result.group == app_result.group);
  BUILTIN_ASSERT_TRUE(builtin_result.application_result.group_index == app_result.group_index);
  BUILTIN_ASSERT_TRUE(fixture_a.handler_calls == 0 && fixture_b.handler_calls == 0);

  BUILTIN_ASSERT_STATUS(BSC_STATUS_OK, bsc_execute_line(&console_a, &workspace_a, "set int -3", strlen("set int -3"), &app_result));
  BUILTIN_ASSERT_STATUS(BSC_STATUS_OK, bsc_execute_line_with_builtins(&console_b, &workspace_b, NULL, "set int -3", strlen("set int -3"), &builtin_result));
  BUILTIN_ASSERT_TRUE(builtin_result.phase == BSC_CONSOLE_PHASE_DISPATCH);
  BUILTIN_ASSERT_TRUE(builtin_result.application_result.command == app_result.command);
  BUILTIN_ASSERT_TRUE(builtin_result.application_result.command_index == app_result.command_index);
  BUILTIN_ASSERT_TRUE(fixture_a.int_value == fixture_b.int_value && fixture_b.int_value == -3);

  fixture_a.handler_calls = 0;
  fixture_b.handler_calls = 0;
  BUILTIN_ASSERT_STATUS(BSC_STATUS_INVALID_ARGUMENT_TYPE, bsc_execute_line(&console_a, &workspace_a, "set int bad", strlen("set int bad"), &app_result));
  BUILTIN_ASSERT_STATUS(BSC_STATUS_INVALID_ARGUMENT_TYPE, bsc_execute_line_with_builtins(&console_b, &workspace_b, NULL, "set int bad", strlen("set int bad"), &builtin_result));
  BUILTIN_ASSERT_TRUE(builtin_result.application_result.command == app_result.command);
  BUILTIN_ASSERT_TRUE(builtin_result.application_result.command_index == app_result.command_index);
  BUILTIN_ASSERT_TRUE(builtin_result.application_result.parse_error.reason == app_result.parse_error.reason);
  BUILTIN_ASSERT_TRUE(builtin_result.application_result.parse_error.arg_index == app_result.parse_error.arg_index);
  BUILTIN_ASSERT_TRUE(builtin_result.application_result.parse_error.token_offset == app_result.parse_error.token_offset);
  BUILTIN_ASSERT_TRUE(fixture_a.handler_calls == 0 && fixture_b.handler_calls == 0);

  builtin_capture_init(&capture_a, sizeof(capture_a.buffer));
  builtin_capture_init(&capture_b, sizeof(capture_b.buffer));
  fixture_a.handler_calls = 0;
  fixture_b.handler_calls = 0;
  BUILTIN_ASSERT_STATUS(BSC_STATUS_OK, bsc_execute_line(&console_a, &workspace_a, "output", 6u, &app_result));
  BUILTIN_ASSERT_STATUS(BSC_STATUS_OK, bsc_execute_line_with_builtins(&console_b, &workspace_b, NULL, "output", 6u, &builtin_result));
  BUILTIN_ASSERT_TRUE(capture_a.used == capture_b.used);
  BUILTIN_ASSERT_TRUE(memcmp(capture_a.buffer, capture_b.buffer, capture_a.used) == 0);
  BUILTIN_ASSERT_TRUE(fixture_a.handler_calls == 1 && fixture_b.handler_calls == 1);

  builtin_capture_init(&capture_a, 3u);
  builtin_capture_init(&capture_b, 3u);
  BUILTIN_ASSERT_STATUS(BSC_STATUS_OUTPUT_TRUNCATED, bsc_execute_line(&console_a, &workspace_a, "output", 6u, &app_result));
  BUILTIN_ASSERT_STATUS(BSC_STATUS_OUTPUT_TRUNCATED, bsc_execute_line_with_builtins(&console_b, &workspace_b, NULL, "output", 6u, &builtin_result));
  BUILTIN_ASSERT_TRUE(builtin_result.builtin == BSC_CONSOLE_BUILTIN_NONE);
  BUILTIN_ASSERT_TRUE(builtin_result.collision == NULL);
  BUILTIN_ASSERT_TRUE(builtin_result.phase == builtin_result.application_result.phase);
  BUILTIN_ASSERT_TRUE(builtin_result.application_result.command == app_result.command);
  BUILTIN_ASSERT_TRUE(capture_a.used == capture_b.used && capture_b.used == 3u);
  BUILTIN_ASSERT_TRUE(builtin_workspace_clean(&workspace_a));
  BUILTIN_ASSERT_TRUE(builtin_workspace_clean(&workspace_b));
  return 0;
}

/** @brief Compare built-in help index output against the pure renderer and verify no application callbacks run. */
static int test_help_index_routes_to_pure_renderer(const char *test_name) {
  bsc_console_t console;
  bsc_console_workspace_t workspace;
  bsc_console_builtins_result_t result;
  builtin_capture_t built_capture;
  builtin_capture_t pure_capture;
  bsc_output_t output;
  bsc_output_t pure_output;
  builtin_fixture_t fixture;
  memset(&fixture, 0, sizeof(fixture));
  fixture.handler_status = BSC_STATUS_OK;
  builtin_capture_init(&built_capture, sizeof(built_capture.buffer));
  builtin_capture_init(&pure_capture, sizeof(pure_capture.buffer));
  output.write = builtin_capture_write;
  output.user = &built_capture;
  pure_output.write = builtin_capture_write;
  pure_output.user = &pure_capture;
  BUILTIN_ASSERT_STATUS(BSC_STATUS_OK, builtin_init_console(&console, &fixture, base_commands, sizeof(base_commands) / sizeof(base_commands[0]), &output));
  bsc_console_workspace_init(&workspace);
  BUILTIN_ASSERT_STATUS(BSC_STATUS_OK, bsc_execute_line_with_builtins(&console, &workspace, NULL, "hElP", 4u, &result));
  BUILTIN_ASSERT_STATUS(BSC_STATUS_OK, bsc_help_render_index(base_commands, sizeof(base_commands) / sizeof(base_commands[0]), NULL, &pure_output));
  BUILTIN_ASSERT_TRUE(result.phase == BSC_CONSOLE_PHASE_BUILTIN);
  BUILTIN_ASSERT_TRUE(result.builtin == BSC_CONSOLE_BUILTIN_HELP_INDEX);
  BUILTIN_ASSERT_TRUE(result.application_result.phase == BSC_CONSOLE_PHASE_NONE);
  BUILTIN_ASSERT_TRUE(fixture.handler_calls == 0);
  BUILTIN_ASSERT_TRUE(fixture.access_calls == 0);
  BUILTIN_ASSERT_TRUE(built_capture.used == pure_capture.used);
  BUILTIN_ASSERT_TRUE(memcmp(built_capture.buffer, pure_capture.buffer, pure_capture.used) == 0);
  BUILTIN_ASSERT_TRUE(builtin_workspace_clean(&workspace));
  return 0;
}


/** @brief Assert one failed exact help path adds no output and leaves no application metadata. */
static int builtin_assert_failed_help_path(const char *test_name,
                                           const bsc_console_t *console,
                                           bsc_console_workspace_t *workspace,
                                           builtin_capture_t *capture,
                                           builtin_fixture_t *fixture,
                                           const char *line) {
  bsc_console_builtins_result_t result;
  size_t before = capture->used;
  int handler_calls = fixture->handler_calls;
  int access_calls = fixture->access_calls;
  BUILTIN_ASSERT_STATUS(BSC_STATUS_UNKNOWN_COMMAND,
                        bsc_execute_line_with_builtins(console, workspace, NULL, line, strlen(line), &result));
  BUILTIN_ASSERT_TRUE(capture->used == before);
  BUILTIN_ASSERT_TRUE(result.phase == BSC_CONSOLE_PHASE_BUILTIN);
  BUILTIN_ASSERT_TRUE(result.builtin == BSC_CONSOLE_BUILTIN_HELP_PATH);
  BUILTIN_ASSERT_TRUE(result.application_result.phase == BSC_CONSOLE_PHASE_NONE);
  BUILTIN_ASSERT_TRUE(result.application_result.command == NULL);
  BUILTIN_ASSERT_TRUE(result.application_result.group == NULL);
  BUILTIN_ASSERT_TRUE(result.collision == NULL);
  BUILTIN_ASSERT_TRUE(fixture->handler_calls == handler_calls);
  BUILTIN_ASSERT_TRUE(fixture->access_calls == access_calls);
  BUILTIN_ASSERT_TRUE(builtin_workspace_clean(workspace));
  return 0;
}

/** @brief Verify exact help paths, quoted tokens, filtered paths, and extra-token path semantics. */
static int test_exact_path_help_routes(const char *test_name) {
  bsc_console_t console;
  bsc_console_workspace_t workspace;
  bsc_console_builtins_result_t result;
  builtin_capture_t capture;
  bsc_output_t output;
  builtin_fixture_t fixture;
  size_t used_after_success;
  /* 128-byte local test line intentionally covers tokenizer overflow without heap storage. */
  char too_many_tokens[128];
  size_t too_many_used = 0u;
  size_t index;
  memset(&fixture, 0, sizeof(fixture));
  fixture.handler_status = BSC_STATUS_OK;
  output.write = builtin_capture_write;
  output.user = &capture;
  BUILTIN_ASSERT_STATUS(BSC_STATUS_OK, builtin_init_console(&console, &fixture, base_commands, sizeof(base_commands) / sizeof(base_commands[0]), &output));
  bsc_console_workspace_init(&workspace);
  builtin_capture_init(&capture, sizeof(capture.buffer));
  BUILTIN_ASSERT_STATUS(BSC_STATUS_OK, bsc_execute_line_with_builtins(&console, &workspace, NULL, "help status", strlen("help status"), &result));
  BUILTIN_ASSERT_TRUE(result.builtin == BSC_CONSOLE_BUILTIN_HELP_PATH);
  BUILTIN_ASSERT_TRUE(builtin_bytes_find(capture.buffer, capture.used, "NAME\n  status"));
  used_after_success = capture.used;
  BUILTIN_ASSERT_STATUS(BSC_STATUS_OK, bsc_execute_line_with_builtins(&console, &workspace, NULL, "help settings", strlen("help settings"), &result));
  BUILTIN_ASSERT_STATUS(BSC_STATUS_OK, bsc_execute_line_with_builtins(&console, &workspace, NULL, "help \"settings\" \"wifi\"", strlen("help \"settings\" \"wifi\""), &result));
  BUILTIN_ASSERT_STATUS(BSC_STATUS_OK, bsc_execute_line_with_builtins(&console, &workspace, NULL, "help settings wifi set password", strlen("help settings wifi set password"), &result));
  BUILTIN_ASSERT_TRUE(!builtin_bytes_find(capture.buffer, capture.used, "actual-secret"));

  BUILTIN_ASSERT_TRUE(builtin_assert_failed_help_path(test_name, &console, &workspace, &capture, &fixture,
                                                       "help unknown") == 0);
  BUILTIN_ASSERT_TRUE(builtin_assert_failed_help_path(test_name, &console, &workspace, &capture, &fixture,
                                                       "help factory") == 0);
  BUILTIN_ASSERT_TRUE(builtin_assert_failed_help_path(test_name, &console, &workspace, &capture, &fixture,
                                                       "help locked") == 0);
  BUILTIN_ASSERT_TRUE(builtin_assert_failed_help_path(test_name, &console, &workspace, &capture, &fixture,
                                                       "help hidden") == 0);
  BUILTIN_ASSERT_TRUE(builtin_assert_failed_help_path(test_name, &console, &workspace, &capture, &fixture,
                                                       "help \"\"") == 0);
  BUILTIN_ASSERT_TRUE(builtin_assert_failed_help_path(test_name, &console, &workspace, &capture, &fixture,
                                                       "help status extra") == 0);
  BUILTIN_ASSERT_TRUE(builtin_assert_failed_help_path(test_name, &console, &workspace, &capture, &fixture,
                                                       "help a b c d e") == 0);
  memcpy(too_many_tokens, "help", 4u);
  too_many_used = 4u;
  for (index = 0u; index < (size_t)BSC_MAX_TOKENS; ++index) {
    too_many_tokens[too_many_used++] = ' ';
    too_many_tokens[too_many_used++] = 'a';
  }
  BUILTIN_ASSERT_STATUS(BSC_STATUS_TOO_MANY_TOKENS,
                        bsc_execute_line_with_builtins(&console,
                                                       &workspace,
                                                       NULL,
                                                       too_many_tokens,
                                                       too_many_used,
                                                       &result));
  BUILTIN_ASSERT_TRUE(capture.used >= used_after_success);
  BUILTIN_ASSERT_TRUE(fixture.handler_calls == 0);
  BUILTIN_ASSERT_TRUE(fixture.access_calls == 0);
  BUILTIN_ASSERT_TRUE(builtin_workspace_clean(&workspace));
  return 0;
}

/** @brief Verify commands rendering and extra-token grammar. */
static int test_commands_routes(const char *test_name) {
  bsc_console_t console;
  bsc_console_workspace_t workspace;
  bsc_console_builtins_result_t result;
  builtin_capture_t built_capture;
  builtin_capture_t pure_capture;
  bsc_output_t output;
  bsc_output_t pure_output;
  builtin_fixture_t fixture;
  memset(&fixture, 0, sizeof(fixture));
  fixture.handler_status = BSC_STATUS_OK;
  builtin_capture_init(&built_capture, sizeof(built_capture.buffer));
  builtin_capture_init(&pure_capture, sizeof(pure_capture.buffer));
  output.write = builtin_capture_write;
  output.user = &built_capture;
  pure_output.write = builtin_capture_write;
  pure_output.user = &pure_capture;
  BUILTIN_ASSERT_STATUS(BSC_STATUS_OK, builtin_init_console(&console, &fixture, base_commands, sizeof(base_commands) / sizeof(base_commands[0]), &output));
  bsc_console_workspace_init(&workspace);
  BUILTIN_ASSERT_STATUS(BSC_STATUS_OK, bsc_execute_line_with_builtins(&console, &workspace, NULL, "CoMmAnDs", 8u, &result));
  BUILTIN_ASSERT_STATUS(BSC_STATUS_OK, bsc_help_render_commands(base_commands, sizeof(base_commands) / sizeof(base_commands[0]), NULL, &pure_output));
  BUILTIN_ASSERT_TRUE(result.builtin == BSC_CONSOLE_BUILTIN_COMMANDS);
  BUILTIN_ASSERT_TRUE(built_capture.used == pure_capture.used);
  BUILTIN_ASSERT_TRUE(memcmp(built_capture.buffer, pure_capture.buffer, pure_capture.used) == 0);
  builtin_capture_init(&built_capture, sizeof(built_capture.buffer));
  BUILTIN_ASSERT_STATUS(BSC_STATUS_EXTRA_ARGUMENT, bsc_execute_line_with_builtins(&console, &workspace, NULL, "commands extra tokens", strlen("commands extra tokens"), &result));
  BUILTIN_ASSERT_TRUE(built_capture.used == 0u);
  BUILTIN_ASSERT_TRUE(fixture.handler_calls == 0);
  BUILTIN_ASSERT_TRUE(fixture.access_calls == 0);
  BUILTIN_ASSERT_TRUE(builtin_workspace_clean(&workspace));
  return 0;
}

/** @brief Verify collisions report the first conflicting descriptor without application execution. */
static int test_collision_policy(const char *test_name) {
  bsc_console_t console;
  bsc_console_workspace_t workspace;
  bsc_console_builtins_result_t result;
  builtin_capture_t capture;
  bsc_output_t output = {builtin_capture_write, &capture};
  builtin_fixture_t fixture;
  const bsc_command_t help_command[] = {
      {path_echo, 1u, BSC_NODE_COMMAND, NULL, 0u, builtin_handler, NULL, BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_NONE, NULL, "Echo", "Echo."},
      {path_HELP, 1u, BSC_NODE_COMMAND, NULL, 0u, builtin_handler, NULL, BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_NONE, NULL, "Help", "Help."},
  };
  const bsc_command_t help_group[] = {
      {path_help, 1u, BSC_NODE_GROUP, NULL, 0u, NULL, NULL, BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_NONE, NULL, "Help group", NULL},
      {path_help_nested, 2u, BSC_NODE_COMMAND, NULL, 0u, builtin_handler, NULL, BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_NONE, NULL, "Nested", "Nested."},
  };
  const bsc_command_t commands_group[] = {
      {path_Commands, 1u, BSC_NODE_GROUP, NULL, 0u, NULL, NULL, BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_NONE, NULL, "Commands", NULL},
      {path_commands_nested, 2u, BSC_NODE_COMMAND, NULL, 0u, builtin_handler, NULL, BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_NONE, NULL, "Nested commands", "Nested commands."},
  };
  const bsc_command_t commands_command[] = {
      {path_echo, 1u, BSC_NODE_COMMAND, NULL, 0u, builtin_handler, NULL, BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_NONE, NULL, "Echo", "Echo."},
      {path_commands, 1u, BSC_NODE_COMMAND, NULL, 0u, builtin_handler, NULL, BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_NONE, NULL, "Commands", "Commands."},
  };
  memset(&fixture, 0, sizeof(fixture));
  fixture.handler_status = BSC_STATUS_OK;
  builtin_capture_init(&capture, sizeof(capture.buffer));
  bsc_console_workspace_init(&workspace);
  BUILTIN_ASSERT_STATUS(BSC_STATUS_OK, builtin_init_console(&console, &fixture, help_command, 2u, &output));
  BUILTIN_ASSERT_STATUS(BSC_STATUS_INVALID_DESCRIPTOR, bsc_execute_line_with_builtins(&console, &workspace, NULL, "help", 4u, &result));
  BUILTIN_ASSERT_TRUE(result.phase == BSC_CONSOLE_PHASE_BUILTIN);
  BUILTIN_ASSERT_TRUE(result.builtin == BSC_CONSOLE_BUILTIN_HELP_INDEX);
  BUILTIN_ASSERT_TRUE(result.collision == &help_command[1]);
  BUILTIN_ASSERT_TRUE(result.collision_index == 1u);
  BUILTIN_ASSERT_TRUE(capture.used == 0u);
  BUILTIN_ASSERT_STATUS(BSC_STATUS_OK, bsc_execute_line_with_builtins(&console, &workspace, NULL, "echo", 4u, &result));
  BUILTIN_ASSERT_TRUE(fixture.handler_calls == 1);
  BUILTIN_ASSERT_STATUS(BSC_STATUS_OK, bsc_execute_line(&console, &workspace, "HELP", 4u, NULL));
  BUILTIN_ASSERT_TRUE(fixture.handler_calls == 2);
  BUILTIN_ASSERT_STATUS(BSC_STATUS_OK, builtin_init_console(&console, &fixture, help_group, 2u, &output));
  BUILTIN_ASSERT_STATUS(BSC_STATUS_INVALID_DESCRIPTOR, bsc_execute_line_with_builtins(&console, &workspace, NULL, "help nested", strlen("help nested"), &result));
  BUILTIN_ASSERT_TRUE(result.collision == &help_group[0]);
  BUILTIN_ASSERT_TRUE(result.collision_index == 0u);
  BUILTIN_ASSERT_STATUS(BSC_STATUS_OK, builtin_init_console(&console, &fixture, commands_group, 2u, &output));
  BUILTIN_ASSERT_STATUS(BSC_STATUS_INVALID_DESCRIPTOR, bsc_execute_line_with_builtins(&console, &workspace, NULL, "commands", 8u, &result));
  BUILTIN_ASSERT_TRUE(result.builtin == BSC_CONSOLE_BUILTIN_COMMANDS);
  BUILTIN_ASSERT_TRUE(result.collision == &commands_group[0]);
  BUILTIN_ASSERT_TRUE(result.collision_index == 0u);
  BUILTIN_ASSERT_STATUS(BSC_STATUS_OK, builtin_init_console(&console, &fixture, commands_command, 2u, &output));
  BUILTIN_ASSERT_STATUS(BSC_STATUS_INVALID_DESCRIPTOR, bsc_execute_line_with_builtins(&console, &workspace, NULL, "commands", 8u, &result));
  BUILTIN_ASSERT_TRUE(result.collision == &commands_command[1]);
  BUILTIN_ASSERT_TRUE(result.collision_index == 1u);
  BUILTIN_ASSERT_TRUE(builtin_workspace_clean(&workspace));
  return 0;
}

/** @brief Verify supplied help visibility options are copied and remain separate from access callbacks. */
static int test_help_visibility_options(const char *test_name) {
  bsc_console_t console;
  bsc_console_workspace_t workspace;
  bsc_console_builtins_result_t result;
  builtin_capture_t capture;
  bsc_output_t output = {builtin_capture_write, &capture};
  bsc_help_options_t options;
  builtin_fixture_t fixture;
  memset(&fixture, 0, sizeof(fixture));
  fixture.handler_status = BSC_STATUS_OK;
  BUILTIN_ASSERT_STATUS(BSC_STATUS_OK, builtin_init_console(&console, &fixture, base_commands, sizeof(base_commands) / sizeof(base_commands[0]), &output));
  bsc_console_workspace_init(&workspace);
  builtin_capture_init(&capture, sizeof(capture.buffer));
  bsc_help_options_init(&options);
  options.include_factory = true;
  options.include_locked = true;
  options.include_hidden = true;
  BUILTIN_ASSERT_STATUS(BSC_STATUS_OK, bsc_execute_line_with_builtins(&console, &workspace, &options, "help factory", strlen("help factory"), &result));
  BUILTIN_ASSERT_STATUS(BSC_STATUS_OK, bsc_execute_line_with_builtins(&console, &workspace, &options, "help locked", strlen("help locked"), &result));
  BUILTIN_ASSERT_STATUS(BSC_STATUS_OK, bsc_execute_line_with_builtins(&console, &workspace, &options, "help hidden", strlen("help hidden"), &result));
  options.include_factory = false;
  BUILTIN_ASSERT_STATUS(BSC_STATUS_UNKNOWN_COMMAND, bsc_execute_line_with_builtins(&console, &workspace, &options, "help factory", strlen("help factory"), &result));
  BUILTIN_ASSERT_TRUE(fixture.access_calls == 0);
  BUILTIN_ASSERT_TRUE(fixture.handler_calls == 0);
  BUILTIN_ASSERT_TRUE(builtin_workspace_clean(&workspace));
  return 0;
}

/** @brief Verify missing, invalid, and short output statuses for built-ins. */
static int test_output_failures(const char *test_name) {
  bsc_console_t console;
  bsc_console_workspace_t workspace;
  bsc_console_builtins_result_t result;
  builtin_capture_t capture;
  bsc_output_t output;
  bsc_output_t invalid_output;
  builtin_fixture_t fixture;
  memset(&fixture, 0, sizeof(fixture));
  fixture.handler_status = BSC_STATUS_OK;
  bsc_console_workspace_init(&workspace);
  BUILTIN_ASSERT_STATUS(BSC_STATUS_OK, builtin_init_console(&console, &fixture, base_commands, sizeof(base_commands) / sizeof(base_commands[0]), NULL));
  BUILTIN_ASSERT_STATUS(BSC_STATUS_INTERNAL_ERROR, bsc_execute_line_with_builtins(&console, &workspace, NULL, "help", 4u, &result));
  BUILTIN_ASSERT_TRUE(result.phase == BSC_CONSOLE_PHASE_BUILTIN);
  invalid_output.write = NULL;
  invalid_output.user = NULL;
  BUILTIN_ASSERT_STATUS(BSC_STATUS_OK, builtin_init_console(&console, &fixture, base_commands, sizeof(base_commands) / sizeof(base_commands[0]), &invalid_output));
  BUILTIN_ASSERT_STATUS(BSC_STATUS_INTERNAL_ERROR, bsc_execute_line_with_builtins(&console, &workspace, NULL, "commands", 8u, &result));
  builtin_capture_init(&capture, sizeof(capture.buffer));
  capture.fail_after = 1u;
  output.write = builtin_capture_write;
  output.user = &capture;
  BUILTIN_ASSERT_STATUS(BSC_STATUS_OK, builtin_init_console(&console, &fixture, base_commands, sizeof(base_commands) / sizeof(base_commands[0]), &output));
  BUILTIN_ASSERT_STATUS(BSC_STATUS_OUTPUT_TRUNCATED, bsc_execute_line_with_builtins(&console, &workspace, NULL, "help", 4u, &result));
  builtin_capture_init(&capture, sizeof(capture.buffer));
  capture.fail_after = 3u;
  BUILTIN_ASSERT_STATUS(BSC_STATUS_OUTPUT_TRUNCATED, bsc_execute_line_with_builtins(&console, &workspace, NULL, "help status", strlen("help status"), &result));
  builtin_capture_init(&capture, sizeof(capture.buffer));
  capture.fail_after = 5u;
  BUILTIN_ASSERT_STATUS(BSC_STATUS_OUTPUT_TRUNCATED, bsc_execute_line_with_builtins(&console, &workspace, NULL, "commands", 8u, &result));
  BUILTIN_ASSERT_TRUE(builtin_workspace_clean(&workspace));
  return 0;
}

/** @brief Verify help metadata validation failures before output and visibility-dependent exposure. */
static int test_validation_failures(const char *test_name) {
  bsc_console_t console;
  bsc_console_workspace_t workspace;
  bsc_console_builtins_result_t result;
  builtin_capture_t capture;
  bsc_output_t output = {builtin_capture_write, &capture};
  bsc_help_options_t options;
  builtin_fixture_t fixture;
  const bsc_command_t bad_summary[] = {{path_bad_summary, 1u, BSC_NODE_COMMAND, NULL, 0u, builtin_handler, NULL, BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_NONE, NULL, NULL, "Bad."}};
  const bsc_command_t bad_description[] = {{path_bad_description, 1u, BSC_NODE_COMMAND, NULL, 0u, builtin_handler, NULL, BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_NONE, NULL, "Bad", NULL}};
  const bsc_command_t bad_control[] = {{path_bad_control, 1u, BSC_NODE_COMMAND, NULL, 0u, builtin_handler, NULL, BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_NONE, NULL, "Bad\r", "Bad."}};
  const bsc_command_t bad_parent[] = {{path_bad_parent_child, 2u, BSC_NODE_COMMAND, NULL, 0u, builtin_handler, NULL, BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_NONE, NULL, "Bad", "Bad."}};
  const bsc_command_t filtered_bad[] = {{path_factory, 1u, BSC_NODE_COMMAND, NULL, 0u, builtin_handler, NULL, BSC_ACCESS_FACTORY, BSC_COMMAND_FLAG_NONE, NULL, NULL, "Bad."}};
  memset(&fixture, 0, sizeof(fixture));
  fixture.handler_status = BSC_STATUS_OK;
  bsc_console_workspace_init(&workspace);
  builtin_capture_init(&capture, sizeof(capture.buffer));
  BUILTIN_ASSERT_STATUS(BSC_STATUS_OK, builtin_init_console(&console, &fixture, bad_summary, 1u, &output));
  BUILTIN_ASSERT_STATUS(BSC_STATUS_INVALID_DESCRIPTOR, bsc_execute_line_with_builtins(&console, &workspace, NULL, "help", 4u, &result));
  BUILTIN_ASSERT_TRUE(capture.used == 0u);
  BUILTIN_ASSERT_STATUS(BSC_STATUS_OK, builtin_init_console(&console, &fixture, bad_description, 1u, &output));
  BUILTIN_ASSERT_STATUS(BSC_STATUS_INVALID_DESCRIPTOR, bsc_execute_line_with_builtins(&console, &workspace, NULL, "commands", 8u, &result));
  BUILTIN_ASSERT_STATUS(BSC_STATUS_OK, builtin_init_console(&console, &fixture, bad_control, 1u, &output));
  BUILTIN_ASSERT_STATUS(BSC_STATUS_INVALID_DESCRIPTOR, bsc_execute_line_with_builtins(&console, &workspace, NULL, "help", 4u, &result));
  BUILTIN_ASSERT_STATUS(BSC_STATUS_OK, builtin_init_console(&console, &fixture, bad_parent, 1u, &output));
  BUILTIN_ASSERT_STATUS(BSC_STATUS_INVALID_DESCRIPTOR, bsc_execute_line_with_builtins(&console, &workspace, NULL, "help", 4u, &result));
  BUILTIN_ASSERT_STATUS(BSC_STATUS_OK, builtin_init_console(&console, &fixture, filtered_bad, 1u, &output));
  BUILTIN_ASSERT_STATUS(BSC_STATUS_OK, bsc_execute_line_with_builtins(&console, &workspace, NULL, "help", 4u, &result));
  bsc_help_options_init(&options);
  options.include_factory = true;
  BUILTIN_ASSERT_STATUS(BSC_STATUS_INVALID_DESCRIPTOR, bsc_execute_line_with_builtins(&console, &workspace, &options, "help", 4u, &result));
  BUILTIN_ASSERT_TRUE(builtin_workspace_clean(&workspace));
  return 0;
}


/** @brief Verify output-callback recursion for same and independent workspaces. */
static int test_output_callback_recursion(const char *test_name) {
  bsc_console_t console;
  bsc_console_workspace_t outer_workspace;
  bsc_console_workspace_t nested_workspace;
  bsc_console_builtins_result_t outer_result;
  builtin_recursive_output_t recursive;
  bsc_output_t output = {builtin_recursive_write, &recursive};
  builtin_fixture_t fixture;
  memset(&fixture, 0, sizeof(fixture));
  memset(&recursive, 0, sizeof(recursive));
  fixture.handler_status = BSC_STATUS_OK;
  builtin_capture_init(&recursive.capture, sizeof(recursive.capture.buffer));
  BUILTIN_ASSERT_STATUS(BSC_STATUS_OK, builtin_init_console(&console, &fixture, base_commands, sizeof(base_commands) / sizeof(base_commands[0]), &output));
  bsc_console_workspace_init(&outer_workspace);
  recursive.console = &console;
  recursive.workspace = &outer_workspace;
  BUILTIN_ASSERT_STATUS(BSC_STATUS_OK, bsc_execute_line_with_builtins(&console, &outer_workspace, NULL, "help", 4u, &outer_result));
  BUILTIN_ASSERT_STATUS(BSC_STATUS_INTERNAL_ERROR, recursive.nested_status);
  BUILTIN_ASSERT_TRUE(recursive.nested_result.phase == BSC_CONSOLE_PHASE_INPUT);
  BUILTIN_ASSERT_TRUE(outer_result.phase == BSC_CONSOLE_PHASE_BUILTIN);
  BUILTIN_ASSERT_TRUE(recursive.capture.used != 0u);
  BUILTIN_ASSERT_TRUE(builtin_workspace_clean(&outer_workspace));

  memset(&recursive, 0, sizeof(recursive));
  builtin_capture_init(&recursive.capture, sizeof(recursive.capture.buffer));
  bsc_console_workspace_init(&outer_workspace);
  bsc_console_workspace_init(&nested_workspace);
  recursive.console = &console;
  recursive.workspace = &nested_workspace;
  BUILTIN_ASSERT_STATUS(BSC_STATUS_OK, bsc_execute_line_with_builtins(&console, &outer_workspace, NULL, "help", 4u, &outer_result));
  BUILTIN_ASSERT_STATUS(BSC_STATUS_OK, recursive.nested_status);
  BUILTIN_ASSERT_TRUE(recursive.nested_result.phase == BSC_CONSOLE_PHASE_BUILTIN);
  BUILTIN_ASSERT_TRUE(recursive.nested_result.builtin == BSC_CONSOLE_BUILTIN_COMMANDS);
  /* Output ordering is intentionally caller-owned; this assertion only proves both calls completed and cleaned up. */
  BUILTIN_ASSERT_TRUE(recursive.capture.used != 0u);
  BUILTIN_ASSERT_TRUE(builtin_workspace_clean(&outer_workspace));
  BUILTIN_ASSERT_TRUE(builtin_workspace_clean(&nested_workspace));
  return 0;
}

/** @brief Verify help options are copied before rendering even if caller storage mutates during output. */
static int test_help_options_copied_during_render(const char *test_name) {
  bsc_console_t console;
  bsc_console_workspace_t workspace;
  bsc_console_builtins_result_t result;
  builtin_recursive_output_t mutating_output;
  bsc_output_t output = {builtin_recursive_write, &mutating_output};
  bsc_help_options_t options;
  builtin_fixture_t fixture;
  memset(&fixture, 0, sizeof(fixture));
  memset(&mutating_output, 0, sizeof(mutating_output));
  fixture.handler_status = BSC_STATUS_OK;
  builtin_capture_init(&mutating_output.capture, sizeof(mutating_output.capture.buffer));
  bsc_help_options_init(&options);
  options.include_factory = true;
  mutating_output.options_to_mutate = &options;
  BUILTIN_ASSERT_STATUS(BSC_STATUS_OK, builtin_init_console(&console, &fixture, base_commands, sizeof(base_commands) / sizeof(base_commands[0]), &output));
  bsc_console_workspace_init(&workspace);
  BUILTIN_ASSERT_STATUS(BSC_STATUS_OK, bsc_execute_line_with_builtins(&console, &workspace, &options, "help", 4u, &result));
  BUILTIN_ASSERT_TRUE(options.include_factory == false);
  BUILTIN_ASSERT_TRUE(builtin_bytes_find(mutating_output.capture.buffer, mutating_output.capture.used, "factory"));
  BUILTIN_ASSERT_TRUE(result.phase == BSC_CONSOLE_PHASE_BUILTIN);
  BUILTIN_ASSERT_TRUE(result.builtin == BSC_CONSOLE_BUILTIN_HELP_INDEX);
  BUILTIN_ASSERT_TRUE(builtin_workspace_clean(&workspace));
  return 0;
}

/** @brief Verify cleanup and same-workspace recursion behavior for built-in-aware execution. */
static int test_reentrancy_and_cleanup(const char *test_name) {
  bsc_console_t console;
  bsc_console_workspace_t workspace;
  bsc_console_workspace_t other_workspace;
  bsc_console_builtins_result_t result;
  builtin_capture_t capture;
  bsc_output_t output = {builtin_capture_write, &capture};
  builtin_fixture_t fixture;
  const bsc_command_t nested_commands[] = {{path_echo, 1u, BSC_NODE_COMMAND, NULL, 0u, builtin_nested_handler, NULL, BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_NONE, NULL, "Echo", "Echo."}};
  memset(&fixture, 0, sizeof(fixture));
  fixture.handler_status = BSC_STATUS_OK;
  builtin_capture_init(&capture, sizeof(capture.buffer));
  BUILTIN_ASSERT_STATUS(BSC_STATUS_OK, builtin_init_console(&console, &fixture, nested_commands, 1u, &output));
  fixture.nested_console = &console;
  fixture.nested_workspace = &workspace;
  bsc_console_workspace_init(&workspace);
  BUILTIN_ASSERT_STATUS(BSC_STATUS_OK, bsc_execute_line_with_builtins(&console, &workspace, NULL, "echo", 4u, &result));
  BUILTIN_ASSERT_STATUS(BSC_STATUS_INTERNAL_ERROR, fixture.nested_status);
  BUILTIN_ASSERT_TRUE(builtin_workspace_clean(&workspace));
  bsc_console_workspace_init(&other_workspace);
  fixture.nested_workspace = &other_workspace;
  BUILTIN_ASSERT_STATUS(BSC_STATUS_OK, bsc_execute_line_with_builtins(&console, &workspace, NULL, "echo", 4u, &result));
  BUILTIN_ASSERT_STATUS(BSC_STATUS_OK, fixture.nested_status);
  BUILTIN_ASSERT_TRUE(builtin_workspace_clean(&workspace));
  BUILTIN_ASSERT_TRUE(builtin_workspace_clean(&other_workspace));
  BUILTIN_ASSERT_STATUS(BSC_STATUS_INVALID_SYNTAX, bsc_execute_line_with_builtins(&console, &workspace, NULL, "echo\n", 5u, &result));
  BUILTIN_ASSERT_TRUE(builtin_workspace_clean(&workspace));
  return 0;
}

/** @brief Verify runtime secret input is neither retained nor exposed by built-in result/output. */
static int test_secret_non_disclosure(const char *test_name) {
  bsc_console_t console;
  bsc_console_workspace_t workspace;
  bsc_console_builtins_result_t result;
  builtin_capture_t capture;
  bsc_output_t output = {builtin_capture_write, &capture};
  builtin_fixture_t fixture;
  memset(&fixture, 0, sizeof(fixture));
  fixture.handler_status = BSC_STATUS_OK;
  builtin_capture_init(&capture, sizeof(capture.buffer));
  BUILTIN_ASSERT_STATUS(BSC_STATUS_OK, builtin_init_console(&console, &fixture, base_commands, sizeof(base_commands) / sizeof(base_commands[0]), &output));
  bsc_console_workspace_init(&workspace);
  BUILTIN_ASSERT_STATUS(BSC_STATUS_OK, bsc_execute_line(&console, &workspace, "secret set actual-secret", strlen("secret set actual-secret"), NULL));
  BUILTIN_ASSERT_TRUE(fixture.secret_seen != NULL);
  BUILTIN_ASSERT_STATUS(BSC_STATUS_OK, bsc_execute_line_with_builtins(&console, &workspace, NULL, "help secret set", strlen("help secret set"), &result));
  BUILTIN_ASSERT_TRUE(!builtin_bytes_find(capture.buffer, capture.used, "actual-secret"));
  BUILTIN_ASSERT_TRUE(result.application_result.command == NULL);
  BUILTIN_ASSERT_TRUE(result.collision == NULL);
  BUILTIN_ASSERT_TRUE(builtin_workspace_clean(&workspace));
  return 0;
}

/** @brief Run complete-line built-in routing tests. */
int bsc_run_console_builtins_tests(void) {
  int failures = 0;
  BUILTIN_RUN_TEST(test_builtins_result_clear);
  BUILTIN_RUN_TEST(test_existing_api_dispatches_help_and_commands);
  BUILTIN_RUN_TEST(test_ordinary_route_equivalence);
  BUILTIN_RUN_TEST(test_help_index_routes_to_pure_renderer);
  BUILTIN_RUN_TEST(test_exact_path_help_routes);
  BUILTIN_RUN_TEST(test_commands_routes);
  BUILTIN_RUN_TEST(test_collision_policy);
  BUILTIN_RUN_TEST(test_help_visibility_options);
  BUILTIN_RUN_TEST(test_output_failures);
  BUILTIN_RUN_TEST(test_validation_failures);
  BUILTIN_RUN_TEST(test_output_callback_recursion);
  BUILTIN_RUN_TEST(test_help_options_copied_during_render);
  BUILTIN_RUN_TEST(test_reentrancy_and_cleanup);
  BUILTIN_RUN_TEST(test_secret_non_disclosure);
  return failures;
}
