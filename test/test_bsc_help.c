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


typedef struct help_fail_sink {
  char buffer[8192];
  size_t used;
  size_t calls;
  size_t fail_call;
  size_t calls_after_failure;
  size_t first_failure_call;
} help_fail_sink_t;

/** @brief Output sink that can short-write one selected callback and record post-failure calls. */
static size_t help_fail_write(void *user, const char *data, size_t length) {
  help_fail_sink_t *sink = (help_fail_sink_t *)user;
  size_t accepted = length;
  sink->calls += 1u;
  if (sink->first_failure_call != 0u) {
    sink->calls_after_failure += 1u;
  }
  if (sink->calls == sink->fail_call && length != 0u) {
    accepted = length - 1u;
    sink->first_failure_call = sink->calls;
  }
  if (accepted > 0u && data != NULL) {
    memcpy(&sink->buffer[sink->used], data, accepted);
    sink->used += accepted;
  }
  return accepted;
}

/** @brief Initialize deterministic short-write fixture storage. */
static void help_fail_sink_init(help_fail_sink_t *sink, size_t fail_call) {
  memset(sink, 0, sizeof(*sink));
  sink->fail_call = fail_call;
}

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
    {"off", 0, "Off"},
    {"sta", 1, "Sta"},
    {"ap", 2, NULL},
};

static bsc_arg_def_t wifi_args[] = {
    {"ssid", BSC_ARG_STRING, 0, 0, 0u, 0u, 0.0f, 0.0f, 1u, 32u, NULL, 0u, "SSID"},
    {"password", BSC_ARG_SECRET, 0, 0, 0u, 0u, 0.0f, 0.0f, 8u, 64u, NULL, 0u, "Pass"},
    {"channel", BSC_ARG_UINT, 0, 0, 1u, 11u, 0.0f, 0.0f, 0u, 0u, NULL, 0u, "Channel"},
    {"power", BSC_ARG_INT, -40, 20, 0u, 0u, 0.0f, 0.0f, 0u, 0u, NULL, 0u, "Power"},
    {"enabled", BSC_ARG_BOOL, 0, 0, 0u, 0u, 0.0f, 0.0f, 0u, 0u, NULL, 0u, "Enable"},
    {"mode", BSC_ARG_ENUM, 0, 0, 0u, 0u, 0.0f, 0.0f, 0u, 0u, mode_choices, 3u, "Mode"},
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
     BSC_COMMAND_FLAG_NONE, help_forbidden_access, "Status", "Status."},
    {path_settings, 1u, BSC_NODE_GROUP, NULL, 0u, NULL, NULL, BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_NONE,
     help_forbidden_access, "Settings", "Settings"},
    {path_settings_wifi, 2u, BSC_NODE_GROUP, NULL, 0u, NULL, NULL, BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_NONE,
     help_forbidden_access, "WiFi", NULL},
    {path_settings_wifi_scan, 3u, BSC_NODE_COMMAND, NULL, 0u, help_forbidden_handler, NULL,
     BSC_ACCESS_FACTORY, BSC_COMMAND_FLAG_NONE, help_forbidden_access, "Scan", "Scan."},
    {path_settings_wifi_set, 3u, BSC_NODE_GROUP, NULL, 0u, NULL, NULL, BSC_ACCESS_NORMAL,
     BSC_COMMAND_FLAG_NONE, help_forbidden_access, "Set", NULL},
    {path_settings_wifi_set_ssid, 4u, BSC_NODE_COMMAND, wifi_args, WIFI_ARG_COUNT, help_forbidden_handler, NULL,
     BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_NONE, help_forbidden_access, "SSID",
     "WiFi set"},
    {path_mode, 1u, BSC_NODE_COMMAND, NULL, 0u, help_forbidden_handler, NULL, BSC_ACCESS_ADVANCED,
     BSC_COMMAND_FLAG_NONE, help_forbidden_access, "Mode", "Mode."},
    {path_factory, 1u, BSC_NODE_COMMAND, NULL, 0u, help_forbidden_handler, NULL, BSC_ACCESS_FACTORY,
     BSC_COMMAND_FLAG_NONE, help_forbidden_access, "Factory", "Factory."},
    {path_locked, 1u, BSC_NODE_COMMAND, NULL, 0u, help_forbidden_handler, NULL, BSC_ACCESS_LOCKED,
     BSC_COMMAND_FLAG_NONE, help_forbidden_access, "Locked", "Locked."},
    {path_hidden, 1u, BSC_NODE_GROUP, NULL, 0u, NULL, NULL, BSC_ACCESS_NORMAL,
     BSC_COMMAND_FLAG_HIDDEN, help_forbidden_access, "Hidden", NULL},
    {path_hidden_factory, 2u, BSC_NODE_COMMAND, NULL, 0u, help_forbidden_handler, NULL, BSC_ACCESS_FACTORY,
     BSC_COMMAND_FLAG_HIDDEN, help_forbidden_access, "HidFact", "HidFact."},
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
    HELP_ASSERT_TRUE(help_bytes_find(capture.buffer, capture.used, "VALID VALUES", strlen("VALID VALUES")) == 1);
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
  wifi_args[0].help = "SSID";
  copy_commands(table);
  mode_choices[0].help = "";
  HELP_ASSERT_STATUS(BSC_STATUS_INVALID_DESCRIPTOR, bsc_help_validate(table, help_command_count, NULL, &error));
  HELP_ASSERT_TRUE(error.reason == BSC_HELP_ERROR_EMPTY_ENUM_CHOICE_HELP);
  mode_choices[0].help = "Off";
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
  table[0].summary = "\xc3\xa9";
  HELP_ASSERT_STATUS(BSC_STATUS_OK, bsc_help_validate(table, help_command_count, NULL, &error));
  return 0;
}

static void fill_help_text(char *text, size_t length, char value) {
  size_t index;
  for (index = 0u; index < length; ++index) {
    text[index] = value;
  }
  text[length] = '\0';
}

static void poison_help_error(bsc_help_validation_error_t *error) {
  error->reason = BSC_HELP_ERROR_EMPTY_SUMMARY;
  error->command_index = 77u;
  error->path_token_index = 78u;
  error->arg_index = 79u;
  error->enum_choice_index = 80u;
  error->required_parent_depth = 81u;
  error->registry_error.reason = BSC_REGISTRY_ERROR_EMPTY_PATH;
  error->registry_error.command_index = 82u;
  error->registry_error.path_token_index = 83u;
  error->registry_error.arg_index = 84u;
  error->registry_error.enum_choice_index = 85u;
  error->registry_error.duplicate_command_index = 86u;
}

static int assert_help_error_cleared(const char *test_name, const bsc_help_validation_error_t *error) {
  HELP_ASSERT_TRUE(error->reason == BSC_HELP_ERROR_NONE);
  HELP_ASSERT_TRUE(error->command_index == 0u);
  HELP_ASSERT_TRUE(error->path_token_index == 0u);
  HELP_ASSERT_TRUE(error->arg_index == 0u);
  HELP_ASSERT_TRUE(error->enum_choice_index == 0u);
  HELP_ASSERT_TRUE(error->required_parent_depth == 0u);
  HELP_ASSERT_TRUE(error->registry_error.reason == BSC_REGISTRY_ERROR_NONE);
  HELP_ASSERT_TRUE(error->registry_error.command_index == 0u);
  HELP_ASSERT_TRUE(error->registry_error.path_token_index == 0u);
  HELP_ASSERT_TRUE(error->registry_error.arg_index == 0u);
  HELP_ASSERT_TRUE(error->registry_error.enum_choice_index == 0u);
  HELP_ASSERT_TRUE(error->registry_error.duplicate_command_index == 0u);
  return 0;
}

