#include "bsc_help.h"

#include "bsc_args.h"
#include "bsc_config.h"

#include <stdio.h>
#include <string.h>

#ifndef BSC_TEST_GOLDEN_DIR
#define BSC_TEST_GOLDEN_DIR "test/golden"
#endif

#define HELP_ASSERT_TRUE(condition)                                                               \
  do {                                                                                            \
    if (!(condition)) {                                                                           \
      printf("FAIL: %s: %s:%d: %s\n", test_name, __FILE__, __LINE__, #condition);                 \
      return 1;                                                                                   \
    }                                                                                             \
  } while (0)
#define HELP_ASSERT_STATUS(expected, actual) HELP_ASSERT_TRUE((expected) == (actual))
#define HELP_RUN_TEST(fn)                                                                         \
  do {                                                                                            \
    int result;                                                                                   \
    const char *test_name = #fn;                                                                  \
    result = fn(test_name);                                                                       \
    if (result != 0) {                                                                            \
      failures += 1;                                                                              \
    } else {                                                                                      \
      printf("PASS: %s\n", test_name);                                                           \
    }                                                                                             \
  } while (0)

typedef struct help_capture {
  char buffer[8192];
  size_t used;
  size_t limit;
  size_t calls;
} help_capture_t;

static int g_handler_calls;
static int g_access_calls;

/** @brief Handler sentinel that must never be reached by help APIs. */
static bsc_status_t help_forbidden_handler(void *app_context,
                                           const bsc_command_t *command,
                                           const bsc_parsed_args_t *args,
                                           bsc_output_t *output) {
  (void)app_context;
  (void)command;
  (void)args;
  (void)output;
  g_handler_calls += 1;
  return BSC_STATUS_APP_ERROR;
}

/** @brief Access sentinel that must never be reached by help APIs. */
static bool help_forbidden_access(void *app_context,
                                  const bsc_command_t *command,
                                  bsc_access_level_t required_access) {
  (void)app_context;
  (void)command;
  (void)required_access;
  g_access_calls += 1;
  return false;
}

/** @brief Capture output bytes with an optional short-write limit. */
static size_t help_capture_write(void *user, const char *data, size_t length) {
  help_capture_t *capture = (help_capture_t *)user;
  size_t available;
  size_t accepted;
  capture->calls += 1u;
  if (data == NULL) {
    return 0u;
  }
  available = capture->limit - capture->used;
  accepted = length < available ? length : available;
  if (accepted > 0u) {
    memcpy(&capture->buffer[capture->used], data, accepted);
    capture->used += accepted;
  }
  return accepted;
}

static const char *const path_status[] = {"status"};
static const char *const path_settings[] = {"settings"};
static const char *const path_settings_wifi[] = {"settings", "wifi"};
static const char *const path_settings_wifi_scan[] = {"settings", "wifi", "scan"};
static const char *const path_settings_wifi_set[] = {"settings", "wifi", "set"};
static const char *const path_settings_wifi_set_ssid[] = {"settings", "wifi", "set", "ssid"};
static const char *const path_mode[] = {"mode"};
static const char *const path_factory[] = {"factory"};
static const char *const path_locked[] = {"locked"};
static const char *const path_hidden[] = {"hidden"};
static const char *const path_hidden_factory[] = {"hidden", "factory"};

static bsc_enum_choice_t mode_choices[] = {
    {"off", 0, "Disable radio"},
    {"sta", 1, "Station mode"},
    {"ap", 2, NULL},
};

static bsc_arg_def_t wifi_args[] = {
    {"ssid", BSC_ARG_STRING, 0, 0, 0u, 0u, 0.0f, 0.0f, 1u, 32u, NULL, 0u, "Network name"},
    {"password", BSC_ARG_SECRET, 0, 0, 0u, 0u, 0.0f, 0.0f, 8u, 64u, NULL, 0u, "Network password"},
    {"channel", BSC_ARG_UINT, 0, 0, 1u, 11u, 0.0f, 0.0f, 0u, 0u, NULL, 0u, "WiFi channel"},
    {"power", BSC_ARG_INT, -40, 20, 0u, 0u, 0.0f, 0.0f, 0u, 0u, NULL, 0u, "Transmit power"},
    {"enabled", BSC_ARG_BOOL, 0, 0, 0u, 0u, 0.0f, 0.0f, 0u, 0u, NULL, 0u, "Enable radio"},
    {"mode", BSC_ARG_ENUM, 0, 0, 0u, 0u, 0.0f, 0.0f, 0u, 0u, mode_choices, 3u, "Radio mode"},
#if BSC_ENABLE_FLOAT
    {"ratio", BSC_ARG_FLOAT, 0, 0, 0u, 0u, -1.5f, 2.25f, 0u, 0u, NULL, 0u, NULL},
#endif
};

#if BSC_ENABLE_FLOAT
#define WIFI_ARG_COUNT 7u
#else
#define WIFI_ARG_COUNT 6u
#endif

static bsc_command_t help_commands[] = {
    {path_status, 1u, BSC_NODE_COMMAND, NULL, 0u, help_forbidden_handler, NULL, BSC_ACCESS_NORMAL,
     BSC_COMMAND_FLAG_NONE, help_forbidden_access, "Show system status", "Prints current status."},
    {path_settings, 1u, BSC_NODE_GROUP, NULL, 0u, NULL, NULL, BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_NONE,
     help_forbidden_access, "Configure device settings", "Settings commands."},
    {path_settings_wifi, 2u, BSC_NODE_GROUP, NULL, 0u, NULL, NULL, BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_NONE,
     help_forbidden_access, "Configure WiFi", NULL},
    {path_settings_wifi_scan, 3u, BSC_NODE_COMMAND, NULL, 0u, help_forbidden_handler, NULL,
     BSC_ACCESS_FACTORY, BSC_COMMAND_FLAG_NONE, help_forbidden_access, "Scan for networks", "Scans WiFi."},
    {path_settings_wifi_set, 3u, BSC_NODE_GROUP, NULL, 0u, NULL, NULL, BSC_ACCESS_NORMAL,
     BSC_COMMAND_FLAG_NONE, help_forbidden_access, "Set WiFi values", NULL},
    {path_settings_wifi_set_ssid, 4u, BSC_NODE_COMMAND, wifi_args, WIFI_ARG_COUNT, help_forbidden_handler, NULL,
     BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_NONE, help_forbidden_access, "Set WiFi SSID",
     "Stores WiFi credentials and radio settings."},
    {path_mode, 1u, BSC_NODE_COMMAND, NULL, 0u, help_forbidden_handler, NULL, BSC_ACCESS_ADVANCED,
     BSC_COMMAND_FLAG_NONE, help_forbidden_access, "Set operating mode", "Sets the operating mode."},
    {path_factory, 1u, BSC_NODE_COMMAND, NULL, 0u, help_forbidden_handler, NULL, BSC_ACCESS_FACTORY,
     BSC_COMMAND_FLAG_NONE, help_forbidden_access, "Factory command", "Factory only."},
    {path_locked, 1u, BSC_NODE_COMMAND, NULL, 0u, help_forbidden_handler, NULL, BSC_ACCESS_LOCKED,
     BSC_COMMAND_FLAG_NONE, help_forbidden_access, "Locked command", "Locked only."},
    {path_hidden, 1u, BSC_NODE_GROUP, NULL, 0u, NULL, NULL, BSC_ACCESS_NORMAL,
     BSC_COMMAND_FLAG_HIDDEN, help_forbidden_access, "Hidden group", NULL},
    {path_hidden_factory, 2u, BSC_NODE_COMMAND, NULL, 0u, help_forbidden_handler, NULL, BSC_ACCESS_FACTORY,
     BSC_COMMAND_FLAG_HIDDEN, help_forbidden_access, "Hidden factory", "Hidden factory."},
};

static const size_t help_command_count = sizeof(help_commands) / sizeof(help_commands[0]);

/** @brief Initialize a capture sink for renderer tests. */
static void help_capture_init(help_capture_t *capture, size_t limit) {
  memset(capture->buffer, 0, sizeof(capture->buffer));
  capture->used = 0u;
  capture->limit = limit;
  capture->calls = 0u;
}

/** @brief Load one golden file in binary mode into caller storage. */
static int help_load_golden(const char *name, char *buffer, size_t capacity, size_t *length) {
  char path[256];
  FILE *file;
  size_t count;
  size_t dir_len = strlen(BSC_TEST_GOLDEN_DIR);
  size_t name_len = strlen(name);
  if (dir_len + 1u + name_len + 1u > sizeof(path)) {
    return 0;
  }
  memcpy(path, BSC_TEST_GOLDEN_DIR, dir_len);
  path[dir_len] = '/';
  memcpy(&path[dir_len + 1u], name, name_len + 1u);
  file = fopen(path, "rb");
  if (file == NULL) {
    return 0;
  }
  count = fread(buffer, 1u, capacity, file);
  if (ferror(file) || !feof(file)) {
    fclose(file);
    return 0;
  }
  fclose(file);
  *length = count;
  return 1;
}


/** @brief Return whether a byte sequence occurs in a captured output buffer. */
static int help_bytes_find(const char *haystack, size_t haystack_len, const char *needle, size_t needle_len) {
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

/** @brief Render and compare against a byte-exact golden fixture. */
static int help_assert_golden(const char *test_name,
                              const char *golden_name,
                              bsc_status_t (*render_fn)(help_capture_t *capture)) {
  help_capture_t capture;
  char expected[8192];
  size_t expected_len = 0u;
  bsc_status_t status;
  HELP_ASSERT_TRUE(help_load_golden(golden_name, expected, sizeof(expected), &expected_len));
  help_capture_init(&capture, sizeof(capture.buffer));
  status = render_fn(&capture);
  HELP_ASSERT_STATUS(BSC_STATUS_OK, status);
  HELP_ASSERT_TRUE(capture.used == expected_len);
  HELP_ASSERT_TRUE(memcmp(capture.buffer, expected, expected_len) == 0);
  HELP_ASSERT_TRUE(expected_len > 0u && expected[expected_len - 1u] == '\n');
  HELP_ASSERT_TRUE(memchr(expected, '\r', expected_len) == NULL);
  return 0;
}

static bsc_status_t render_index_capture(help_capture_t *capture) {
  bsc_output_t output = {help_capture_write, capture};
  return bsc_help_render_index(help_commands, help_command_count, NULL, &output);
}

static bsc_status_t render_commands_capture(help_capture_t *capture) {
  bsc_output_t output = {help_capture_write, capture};
  return bsc_help_render_commands(help_commands, help_command_count, NULL, &output);
}

static bsc_status_t render_settings_capture(help_capture_t *capture) {
  bsc_output_t output = {help_capture_write, capture};
  bsc_string_view_t tokens[] = {bsc_string_view_from_cstr("settings")};
  return bsc_help_render_path(help_commands, help_command_count, tokens, 1u, NULL, &output);
}

static bsc_status_t render_wifi_set_capture(help_capture_t *capture) {
  bsc_output_t output = {help_capture_write, capture};
  bsc_string_view_t tokens[] = {bsc_string_view_from_cstr("settings"), bsc_string_view_from_cstr("wifi"),
                                bsc_string_view_from_cstr("set"), bsc_string_view_from_cstr("ssid")};
  return bsc_help_render_path(help_commands, help_command_count, tokens, 4u, NULL, &output);
}

static int test_help_golden_outputs(const char *test_name) {
  HELP_ASSERT_TRUE(help_assert_golden(test_name, "help_index.txt", render_index_capture) == 0);
  HELP_ASSERT_TRUE(help_assert_golden(test_name, "commands.txt", render_commands_capture) == 0);
  HELP_ASSERT_TRUE(help_assert_golden(test_name, "group_settings.txt", render_settings_capture) == 0);
#if BSC_ENABLE_FLOAT && BSC_MAX_FLOAT_FRACTION_DIGITS == 6u
  HELP_ASSERT_TRUE(help_assert_golden(test_name, "command_wifi_set_ssid.txt", render_wifi_set_capture) == 0);
#else
  {
    help_capture_t capture;
    help_capture_init(&capture, sizeof(capture.buffer));
    HELP_ASSERT_STATUS(BSC_STATUS_OK, render_wifi_set_capture(&capture));
    HELP_ASSERT_TRUE(capture.used > 0u);
  }
#endif
  HELP_ASSERT_TRUE(g_handler_calls == 0);
  HELP_ASSERT_TRUE(g_access_calls == 0);
  return 0;
}

static int test_help_options_defaults(const char *test_name) {
  bsc_help_options_t options;
  memset(&options, 0, sizeof(options));
  bsc_help_options_init(&options);
  HELP_ASSERT_TRUE(options.include_advanced);
  HELP_ASSERT_TRUE(!options.include_factory);
  HELP_ASSERT_TRUE(!options.include_locked);
  HELP_ASSERT_TRUE(!options.include_hidden);
  bsc_help_options_init(NULL);
  return 0;
}

static int test_help_validation_successes(const char *test_name) {
  bsc_help_validation_error_t error;
  HELP_ASSERT_STATUS(BSC_STATUS_OK, bsc_help_validate(help_commands, help_command_count, NULL, &error));
  HELP_ASSERT_TRUE(error.reason == BSC_HELP_ERROR_NONE);
  return 0;
}

/** @brief Create mutable descriptor copy for focused validation failure tests. */
static void copy_commands(bsc_command_t *out) { memcpy(out, help_commands, sizeof(help_commands)); }

static int test_help_validation_required_and_optional_text(const char *test_name) {
  bsc_command_t table[sizeof(help_commands) / sizeof(help_commands[0])];
  bsc_help_validation_error_t error;
  copy_commands(table);
  table[0].summary = NULL;
  HELP_ASSERT_STATUS(BSC_STATUS_INVALID_DESCRIPTOR, bsc_help_validate(table, help_command_count, NULL, &error));
  HELP_ASSERT_TRUE(error.reason == BSC_HELP_ERROR_MISSING_SUMMARY);
  copy_commands(table);
  table[0].summary = "";
  HELP_ASSERT_STATUS(BSC_STATUS_INVALID_DESCRIPTOR, bsc_help_validate(table, help_command_count, NULL, &error));
  HELP_ASSERT_TRUE(error.reason == BSC_HELP_ERROR_EMPTY_SUMMARY);
  copy_commands(table);
  table[0].description = NULL;
  HELP_ASSERT_STATUS(BSC_STATUS_INVALID_DESCRIPTOR, bsc_help_validate(table, help_command_count, NULL, &error));
  HELP_ASSERT_TRUE(error.reason == BSC_HELP_ERROR_MISSING_EXECUTABLE_DESCRIPTION);
  copy_commands(table);
  table[0].description = "";
  HELP_ASSERT_STATUS(BSC_STATUS_INVALID_DESCRIPTOR, bsc_help_validate(table, help_command_count, NULL, &error));
  HELP_ASSERT_TRUE(error.reason == BSC_HELP_ERROR_EMPTY_DESCRIPTION);
  copy_commands(table);
  wifi_args[0].help = "";
  HELP_ASSERT_STATUS(BSC_STATUS_INVALID_DESCRIPTOR, bsc_help_validate(table, help_command_count, NULL, &error));
  HELP_ASSERT_TRUE(error.reason == BSC_HELP_ERROR_EMPTY_ARGUMENT_HELP);
  wifi_args[0].help = "Network name";
  copy_commands(table);
  mode_choices[0].help = "";
  HELP_ASSERT_STATUS(BSC_STATUS_INVALID_DESCRIPTOR, bsc_help_validate(table, help_command_count, NULL, &error));
  HELP_ASSERT_TRUE(error.reason == BSC_HELP_ERROR_EMPTY_ENUM_CHOICE_HELP);
  mode_choices[0].help = "Disable radio";
  return 0;
}

static int test_help_validation_control_and_bounds(const char *test_name) {
  static char max_text[BSC_MAX_HELP_TEXT_LEN + 1u];
  static char too_long[BSC_MAX_HELP_TEXT_LEN + 2u];
  bsc_command_t table[sizeof(help_commands) / sizeof(help_commands[0])];
  bsc_help_validation_error_t error;
  size_t index;
  for (index = 0u; index < (size_t)BSC_MAX_HELP_TEXT_LEN; ++index) max_text[index] = 'a';
  max_text[BSC_MAX_HELP_TEXT_LEN] = '\0';
  for (index = 0u; index <= (size_t)BSC_MAX_HELP_TEXT_LEN; ++index) too_long[index] = 'b';
  too_long[BSC_MAX_HELP_TEXT_LEN + 1u] = '\0';
  copy_commands(table);
  table[0].summary = max_text;
  HELP_ASSERT_STATUS(BSC_STATUS_OK, bsc_help_validate(table, help_command_count, NULL, &error));
  copy_commands(table);
  table[0].summary = too_long;
  HELP_ASSERT_STATUS(BSC_STATUS_INVALID_DESCRIPTOR, bsc_help_validate(table, help_command_count, NULL, &error));
  HELP_ASSERT_TRUE(error.reason == BSC_HELP_ERROR_SUMMARY_TOO_LONG);
  copy_commands(table);
  table[0].summary = "bad\rtext";
  HELP_ASSERT_STATUS(BSC_STATUS_INVALID_DESCRIPTOR, bsc_help_validate(table, help_command_count, NULL, &error));
  HELP_ASSERT_TRUE(error.reason == BSC_HELP_ERROR_INVALID_HELP_TEXT_CONTROL_BYTE);
  copy_commands(table);
  table[0].summary = "bad\ntext";
  HELP_ASSERT_STATUS(BSC_STATUS_INVALID_DESCRIPTOR, bsc_help_validate(table, help_command_count, NULL, &error));
  copy_commands(table);
  table[0].summary = "bad\x01";
  HELP_ASSERT_STATUS(BSC_STATUS_INVALID_DESCRIPTOR, bsc_help_validate(table, help_command_count, NULL, &error));
  copy_commands(table);
  table[0].summary = "bad\x7f";
  HELP_ASSERT_STATUS(BSC_STATUS_INVALID_DESCRIPTOR, bsc_help_validate(table, help_command_count, NULL, &error));
  copy_commands(table);
  table[0].summary = "nonascii \xc3\xa9";
  HELP_ASSERT_STATUS(BSC_STATUS_OK, bsc_help_validate(table, help_command_count, NULL, &error));
  return 0;
}

static int test_help_validation_registry_and_parent_groups(const char *test_name) {
  bsc_command_t table[sizeof(help_commands) / sizeof(help_commands[0])];
  bsc_help_validation_error_t error;
  copy_commands(table);
  table[0].path = NULL;
  HELP_ASSERT_STATUS(BSC_STATUS_INVALID_DESCRIPTOR, bsc_help_validate(table, help_command_count, NULL, &error));
  HELP_ASSERT_TRUE(error.reason == BSC_HELP_ERROR_REGISTRY_INVALID);
  HELP_ASSERT_TRUE(error.registry_error.reason == BSC_REGISTRY_ERROR_NULL_PATH);
  copy_commands(table);
  table[1].flags = BSC_COMMAND_FLAG_HIDDEN;
  HELP_ASSERT_STATUS(BSC_STATUS_INVALID_DESCRIPTOR, bsc_help_validate(table, help_command_count, NULL, &error));
  HELP_ASSERT_TRUE(error.reason == BSC_HELP_ERROR_MISSING_VISIBLE_PARENT_GROUP);
  HELP_ASSERT_TRUE(error.required_parent_depth == 1u);
  copy_commands(table);
  table[2].flags = BSC_COMMAND_FLAG_HIDDEN;
  HELP_ASSERT_STATUS(BSC_STATUS_INVALID_DESCRIPTOR, bsc_help_validate(table, help_command_count, NULL, &error));
  HELP_ASSERT_TRUE(error.reason == BSC_HELP_ERROR_MISSING_VISIBLE_PARENT_GROUP);
  HELP_ASSERT_TRUE(error.required_parent_depth == 2u);
  return 0;
}

static int test_help_validation_filtered_metadata(const char *test_name) {
  bsc_command_t table[sizeof(help_commands) / sizeof(help_commands[0])];
  bsc_help_validation_error_t error;
  bsc_help_options_t options;
  copy_commands(table);
  table[7].summary = NULL;
  HELP_ASSERT_STATUS(BSC_STATUS_OK, bsc_help_validate(table, help_command_count, NULL, &error));
  bsc_help_options_init(&options);
  options.include_factory = true;
  HELP_ASSERT_STATUS(BSC_STATUS_INVALID_DESCRIPTOR, bsc_help_validate(table, help_command_count, &options, &error));
  HELP_ASSERT_TRUE(error.reason == BSC_HELP_ERROR_MISSING_SUMMARY);
  return 0;
}

static int test_help_lookup_behaviors(const char *test_name) {
  bsc_help_lookup_result_t result;
  bsc_help_options_t options;
  bsc_string_view_t status_token[] = {bsc_string_view_from_cstr("STATUS")};
  bsc_string_view_t settings_token[] = {bsc_string_view_from_cstr("settings")};
  bsc_string_view_t wifi_tokens[] = {bsc_string_view_from_cstr("settings"), bsc_string_view_from_cstr("wifi")};
  bsc_string_view_t command_tokens[] = {bsc_string_view_from_cstr("settings"), bsc_string_view_from_cstr("wifi"),
                                        bsc_string_view_from_cstr("set"), bsc_string_view_from_cstr("ssid")};
  bsc_string_view_t unknown[] = {bsc_string_view_from_cstr("settings"), bsc_string_view_from_cstr("bad")};
  bsc_string_view_t factory[] = {bsc_string_view_from_cstr("factory")};
  bsc_string_view_t locked[] = {bsc_string_view_from_cstr("locked")};
  bsc_string_view_t hidden[] = {bsc_string_view_from_cstr("hidden")};
  bsc_string_view_t hidden_factory[] = {bsc_string_view_from_cstr("hidden"), bsc_string_view_from_cstr("factory")};
  bsc_help_lookup_result_clear(&result);
  HELP_ASSERT_STATUS(BSC_STATUS_OK, bsc_help_find_path(help_commands, help_command_count, status_token, 1u, NULL, &result));
  HELP_ASSERT_TRUE(result.command == &help_commands[0]);
  HELP_ASSERT_STATUS(BSC_STATUS_OK, bsc_help_find_path(help_commands, help_command_count, settings_token, 1u, NULL, &result));
  HELP_ASSERT_TRUE(result.command == &help_commands[1]);
  HELP_ASSERT_STATUS(BSC_STATUS_OK, bsc_help_find_path(help_commands, help_command_count, wifi_tokens, 2u, NULL, &result));
  HELP_ASSERT_TRUE(result.command == &help_commands[2]);
  HELP_ASSERT_STATUS(BSC_STATUS_OK, bsc_help_find_path(help_commands, help_command_count, command_tokens, 4u, NULL, &result));
  HELP_ASSERT_TRUE(result.command == &help_commands[5]);
  HELP_ASSERT_STATUS(BSC_STATUS_NO_INPUT, bsc_help_find_path(help_commands, help_command_count, command_tokens, 0u, NULL, &result));
  HELP_ASSERT_STATUS(BSC_STATUS_UNKNOWN_COMMAND, bsc_help_find_path(help_commands, help_command_count, unknown, 2u, NULL, &result));
  HELP_ASSERT_STATUS(BSC_STATUS_UNKNOWN_COMMAND, bsc_help_find_path(help_commands, help_command_count, factory, 1u, NULL, &result));
  HELP_ASSERT_STATUS(BSC_STATUS_UNKNOWN_COMMAND, bsc_help_find_path(help_commands, help_command_count, locked, 1u, NULL, &result));
  HELP_ASSERT_STATUS(BSC_STATUS_UNKNOWN_COMMAND, bsc_help_find_path(help_commands, help_command_count, hidden, 1u, NULL, &result));
  bsc_help_options_init(&options);
  options.include_advanced = false;
  status_token[0] = bsc_string_view_from_cstr("mode");
  HELP_ASSERT_STATUS(BSC_STATUS_UNKNOWN_COMMAND, bsc_help_find_path(help_commands, help_command_count, status_token, 1u, &options, &result));
  bsc_help_options_init(&options);
  options.include_factory = true;
  HELP_ASSERT_STATUS(BSC_STATUS_OK, bsc_help_find_path(help_commands, help_command_count, factory, 1u, &options, &result));
  bsc_help_options_init(&options);
  options.include_locked = true;
  HELP_ASSERT_STATUS(BSC_STATUS_OK, bsc_help_find_path(help_commands, help_command_count, locked, 1u, &options, &result));
  bsc_help_options_init(&options);
  options.include_hidden = true;
  HELP_ASSERT_STATUS(BSC_STATUS_OK, bsc_help_find_path(help_commands, help_command_count, hidden, 1u, &options, &result));
  HELP_ASSERT_STATUS(BSC_STATUS_UNKNOWN_COMMAND, bsc_help_find_path(help_commands, help_command_count, hidden_factory, 2u, &options, &result));
  options.include_factory = true;
  HELP_ASSERT_STATUS(BSC_STATUS_OK, bsc_help_find_path(help_commands, help_command_count, hidden_factory, 2u, &options, &result));
  HELP_ASSERT_TRUE(g_handler_calls == 0 && g_access_calls == 0);
  return 0;
}

static int test_help_output_failures(const char *test_name) {
  help_capture_t capture;
  bsc_output_t output;
  bsc_string_view_t tokens[] = {bsc_string_view_from_cstr("settings"), bsc_string_view_from_cstr("wifi"),
                                bsc_string_view_from_cstr("set"), bsc_string_view_from_cstr("ssid")};
  size_t calls;
  HELP_ASSERT_STATUS(BSC_STATUS_INTERNAL_ERROR, bsc_help_render_index(help_commands, help_command_count, NULL, NULL));
  output.write = NULL;
  output.user = NULL;
  HELP_ASSERT_STATUS(BSC_STATUS_INTERNAL_ERROR, bsc_help_render_commands(help_commands, help_command_count, NULL, &output));
  help_capture_init(&capture, 0u);
  output.write = help_capture_write;
  output.user = &capture;
  HELP_ASSERT_STATUS(BSC_STATUS_OUTPUT_TRUNCATED, bsc_help_render_index(help_commands, help_command_count, NULL, &output));
  calls = capture.calls;
  HELP_ASSERT_TRUE(calls == 1u);
  help_capture_init(&capture, 5u);
  HELP_ASSERT_STATUS(BSC_STATUS_OUTPUT_TRUNCATED, bsc_help_render_path(help_commands, help_command_count, tokens, 4u, NULL, &output));
  calls = capture.calls;
  HELP_ASSERT_TRUE(calls == capture.calls);
  HELP_ASSERT_TRUE(capture.used == 5u);
  return 0;
}

static int test_help_invalid_metadata_emits_no_output(const char *test_name) {
  bsc_command_t table[sizeof(help_commands) / sizeof(help_commands[0])];
  help_capture_t capture;
  bsc_output_t output;
  copy_commands(table);
  table[0].summary = NULL;
  help_capture_init(&capture, sizeof(capture.buffer));
  output.write = help_capture_write;
  output.user = &capture;
  HELP_ASSERT_STATUS(BSC_STATUS_INVALID_DESCRIPTOR, bsc_help_render_index(table, help_command_count, NULL, &output));
  HELP_ASSERT_TRUE(capture.used == 0u);
  return 0;
}

static int test_help_maximum_bounds_and_reuse(const char *test_name) {
  help_capture_t first;
  help_capture_t second;
  bsc_output_t out1;
  bsc_output_t out2;
  bsc_help_validation_error_t error;
  HELP_ASSERT_TRUE(help_command_count <= (size_t)BSC_MAX_COMMANDS);
  HELP_ASSERT_TRUE(path_settings_wifi_set_ssid[3] != NULL);
#if BSC_ENABLE_FLOAT
  HELP_ASSERT_TRUE(wifi_args[6].type == BSC_ARG_FLOAT);
#endif
  HELP_ASSERT_TRUE(help_commands[4].path_len <= (size_t)BSC_MAX_PATH_TOKENS);
  HELP_ASSERT_TRUE(help_commands[4].arg_count <= (size_t)BSC_MAX_ARGS);
  HELP_ASSERT_TRUE(mode_choices[2].name != NULL && 3u <= (size_t)BSC_MAX_ENUM_CHOICES);
  HELP_ASSERT_STATUS(BSC_STATUS_OK, bsc_help_validate(help_commands, help_command_count, NULL, &error));
  help_capture_init(&first, sizeof(first.buffer));
  help_capture_init(&second, sizeof(second.buffer));
  out1.write = help_capture_write;
  out1.user = &first;
  out2.write = help_capture_write;
  out2.user = &second;
  HELP_ASSERT_STATUS(BSC_STATUS_OK, bsc_help_render_commands(help_commands, help_command_count, NULL, &out1));
  HELP_ASSERT_STATUS(BSC_STATUS_OK, bsc_help_render_commands(help_commands, help_command_count, NULL, &out2));
  HELP_ASSERT_TRUE(first.used == second.used);
  HELP_ASSERT_TRUE(memcmp(first.buffer, second.buffer, first.used) == 0);
  return 0;
}

static int test_help_secret_non_disclosure(const char *test_name) {
  help_capture_t capture;
  bsc_output_t output;
  bsc_string_view_t tokens[] = {bsc_string_view_from_cstr("settings"), bsc_string_view_from_cstr("wifi"),
                                bsc_string_view_from_cstr("set"), bsc_string_view_from_cstr("ssid")};
  const char *runtime_secret = "runtime-secret-sentinel";
  help_capture_init(&capture, sizeof(capture.buffer));
  output.write = help_capture_write;
  output.user = &capture;
  HELP_ASSERT_STATUS(BSC_STATUS_OK, bsc_help_render_path(help_commands, help_command_count, tokens, 4u, NULL, &output));
  HELP_ASSERT_TRUE(help_bytes_find(capture.buffer, capture.used, runtime_secret, strlen(runtime_secret)) == 0);
  HELP_ASSERT_TRUE(help_bytes_find(capture.buffer, capture.used, "secret, 8..64 bytes", strlen("secret, 8..64 bytes")) == 1);
  return 0;
}

int bsc_run_help_tests(void) {
  int failures = 0;
  HELP_RUN_TEST(test_help_options_defaults);
  HELP_RUN_TEST(test_help_validation_successes);
  HELP_RUN_TEST(test_help_validation_required_and_optional_text);
  HELP_RUN_TEST(test_help_validation_control_and_bounds);
  HELP_RUN_TEST(test_help_validation_registry_and_parent_groups);
  HELP_RUN_TEST(test_help_validation_filtered_metadata);
  HELP_RUN_TEST(test_help_lookup_behaviors);
  HELP_RUN_TEST(test_help_golden_outputs);
  HELP_RUN_TEST(test_help_output_failures);
  HELP_RUN_TEST(test_help_invalid_metadata_emits_no_output);
  HELP_RUN_TEST(test_help_maximum_bounds_and_reuse);
  HELP_RUN_TEST(test_help_secret_non_disclosure);
  return failures;
}
