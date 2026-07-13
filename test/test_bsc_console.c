#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bsc_console.h"

#define CONSOLE_TEST_ASSERT_TRUE(condition)                                                        \
  do {                                                                                             \
    if (!(condition)) {                                                                            \
      printf("FAIL: %s: %s:%d: %s\n", test_name, __FILE__, __LINE__, #condition);                \
      return 1;                                                                                    \
    }                                                                                              \
  } while (0)
#define CONSOLE_TEST_ASSERT_STATUS(expected, actual) CONSOLE_TEST_ASSERT_TRUE((expected) == (actual))
#define RUN_CONSOLE_TEST(fn)                                                                       \
  do {                                                                                             \
    int result;                                                                                    \
    const char *test_name = #fn;                                                                   \
    result = fn(test_name);                                                                        \
    if (result != 0) {                                                                             \
      failures += 1;                                                                               \
    } else {                                                                                       \
      printf("PASS: %s\n", test_name);                                                           \
    }                                                                                              \
  } while (0)

/** @brief Mutable application fixture observed by console handlers and access callbacks. */
typedef struct console_fixture {
  int handler_calls;
  int access_calls;
  int allow_access;
  bsc_status_t handler_status;
  void *expected_app_context;
  const bsc_command_t *handler_command;
  const bsc_parsed_args_t *handler_args;
  bsc_output_t *handler_output;
  void *handler_output_user;
  int saw_command_context;
  int command_context_value;
  int int_value;
  uint32_t uint_value;
  bool bool_value;
  int32_t enum_value;
  bsc_string_view_t text_value;
  bsc_string_view_t secret_value;
  char text_copy[16];
  size_t text_copy_length;
  bsc_console_t *nested_console;
  bsc_console_workspace_t *nested_workspace;
  bsc_status_t nested_status;
} console_fixture_t;

/** @brief Bounded output sink used to prove the console copies output wrappers by value. */
typedef struct console_sink {
  char bytes[8];
  size_t used;
} console_sink_t;

/** Command path fixture for the status command. */
static const char *const k_status_path[] = {"status"};
/** Command path fixture for a signed integer command. */
static const char *const k_set_int_path[] = {"set", "int"};
/** Command path fixture for an unsigned integer command. */
static const char *const k_set_uint_path[] = {"set", "uint"};
/** Command path fixture for a boolean command. */
static const char *const k_set_bool_path[] = {"set", "bool"};
/** Command path fixture for an enum command. */
static const char *const k_set_mode_path[] = {"set", "mode"};
/** Command path fixture for a string command. */
static const char *const k_set_name_path[] = {"set", "name"};
/** Command path fixture for a secret command. */
static const char *const k_set_secret_path[] = {"set", "secret"};
/** Command path fixture for a group. */
static const char *const k_set_group_path[] = {"set"};
/** Command path fixture for a default-denied command. */
static const char *const k_factory_path[] = {"factory"};
/** Command path fixture for a callback-controlled command. */
static const char *const k_unlock_path[] = {"unlock"};
/** Command path fixture for nested-command selection. */
static const char *const k_settings_path[] = {"settings", "wifi", "status"};
/** Command path fixture for longest-path selection. */
static const char *const k_settings_short_path[] = {"settings"};
#if BSC_ENABLE_FLOAT
/** Command path fixture for float-enabled builds. */
static const char *const k_set_float_path[] = {"set", "float"};
#endif
/** Enum choices used by complete-line enum tests. */
static const bsc_enum_choice_t k_modes[] = {{"low", 10, "Low"}, {"high", 20, "High"}};
/** Signed argument fixture. */
static const bsc_arg_def_t k_int_args[] = {{"value", BSC_ARG_INT, -5, 5, 0u, 0u, 0.0f, 0.0f, 0u, 0u, NULL, 0u, NULL}};
/** Unsigned argument fixture. */
static const bsc_arg_def_t k_uint_args[] = {{"count", BSC_ARG_UINT, 0, 0, 1u, 5u, 0.0f, 0.0f, 0u, 0u, NULL, 0u, NULL}};
/** Boolean argument fixture. */
static const bsc_arg_def_t k_bool_args[] = {{"enabled", BSC_ARG_BOOL, 0, 0, 0u, 0u, 0.0f, 0.0f, 0u, 0u, NULL, 0u, NULL}};
/** Enum argument fixture. */
static const bsc_arg_def_t k_mode_args[] = {{"mode", BSC_ARG_ENUM, 0, 0, 0u, 0u, 0.0f, 0.0f, 0u, 0u, k_modes, 2u, NULL}};
/** Bounded string argument fixture. */
static const bsc_arg_def_t k_name_args[] = {{"name", BSC_ARG_STRING, 0, 0, 0u, 0u, 0.0f, 0.0f, 2u, 8u, NULL, 0u, NULL}};
/** Bounded secret argument fixture. */
static const bsc_arg_def_t k_secret_args[] = {{"secret", BSC_ARG_SECRET, 0, 0, 0u, 0u, 0.0f, 0.0f, 3u, 8u, NULL, 0u, NULL}};
#if BSC_ENABLE_FLOAT
/** Float argument fixture used only when float parsing is enabled. */
static const bsc_arg_def_t k_float_args[] = {{"ratio", BSC_ARG_FLOAT, 0, 0, 0u, 0u, -2.0f, 2.0f, 0u, 0u, NULL, 0u, NULL}};
#endif
/** Command-context fixture value observed by handlers. */
static int g_command_context = 42;

/** @brief Capture output bytes into a bounded sink and report short writes when full. */
static size_t console_sink_write(void *user, const char *data, size_t length) {
  console_sink_t *sink = (console_sink_t *)user;
  size_t available;
  size_t to_copy;
  if (sink == NULL || data == NULL) {
    return 0u;
  }
  available = sizeof(sink->bytes) - sink->used;
  to_copy = length < available ? length : available;
  if (to_copy != 0u) {
    memcpy(&sink->bytes[sink->used], data, to_copy);
    sink->used += to_copy;
  }
  return to_copy;
}

/** @brief Access callback that records invocation and follows fixture-controlled allow/deny policy. */
static bool console_access_callback(void *app_context,
                                    const struct bsc_command *command,
                                    bsc_access_level_t required_access) {
  console_fixture_t *fixture = (console_fixture_t *)app_context;
  (void)command;
  (void)required_access;
  if (fixture != NULL) {
    fixture->access_calls += 1;
    return fixture->allow_access != 0;
  }
  return false;
}

/** @brief Handler that records context, command, output, command_context, and parsed values. */
static bsc_status_t console_record_handler(void *app_context,
                                           const struct bsc_command *command,
                                           const struct bsc_parsed_args *args,
                                           bsc_output_t *output) {
  console_fixture_t *fixture = (console_fixture_t *)app_context;
  if (fixture != NULL) {
    fixture->handler_calls += 1;
    fixture->handler_command = command;
    fixture->handler_args = args;
    fixture->handler_output = output;
    fixture->handler_output_user = output != NULL ? output->user : NULL;
    if (fixture->expected_app_context != NULL && fixture->expected_app_context != app_context) {
      return BSC_STATUS_APP_ERROR;
    }
    if (command != NULL && command->command_context == &g_command_context) {
      fixture->saw_command_context = 1;
    }
    if (args != NULL && args->count > 0u) {
      switch (args->values[0].type) {
      case BSC_ARG_INT:
        fixture->int_value = args->values[0].data.int_value;
        break;
      case BSC_ARG_UINT:
        fixture->uint_value = args->values[0].data.uint_value;
        break;
      case BSC_ARG_BOOL:
        fixture->bool_value = args->values[0].data.bool_value;
        break;
      case BSC_ARG_ENUM:
        fixture->enum_value = args->values[0].data.enum_value;
        break;
      case BSC_ARG_STRING:
        fixture->text_value = args->values[0].data.text_value;
        fixture->text_copy_length = args->values[0].data.text_value.length < sizeof(fixture->text_copy) ? args->values[0].data.text_value.length : sizeof(fixture->text_copy);
        memcpy(fixture->text_copy, args->values[0].data.text_value.data, fixture->text_copy_length);
        break;
      case BSC_ARG_SECRET:
        fixture->secret_value = args->values[0].data.text_value;
        break;
      default:
        break;
      }
    }
    return fixture->handler_status;
  }
  return BSC_STATUS_OK;
}

/** @brief Handler that writes through output so dispatch can propagate output truncation. */
static bsc_status_t console_output_handler(void *app_context,
                                           const struct bsc_command *command,
                                           const struct bsc_parsed_args *args,
                                           bsc_output_t *output) {
  (void)app_context;
  (void)command;
  (void)args;
  return bsc_out_write(output, "0123456789");
}

/** @brief Handler that attempts nested execution with fixture-supplied console/workspace. */
static bsc_status_t console_nested_handler(void *app_context,
                                           const struct bsc_command *command,
                                           const struct bsc_parsed_args *args,
                                           bsc_output_t *output) {
  console_fixture_t *fixture = (console_fixture_t *)app_context;
  (void)command;
  (void)output;
  if (fixture != NULL) {
    fixture->handler_calls += 1;
    if (args != NULL && args->count > 0u) {
      fixture->text_value = args->values[0].data.text_value;
    }
    fixture->nested_status = bsc_execute_line(fixture->nested_console,
                                              fixture->nested_workspace,
                                              "status",
                                              strlen("status"),
                                              NULL);
    if (args != NULL && args->count > 0u && args->values[0].data.text_value.data != fixture->text_value.data) {
      return BSC_STATUS_APP_ERROR;
    }
  }
  return BSC_STATUS_OK;
}

/** @brief Build the ordinary console descriptor table used by most complete-line tests. */
static const bsc_command_t k_commands[] = {
    {k_set_group_path, 1u, BSC_NODE_GROUP, NULL, 0u, NULL, NULL, BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_NONE, NULL, NULL, NULL},
    {k_status_path, 1u, BSC_NODE_COMMAND, NULL, 0u, console_record_handler, &g_command_context, BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_NONE, NULL, NULL, NULL},
    {k_set_int_path, 2u, BSC_NODE_COMMAND, k_int_args, 1u, console_record_handler, &g_command_context, BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_NONE, NULL, NULL, NULL},
    {k_set_uint_path, 2u, BSC_NODE_COMMAND, k_uint_args, 1u, console_record_handler, &g_command_context, BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_NONE, NULL, NULL, NULL},
    {k_set_bool_path, 2u, BSC_NODE_COMMAND, k_bool_args, 1u, console_record_handler, &g_command_context, BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_NONE, NULL, NULL, NULL},
    {k_set_mode_path, 2u, BSC_NODE_COMMAND, k_mode_args, 1u, console_record_handler, &g_command_context, BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_NONE, NULL, NULL, NULL},
    {k_set_name_path, 2u, BSC_NODE_COMMAND, k_name_args, 1u, console_record_handler, &g_command_context, BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_NONE, NULL, NULL, NULL},
    {k_set_secret_path, 2u, BSC_NODE_COMMAND, k_secret_args, 1u, console_record_handler, &g_command_context, BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_NONE, NULL, NULL, NULL},
    {k_factory_path, 1u, BSC_NODE_COMMAND, NULL, 0u, console_record_handler, &g_command_context, BSC_ACCESS_FACTORY, BSC_COMMAND_FLAG_NONE, NULL, NULL, NULL},
    {k_unlock_path, 1u, BSC_NODE_COMMAND, k_int_args, 1u, console_record_handler, &g_command_context, BSC_ACCESS_LOCKED, BSC_COMMAND_FLAG_NONE, console_access_callback, NULL, NULL},
    {k_settings_short_path, 1u, BSC_NODE_COMMAND, NULL, 0u, console_record_handler, &g_command_context, BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_NONE, NULL, NULL, NULL},
    {k_settings_path, 3u, BSC_NODE_COMMAND, NULL, 0u, console_record_handler, &g_command_context, BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_NONE, NULL, NULL, NULL},
};

/** @brief Build a console around the standard descriptor table and supplied fixture/output. */
static bsc_status_t init_console(bsc_console_t *console, console_fixture_t *fixture, const bsc_output_t *output) {
  bsc_console_config_t config;
  config.commands = k_commands;
  config.command_count = sizeof(k_commands) / sizeof(k_commands[0]);
  config.app_context = fixture;
  config.output = output;
  return bsc_console_init(console, &config, NULL);
}

/** @brief Return true when a view points into a workspace line buffer. */
static bool view_points_into_workspace(bsc_string_view_t view, const bsc_console_workspace_t *workspace) {
  const char *begin = workspace->line_buffer;
  const char *end = &workspace->line_buffer[BSC_MAX_LINE_LEN + 1u];
  return view.data >= begin && view.data < end && view.data + view.length <= end;
}

/** @brief Verify console initialization rejects invalid API and registry inputs deterministically. */
static int test_console_initialization_failures(const char *test_name) {
  bsc_console_t console;
  bsc_registry_validation_error_t error;
  bsc_console_config_t config;
  const char *const dup_path[] = {"dup"};
  const bsc_command_t duplicate_commands[] = {
      {dup_path, 1u, BSC_NODE_COMMAND, NULL, 0u, console_record_handler, NULL, BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_NONE, NULL, NULL, NULL},
      {dup_path, 1u, BSC_NODE_COMMAND, NULL, 0u, console_record_handler, NULL, BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_NONE, NULL, NULL, NULL},
  };
  const bsc_command_t invalid_command[] = {{k_status_path, 1u, BSC_NODE_COMMAND, NULL, 0u, NULL, NULL, BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_NONE, NULL, NULL, NULL}};
  error.reason = BSC_REGISTRY_ERROR_DUPLICATE_COMMAND_PATH;
  CONSOLE_TEST_ASSERT_STATUS(BSC_STATUS_INTERNAL_ERROR, bsc_console_init(NULL, NULL, &error));
  CONSOLE_TEST_ASSERT_TRUE(error.reason == BSC_REGISTRY_ERROR_NONE);
  memset(&console, 0x5a, sizeof(console));
  CONSOLE_TEST_ASSERT_STATUS(BSC_STATUS_INTERNAL_ERROR, bsc_console_init(&console, NULL, NULL));
  CONSOLE_TEST_ASSERT_TRUE(!console.initialized);
  config.commands = NULL;
  config.command_count = 1u;
  config.app_context = NULL;
  config.output = NULL;
  CONSOLE_TEST_ASSERT_STATUS(BSC_STATUS_INVALID_DESCRIPTOR, bsc_console_init(&console, &config, &error));
  CONSOLE_TEST_ASSERT_TRUE(!console.initialized);
  CONSOLE_TEST_ASSERT_TRUE(error.reason == BSC_REGISTRY_ERROR_NULL_COMMANDS);
  config.commands = k_commands;
  config.command_count = 0u;
  CONSOLE_TEST_ASSERT_STATUS(BSC_STATUS_INVALID_DESCRIPTOR, bsc_console_init(&console, &config, &error));
  CONSOLE_TEST_ASSERT_TRUE(error.reason == BSC_REGISTRY_ERROR_ZERO_COMMANDS);
  config.command_count = (size_t)BSC_MAX_COMMANDS + 1u;
  CONSOLE_TEST_ASSERT_STATUS(BSC_STATUS_INVALID_DESCRIPTOR, bsc_console_init(&console, &config, &error));
  CONSOLE_TEST_ASSERT_TRUE(error.reason == BSC_REGISTRY_ERROR_TOO_MANY_COMMANDS);
  config.commands = invalid_command;
  config.command_count = 1u;
  CONSOLE_TEST_ASSERT_STATUS(BSC_STATUS_INVALID_DESCRIPTOR, bsc_console_init(&console, &config, &error));
  CONSOLE_TEST_ASSERT_TRUE(error.reason == BSC_REGISTRY_ERROR_COMMAND_MISSING_HANDLER);
  config.commands = duplicate_commands;
  config.command_count = 2u;
  CONSOLE_TEST_ASSERT_STATUS(BSC_STATUS_INVALID_DESCRIPTOR, bsc_console_init(&console, &config, &error));
  CONSOLE_TEST_ASSERT_TRUE(error.reason == BSC_REGISTRY_ERROR_DUPLICATE_COMMAND_PATH);
  return 0;
}

/** @brief Verify valid initialization, inert failed reinitialization, NULL context/output, and output copy-by-value. */
static int test_console_initialization_success_and_reinit(const char *test_name) {
  bsc_console_t console;
  bsc_console_workspace_t workspace;
  bsc_console_result_t result;
  console_fixture_t fixture;
  console_sink_t sink_a = {{0}, 0u};
  console_sink_t sink_b = {{0}, 0u};
  bsc_output_t out;
  bsc_console_config_t config;
  memset(&fixture, 0, sizeof(fixture));
  fixture.handler_status = BSC_STATUS_OK;
  fixture.expected_app_context = &fixture;
  out.write = console_sink_write;
  out.user = &sink_a;
  CONSOLE_TEST_ASSERT_STATUS(BSC_STATUS_OK, init_console(&console, &fixture, &out));
  CONSOLE_TEST_ASSERT_TRUE(console.initialized);
  CONSOLE_TEST_ASSERT_TRUE(console.has_output);
  out.user = &sink_b;
  bsc_console_workspace_init(&workspace);
  CONSOLE_TEST_ASSERT_STATUS(BSC_STATUS_OK, bsc_execute_line(&console, &workspace, "status", 6u, &result));
  CONSOLE_TEST_ASSERT_TRUE(fixture.handler_output != NULL);
  CONSOLE_TEST_ASSERT_TRUE(fixture.handler_output_user == &sink_a);
  config.commands = NULL;
  config.command_count = 1u;
  config.app_context = NULL;
  config.output = NULL;
  CONSOLE_TEST_ASSERT_STATUS(BSC_STATUS_INVALID_DESCRIPTOR, bsc_console_init(&console, &config, NULL));
  CONSOLE_TEST_ASSERT_TRUE(!console.initialized);
  CONSOLE_TEST_ASSERT_STATUS(BSC_STATUS_INTERNAL_ERROR, bsc_execute_line(&console, &workspace, "status", 6u, NULL));
  config.commands = k_commands;
  config.command_count = sizeof(k_commands) / sizeof(k_commands[0]);
  CONSOLE_TEST_ASSERT_STATUS(BSC_STATUS_OK, bsc_console_init(&console, &config, NULL));
  CONSOLE_TEST_ASSERT_TRUE(console.initialized);
  CONSOLE_TEST_ASSERT_TRUE(!console.has_output);
  return 0;
}

/** @brief Verify workspace and result initialization clear every public field. */
static int test_workspace_and_result_initialization(const char *test_name) {
  bsc_console_workspace_t workspace;
  bsc_console_result_t result;
  size_t index;
  bsc_console_workspace_init(NULL);
  bsc_console_result_clear(NULL);
  memset(&workspace, 0x7f, sizeof(workspace));
  bsc_console_workspace_init(&workspace);
  for (index = 0u; index < sizeof(workspace.line_buffer); ++index) {
    CONSOLE_TEST_ASSERT_TRUE(workspace.line_buffer[index] == '\0');
  }
  for (index = 0u; index < (size_t)BSC_MAX_TOKENS; ++index) {
    CONSOLE_TEST_ASSERT_TRUE(workspace.tokens[index].data == NULL);
    CONSOLE_TEST_ASSERT_TRUE(workspace.tokens[index].length == 0u);
  }
  CONSOLE_TEST_ASSERT_TRUE(workspace.token_count == 0u);
  CONSOLE_TEST_ASSERT_TRUE(workspace.match_result.command == NULL);
  CONSOLE_TEST_ASSERT_TRUE(workspace.parsed_args.count == 0u);
  CONSOLE_TEST_ASSERT_TRUE(workspace.parse_error.reason == BSC_ARG_PARSE_ERROR_NONE);
  CONSOLE_TEST_ASSERT_TRUE(!workspace.execution_active);
  memset(&result, 0x7f, sizeof(result));
  bsc_console_result_clear(&result);
  CONSOLE_TEST_ASSERT_TRUE(result.phase == BSC_CONSOLE_PHASE_NONE);
  CONSOLE_TEST_ASSERT_TRUE(result.command == NULL);
  CONSOLE_TEST_ASSERT_TRUE(result.command_index == 0u);
  CONSOLE_TEST_ASSERT_TRUE(result.group == NULL);
  CONSOLE_TEST_ASSERT_TRUE(result.group_index == 0u);
  CONSOLE_TEST_ASSERT_TRUE(result.parse_error.reason == BSC_ARG_PARSE_ERROR_NONE);
  return 0;
}

/** @brief Verify input validation statuses and phases, including embedded NUL and line length. */
static int test_input_validation_and_boundaries(const char *test_name) {
  bsc_console_t console;
  bsc_console_workspace_t workspace;
  bsc_console_result_t result;
  console_fixture_t fixture;
  char exact[BSC_MAX_LINE_LEN];
  char too_long[BSC_MAX_LINE_LEN + 1u];
  char embedded[] = {'s', 't', '\0', 'a'};
  memset(&fixture, 0, sizeof(fixture));
  fixture.handler_status = BSC_STATUS_OK;
  CONSOLE_TEST_ASSERT_STATUS(BSC_STATUS_OK, init_console(&console, &fixture, NULL));
  bsc_console_workspace_init(&workspace);
  CONSOLE_TEST_ASSERT_STATUS(BSC_STATUS_NO_INPUT, bsc_execute_line(&console, &workspace, NULL, 0u, &result));
  CONSOLE_TEST_ASSERT_TRUE(result.phase == BSC_CONSOLE_PHASE_INPUT);
  CONSOLE_TEST_ASSERT_STATUS(BSC_STATUS_INTERNAL_ERROR, bsc_execute_line(&console, &workspace, NULL, 1u, &result));
  CONSOLE_TEST_ASSERT_TRUE(result.phase == BSC_CONSOLE_PHASE_INPUT);
  CONSOLE_TEST_ASSERT_STATUS(BSC_STATUS_NO_INPUT, bsc_execute_line(&console, &workspace, "", 0u, &result));
  CONSOLE_TEST_ASSERT_TRUE(result.phase == BSC_CONSOLE_PHASE_INPUT);
  CONSOLE_TEST_ASSERT_STATUS(BSC_STATUS_INVALID_SYNTAX, bsc_execute_line(&console, &workspace, embedded, sizeof(embedded), &result));
  CONSOLE_TEST_ASSERT_TRUE(result.phase == BSC_CONSOLE_PHASE_INPUT);
  memset(exact, ' ', sizeof(exact));
  CONSOLE_TEST_ASSERT_STATUS(BSC_STATUS_NO_INPUT, bsc_execute_line(&console, &workspace, exact, sizeof(exact), &result));
  CONSOLE_TEST_ASSERT_TRUE(result.phase == BSC_CONSOLE_PHASE_TOKENIZE);
  memset(too_long, ' ', sizeof(too_long));
  CONSOLE_TEST_ASSERT_STATUS(BSC_STATUS_LINE_TOO_LONG, bsc_execute_line(&console, &workspace, too_long, sizeof(too_long), &result));
  CONSOLE_TEST_ASSERT_TRUE(result.phase == BSC_CONSOLE_PHASE_INPUT);
  return 0;
}

/** @brief Verify tokenizer boundary statuses flow through complete-line execution. */
static int test_tokenizer_statuses_and_spacing(const char *test_name) {
  bsc_console_t console;
  bsc_console_workspace_t workspace;
  bsc_console_result_t result;
  console_fixture_t fixture;
  char max_token[BSC_MAX_TOKEN_LEN + 1u];
  char over_token[BSC_MAX_TOKEN_LEN + 2u];
  char many_tokens[BSC_MAX_TOKENS * 2u + 3u];
  size_t index;
  memset(&fixture, 0, sizeof(fixture));
  fixture.handler_status = BSC_STATUS_OK;
  CONSOLE_TEST_ASSERT_STATUS(BSC_STATUS_OK, init_console(&console, &fixture, NULL));
  bsc_console_workspace_init(&workspace);
  CONSOLE_TEST_ASSERT_STATUS(BSC_STATUS_NO_INPUT, bsc_execute_line(&console, &workspace, " \t  ", 4u, &result));
  CONSOLE_TEST_ASSERT_TRUE(result.phase == BSC_CONSOLE_PHASE_TOKENIZE);
  CONSOLE_TEST_ASSERT_STATUS(BSC_STATUS_OK, bsc_execute_line(&console, &workspace, "\t status \t", 9u, &result));
  CONSOLE_TEST_ASSERT_TRUE(result.phase == BSC_CONSOLE_PHASE_DISPATCH);
  CONSOLE_TEST_ASSERT_STATUS(BSC_STATUS_INVALID_SYNTAX, bsc_execute_line(&console, &workspace, "status\r", 7u, &result));
  CONSOLE_TEST_ASSERT_TRUE(result.phase == BSC_CONSOLE_PHASE_TOKENIZE);
  CONSOLE_TEST_ASSERT_STATUS(BSC_STATUS_INVALID_SYNTAX, bsc_execute_line(&console, &workspace, "status\n", 7u, &result));
  CONSOLE_TEST_ASSERT_STATUS(BSC_STATUS_INVALID_SYNTAX, bsc_execute_line(&console, &workspace, "status\r\n", 8u, &result));
  CONSOLE_TEST_ASSERT_STATUS(BSC_STATUS_UNTERMINATED_QUOTE, bsc_execute_line(&console, &workspace, "set name \"abc", 13u, &result));
  memset(max_token, 'a', sizeof(max_token));
  max_token[BSC_MAX_TOKEN_LEN] = '\0';
  CONSOLE_TEST_ASSERT_STATUS(BSC_STATUS_UNKNOWN_COMMAND, bsc_execute_line(&console, &workspace, max_token, BSC_MAX_TOKEN_LEN, &result));
  CONSOLE_TEST_ASSERT_TRUE(result.phase == BSC_CONSOLE_PHASE_MATCH);
  memset(over_token, 'a', sizeof(over_token));
  over_token[BSC_MAX_TOKEN_LEN + 1u] = '\0';
  CONSOLE_TEST_ASSERT_STATUS(BSC_STATUS_TOKEN_TOO_LONG, bsc_execute_line(&console, &workspace, over_token, BSC_MAX_TOKEN_LEN + 1u, &result));
  for (index = 0u; index < sizeof(many_tokens); ++index) {
    many_tokens[index] = (index % 2u) == 0u ? 'x' : ' ';
  }
  CONSOLE_TEST_ASSERT_STATUS(BSC_STATUS_TOO_MANY_TOKENS, bsc_execute_line(&console, &workspace, many_tokens, sizeof(many_tokens), &result));
  return 0;
}

/** @brief Verify complete-line dispatch for supported argument types and nested/longest paths. */
static int test_complete_pipeline_successes(const char *test_name) {
  bsc_console_t console;
  bsc_console_workspace_t workspace;
  bsc_console_result_t result;
  console_fixture_t fixture;
  const char non_terminated[] = {'s', 't', 'a', 't', 'u', 's', 'x'};
  memset(&fixture, 0, sizeof(fixture));
  fixture.handler_status = BSC_STATUS_OK;
  fixture.expected_app_context = &fixture;
  CONSOLE_TEST_ASSERT_STATUS(BSC_STATUS_OK, init_console(&console, &fixture, NULL));
  bsc_console_workspace_init(&workspace);
  CONSOLE_TEST_ASSERT_STATUS(BSC_STATUS_OK, bsc_execute_line(&console, &workspace, non_terminated, 6u, &result));
  CONSOLE_TEST_ASSERT_TRUE(result.command == &k_commands[1]);
  CONSOLE_TEST_ASSERT_TRUE(fixture.handler_calls == 1);
  CONSOLE_TEST_ASSERT_TRUE(fixture.saw_command_context);
  CONSOLE_TEST_ASSERT_STATUS(BSC_STATUS_OK, bsc_execute_line(&console, &workspace, "set int -3", 10u, &result));
  CONSOLE_TEST_ASSERT_TRUE(fixture.int_value == -3);
  CONSOLE_TEST_ASSERT_STATUS(BSC_STATUS_OK, bsc_execute_line(&console, &workspace, "set uint 4", 10u, &result));
  CONSOLE_TEST_ASSERT_TRUE(fixture.uint_value == 4u);
  CONSOLE_TEST_ASSERT_STATUS(BSC_STATUS_OK, bsc_execute_line(&console, &workspace, "set bool true", 13u, &result));
  CONSOLE_TEST_ASSERT_TRUE(fixture.bool_value);
  CONSOLE_TEST_ASSERT_STATUS(BSC_STATUS_OK, bsc_execute_line(&console, &workspace, "set mode high", 13u, &result));
  CONSOLE_TEST_ASSERT_TRUE(fixture.enum_value == 20);
  CONSOLE_TEST_ASSERT_STATUS(BSC_STATUS_OK, bsc_execute_line(&console, &workspace, "set name \"a\\\"b\"", strlen("set name \"a\\\"b\""), &result));
  CONSOLE_TEST_ASSERT_TRUE(fixture.text_value.length == 3u);
  CONSOLE_TEST_ASSERT_TRUE(memcmp(fixture.text_copy, "a\"b", 3u) == 0);
  CONSOLE_TEST_ASSERT_STATUS(BSC_STATUS_OK, bsc_execute_line(&console, &workspace, "set secret hush", 15u, &result));
  CONSOLE_TEST_ASSERT_TRUE(fixture.secret_value.length == 4u);
  CONSOLE_TEST_ASSERT_STATUS(BSC_STATUS_OK, bsc_execute_line(&console, &workspace, "settings wifi status", 20u, &result));
  CONSOLE_TEST_ASSERT_TRUE(result.command == &k_commands[11]);
  return 0;
}

/** @brief Verify parser, matcher, group, access, and handler statuses remain truthful dispatch outputs. */
static int test_pipeline_failures_access_and_handler_statuses(const char *test_name) {
  bsc_console_t console;
  bsc_console_workspace_t workspace;
  bsc_console_result_t result;
  console_fixture_t fixture;
  memset(&fixture, 0, sizeof(fixture));
  fixture.handler_status = BSC_STATUS_OK;
  CONSOLE_TEST_ASSERT_STATUS(BSC_STATUS_OK, init_console(&console, &fixture, NULL));
  bsc_console_workspace_init(&workspace);
  CONSOLE_TEST_ASSERT_STATUS(BSC_STATUS_UNKNOWN_COMMAND, bsc_execute_line(&console, &workspace, "missing", 7u, &result));
  CONSOLE_TEST_ASSERT_TRUE(result.phase == BSC_CONSOLE_PHASE_MATCH);
  CONSOLE_TEST_ASSERT_STATUS(BSC_STATUS_GROUP_REQUIRES_SUBCOMMAND, bsc_execute_line(&console, &workspace, "set", 3u, &result));
  CONSOLE_TEST_ASSERT_TRUE(result.phase == BSC_CONSOLE_PHASE_MATCH);
  CONSOLE_TEST_ASSERT_TRUE(result.group == &k_commands[0]);
  CONSOLE_TEST_ASSERT_STATUS(BSC_STATUS_MISSING_ARGUMENT, bsc_execute_line(&console, &workspace, "set int", 7u, &result));
  CONSOLE_TEST_ASSERT_TRUE(result.phase == BSC_CONSOLE_PHASE_DISPATCH);
  CONSOLE_TEST_ASSERT_TRUE(result.parse_error.reason == BSC_ARG_PARSE_ERROR_MISSING_ARGUMENT);
  CONSOLE_TEST_ASSERT_STATUS(BSC_STATUS_EXTRA_ARGUMENT, bsc_execute_line(&console, &workspace, "status extra", 12u, &result));
  CONSOLE_TEST_ASSERT_TRUE(result.parse_error.reason == BSC_ARG_PARSE_ERROR_EXTRA_ARGUMENT);
  CONSOLE_TEST_ASSERT_STATUS(BSC_STATUS_INVALID_ARGUMENT_TYPE, bsc_execute_line(&console, &workspace, "set int abc", 11u, &result));
  CONSOLE_TEST_ASSERT_STATUS(BSC_STATUS_ARGUMENT_OUT_OF_RANGE, bsc_execute_line(&console, &workspace, "set int 9", 9u, &result));
  CONSOLE_TEST_ASSERT_STATUS(BSC_STATUS_INVALID_ENUM_VALUE, bsc_execute_line(&console, &workspace, "set mode bad", 12u, &result));
  CONSOLE_TEST_ASSERT_STATUS(BSC_STATUS_ARGUMENT_TOO_SHORT, bsc_execute_line(&console, &workspace, "set name a", 10u, &result));
  CONSOLE_TEST_ASSERT_STATUS(BSC_STATUS_ARGUMENT_TOO_LONG, bsc_execute_line(&console, &workspace, "set name abcdefghi", 18u, &result));
  fixture.handler_calls = 0;
  CONSOLE_TEST_ASSERT_STATUS(BSC_STATUS_ACCESS_DENIED, bsc_execute_line(&console, &workspace, "factory", 7u, &result));
  CONSOLE_TEST_ASSERT_TRUE(result.phase == BSC_CONSOLE_PHASE_DISPATCH);
  CONSOLE_TEST_ASSERT_TRUE(result.parse_error.reason == BSC_ARG_PARSE_ERROR_NONE);
  CONSOLE_TEST_ASSERT_TRUE(fixture.handler_calls == 0);
  fixture.allow_access = 1;
  CONSOLE_TEST_ASSERT_STATUS(BSC_STATUS_OK, bsc_execute_line(&console, &workspace, "unlock 1", 8u, &result));
  fixture.allow_access = 0;
  CONSOLE_TEST_ASSERT_STATUS(BSC_STATUS_ACCESS_DENIED, bsc_execute_line(&console, &workspace, "unlock bad", 10u, &result));
  CONSOLE_TEST_ASSERT_TRUE(result.parse_error.reason == BSC_ARG_PARSE_ERROR_NONE);
  fixture.handler_status = (bsc_status_t)999;
  CONSOLE_TEST_ASSERT_STATUS(BSC_STATUS_APP_ERROR, bsc_execute_line(&console, &workspace, "status", 6u, &result));
  fixture.handler_status = BSC_STATUS_MISSING_ARGUMENT;
  CONSOLE_TEST_ASSERT_STATUS(BSC_STATUS_MISSING_ARGUMENT, bsc_execute_line(&console, &workspace, "status", 6u, &result));
  CONSOLE_TEST_ASSERT_TRUE(result.phase == BSC_CONSOLE_PHASE_DISPATCH);
  CONSOLE_TEST_ASSERT_TRUE(result.parse_error.reason == BSC_ARG_PARSE_ERROR_NONE);
  return 0;
}

/** @brief Verify output-neutral console passing and output truncation from handlers. */
static int test_output_passing_and_truncation(const char *test_name) {
  bsc_console_t console;
  bsc_console_workspace_t workspace;
  bsc_console_result_t result;
  console_sink_t sink = {{0}, 0u};
  bsc_output_t output = {console_sink_write, &sink};
  bsc_console_config_t config;
  const bsc_command_t output_command[] = {{k_status_path, 1u, BSC_NODE_COMMAND, NULL, 0u, console_output_handler, NULL, BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_NONE, NULL, NULL, NULL}};
  config.commands = output_command;
  config.command_count = 1u;
  config.app_context = NULL;
  config.output = &output;
  CONSOLE_TEST_ASSERT_STATUS(BSC_STATUS_OK, bsc_console_init(&console, &config, NULL));
  bsc_console_workspace_init(&workspace);
  CONSOLE_TEST_ASSERT_STATUS(BSC_STATUS_OUTPUT_TRUNCATED, bsc_execute_line(&console, &workspace, "status", 6u, &result));
  CONSOLE_TEST_ASSERT_TRUE(result.phase == BSC_CONSOLE_PHASE_DISPATCH);
  CONSOLE_TEST_ASSERT_TRUE(sink.used == sizeof(sink.bytes));
  config.output = NULL;
  CONSOLE_TEST_ASSERT_STATUS(BSC_STATUS_OK, bsc_console_init(&console, &config, NULL));
  CONSOLE_TEST_ASSERT_STATUS(BSC_STATUS_INTERNAL_ERROR, bsc_execute_line(&console, &workspace, "status", 6u, &result));
  return 0;
}

/** @brief Verify workspace cleanup removes transient data and supports sequential reuse. */
static int test_workspace_cleanup_and_aliasing(const char *test_name) {
  bsc_console_t console;
  bsc_console_workspace_t workspace;
  bsc_console_result_t result;
  console_fixture_t fixture;
  size_t index;
  memset(&fixture, 0, sizeof(fixture));
  fixture.handler_status = BSC_STATUS_APP_ERROR;
  CONSOLE_TEST_ASSERT_STATUS(BSC_STATUS_OK, init_console(&console, &fixture, NULL));
  bsc_console_workspace_init(&workspace);
  memcpy(workspace.line_buffer, "set name abc", 12u);
  CONSOLE_TEST_ASSERT_STATUS(BSC_STATUS_APP_ERROR, bsc_execute_line(&console, &workspace, workspace.line_buffer, 12u, &result));
  CONSOLE_TEST_ASSERT_TRUE(view_points_into_workspace(fixture.text_value, &workspace));
  for (index = 0u; index < sizeof(workspace.line_buffer); ++index) {
    CONSOLE_TEST_ASSERT_TRUE(workspace.line_buffer[index] == '\0');
  }
  CONSOLE_TEST_ASSERT_TRUE(workspace.token_count == 0u);
  CONSOLE_TEST_ASSERT_TRUE(workspace.match_result.command == NULL);
  CONSOLE_TEST_ASSERT_TRUE(workspace.parsed_args.count == 0u);
  CONSOLE_TEST_ASSERT_TRUE(workspace.parse_error.reason == BSC_ARG_PARSE_ERROR_NONE);
  CONSOLE_TEST_ASSERT_TRUE(!workspace.execution_active);
  fixture.handler_status = BSC_STATUS_OK;
  CONSOLE_TEST_ASSERT_STATUS(BSC_STATUS_OK, bsc_execute_line(&console, &workspace, "status", 6u, &result));
  CONSOLE_TEST_ASSERT_STATUS(BSC_STATUS_UNKNOWN_COMMAND, bsc_execute_line(&console, &workspace, "unknown", 7u, &result));
  for (index = 0u; index < sizeof(workspace.line_buffer); ++index) {
    CONSOLE_TEST_ASSERT_TRUE(workspace.line_buffer[index] == '\0');
  }
  return 0;
}

/** @brief Verify recursive same-workspace execution is rejected without corrupting the outer handler view. */
static int test_same_workspace_recursion_guard(const char *test_name) {
  bsc_console_t console;
  bsc_console_workspace_t workspace;
  bsc_console_result_t result;
  console_fixture_t fixture;
  const bsc_command_t nested_commands[] = {
      {k_status_path, 1u, BSC_NODE_COMMAND, NULL, 0u, console_record_handler, NULL, BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_NONE, NULL, NULL, NULL},
      {k_set_name_path, 2u, BSC_NODE_COMMAND, k_name_args, 1u, console_nested_handler, NULL, BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_NONE, NULL, NULL, NULL},
  };
  bsc_console_config_t config;
  memset(&fixture, 0, sizeof(fixture));
  fixture.nested_console = &console;
  fixture.nested_workspace = &workspace;
  config.commands = nested_commands;
  config.command_count = 2u;
  config.app_context = &fixture;
  config.output = NULL;
  CONSOLE_TEST_ASSERT_STATUS(BSC_STATUS_OK, bsc_console_init(&console, &config, NULL));
  bsc_console_workspace_init(&workspace);
  CONSOLE_TEST_ASSERT_STATUS(BSC_STATUS_OK, bsc_execute_line(&console, &workspace, "set name abc", 12u, &result));
  CONSOLE_TEST_ASSERT_TRUE(fixture.nested_status == BSC_STATUS_INTERNAL_ERROR);
  CONSOLE_TEST_ASSERT_TRUE(fixture.text_value.length == 3u);
  CONSOLE_TEST_ASSERT_TRUE(!workspace.execution_active);
  CONSOLE_TEST_ASSERT_STATUS(BSC_STATUS_OK, bsc_execute_line(&console, &workspace, "status", 6u, NULL));
  return 0;
}

/** @brief Verify the same immutable console can execute with two independent workspaces. */
static int test_same_console_distinct_workspaces(const char *test_name) {
  bsc_console_t console;
  bsc_console_workspace_t workspace_a;
  bsc_console_workspace_t workspace_b;
  console_fixture_t fixture;
  memset(&fixture, 0, sizeof(fixture));
  fixture.handler_status = BSC_STATUS_OK;
  CONSOLE_TEST_ASSERT_STATUS(BSC_STATUS_OK, init_console(&console, &fixture, NULL));
  bsc_console_workspace_init(&workspace_a);
  bsc_console_workspace_init(&workspace_b);
  CONSOLE_TEST_ASSERT_TRUE(sizeof(console) < sizeof(workspace_a));
  CONSOLE_TEST_ASSERT_STATUS(BSC_STATUS_OK, bsc_execute_line(&console, &workspace_a, "status", 6u, NULL));
  CONSOLE_TEST_ASSERT_STATUS(BSC_STATUS_OK, bsc_execute_line(&console, &workspace_b, "set int 2", 9u, NULL));
  CONSOLE_TEST_ASSERT_TRUE(fixture.int_value == 2);
  return 0;
}

#if BSC_ENABLE_FLOAT
/** @brief Verify float command execution when float support is enabled. */
static int test_float_enabled_console_execution(const char *test_name) {
  bsc_console_t console;
  bsc_console_workspace_t workspace;
  console_fixture_t fixture;
  const bsc_command_t float_command[] = {{k_set_float_path, 2u, BSC_NODE_COMMAND, k_float_args, 1u, console_record_handler, NULL, BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_NONE, NULL, NULL, NULL}};
  bsc_console_config_t config;
  memset(&fixture, 0, sizeof(fixture));
  fixture.handler_status = BSC_STATUS_OK;
  config.commands = float_command;
  config.command_count = 1u;
  config.app_context = &fixture;
  config.output = NULL;
  CONSOLE_TEST_ASSERT_STATUS(BSC_STATUS_OK, bsc_console_init(&console, &config, NULL));
  bsc_console_workspace_init(&workspace);
  CONSOLE_TEST_ASSERT_STATUS(BSC_STATUS_OK, bsc_execute_line(&console, &workspace, "set float 1.5", 13u, NULL));
  return 0;
}
#else
/** @brief Verify float descriptors are rejected by console initialization when float support is disabled. */
static int test_float_disabled_console_initialization(const char *test_name) {
  bsc_console_t console;
  bsc_registry_validation_error_t error;
  const char *const float_path[] = {"set", "float"};
  const bsc_arg_def_t float_args[] = {{"ratio", BSC_ARG_FLOAT, 0, 0, 0u, 0u, -1.0f, 1.0f, 0u, 0u, NULL, 0u, NULL}};
  const bsc_command_t float_command[] = {{float_path, 2u, BSC_NODE_COMMAND, float_args, 1u, console_record_handler, NULL, BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_NONE, NULL, NULL, NULL}};
  bsc_console_config_t config;
  config.commands = float_command;
  config.command_count = 1u;
  config.app_context = NULL;
  config.output = NULL;
  CONSOLE_TEST_ASSERT_STATUS(BSC_STATUS_INVALID_DESCRIPTOR, bsc_console_init(&console, &config, &error));
  CONSOLE_TEST_ASSERT_TRUE(error.reason == BSC_REGISTRY_ERROR_FLOAT_DISABLED);
  return 0;
}
#endif

/** @brief Run all complete-line console orchestration tests. */
int bsc_run_console_tests(void) {
  int failures = 0;
  RUN_CONSOLE_TEST(test_console_initialization_failures);
  RUN_CONSOLE_TEST(test_console_initialization_success_and_reinit);
  RUN_CONSOLE_TEST(test_workspace_and_result_initialization);
  RUN_CONSOLE_TEST(test_input_validation_and_boundaries);
  RUN_CONSOLE_TEST(test_tokenizer_statuses_and_spacing);
  RUN_CONSOLE_TEST(test_complete_pipeline_successes);
  RUN_CONSOLE_TEST(test_pipeline_failures_access_and_handler_statuses);
  RUN_CONSOLE_TEST(test_output_passing_and_truncation);
  RUN_CONSOLE_TEST(test_workspace_cleanup_and_aliasing);
  RUN_CONSOLE_TEST(test_same_workspace_recursion_guard);
  RUN_CONSOLE_TEST(test_same_console_distinct_workspaces);
#if BSC_ENABLE_FLOAT
  RUN_CONSOLE_TEST(test_float_enabled_console_execution);
#else
  RUN_CONSOLE_TEST(test_float_disabled_console_initialization);
#endif
  return failures;
}