static int test_help_validation_all_prose_bounds(const char *test_name) {
  static char max_text[BSC_MAX_HELP_TEXT_LEN + 1u];
  static char too_long[BSC_MAX_HELP_TEXT_LEN + 2u];
  bsc_command_t table[sizeof(help_commands) / sizeof(help_commands[0])];
  bsc_arg_def_t args[WIFI_ARG_COUNT];
  bsc_enum_choice_t choices[3];
  bsc_help_validation_error_t error;
  fill_help_text(max_text, (size_t)BSC_MAX_HELP_TEXT_LEN, 'm');
  fill_help_text(too_long, (size_t)BSC_MAX_HELP_TEXT_LEN + 1u, 'x');

  copy_commands(table);
  table[0].description = max_text;
  HELP_ASSERT_STATUS(BSC_STATUS_OK, bsc_help_validate(table, help_command_count, NULL, &error));
  copy_commands(table);
  table[0].description = too_long;
  HELP_ASSERT_STATUS(BSC_STATUS_INVALID_DESCRIPTOR, bsc_help_validate(table, help_command_count, NULL, &error));
  HELP_ASSERT_TRUE(error.reason == BSC_HELP_ERROR_DESCRIPTION_TOO_LONG);
  HELP_ASSERT_TRUE(error.command_index == 0u && error.path_token_index == 0u);
  HELP_ASSERT_TRUE(error.arg_index == 0u && error.enum_choice_index == 0u && error.required_parent_depth == 0u);

  copy_commands(table);
  memcpy(args, wifi_args, sizeof(args));
  table[5].args = args;
  args[0].help = max_text;
  HELP_ASSERT_STATUS(BSC_STATUS_OK, bsc_help_validate(table, help_command_count, NULL, &error));
  args[0].help = too_long;
  HELP_ASSERT_STATUS(BSC_STATUS_INVALID_DESCRIPTOR, bsc_help_validate(table, help_command_count, NULL, &error));
  HELP_ASSERT_TRUE(error.reason == BSC_HELP_ERROR_ARGUMENT_HELP_TOO_LONG);
  HELP_ASSERT_TRUE(error.command_index == 5u && error.arg_index == 0u);
  HELP_ASSERT_TRUE(error.path_token_index == 0u && error.enum_choice_index == 0u && error.required_parent_depth == 0u);

  copy_commands(table);
  memcpy(args, wifi_args, sizeof(args));
  memcpy(choices, mode_choices, sizeof(choices));
  args[5].enum_choices = choices;
  table[5].args = args;
  choices[0].help = max_text;
  HELP_ASSERT_STATUS(BSC_STATUS_OK, bsc_help_validate(table, help_command_count, NULL, &error));
  choices[0].help = too_long;
  HELP_ASSERT_STATUS(BSC_STATUS_INVALID_DESCRIPTOR, bsc_help_validate(table, help_command_count, NULL, &error));
  HELP_ASSERT_TRUE(error.reason == BSC_HELP_ERROR_ENUM_CHOICE_HELP_TOO_LONG);
  HELP_ASSERT_TRUE(error.command_index == 5u && error.arg_index == 5u && error.enum_choice_index == 0u);
  HELP_ASSERT_TRUE(error.path_token_index == 0u && error.required_parent_depth == 0u);
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

static int assert_rendered_text(const char *test_name,
                                bsc_status_t status,
                                const help_capture_t *capture,
                                const char *expected) {
  size_t expected_len = strlen(expected);
  HELP_ASSERT_STATUS(BSC_STATUS_OK, status);
  HELP_ASSERT_TRUE(capture->used == expected_len);
  HELP_ASSERT_TRUE(memcmp(capture->buffer, expected, expected_len) == 0);
  return 0;
}

static bsc_status_t render_index_with_options(const bsc_help_options_t *options, help_capture_t *capture) {
  bsc_output_t output = {help_capture_write, capture};
  help_capture_init(capture, sizeof(capture->buffer));
  return bsc_help_render_index(help_commands, help_command_count, options, &output);
}

static bsc_status_t render_commands_with_options(const bsc_help_options_t *options, help_capture_t *capture) {
  bsc_output_t output = {help_capture_write, capture};
  help_capture_init(capture, sizeof(capture->buffer));
  return bsc_help_render_commands(help_commands, help_command_count, options, &output);
}

static int test_help_rendered_visibility_options(const char *test_name) {
  static const char default_index[] = "COMMANDS\n  status - Status\n  settings - Settings\n  mode - Mode\n";
  static const char no_advanced_index[] = "COMMANDS\n  status - Status\n  settings - Settings\n";
  static const char factory_index[] =
      "COMMANDS\n  status - Status\n  settings - Settings\n  mode - Mode\n  factory - Factory\n";
  static const char locked_index[] =
      "COMMANDS\n  status - Status\n  settings - Settings\n  mode - Mode\n  locked - Locked\n";
  static const char hidden_index[] =
      "COMMANDS\n  status - Status\n  settings - Settings\n  mode - Mode\n  hidden - Hidden\n";
  static const char hidden_factory_index[] =
      "COMMANDS\n  status - Status\n  settings - Settings\n  mode - Mode\n  factory - Factory\n"
      "  hidden - Hidden\n";
  static const char default_commands[] =
      "COMMANDS\n  status - Status\n  settings wifi set ssid - SSID\n  mode - Mode\n";
  static const char no_advanced_commands[] =
      "COMMANDS\n  status - Status\n  settings wifi set ssid - SSID\n";
  static const char factory_commands[] =
      "COMMANDS\n  status - Status\n  settings wifi scan - Scan\n  settings wifi set ssid - SSID\n"
      "  mode - Mode\n  factory - Factory\n";
  static const char locked_commands[] =
      "COMMANDS\n  status - Status\n  settings wifi set ssid - SSID\n  mode - Mode\n  locked - Locked\n";
  static const char hidden_commands[] =
      "COMMANDS\n  status - Status\n  settings wifi set ssid - SSID\n  mode - Mode\n";
  static const char hidden_factory_commands[] =
      "COMMANDS\n  status - Status\n  settings wifi scan - Scan\n  settings wifi set ssid - SSID\n"
      "  mode - Mode\n  factory - Factory\n  hidden factory - HidFact\n";
  help_capture_t capture;
  bsc_help_options_t options;

  g_handler_calls = 0;
  g_access_calls = 0;
  HELP_ASSERT_TRUE(assert_rendered_text(test_name, render_index_with_options(NULL, &capture), &capture, default_index) == 0);
  HELP_ASSERT_TRUE(assert_rendered_text(test_name, render_commands_with_options(NULL, &capture), &capture, default_commands) == 0);

  bsc_help_options_init(&options);
  options.include_advanced = false;
  HELP_ASSERT_TRUE(assert_rendered_text(test_name, render_index_with_options(&options, &capture), &capture, no_advanced_index) == 0);
  HELP_ASSERT_TRUE(assert_rendered_text(test_name, render_commands_with_options(&options, &capture), &capture, no_advanced_commands) == 0);

  bsc_help_options_init(&options);
  options.include_factory = true;
  HELP_ASSERT_TRUE(assert_rendered_text(test_name, render_index_with_options(&options, &capture), &capture, factory_index) == 0);
  HELP_ASSERT_TRUE(assert_rendered_text(test_name, render_commands_with_options(&options, &capture), &capture, factory_commands) == 0);

  bsc_help_options_init(&options);
  options.include_locked = true;
  HELP_ASSERT_TRUE(assert_rendered_text(test_name, render_index_with_options(&options, &capture), &capture, locked_index) == 0);
  HELP_ASSERT_TRUE(assert_rendered_text(test_name, render_commands_with_options(&options, &capture), &capture, locked_commands) == 0);

  bsc_help_options_init(&options);
  options.include_hidden = true;
  HELP_ASSERT_TRUE(assert_rendered_text(test_name, render_index_with_options(&options, &capture), &capture, hidden_index) == 0);
  HELP_ASSERT_TRUE(assert_rendered_text(test_name, render_commands_with_options(&options, &capture), &capture, hidden_commands) == 0);

  options.include_factory = true;
  HELP_ASSERT_TRUE(assert_rendered_text(test_name, render_index_with_options(&options, &capture), &capture, hidden_factory_index) == 0);
  HELP_ASSERT_TRUE(assert_rendered_text(test_name, render_commands_with_options(&options, &capture), &capture, hidden_factory_commands) == 0);

  HELP_ASSERT_TRUE(g_handler_calls == 0);
  HELP_ASSERT_TRUE(g_access_calls == 0);
  return 0;
}

static void poison_lookup_result(bsc_help_lookup_result_t *result) {
  result->command = &help_commands[0];
  result->command_index = 99u;
}

static int assert_lookup_cleared(const char *test_name, const bsc_help_lookup_result_t *result) {
  HELP_ASSERT_TRUE(result->command == NULL);
  HELP_ASSERT_TRUE(result->command_index == 0u);
  return 0;
}

static int test_help_lookup_result_clearing_failures(const char *test_name) {
  bsc_command_t table[sizeof(help_commands) / sizeof(help_commands[0])];
  bsc_help_lookup_result_t result;
  bsc_string_view_t status_token[] = {bsc_string_view_from_cstr("status")};
  bsc_string_view_t unknown[] = {bsc_string_view_from_cstr("unknown")};
  bsc_string_view_t factory[] = {bsc_string_view_from_cstr("factory")};

  poison_lookup_result(&result);
  HELP_ASSERT_STATUS(BSC_STATUS_NO_INPUT, bsc_help_find_path(help_commands, help_command_count, status_token, 0u, NULL, &result));
  HELP_ASSERT_TRUE(assert_lookup_cleared(test_name, &result) == 0);

  poison_lookup_result(&result);
  HELP_ASSERT_STATUS(BSC_STATUS_INTERNAL_ERROR, bsc_help_find_path(help_commands, help_command_count, NULL, 1u, NULL, &result));
  HELP_ASSERT_TRUE(assert_lookup_cleared(test_name, &result) == 0);

  poison_lookup_result(&result);
  HELP_ASSERT_STATUS(BSC_STATUS_UNKNOWN_COMMAND, bsc_help_find_path(help_commands, help_command_count, unknown, 1u, NULL, &result));
  HELP_ASSERT_TRUE(assert_lookup_cleared(test_name, &result) == 0);

  poison_lookup_result(&result);
  HELP_ASSERT_STATUS(BSC_STATUS_UNKNOWN_COMMAND, bsc_help_find_path(help_commands, help_command_count, factory, 1u, NULL, &result));
  HELP_ASSERT_TRUE(assert_lookup_cleared(test_name, &result) == 0);

  copy_commands(table);
  table[0].path = NULL;
  poison_lookup_result(&result);
  HELP_ASSERT_STATUS(BSC_STATUS_INVALID_DESCRIPTOR, bsc_help_find_path(table, help_command_count, status_token, 1u, NULL, &result));
  HELP_ASSERT_TRUE(assert_lookup_cleared(test_name, &result) == 0);

  HELP_ASSERT_STATUS(BSC_STATUS_OK, bsc_help_find_path(help_commands, help_command_count, status_token, 1u, NULL, &result));
  HELP_ASSERT_TRUE(result.command == &help_commands[0]);
  HELP_ASSERT_TRUE(result.command_index == 0u);
  return 0;
}

static int test_help_validation_diagnostic_clearing(const char *test_name) {
  bsc_command_t table[sizeof(help_commands) / sizeof(help_commands[0])];
  bsc_help_validation_error_t error;

  poison_help_error(&error);
  HELP_ASSERT_STATUS(BSC_STATUS_OK, bsc_help_validate(help_commands, help_command_count, NULL, &error));
  HELP_ASSERT_TRUE(assert_help_error_cleared(test_name, &error) == 0);

  copy_commands(table);
  table[0].summary = "";
  poison_help_error(&error);
  HELP_ASSERT_STATUS(BSC_STATUS_INVALID_DESCRIPTOR, bsc_help_validate(table, help_command_count, NULL, &error));
  HELP_ASSERT_TRUE(error.reason == BSC_HELP_ERROR_EMPTY_SUMMARY);
  HELP_ASSERT_TRUE(error.command_index == 0u);
  HELP_ASSERT_TRUE(error.path_token_index == 0u);
  HELP_ASSERT_TRUE(error.arg_index == 0u);
  HELP_ASSERT_TRUE(error.enum_choice_index == 0u);
  HELP_ASSERT_TRUE(error.required_parent_depth == 0u);
  HELP_ASSERT_TRUE(error.registry_error.reason == BSC_REGISTRY_ERROR_NONE);
  HELP_ASSERT_TRUE(error.registry_error.command_index == 0u);
  HELP_ASSERT_TRUE(error.registry_error.path_token_index == 0u);
  HELP_ASSERT_TRUE(error.registry_error.arg_index == 0u);
  HELP_ASSERT_TRUE(error.registry_error.enum_choice_index == 0u);
  HELP_ASSERT_TRUE(error.registry_error.duplicate_command_index == 0u);

  copy_commands(table);
  table[0].path = NULL;
  poison_help_error(&error);
  HELP_ASSERT_STATUS(BSC_STATUS_INVALID_DESCRIPTOR, bsc_help_validate(table, help_command_count, NULL, &error));
  HELP_ASSERT_TRUE(error.reason == BSC_HELP_ERROR_REGISTRY_INVALID);
  HELP_ASSERT_TRUE(error.command_index == 0u);
  HELP_ASSERT_TRUE(error.path_token_index == 0u);
  HELP_ASSERT_TRUE(error.arg_index == 0u);
  HELP_ASSERT_TRUE(error.enum_choice_index == 0u);
  HELP_ASSERT_TRUE(error.required_parent_depth == 0u);
  HELP_ASSERT_TRUE(error.registry_error.reason == BSC_REGISTRY_ERROR_NULL_PATH);
  HELP_ASSERT_TRUE(error.registry_error.command_index == 0u);
  HELP_ASSERT_TRUE(error.registry_error.path_token_index == 0u);
  HELP_ASSERT_TRUE(error.registry_error.arg_index == 0u);
  HELP_ASSERT_TRUE(error.registry_error.enum_choice_index == 0u);
  HELP_ASSERT_TRUE(error.registry_error.duplicate_command_index == 0u);
  return 0;
}

static int test_help_invalid_api_inputs(const char *test_name) {
  bsc_command_t table[sizeof(help_commands) / sizeof(help_commands[0])];
  help_capture_t capture;
  bsc_output_t output;
  bsc_string_view_t status_token[] = {bsc_string_view_from_cstr("status")};
  bsc_string_view_t unknown[] = {bsc_string_view_from_cstr("unknown")};
  bsc_string_view_t factory[] = {bsc_string_view_from_cstr("factory")};

  HELP_ASSERT_STATUS(BSC_STATUS_INVALID_DESCRIPTOR, bsc_help_validate(NULL, 1u, NULL, NULL));
  HELP_ASSERT_STATUS(BSC_STATUS_INVALID_DESCRIPTOR,
                     bsc_help_validate(help_commands, (size_t)BSC_MAX_COMMANDS + 1u, NULL, NULL));
  HELP_ASSERT_STATUS(BSC_STATUS_INTERNAL_ERROR,
                     bsc_help_find_path(help_commands, help_command_count, status_token, 1u, NULL, NULL));
  HELP_ASSERT_STATUS(BSC_STATUS_INTERNAL_ERROR,
                     bsc_help_find_path(help_commands, help_command_count, NULL, 1u, NULL,
                                        &(bsc_help_lookup_result_t){&help_commands[0], 1u}));
  HELP_ASSERT_STATUS(BSC_STATUS_NO_INPUT,
                     bsc_help_find_path(help_commands, help_command_count, status_token, 0u, NULL,
                                        &(bsc_help_lookup_result_t){&help_commands[0], 1u}));

  HELP_ASSERT_STATUS(BSC_STATUS_INTERNAL_ERROR, bsc_help_render_index(help_commands, help_command_count, NULL, NULL));
  output.write = NULL;
  output.user = NULL;
  HELP_ASSERT_STATUS(BSC_STATUS_INTERNAL_ERROR, bsc_help_render_commands(help_commands, help_command_count, NULL, &output));

  copy_commands(table);
  table[0].summary = NULL;
  help_capture_init(&capture, sizeof(capture.buffer));
  output.write = help_capture_write;
  output.user = &capture;
  HELP_ASSERT_STATUS(BSC_STATUS_INVALID_DESCRIPTOR, bsc_help_render_commands(table, help_command_count, NULL, &output));
  HELP_ASSERT_TRUE(capture.calls == 0u && capture.used == 0u);

  help_capture_init(&capture, sizeof(capture.buffer));
  HELP_ASSERT_STATUS(BSC_STATUS_UNKNOWN_COMMAND,
                     bsc_help_render_path(help_commands, help_command_count, unknown, 1u, NULL, &output));
  HELP_ASSERT_TRUE(capture.calls == 0u && capture.used == 0u);

  help_capture_init(&capture, sizeof(capture.buffer));
  HELP_ASSERT_STATUS(BSC_STATUS_UNKNOWN_COMMAND,
                     bsc_help_render_path(help_commands, help_command_count, factory, 1u, NULL, &output));
  HELP_ASSERT_TRUE(capture.calls == 0u && capture.used == 0u);
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

typedef struct help_expected {
  char buffer[8192];
  size_t used;
  int overflow;
} help_expected_t;

static void help_expected_init(help_expected_t *expected) {
  expected->used = 0u;
  expected->overflow = 0;
}

static void help_expected_append(help_expected_t *expected, const char *text) {
  size_t length = strlen(text);
  if (length > sizeof(expected->buffer) - expected->used) {
    expected->overflow = 1;
    return;
  }
  memcpy(&expected->buffer[expected->used], text, length);
  expected->used += length;
}

static int help_expect_capture_equals(const char *test_name,
                                      const help_capture_t *capture,
                                      const help_expected_t *expected) {
  HELP_ASSERT_TRUE(!expected->overflow);
  HELP_ASSERT_TRUE(capture->used == expected->used);
  HELP_ASSERT_TRUE(expected->used > 0u);
  HELP_ASSERT_TRUE(expected->buffer[expected->used - 1u] == '\n');
  HELP_ASSERT_TRUE(memcmp(capture->buffer, expected->buffer, expected->used) == 0);
  return 0;
}

static void help_fill_indexed_name(char *name, char prefix, size_t index) {
  name[0] = prefix;
  name[1] = (char)('0' + ((index / 10u) % 10u));
  name[2] = (char)('0' + (index % 10u));
  name[3] = '\0';
}

static int test_help_maximum_command_count_rendering(const char *test_name) {
  static char command_names[BSC_MAX_COMMANDS][8];
  static char summaries[BSC_MAX_COMMANDS][8];
  static char descriptions[BSC_MAX_COMMANDS][8];
  static const char *command_paths[BSC_MAX_COMMANDS][1];
  static bsc_command_t commands[BSC_MAX_COMMANDS];
  help_expected_t expected;
  help_capture_t first;
  help_capture_t second;
  bsc_output_t output;
  bsc_help_validation_error_t error;
  size_t index;

  help_expected_init(&expected);
  help_expected_append(&expected, "COMMANDS\n");
  for (index = 0u; index < (size_t)BSC_MAX_COMMANDS; ++index) {
    help_fill_indexed_name(command_names[index], 'c', index);
    help_fill_indexed_name(summaries[index], 'S', index);
    help_fill_indexed_name(descriptions[index], 'D', index);
    command_paths[index][0] = command_names[index];
    commands[index] = (bsc_command_t){command_paths[index], 1u, BSC_NODE_COMMAND, NULL, 0u,
                                      help_forbidden_handler, NULL, BSC_ACCESS_NORMAL,
                                      BSC_COMMAND_FLAG_NONE, help_forbidden_access,
                                      summaries[index], descriptions[index]};
    help_expected_append(&expected, "  ");
    help_expected_append(&expected, command_names[index]);
    help_expected_append(&expected, " - ");
    help_expected_append(&expected, summaries[index]);
    help_expected_append(&expected, "\n");
  }

  g_handler_calls = 0;
  g_access_calls = 0;
  HELP_ASSERT_STATUS(BSC_STATUS_OK, bsc_help_validate(commands, (size_t)BSC_MAX_COMMANDS, NULL, &error));
  help_capture_init(&first, sizeof(first.buffer));
  output.write = help_capture_write;
  output.user = &first;
  HELP_ASSERT_STATUS(BSC_STATUS_OK, bsc_help_render_commands(commands, (size_t)BSC_MAX_COMMANDS, NULL, &output));
  HELP_ASSERT_TRUE(help_expect_capture_equals(test_name, &first, &expected) == 0);
  help_capture_init(&second, sizeof(second.buffer));
  output.user = &second;
  HELP_ASSERT_STATUS(BSC_STATUS_OK, bsc_help_render_commands(commands, (size_t)BSC_MAX_COMMANDS, NULL, &output));
  HELP_ASSERT_TRUE(second.used == first.used);
  HELP_ASSERT_TRUE(memcmp(second.buffer, first.buffer, first.used) == 0);
  HELP_ASSERT_TRUE(g_handler_calls == 0 && g_access_calls == 0);
  return 0;
}

static void help_expected_append_path(help_expected_t *expected, char names[BSC_MAX_PATH_TOKENS][8], size_t count) {
  size_t index;
  for (index = 0u; index < count; ++index) {
    if (index != 0u) help_expected_append(expected, " ");
    help_expected_append(expected, names[index]);
  }
}

static int test_help_maximum_path_depth_rendering(const char *test_name) {
  static char names[BSC_MAX_PATH_TOKENS][8];
  static const char *paths[BSC_MAX_PATH_TOKENS][BSC_MAX_PATH_TOKENS];
  static bsc_command_t commands[BSC_MAX_PATH_TOKENS];
  bsc_string_view_t tokens[BSC_MAX_PATH_TOKENS];
  help_expected_t expected;
  help_capture_t capture;
  bsc_output_t output;
  bsc_help_lookup_result_t result;
  bsc_help_validation_error_t error;
  size_t index;

  for (index = 0u; index < (size_t)BSC_MAX_PATH_TOKENS; ++index) {
    size_t depth;
    help_fill_indexed_name(names[index], 'p', index);
    tokens[index] = bsc_string_view_from_cstr(names[index]);
    for (depth = 0u; depth <= index; ++depth) {
      paths[index][depth] = names[depth];
    }
    commands[index] = (bsc_command_t){paths[index], index + 1u,
                                      index + 1u == (size_t)BSC_MAX_PATH_TOKENS ? BSC_NODE_COMMAND : BSC_NODE_GROUP,
                                      NULL, 0u,
                                      index + 1u == (size_t)BSC_MAX_PATH_TOKENS ? help_forbidden_handler : NULL,
                                      NULL, BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_NONE, help_forbidden_access,
                                      "Deep", index + 1u == (size_t)BSC_MAX_PATH_TOKENS ? "Deep." : NULL};
  }
  help_expected_init(&expected);
  help_expected_append(&expected, "NAME\n  ");
  help_expected_append_path(&expected, names, (size_t)BSC_MAX_PATH_TOKENS);
  help_expected_append(&expected, " - Deep\n\nSYNOPSIS\n  ");
  help_expected_append_path(&expected, names, (size_t)BSC_MAX_PATH_TOKENS);
  help_expected_append(&expected, "\n\nDESCRIPTION\n  Deep.\n");

  HELP_ASSERT_STATUS(BSC_STATUS_OK, bsc_help_validate(commands, (size_t)BSC_MAX_PATH_TOKENS, NULL, &error));
  HELP_ASSERT_STATUS(BSC_STATUS_OK, bsc_help_find_path(commands, (size_t)BSC_MAX_PATH_TOKENS,
                                                       tokens, (size_t)BSC_MAX_PATH_TOKENS, NULL, &result));
  HELP_ASSERT_TRUE(result.command == &commands[BSC_MAX_PATH_TOKENS - 1u]);
  HELP_ASSERT_TRUE(result.command_index == (size_t)BSC_MAX_PATH_TOKENS - 1u);
  help_capture_init(&capture, sizeof(capture.buffer));
  output.write = help_capture_write;
  output.user = &capture;
  HELP_ASSERT_STATUS(BSC_STATUS_OK, bsc_help_render_path(commands, (size_t)BSC_MAX_PATH_TOKENS,
                                                        tokens, (size_t)BSC_MAX_PATH_TOKENS, NULL, &output));
  HELP_ASSERT_TRUE(help_expect_capture_equals(test_name, &capture, &expected) == 0);
  HELP_ASSERT_TRUE(g_handler_calls == 0 && g_access_calls == 0);
  return 0;
}

static int test_help_maximum_argument_count_rendering(const char *test_name) {
  static const char *const path[] = {"argmax"};
  static char arg_names[BSC_MAX_ARGS][8];
  static char arg_help[BSC_MAX_ARGS][8];
  static bsc_arg_def_t args[BSC_MAX_ARGS];
  static bsc_command_t command[1];
  help_expected_t expected;
  help_capture_t capture;
  bsc_output_t output;
  bsc_string_view_t token[] = {bsc_string_view_from_cstr("argmax")};
  bsc_help_validation_error_t error;
  size_t index;

  help_expected_init(&expected);
  command[0] = (bsc_command_t){path, 1u, BSC_NODE_COMMAND, args, (size_t)BSC_MAX_ARGS,
                               help_forbidden_handler, NULL, BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_NONE,
                               help_forbidden_access, "Args", "Args."};
  help_expected_append(&expected, "NAME\n  argmax - Args\n\nSYNOPSIS\n  argmax");
  for (index = 0u; index < (size_t)BSC_MAX_ARGS; ++index) {
    help_fill_indexed_name(arg_names[index], 'a', index);
    help_fill_indexed_name(arg_help[index], 'h', index);
    args[index] = (bsc_arg_def_t){arg_names[index], BSC_ARG_UINT, 0, 0, 0u, 9u, 0.0f, 0.0f,
                                  0u, 0u, NULL, 0u, arg_help[index]};
    help_expected_append(&expected, " <");
    help_expected_append(&expected, arg_names[index]);
    help_expected_append(&expected, ">");
  }
  help_expected_append(&expected, "\n\nDESCRIPTION\n  Args.\n\nARGUMENTS\n");
  for (index = 0u; index < (size_t)BSC_MAX_ARGS; ++index) {
    help_expected_append(&expected, "  ");
    help_expected_append(&expected, arg_names[index]);
    help_expected_append(&expected, " - ");
    help_expected_append(&expected, arg_help[index]);
    help_expected_append(&expected, "\n");
  }
  help_expected_append(&expected, "\nVALID VALUES\n");
  for (index = 0u; index < (size_t)BSC_MAX_ARGS; ++index) {
    help_expected_append(&expected, "  ");
    help_expected_append(&expected, arg_names[index]);
    help_expected_append(&expected, ": unsigned integer, 0..9\n");
  }

  HELP_ASSERT_STATUS(BSC_STATUS_OK, bsc_help_validate(command, 1u, NULL, &error));
  help_capture_init(&capture, sizeof(capture.buffer));
  output.write = help_capture_write;
  output.user = &capture;
  HELP_ASSERT_STATUS(BSC_STATUS_OK, bsc_help_render_path(command, 1u, token, 1u, NULL, &output));
  HELP_ASSERT_TRUE(help_expect_capture_equals(test_name, &capture, &expected) == 0);
  HELP_ASSERT_TRUE(g_handler_calls == 0 && g_access_calls == 0);
  return 0;
}

static int test_help_maximum_enum_choice_rendering(const char *test_name) {
  static const char *const path[] = {"enummax"};
  static char choice_names[BSC_MAX_ENUM_CHOICES][8];
  static char choice_help[BSC_MAX_ENUM_CHOICES][8];
  static bsc_enum_choice_t choices[BSC_MAX_ENUM_CHOICES];
  static bsc_arg_def_t arg[] = {{"mode", BSC_ARG_ENUM, 0, 0, 0u, 0u, 0.0f, 0.0f,
                                0u, 0u, choices, BSC_MAX_ENUM_CHOICES, NULL}};
  static bsc_command_t command[] = {{path, 1u, BSC_NODE_COMMAND, arg, 1u, help_forbidden_handler, NULL,
                                    BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_NONE, help_forbidden_access,
                                    "Enum", "Enum."}};
  help_expected_t expected;
  help_capture_t capture;
  bsc_output_t output;
  bsc_string_view_t token[] = {bsc_string_view_from_cstr("enummax")};
  bsc_help_validation_error_t error;
  size_t index;

  help_expected_init(&expected);
  help_expected_append(&expected, "NAME\n  enummax - Enum\n\nSYNOPSIS\n  enummax <mode>\n\n"
                                  "DESCRIPTION\n  Enum.\n\nARGUMENTS\n  mode\n\nVALID VALUES\n  mode: ");
  for (index = 0u; index < (size_t)BSC_MAX_ENUM_CHOICES; ++index) {
    help_fill_indexed_name(choice_names[index], 'e', index);
    help_fill_indexed_name(choice_help[index], 'h', index);
    choices[index] = (bsc_enum_choice_t){choice_names[index], (int32_t)index, choice_help[index]};
    if (index != 0u) help_expected_append(&expected, " | ");
    help_expected_append(&expected, choice_names[index]);
  }
  help_expected_append(&expected, "\n");
  for (index = 0u; index < (size_t)BSC_MAX_ENUM_CHOICES; ++index) {
    help_expected_append(&expected, "    ");
    help_expected_append(&expected, choice_names[index]);
    help_expected_append(&expected, " - ");
    help_expected_append(&expected, choice_help[index]);
    help_expected_append(&expected, "\n");
  }

  HELP_ASSERT_STATUS(BSC_STATUS_OK, bsc_help_validate(command, 1u, NULL, &error));
  help_capture_init(&capture, sizeof(capture.buffer));
  output.write = help_capture_write;
  output.user = &capture;
  HELP_ASSERT_STATUS(BSC_STATUS_OK, bsc_help_render_path(command, 1u, token, 1u, NULL, &output));
  HELP_ASSERT_TRUE(help_expect_capture_equals(test_name, &capture, &expected) == 0);
  HELP_ASSERT_TRUE(g_handler_calls == 0 && g_access_calls == 0);
  return 0;
}

static int test_help_maximum_immediate_children_rendering(const char *test_name) {
  static const char *const root_path[] = {"root"};
  static char child_names[BSC_MAX_COMMANDS - 1u][8];
  static char summaries[BSC_MAX_COMMANDS - 1u][8];
  static char descriptions[BSC_MAX_COMMANDS - 1u][8];
  static const char *child_paths[BSC_MAX_COMMANDS - 1u][2];
  static bsc_command_t commands[BSC_MAX_COMMANDS];
  help_expected_t expected;
  help_capture_t capture;
  bsc_output_t output;
  bsc_string_view_t token[] = {bsc_string_view_from_cstr("root")};
  bsc_help_lookup_result_t result;
  bsc_help_validation_error_t error;
  size_t index;

  commands[0] = (bsc_command_t){root_path, 1u, BSC_NODE_GROUP, NULL, 0u, NULL, NULL,
                                BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_NONE, help_forbidden_access,
                                "Root", NULL};
  help_expected_init(&expected);
  help_expected_append(&expected, "NAME\n  root - Root\n\nCOMMANDS\n");
  for (index = 0u; index < (size_t)BSC_MAX_COMMANDS - 1u; ++index) {
    help_fill_indexed_name(child_names[index], 'c', index);
    help_fill_indexed_name(summaries[index], 'S', index);
    help_fill_indexed_name(descriptions[index], 'D', index);
    child_paths[index][0] = "root";
    child_paths[index][1] = child_names[index];
    commands[index + 1u] = (bsc_command_t){child_paths[index], 2u, BSC_NODE_COMMAND, NULL, 0u,
                                           help_forbidden_handler, NULL, BSC_ACCESS_NORMAL,
                                           BSC_COMMAND_FLAG_NONE, help_forbidden_access,
                                           summaries[index], descriptions[index]};
    help_expected_append(&expected, "  root ");
    help_expected_append(&expected, child_names[index]);
    help_expected_append(&expected, " - ");
    help_expected_append(&expected, summaries[index]);
    help_expected_append(&expected, "\n");
  }

  HELP_ASSERT_STATUS(BSC_STATUS_OK, bsc_help_validate(commands, (size_t)BSC_MAX_COMMANDS, NULL, &error));
  HELP_ASSERT_STATUS(BSC_STATUS_OK, bsc_help_find_path(commands, (size_t)BSC_MAX_COMMANDS, token, 1u, NULL, &result));
  HELP_ASSERT_TRUE(result.command == &commands[0] && result.command_index == 0u);
  help_capture_init(&capture, sizeof(capture.buffer));
  output.write = help_capture_write;
  output.user = &capture;
  HELP_ASSERT_STATUS(BSC_STATUS_OK, bsc_help_render_path(commands, (size_t)BSC_MAX_COMMANDS, token, 1u, NULL, &output));
  HELP_ASSERT_TRUE(help_expect_capture_equals(test_name, &capture, &expected) == 0);
  HELP_ASSERT_TRUE(g_handler_calls == 0 && g_access_calls == 0);
  return 0;
}

static int test_help_maximum_identifier_lengths_rendering(const char *test_name) {
  static char path_token[BSC_MAX_TOKEN_LEN + 1u];
  static char arg_name[BSC_MAX_TOKEN_LEN + 1u];
  static char choice_name[BSC_MAX_TOKEN_LEN + 1u];
  static const char *path[] = {path_token};
  static bsc_enum_choice_t choices[] = {{choice_name, 1, "Choice"}};
  static bsc_arg_def_t arg[] = {{arg_name, BSC_ARG_ENUM, 0, 0, 0u, 0u, 0.0f, 0.0f,
                                0u, 0u, choices, 1u, "Arg"}};
  static bsc_command_t command[] = {{path, 1u, BSC_NODE_COMMAND, arg, 1u, help_forbidden_handler, NULL,
                                    BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_NONE, help_forbidden_access,
                                    "Ident", "Ident."}};
  help_expected_t expected;
  help_capture_t capture;
  bsc_output_t output;
  bsc_string_view_t token;
  bsc_help_lookup_result_t result;
  bsc_help_validation_error_t error;
  size_t index;

  for (index = 0u; index < (size_t)BSC_MAX_TOKEN_LEN; ++index) {
    path_token[index] = 'p';
    arg_name[index] = 'a';
    choice_name[index] = 'e';
  }
  path_token[BSC_MAX_TOKEN_LEN] = '\0';
  arg_name[BSC_MAX_TOKEN_LEN] = '\0';
  choice_name[BSC_MAX_TOKEN_LEN] = '\0';
  token.data = path_token;
  token.length = (size_t)BSC_MAX_TOKEN_LEN;

  help_expected_init(&expected);
  help_expected_append(&expected, "NAME\n  ");
  help_expected_append(&expected, path_token);
  help_expected_append(&expected, " - Ident\n\nSYNOPSIS\n  ");
  help_expected_append(&expected, path_token);
  help_expected_append(&expected, " <");
  help_expected_append(&expected, arg_name);
  help_expected_append(&expected, ">\n\nDESCRIPTION\n  Ident.\n\nARGUMENTS\n  ");
  help_expected_append(&expected, arg_name);
  help_expected_append(&expected, " - Arg\n\nVALID VALUES\n  ");
  help_expected_append(&expected, arg_name);
  help_expected_append(&expected, ": ");
  help_expected_append(&expected, choice_name);
  help_expected_append(&expected, "\n    ");
  help_expected_append(&expected, choice_name);
  help_expected_append(&expected, " - Choice\n");

  HELP_ASSERT_STATUS(BSC_STATUS_OK, bsc_help_validate(command, 1u, NULL, &error));
  HELP_ASSERT_STATUS(BSC_STATUS_OK, bsc_help_find_path(command, 1u, &token, 1u, NULL, &result));
  HELP_ASSERT_TRUE(result.command == &command[0] && result.command_index == 0u);
  help_capture_init(&capture, sizeof(capture.buffer));
  output.write = help_capture_write;
  output.user = &capture;
  HELP_ASSERT_STATUS(BSC_STATUS_OK, bsc_help_render_path(command, 1u, &token, 1u, NULL, &output));
  HELP_ASSERT_TRUE(help_expect_capture_equals(test_name, &capture, &expected) == 0);
  HELP_ASSERT_TRUE(g_handler_calls == 0 && g_access_calls == 0);
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


/** @brief Render with each callback short-written and verify exact prefix/no continuation. */
static int help_assert_all_boundaries(const char *test_name, bsc_status_t (*render)(help_capture_t *capture)) {
  help_capture_t success;
  size_t fail_call;
  help_capture_init(&success, sizeof(success.buffer));
  HELP_ASSERT_STATUS(BSC_STATUS_OK, render(&success));
  for (fail_call = 1u; fail_call <= success.calls; ++fail_call) {
    help_fail_sink_t sink;
    bsc_output_t output;
    bsc_status_t status;
    bsc_string_view_t tokens[] = {bsc_string_view_from_cstr("settings"), bsc_string_view_from_cstr("wifi"),
                                  bsc_string_view_from_cstr("set"), bsc_string_view_from_cstr("ssid")};
    help_fail_sink_init(&sink, fail_call);
    output.write = help_fail_write;
    output.user = &sink;
    if (render == render_index_capture) {
      status = bsc_help_render_index(help_commands, help_command_count, NULL, &output);
    } else if (render == render_commands_capture) {
      status = bsc_help_render_commands(help_commands, help_command_count, NULL, &output);
    } else if (render == render_settings_capture) {
      bsc_string_view_t group_token[] = {bsc_string_view_from_cstr("settings")};
      status = bsc_help_render_path(help_commands, help_command_count, group_token, 1u, NULL, &output);
    } else {
      status = bsc_help_render_path(help_commands, help_command_count, tokens, 4u, NULL, &output);
    }
    HELP_ASSERT_STATUS(BSC_STATUS_OUTPUT_TRUNCATED, status);
    HELP_ASSERT_TRUE(sink.first_failure_call == fail_call);
    HELP_ASSERT_TRUE(sink.calls_after_failure == 0u);
    HELP_ASSERT_TRUE(sink.used < success.used);
    HELP_ASSERT_TRUE(memcmp(sink.buffer, success.buffer, sink.used) == 0);
  }
  return 0;
}

static int test_help_short_write_every_boundary(const char *test_name) {
  HELP_ASSERT_TRUE(help_assert_all_boundaries(test_name, render_index_capture) == 0);
  HELP_ASSERT_TRUE(help_assert_all_boundaries(test_name, render_commands_capture) == 0);
  HELP_ASSERT_TRUE(help_assert_all_boundaries(test_name, render_settings_capture) == 0);
  HELP_ASSERT_TRUE(help_assert_all_boundaries(test_name, render_wifi_set_capture) == 0);
  return 0;
}

/** @brief Verify control-byte rejection for all identifier categories and non-ASCII acceptance. */
static int test_help_identifier_control_validation(const char *test_name) {
  static const char *const bad_path_cr[] = {"bad\rpath"};
  static const char *const bad_path_lf[] = {"bad\npath"};
  static const char *const bad_path_ctl[] = {"bad\x01path"};
  static const char *const bad_path_del[] = {"bad\x7fpath"};
  static const char *const good_path_utf8[] = {"caf\xc3\xa9"};
  static bsc_enum_choice_t enum_bad[] = {{"bad", 1, NULL}};
  static bsc_arg_def_t enum_arg[] = {{"mode", BSC_ARG_ENUM, 0, 0, 0u, 0u, 0.0f, 0.0f, 0u, 0u, enum_bad, 1u, NULL}};
  static bsc_arg_def_t bad_arg[] = {{"bad", BSC_ARG_STRING, 0, 0, 0u, 0u, 0.0f, 0.0f, 0u, 8u, NULL, 0u, NULL}};
  bsc_command_t command = {bad_path_cr, 1u, BSC_NODE_COMMAND, NULL, 0u, help_forbidden_handler, NULL,
                           BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_NONE, NULL, "Sum", "Desc"};
  bsc_help_validation_error_t error;
  help_capture_t capture;
  bsc_output_t output;
  const char *bad_arg_names[] = {"bad\rarg", "bad\narg", "bad" "\x01" "arg", "bad" "\x7f" "arg", "arg\xc3\xa9"};
  const char *bad_choice_names[] = {"bad\rchoice", "bad\nchoice", "bad" "\x01" "choice", "bad" "\x7f" "choice", "choice\xc3\xa9"};
  size_t index;

  HELP_ASSERT_STATUS(BSC_STATUS_INVALID_DESCRIPTOR, bsc_help_validate(&command, 1u, NULL, &error));
  HELP_ASSERT_TRUE(error.reason == BSC_HELP_ERROR_INVALID_PATH_TOKEN_CONTROL_BYTE);
  HELP_ASSERT_TRUE(error.command_index == 0u && error.path_token_index == 0u && error.arg_index == 0u && error.enum_choice_index == 0u);
  command.path = bad_path_lf;
  HELP_ASSERT_STATUS(BSC_STATUS_INVALID_DESCRIPTOR, bsc_help_validate(&command, 1u, NULL, &error));
  command.path = bad_path_ctl;
  HELP_ASSERT_STATUS(BSC_STATUS_INVALID_DESCRIPTOR, bsc_help_validate(&command, 1u, NULL, &error));
  command.path = bad_path_del;
  HELP_ASSERT_STATUS(BSC_STATUS_INVALID_DESCRIPTOR, bsc_help_validate(&command, 1u, NULL, &error));
  command.path = good_path_utf8;
  HELP_ASSERT_STATUS(BSC_STATUS_OK, bsc_help_validate(&command, 1u, NULL, &error));

  command.path = path_status;
  command.args = bad_arg;
  command.arg_count = 1u;
  for (index = 0u; index < 5u; ++index) {
    bad_arg[0].name = bad_arg_names[index];
    if (index == 4u) {
      HELP_ASSERT_STATUS(BSC_STATUS_OK, bsc_help_validate(&command, 1u, NULL, &error));
    } else {
      HELP_ASSERT_STATUS(BSC_STATUS_INVALID_DESCRIPTOR, bsc_help_validate(&command, 1u, NULL, &error));
      HELP_ASSERT_TRUE(error.reason == BSC_HELP_ERROR_INVALID_ARGUMENT_NAME_CONTROL_BYTE);
      HELP_ASSERT_TRUE(error.arg_index == 0u && error.path_token_index == 0u && error.enum_choice_index == 0u);
    }
  }

  command.args = enum_arg;
  for (index = 0u; index < 5u; ++index) {
    enum_bad[0].name = bad_choice_names[index];
    if (index == 4u) {
      HELP_ASSERT_STATUS(BSC_STATUS_OK, bsc_help_validate(&command, 1u, NULL, &error));
    } else {
      HELP_ASSERT_STATUS(BSC_STATUS_INVALID_DESCRIPTOR, bsc_help_validate(&command, 1u, NULL, &error));
      HELP_ASSERT_TRUE(error.reason == BSC_HELP_ERROR_INVALID_ENUM_CHOICE_NAME_CONTROL_BYTE);
      HELP_ASSERT_TRUE(error.arg_index == 0u && error.enum_choice_index == 0u && error.path_token_index == 0u);
    }
  }

  command.path = bad_path_lf;
  command.args = NULL;
  command.arg_count = 0u;
  help_capture_init(&capture, sizeof(capture.buffer));
  output.write = help_capture_write;
  output.user = &capture;
  HELP_ASSERT_STATUS(BSC_STATUS_INVALID_DESCRIPTOR, bsc_help_render_index(&command, 1u, NULL, &output));
  HELP_ASSERT_TRUE(capture.calls == 0u && capture.used == 0u);
  return 0;
}

/** @brief Verify a small prose bound does not truncate identifiers or section headings. */
static int test_help_small_prose_limit_identifier_regression(const char *test_name) {
  static const char *const path[] = {"longcommand"};
  static bsc_enum_choice_t choices[] = {{"longchoice", 1, NULL}};
  static bsc_arg_def_t args[] = {{"longargument", BSC_ARG_ENUM, 0, 0, 0u, 0u, 0.0f, 0.0f,
                                  0u, 0u, choices, 1u, NULL}};
  static bsc_command_t command = {path, 1u, BSC_NODE_COMMAND, args, 1u, help_forbidden_handler, NULL,
                                  BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_NONE, NULL, "Summary", "Details"};
  static const char expected[] =
      "NAME\n"
      "  longcommand - Summary\n"
      "\n"
      "SYNOPSIS\n"
      "  longcommand <longargument>\n"
      "\n"
      "DESCRIPTION\n"
      "  Details\n"
      "\n"
      "ARGUMENTS\n"
      "  longargument\n"
      "\n"
      "VALID VALUES\n"
      "  longargument: longchoice\n";
  help_capture_t capture;
  bsc_output_t output;
  bsc_string_view_t tokens[] = {bsc_string_view_from_cstr("longcommand")};
  size_t expected_len = strlen(expected);
  help_capture_init(&capture, sizeof(capture.buffer));
  output.write = help_capture_write;
  output.user = &capture;
  HELP_ASSERT_STATUS(BSC_STATUS_OK, bsc_help_render_path(&command, 1u, tokens, 1u, NULL, &output));
  HELP_ASSERT_TRUE(capture.used == expected_len);
  HELP_ASSERT_TRUE(memcmp(capture.buffer, expected, expected_len) == 0);
  return 0;
}


#if BSC_ENABLE_FLOAT
#if BSC_MAX_FLOAT_FRACTION_DIGITS == 1u
#define HELP_FLOAT_HALF 0.25f
#define HELP_FLOAT_MAXDIGIT 1.1f
#define HELP_FLOAT_EXPECTED \
  "NAME\n  floatfmt - Float\n\nSYNOPSIS\n  floatfmt <zero> <integers> <one> <trim> <half> <limit> <maxdigit>\n\nDESCRIPTION\n  Float\n\nARGUMENTS\n  zero\n  integers\n  one\n  trim\n  half\n  limit\n  maxdigit\n\nVALID VALUES\n  zero: decimal, 0..0\n  integers: decimal, -2..2\n  one: decimal, 1.5..1.5\n  trim: decimal, 1.3..1.3\n  half: decimal, -0.3..0.3\n  limit: decimal, -1000000000..1000000000\n  maxdigit: decimal, 1.1..1.1\n"
#elif BSC_MAX_FLOAT_FRACTION_DIGITS == 2u
#define HELP_FLOAT_HALF 0.125f
#define HELP_FLOAT_MAXDIGIT 1.01f
#define HELP_FLOAT_EXPECTED \
  "NAME\n  floatfmt - Float\n\nSYNOPSIS\n  floatfmt <zero> <integers> <one> <trim> <half> <limit> <maxdigit>\n\nDESCRIPTION\n  Float\n\nARGUMENTS\n  zero\n  integers\n  one\n  trim\n  half\n  limit\n  maxdigit\n\nVALID VALUES\n  zero: decimal, 0..0\n  integers: decimal, -2..2\n  one: decimal, 1.5..1.5\n  trim: decimal, 1.25..1.25\n  half: decimal, -0.13..0.13\n  limit: decimal, -1000000000..1000000000\n  maxdigit: decimal, 1.01..1.01\n"
#elif BSC_MAX_FLOAT_FRACTION_DIGITS == 3u
#define HELP_FLOAT_HALF 0.0625f
#define HELP_FLOAT_MAXDIGIT 1.001f
#define HELP_FLOAT_EXPECTED \
  "NAME\n  floatfmt - Float\n\nSYNOPSIS\n  floatfmt <zero> <integers> <one> <trim> <half> <limit> <maxdigit>\n\nDESCRIPTION\n  Float\n\nARGUMENTS\n  zero\n  integers\n  one\n  trim\n  half\n  limit\n  maxdigit\n\nVALID VALUES\n  zero: decimal, 0..0\n  integers: decimal, -2..2\n  one: decimal, 1.5..1.5\n  trim: decimal, 1.25..1.25\n  half: decimal, -0.063..0.063\n  limit: decimal, -1000000000..1000000000\n  maxdigit: decimal, 1.001..1.001\n"
#elif BSC_MAX_FLOAT_FRACTION_DIGITS == 4u
#define HELP_FLOAT_HALF 0.03125f
#define HELP_FLOAT_MAXDIGIT 1.0001f
#define HELP_FLOAT_EXPECTED \
  "NAME\n  floatfmt - Float\n\nSYNOPSIS\n  floatfmt <zero> <integers> <one> <trim> <half> <limit> <maxdigit>\n\nDESCRIPTION\n  Float\n\nARGUMENTS\n  zero\n  integers\n  one\n  trim\n  half\n  limit\n  maxdigit\n\nVALID VALUES\n  zero: decimal, 0..0\n  integers: decimal, -2..2\n  one: decimal, 1.5..1.5\n  trim: decimal, 1.25..1.25\n  half: decimal, -0.0313..0.0313\n  limit: decimal, -1000000000..1000000000\n  maxdigit: decimal, 1.0001..1.0001\n"
#elif BSC_MAX_FLOAT_FRACTION_DIGITS == 5u
#define HELP_FLOAT_HALF 0.015625f
#define HELP_FLOAT_MAXDIGIT 1.00001f
#define HELP_FLOAT_EXPECTED \
  "NAME\n  floatfmt - Float\n\nSYNOPSIS\n  floatfmt <zero> <integers> <one> <trim> <half> <limit> <maxdigit>\n\nDESCRIPTION\n  Float\n\nARGUMENTS\n  zero\n  integers\n  one\n  trim\n  half\n  limit\n  maxdigit\n\nVALID VALUES\n  zero: decimal, 0..0\n  integers: decimal, -2..2\n  one: decimal, 1.5..1.5\n  trim: decimal, 1.25..1.25\n  half: decimal, -0.01563..0.01563\n  limit: decimal, -1000000000..1000000000\n  maxdigit: decimal, 1.00001..1.00001\n"
#else
#define HELP_FLOAT_HALF 0.0078125f
#define HELP_FLOAT_MAXDIGIT 1.000001f
#define HELP_FLOAT_EXPECTED \
  "NAME\n  floatfmt - Float\n\nSYNOPSIS\n  floatfmt <zero> <integers> <one> <trim> <half> <limit> <maxdigit>\n\nDESCRIPTION\n  Float\n\nARGUMENTS\n  zero\n  integers\n  one\n  trim\n  half\n  limit\n  maxdigit\n\nVALID VALUES\n  zero: decimal, 0..0\n  integers: decimal, -2..2\n  one: decimal, 1.5..1.5\n  trim: decimal, 1.25..1.25\n  half: decimal, -0.007813..0.007813\n  limit: decimal, -1000000000..1000000000\n  maxdigit: decimal, 1.000001..1.000001\n"
#endif

static int test_help_float_precision_exact_output(const char *test_name) {
  static const char *const path[] = {"floatfmt"};
  static bsc_arg_def_t args[7];
  bsc_command_t command;
  help_capture_t capture;
  bsc_output_t output;
  bsc_string_view_t tokens[] = {bsc_string_view_from_cstr("floatfmt")};
  size_t expected_len = strlen(HELP_FLOAT_EXPECTED);
  args[0] = (bsc_arg_def_t){"zero", BSC_ARG_FLOAT, 0, 0, 0u, 0u, -0.0f, 0.0f, 0u, 0u, NULL, 0u, NULL};
  args[1] = (bsc_arg_def_t){"integers", BSC_ARG_FLOAT, 0, 0, 0u, 0u, -2.0f, 2.0f, 0u, 0u, NULL, 0u, NULL};
  args[2] = (bsc_arg_def_t){"one", BSC_ARG_FLOAT, 0, 0, 0u, 0u, 1.5f, 1.5f, 0u, 0u, NULL, 0u, NULL};
  args[3] = (bsc_arg_def_t){"trim", BSC_ARG_FLOAT, 0, 0, 0u, 0u, 1.25f, 1.25f, 0u, 0u, NULL, 0u, NULL};
  args[4] = (bsc_arg_def_t){"half", BSC_ARG_FLOAT, 0, 0, 0u, 0u, -HELP_FLOAT_HALF, HELP_FLOAT_HALF, 0u, 0u, NULL, 0u, NULL};
  args[5] = (bsc_arg_def_t){"limit", BSC_ARG_FLOAT, 0, 0, 0u, 0u, -(float)BSC_COMPACT_FLOAT_MAX_MAGNITUDE, (float)BSC_COMPACT_FLOAT_MAX_MAGNITUDE, 0u, 0u, NULL, 0u, NULL};
  args[6] = (bsc_arg_def_t){"maxdigit", BSC_ARG_FLOAT, 0, 0, 0u, 0u, HELP_FLOAT_MAXDIGIT, HELP_FLOAT_MAXDIGIT, 0u, 0u, NULL, 0u, NULL};
  command = (bsc_command_t){path, 1u, BSC_NODE_COMMAND, args, 7u, help_forbidden_handler, NULL, BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_NONE, NULL, "Float", "Float"};
  help_capture_init(&capture, sizeof(capture.buffer));
  output.write = help_capture_write;
  output.user = &capture;
  HELP_ASSERT_STATUS(BSC_STATUS_OK, bsc_help_render_path(&command, 1u, tokens, 1u, NULL, &output));
  HELP_ASSERT_TRUE(capture.used == expected_len);
  HELP_ASSERT_TRUE(memcmp(capture.buffer, HELP_FLOAT_EXPECTED, expected_len) == 0);
  return 0;
}
#endif

int bsc_run_help_tests(void) {
  int failures = 0;
  HELP_RUN_TEST(test_help_options_defaults);
  HELP_RUN_TEST(test_help_validation_successes);
  HELP_RUN_TEST(test_help_validation_required_and_optional_text);
  HELP_RUN_TEST(test_help_validation_control_and_bounds);
  HELP_RUN_TEST(test_help_validation_all_prose_bounds);
  HELP_RUN_TEST(test_help_validation_registry_and_parent_groups);
  HELP_RUN_TEST(test_help_validation_filtered_metadata);
  HELP_RUN_TEST(test_help_lookup_behaviors);
  HELP_RUN_TEST(test_help_rendered_visibility_options);
  HELP_RUN_TEST(test_help_lookup_result_clearing_failures);
  HELP_RUN_TEST(test_help_validation_diagnostic_clearing);
  HELP_RUN_TEST(test_help_invalid_api_inputs);
  HELP_RUN_TEST(test_help_golden_outputs);
  HELP_RUN_TEST(test_help_output_failures);
  HELP_RUN_TEST(test_help_short_write_every_boundary);
  HELP_RUN_TEST(test_help_identifier_control_validation);
  HELP_RUN_TEST(test_help_small_prose_limit_identifier_regression);
  HELP_RUN_TEST(test_help_invalid_metadata_emits_no_output);
  HELP_RUN_TEST(test_help_maximum_command_count_rendering);
  HELP_RUN_TEST(test_help_maximum_path_depth_rendering);
  HELP_RUN_TEST(test_help_maximum_argument_count_rendering);
  HELP_RUN_TEST(test_help_maximum_enum_choice_rendering);
  HELP_RUN_TEST(test_help_maximum_immediate_children_rendering);
  HELP_RUN_TEST(test_help_maximum_identifier_lengths_rendering);
#if BSC_ENABLE_FLOAT
  HELP_RUN_TEST(test_help_float_precision_exact_output);
#endif
  HELP_RUN_TEST(test_help_secret_non_disclosure);
  return failures;
}
