#include "bsc_help.h"

#include "bsc_config.h"

#include <stdio.h>
#include <string.h>

#define EXT_ASSERT_TRUE(condition)                                                                \
  do {                                                                                            \
    if (!(condition)) {                                                                           \
      printf("FAIL: %s: %s:%d: %s\n", test_name, __FILE__, __LINE__, #condition);                \
      return 1;                                                                                   \
    }                                                                                             \
  } while (0)

#define EXT_ASSERT_STATUS(expected, actual) EXT_ASSERT_TRUE((expected) == (actual))

#ifndef BSC_TEST_GOLDEN_DIR
#define BSC_TEST_GOLDEN_DIR "test/golden"
#endif

#define EXT_RUN_TEST(fn)                                                                          \
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

static const char *const path_status[] = {"status"};
static const char *const path_wifi[] = {"wifi"};
static const char *const path_advanced[] = {"advanced"};
static const char *const path_factory[] = {"factory"};
static const char *const path_locked[] = {"locked"};
static const char *const path_hidden[] = {"hidden"};
static const char *const path_hidden_factory[] = {"hidden_factory"};
static int extended_handler_calls;
static int extended_access_calls;

/** @brief Handler sentinel that pure topic lookup must never invoke. */
static bsc_status_t extended_forbidden_handler(void *app_context,
                                               const bsc_command_t *command,
                                               const struct bsc_parsed_args *args,
                                               bsc_output_t *output) {
  (void)app_context;
  (void)command;
  (void)args;
  (void)output;
  extended_handler_calls += 1;
  return BSC_STATUS_APP_ERROR;
}

/** @brief Access sentinel that pure topic lookup must never invoke. */
static bool extended_forbidden_access(void *app_context,
                                      const bsc_command_t *command,
                                      bsc_access_level_t required_access) {
  (void)app_context;
  (void)command;
  (void)required_access;
  extended_access_calls += 1;
  return false;
}

static const bsc_command_t extended_commands[] = {
    {path_status, 1u, BSC_NODE_COMMAND, NULL, 0u, extended_forbidden_handler, NULL, BSC_ACCESS_NORMAL,
     BSC_COMMAND_FLAG_NONE, extended_forbidden_access, "Status", "Status."},
    {path_wifi, 1u, BSC_NODE_GROUP, NULL, 0u, NULL, NULL, BSC_ACCESS_NORMAL,
     BSC_COMMAND_FLAG_NONE, extended_forbidden_access, "WiFi", NULL},
    {path_advanced, 1u, BSC_NODE_COMMAND, NULL, 0u, extended_forbidden_handler, NULL, BSC_ACCESS_ADVANCED,
     BSC_COMMAND_FLAG_NONE, extended_forbidden_access, "Advanced", "Advanced."},
    {path_factory, 1u, BSC_NODE_COMMAND, NULL, 0u, extended_forbidden_handler, NULL, BSC_ACCESS_FACTORY,
     BSC_COMMAND_FLAG_NONE, extended_forbidden_access, "Factory", "Factory."},
    {path_locked, 1u, BSC_NODE_COMMAND, NULL, 0u, extended_forbidden_handler, NULL, BSC_ACCESS_LOCKED,
     BSC_COMMAND_FLAG_NONE, extended_forbidden_access, "Locked", "Locked."},
    {path_hidden, 1u, BSC_NODE_COMMAND, NULL, 0u, extended_forbidden_handler, NULL, BSC_ACCESS_NORMAL,
     BSC_COMMAND_FLAG_HIDDEN, extended_forbidden_access, "Hidden", "Hidden."},
    {path_hidden_factory, 1u, BSC_NODE_COMMAND, NULL, 0u, extended_forbidden_handler, NULL, BSC_ACCESS_FACTORY,
     BSC_COMMAND_FLAG_HIDDEN, extended_forbidden_access, "Hidden Factory", "Hidden factory."},
};

static const bsc_help_topic_t extended_topics[] = {
    {&extended_commands[0], "overview", "Status overview", NULL, {NULL, 0u}, {NULL, 0u}, NULL, 0u, NULL, 0u},
    {&extended_commands[1], "overview", "WiFi overview", NULL, {NULL, 0u}, {NULL, 0u}, NULL, 0u, NULL, 0u},
    {&extended_commands[0], "details", "Status details", NULL, {NULL, 0u}, {NULL, 0u}, NULL, 0u, NULL, 0u},
    {&extended_commands[2], "advanced", "Advanced topic", NULL, {NULL, 0u}, {NULL, 0u}, NULL, 0u, NULL, 0u},
    {&extended_commands[3], "factory", "Factory topic", NULL, {NULL, 0u}, {NULL, 0u}, NULL, 0u, NULL, 0u},
    {&extended_commands[4], "locked", "Locked topic", NULL, {NULL, 0u}, {NULL, 0u}, NULL, 0u, NULL, 0u},
    {&extended_commands[5], "hidden", "Hidden topic", NULL, {NULL, 0u}, {NULL, 0u}, NULL, 0u, NULL, 0u},
    {&extended_commands[6], "service", "Hidden factory topic", NULL, {NULL, 0u}, {NULL, 0u}, NULL, 0u, NULL, 0u},
};

/** @brief Return the shared valid pure-topic lookup catalog. */
static bsc_help_catalog_t extended_valid_catalog(void) {
  bsc_help_catalog_t catalog;
  catalog.commands = extended_commands;
  catalog.command_count = sizeof(extended_commands) / sizeof(extended_commands[0]);
  catalog.targets = NULL;
  catalog.target_count = 0u;
  catalog.topics = extended_topics;
  catalog.topic_count = sizeof(extended_topics) / sizeof(extended_topics[0]);
  return catalog;
}

/** @brief Return a one-token string-view path from a static C string. */
static bsc_string_view_t extended_token(const char *text) {
  return bsc_string_view_from_cstr(text);
}

/** @brief Assert that lookup failure clears all topic result fields. */
static int expect_cleared_failure(const char *test_name,
                                  bsc_status_t expected,
                                  const bsc_help_catalog_t *catalog,
                                  const bsc_string_view_t *path,
                                  size_t path_count,
                                  bsc_string_view_t topic_id,
                                  const bsc_help_options_t *options) {
  bsc_help_topic_lookup_result_t result;
  result.topic = &extended_topics[0];
  result.topic_index = 99u;
  result.parent_command_index = 77u;
  EXT_ASSERT_STATUS(expected, bsc_help_find_topic(catalog, path, path_count, topic_id, options, &result));
  EXT_ASSERT_TRUE(result.topic == NULL);
  EXT_ASSERT_TRUE(result.topic_index == 0u);
  EXT_ASSERT_TRUE(result.parent_command_index == 0u);
  return 0;
}



typedef struct ext_capture {
  char buffer[8192];
  size_t used;
  size_t calls;
} ext_capture_t;

typedef struct ext_fail_sink {
  size_t calls;
  size_t fail_call;
  size_t calls_after_failure;
  size_t first_failure_call;
} ext_fail_sink_t;

static const char *const ext2_path_status[] = {"status"};
static const char *const ext2_path_settings[] = {"settings"};
static const char *const ext2_path_settings_wifi[] = {"settings", "wifi"};
static const char *const ext2_path_settings_secret[] = {"settings", "secret"};
static const char *const ext2_path_settings_wifi_set[] = {"settings", "wifi", "set"};
static const char *const ext2_path_settings_wifi_set_ssid[] = {"settings", "wifi", "set", "ssid"};
static const char *const ext2_path_factory_reset[] = {"factory", "wifi", "reset"};

static bsc_enum_choice_t ext2_mode_choices[] = {
    {"off", 0, "Off"},
    {"sta", 1, "Sta"},
    {"ap", 2, NULL},
};

static bsc_arg_def_t ext2_wifi_args[] = {
    {"ssid", BSC_ARG_STRING, 0, 0, 0u, 0u, 0.0f, 0.0f, 1u, 32u, NULL, 0u, "SSID"},
    {"password", BSC_ARG_SECRET, 0, 0, 0u, 0u, 0.0f, 0.0f, 8u, 64u, NULL, 0u, "Pass"},
    {"channel", BSC_ARG_UINT, 0, 0, 1u, 11u, 0.0f, 0.0f, 0u, 0u, NULL, 0u, "Channel"},
    {"power", BSC_ARG_INT, -40, 20, 0u, 0u, 0.0f, 0.0f, 0u, 0u, NULL, 0u, "Power"},
    {"enabled", BSC_ARG_BOOL, 0, 0, 0u, 0u, 0.0f, 0.0f, 0u, 0u, NULL, 0u, "Enable"},
    {"mode", BSC_ARG_ENUM, 0, 0, 0u, 0u, 0.0f, 0.0f, 0u, 0u, ext2_mode_choices, 3u, "Mode"},
#if BSC_ENABLE_FLOAT
    {"ratio", BSC_ARG_FLOAT, 0, 0, 0u, 0u, -1.5f, 2.25f, 0u, 0u, NULL, 0u, NULL},
#endif
};

#if BSC_ENABLE_FLOAT
#define EXT2_WIFI_ARG_COUNT 7u
#else
#define EXT2_WIFI_ARG_COUNT 6u
#endif

static bsc_command_t ext2_commands[] = {
    {ext2_path_status, 1u, BSC_NODE_COMMAND, NULL, 0u, extended_forbidden_handler, NULL, BSC_ACCESS_NORMAL,
     BSC_COMMAND_FLAG_NONE, extended_forbidden_access, "Status", "Status."},
    {ext2_path_settings, 1u, BSC_NODE_GROUP, NULL, 0u, NULL, NULL, BSC_ACCESS_NORMAL,
     BSC_COMMAND_FLAG_NONE, extended_forbidden_access, "Settings", "Settings"},
    {ext2_path_settings_wifi, 2u, BSC_NODE_GROUP, NULL, 0u, NULL, NULL, BSC_ACCESS_NORMAL,
     BSC_COMMAND_FLAG_NONE, extended_forbidden_access, "WiFi", NULL},
    {ext2_path_settings_secret, 2u, BSC_NODE_COMMAND, NULL, 0u, extended_forbidden_handler, NULL, BSC_ACCESS_FACTORY,
     BSC_COMMAND_FLAG_NONE, extended_forbidden_access, "Secret", "Secret."},
    {ext2_path_settings_wifi_set, 3u, BSC_NODE_GROUP, NULL, 0u, NULL, NULL, BSC_ACCESS_NORMAL,
     BSC_COMMAND_FLAG_NONE, extended_forbidden_access, "Set", NULL},
    {ext2_path_settings_wifi_set_ssid, 4u, BSC_NODE_COMMAND, ext2_wifi_args, EXT2_WIFI_ARG_COUNT,
     extended_forbidden_handler, NULL, BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_NONE, extended_forbidden_access, "SSID",
     "WiFi set"},
    {ext2_path_factory_reset, 3u, BSC_NODE_COMMAND, NULL, 0u, extended_forbidden_handler, NULL, BSC_ACCESS_FACTORY,
     BSC_COMMAND_FLAG_NONE, extended_forbidden_access, "Factory WiFi reset", "Factory WiFi reset."},
};

static const char *const ext2_command_notes[] = {
    "Stored SSIDs are applied on the next WiFi reconnect.",
    "Use quoted strings when the SSID contains spaces.",
};
static const char *const ext2_command_warnings[] = {
    "Do not paste production passwords into shared logs.",
    "Changing WiFi credentials can disconnect the console session.",
};
static const bsc_help_example_t ext2_command_examples[] = {
    {"settings wifi set ssid \"Shop AP\" <new-password> 6 10 on sta 1.0",
     "Configure a WPA network using a placeholder password."},
    {"settings wifi set ssid Lab <secret> 1 0 on ap 0.5", NULL},
};
static const bsc_help_related_t ext2_command_related[] = {
    {&ext2_commands[2]},
    {&ext2_commands[6]},
    {&ext2_commands[0]},
};
static const char *const ext2_group_notes[] = {"Settings changes are stored by the application."};
static const char *const ext2_group_warnings[] = {"Some settings can affect active connections."};
static const char *const ext2_topic_notes[] = {
    "Secret argument values are runtime input and are never read by help rendering."};
static const char *const ext2_topic_warnings[] = {"Help examples must not contain production credentials."};
static const bsc_help_example_t ext2_topic_examples[] = {
    {"settings wifi set ssid \"Shop AP\" <new-password> 6 10 on sta 1.0",
     "Demonstrates quoting an SSID while redacting the secret."},
    {"settings wifi set ssid Lab <secret> 1 0 on ap 0.5", NULL},
};
static const bsc_help_related_t ext2_topic_related[] = {
    {&ext2_commands[5]},
    {&ext2_commands[6]},
    {&ext2_commands[0]},
};
static const bsc_help_target_t ext2_targets[] = {
    {&ext2_commands[5], {ext2_command_notes, 2u}, {ext2_command_warnings, 2u}, ext2_command_examples, 2u,
     ext2_command_related, 3u},
    {&ext2_commands[1], {ext2_group_notes, 1u}, {ext2_group_warnings, 1u}, NULL, 0u, NULL, 0u},
};
static const bsc_help_topic_t ext2_topics[] = {
    {&ext2_commands[5], "security", "Password handling and safe examples",
     "Use static example placeholders for secrets and avoid copying real credentials into logs.",
     {ext2_topic_notes, 1u}, {ext2_topic_warnings, 1u}, ext2_topic_examples, 2u, ext2_topic_related, 3u},
    {&ext2_commands[5], "reconnect", "Reconnect behavior after credential changes", NULL, {NULL, 0u}, {NULL, 0u},
     NULL, 0u, NULL, 0u},
    {&ext2_commands[1], "profiles", "How saved settings profiles are selected", NULL, {NULL, 0u}, {NULL, 0u}, NULL,
     0u, NULL, 0u},
};

/** @brief Return a valid rendering catalog with target metadata and topics. */
static bsc_help_catalog_t ext2_render_catalog(void) {
  bsc_help_catalog_t catalog;
  catalog.commands = ext2_commands;
  catalog.command_count = sizeof(ext2_commands) / sizeof(ext2_commands[0]);
  catalog.targets = ext2_targets;
  catalog.target_count = sizeof(ext2_targets) / sizeof(ext2_targets[0]);
  catalog.topics = ext2_topics;
  catalog.topic_count = sizeof(ext2_topics) / sizeof(ext2_topics[0]);
  return catalog;
}

/** @brief Initialize a capture sink for extended renderer tests. */
static void ext_capture_init(ext_capture_t *capture) {
  memset(capture->buffer, 0, sizeof(capture->buffer));
  capture->used = 0u;
  capture->calls = 0u;
}

/** @brief Capture output bytes for exact renderer tests. */
static size_t ext_capture_write(void *user, const char *data, size_t length) {
  ext_capture_t *capture = (ext_capture_t *)user;
  capture->calls += 1u;
  if (data != NULL && length > 0u) {
    memcpy(&capture->buffer[capture->used], data, length);
    capture->used += length;
  }
  return length;
}

/** @brief Simulate one selected short write and record any later callbacks. */
static size_t ext_fail_write(void *user, const char *data, size_t length) {
  ext_fail_sink_t *sink = (ext_fail_sink_t *)user;
  (void)data;
  sink->calls += 1u;
  if (sink->first_failure_call != 0u) {
    sink->calls_after_failure += 1u;
  }
  if (sink->calls == sink->fail_call && length != 0u) {
    sink->first_failure_call = sink->calls;
    return length - 1u;
  }
  return length;
}

/** @brief Load a golden fixture into caller-owned storage. */
static int ext_load_golden(const char *name, char *buffer, size_t capacity, size_t *length) {
  char path[256];
  FILE *file;
  size_t count;
  size_t dir_len = strlen(BSC_TEST_GOLDEN_DIR);
  size_t name_len = strlen(name);
  if (dir_len + 1u + name_len + 1u > sizeof(path)) return 0;
  memcpy(path, BSC_TEST_GOLDEN_DIR, dir_len);
  path[dir_len] = '/';
  memcpy(&path[dir_len + 1u], name, name_len + 1u);
  file = fopen(path, "rb");
  if (file == NULL) return 0;
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
static int ext_bytes_find(const char *haystack, size_t haystack_len, const char *needle, size_t needle_len) {
  size_t index;
  if (needle_len == 0u) return 1;
  if (haystack_len < needle_len) return 0;
  for (index = 0u; index <= haystack_len - needle_len; ++index) {
    if (memcmp(&haystack[index], needle, needle_len) == 0) return 1;
  }
  return 0;
}

/** @brief Verify final LF, no CR, and no trailing spaces in expected bytes. */
static int ext_assert_clean_bytes(const char *test_name, const char *buffer, size_t length) {
  size_t index;
  EXT_ASSERT_TRUE(length > 0u);
  EXT_ASSERT_TRUE(buffer[length - 1u] == '\n');
  for (index = 0u; index < length; ++index) {
    EXT_ASSERT_TRUE(buffer[index] != '\r');
    if (buffer[index] == '\n' && index > 0u) {
      EXT_ASSERT_TRUE(buffer[index - 1u] != ' ');
    }
  }
  return 0;
}

typedef bsc_status_t (*ext_render_fn_t)(bsc_output_t *output);

/** @brief Render command fixture with full metadata. */
static bsc_status_t ext_render_command_full(bsc_output_t *output) {
  bsc_help_catalog_t catalog = ext2_render_catalog();
  bsc_string_view_t path[] = {extended_token("settings"), extended_token("wifi"), extended_token("set"),
                              extended_token("ssid")};
  return bsc_help_render_catalog_path(&catalog, path, 4u, NULL, output);
}

/** @brief Render group fixture with metadata. */
static bsc_status_t ext_render_group_full(bsc_output_t *output) {
  bsc_help_catalog_t catalog = ext2_render_catalog();
  bsc_string_view_t path[] = {extended_token("settings")};
  return bsc_help_render_catalog_path(&catalog, path, 1u, NULL, output);
}

/** @brief Render topic fixture with metadata. */
static bsc_status_t ext_render_topic_full(bsc_output_t *output) {
  bsc_help_catalog_t catalog = ext2_render_catalog();
  bsc_string_view_t path[] = {extended_token("settings"), extended_token("wifi"), extended_token("set"),
                              extended_token("ssid")};
  return bsc_help_render_topic(&catalog, path, 4u, extended_token("security"), NULL, output);
}

/** @brief Render command through catalog-aware path with no applicable metadata. */
static bsc_status_t ext_render_command_no_metadata(bsc_output_t *output) {
  bsc_help_catalog_t catalog = ext2_render_catalog();
  bsc_string_view_t path[] = {extended_token("settings"), extended_token("wifi"), extended_token("set"),
                              extended_token("ssid")};
  catalog.targets = NULL;
  catalog.target_count = 0u;
  catalog.topics = NULL;
  catalog.topic_count = 0u;
  return bsc_help_render_catalog_path(&catalog, path, 4u, NULL, output);
}

/** @brief Render command through ordinary path for compatibility comparison. */
static bsc_status_t ext_render_command_ordinary(bsc_output_t *output) {
  bsc_string_view_t path[] = {extended_token("settings"), extended_token("wifi"), extended_token("set"),
                              extended_token("ssid")};
  return bsc_help_render_path(ext2_commands, sizeof(ext2_commands) / sizeof(ext2_commands[0]), path, 4u, NULL, output);
}

/** @brief Compare one renderer to an exact golden fixture. */
static int ext_assert_golden(const char *test_name, const char *golden_name, ext_render_fn_t render_fn) {
  char expected[8192];
  size_t expected_len = 0u;
  ext_capture_t capture;
  bsc_output_t output = {ext_capture_write, &capture};
  EXT_ASSERT_TRUE(ext_load_golden(golden_name, expected, sizeof(expected), &expected_len));
  EXT_ASSERT_TRUE(ext_assert_clean_bytes(test_name, expected, expected_len) == 0);
  ext_capture_init(&capture);
  EXT_ASSERT_STATUS(BSC_STATUS_OK, render_fn(&output));
  EXT_ASSERT_TRUE(capture.used == expected_len);
  EXT_ASSERT_TRUE(memcmp(capture.buffer, expected, expected_len) == 0);
  return 0;
}

/** @brief Return callback count for one successful render. */
static int ext_count_render_callbacks(const char *test_name, ext_render_fn_t render_fn, size_t *calls) {
  ext_capture_t capture;
  bsc_output_t output = {ext_capture_write, &capture};
  ext_capture_init(&capture);
  EXT_ASSERT_STATUS(BSC_STATUS_OK, render_fn(&output));
  EXT_ASSERT_TRUE(capture.calls > 0u);
  *calls = capture.calls;
  return 0;
}

/** @brief Verify every output callback boundary stops after the first short write. */
static int ext_assert_all_short_writes(const char *test_name, ext_render_fn_t render_fn) {
  size_t total_calls = 0u;
  size_t fail_call;
  EXT_ASSERT_TRUE(ext_count_render_callbacks(test_name, render_fn, &total_calls) == 0);
  for (fail_call = 1u; fail_call <= total_calls; ++fail_call) {
    ext_fail_sink_t sink;
    bsc_output_t output;
    memset(&sink, 0, sizeof(sink));
    sink.fail_call = fail_call;
    output.write = ext_fail_write;
    output.user = &sink;
    EXT_ASSERT_STATUS(BSC_STATUS_OUTPUT_TRUNCATED, render_fn(&output));
    EXT_ASSERT_TRUE(sink.first_failure_call == fail_call);
    EXT_ASSERT_TRUE(sink.calls_after_failure == 0u);
  }
  return 0;
}
/** @brief Verify topic result clearing resets borrowed pointers and indexes and accepts NULL. */
static int test_topic_result_clear(const char *test_name) {
  bsc_help_topic_lookup_result_t result;
  result.topic = &extended_topics[0];
  result.topic_index = 3u;
  result.parent_command_index = 4u;
  bsc_help_topic_lookup_result_clear(&result);
  EXT_ASSERT_TRUE(result.topic == NULL);
  EXT_ASSERT_TRUE(result.topic_index == 0u);
  EXT_ASSERT_TRUE(result.parent_command_index == 0u);
  bsc_help_topic_lookup_result_clear(NULL);
  return 0;
}

/** @brief Verify successful topic lookup returns exact borrowed catalog identity and indexes. */
static int test_topic_lookup_success_and_identity(const char *test_name) {
  bsc_help_catalog_t catalog = extended_valid_catalog();
  bsc_string_view_t status_path = extended_token("status");
  bsc_help_topic_lookup_result_t result;
  EXT_ASSERT_STATUS(BSC_STATUS_OK,
                    bsc_help_find_topic(&catalog, &status_path, 1u, extended_token("overview"), NULL, &result));
  EXT_ASSERT_TRUE(result.topic == &extended_topics[0]);
  EXT_ASSERT_TRUE(result.topic_index == 0u);
  EXT_ASSERT_TRUE(result.parent_command_index == 0u);
  EXT_ASSERT_TRUE(result.topic->parent == &extended_commands[result.parent_command_index]);
  EXT_ASSERT_STATUS(BSC_STATUS_OK,
                    bsc_help_find_topic(&catalog, &status_path, 1u, extended_token("DETAILS"), NULL, &result));
  EXT_ASSERT_TRUE(result.topic == &extended_topics[2]);
  EXT_ASSERT_TRUE(result.topic_index == 2u);
  EXT_ASSERT_TRUE(result.parent_command_index == 0u);
  return 0;
}

/** @brief Verify identical topic IDs are scoped by exact parent descriptor pointer. */
static int test_topic_lookup_same_id_under_different_parents(const char *test_name) {
  bsc_help_catalog_t catalog = extended_valid_catalog();
  bsc_string_view_t status_path = extended_token("status");
  bsc_string_view_t wifi_path = extended_token("wifi");
  bsc_help_topic_lookup_result_t result;
  EXT_ASSERT_STATUS(BSC_STATUS_OK,
                    bsc_help_find_topic(&catalog, &status_path, 1u, extended_token("overview"), NULL, &result));
  EXT_ASSERT_TRUE(result.topic == &extended_topics[0]);
  EXT_ASSERT_STATUS(BSC_STATUS_OK,
                    bsc_help_find_topic(&catalog, &wifi_path, 1u, extended_token("overview"), NULL, &result));
  EXT_ASSERT_TRUE(result.topic == &extended_topics[1]);
  EXT_ASSERT_TRUE(result.parent_command_index == 1u);
  return 0;
}

/** @brief Verify static help visibility controls topic parent availability without access callbacks. */
static int test_topic_lookup_visibility_options(const char *test_name) {
  bsc_help_catalog_t catalog = extended_valid_catalog();
  bsc_help_options_t options;
  bsc_string_view_t advanced_path = extended_token("advanced");
  bsc_string_view_t factory_path = extended_token("factory");
  bsc_string_view_t locked_path = extended_token("locked");
  bsc_string_view_t hidden_path = extended_token("hidden");
  bsc_string_view_t hidden_factory_path = extended_token("hidden_factory");
  bsc_help_topic_lookup_result_t result;

  bsc_help_options_init(&options);
  extended_handler_calls = 0;
  extended_access_calls = 0;
  EXT_ASSERT_STATUS(BSC_STATUS_OK,
                    bsc_help_find_topic(&catalog, &advanced_path, 1u, extended_token("advanced"), NULL, &result));
  options.include_advanced = false;
  EXT_ASSERT_TRUE(expect_cleared_failure(test_name, BSC_STATUS_UNKNOWN_COMMAND, &catalog, &advanced_path, 1u,
                                         extended_token("advanced"), &options) == 0);
  bsc_help_options_init(&options);
  EXT_ASSERT_TRUE(expect_cleared_failure(test_name, BSC_STATUS_UNKNOWN_COMMAND, &catalog, &factory_path, 1u,
                                         extended_token("factory"), &options) == 0);
  options.include_factory = true;
  EXT_ASSERT_STATUS(BSC_STATUS_OK,
                    bsc_help_find_topic(&catalog, &factory_path, 1u, extended_token("factory"), &options, &result));
  bsc_help_options_init(&options);
  EXT_ASSERT_TRUE(expect_cleared_failure(test_name, BSC_STATUS_UNKNOWN_COMMAND, &catalog, &locked_path, 1u,
                                         extended_token("locked"), &options) == 0);
  options.include_locked = true;
  EXT_ASSERT_STATUS(BSC_STATUS_OK,
                    bsc_help_find_topic(&catalog, &locked_path, 1u, extended_token("locked"), &options, &result));
  bsc_help_options_init(&options);
  EXT_ASSERT_TRUE(expect_cleared_failure(test_name, BSC_STATUS_UNKNOWN_COMMAND, &catalog, &hidden_path, 1u,
                                         extended_token("hidden"), &options) == 0);
  options.include_hidden = true;
  EXT_ASSERT_STATUS(BSC_STATUS_OK,
                    bsc_help_find_topic(&catalog, &hidden_path, 1u, extended_token("hidden"), &options, &result));
  bsc_help_options_init(&options);
  options.include_hidden = true;
  EXT_ASSERT_TRUE(expect_cleared_failure(test_name, BSC_STATUS_UNKNOWN_COMMAND, &catalog, &hidden_factory_path, 1u,
                                         extended_token("service"), &options) == 0);
  options.include_factory = true;
  EXT_ASSERT_STATUS(BSC_STATUS_OK,
                    bsc_help_find_topic(&catalog, &hidden_factory_path, 1u, extended_token("service"), &options,
                                        &result));
  EXT_ASSERT_TRUE(extended_handler_calls == 0);
  EXT_ASSERT_TRUE(extended_access_calls == 0);
  return 0;
}

/** @brief Verify lookup failure statuses and missing-input precedence. */
static int test_topic_lookup_failure_statuses_and_precedence(const char *test_name) {
  bsc_help_catalog_t catalog = extended_valid_catalog();
  bsc_string_view_t status_path = extended_token("status");
  bsc_string_view_t unknown_path = extended_token("unknown");
  bsc_string_view_t null_view = bsc_string_view_from_parts(NULL, 3u);
  bsc_help_topic_lookup_result_t result;

  EXT_ASSERT_TRUE(expect_cleared_failure(test_name, BSC_STATUS_UNKNOWN_COMMAND, &catalog, &unknown_path, 1u,
                                         extended_token("overview"), NULL) == 0);
  EXT_ASSERT_TRUE(expect_cleared_failure(test_name, BSC_STATUS_UNKNOWN_TOPIC, &catalog, &status_path, 1u,
                                         extended_token("missing"), NULL) == 0);
  EXT_ASSERT_TRUE(expect_cleared_failure(test_name, BSC_STATUS_NO_INPUT, NULL, NULL, 0u,
                                         bsc_string_view_from_parts(NULL, 9u), NULL) == 0);
  EXT_ASSERT_TRUE(expect_cleared_failure(test_name, BSC_STATUS_NO_INPUT, NULL, &status_path, 1u,
                                         bsc_string_view_from_parts(NULL, 0u), NULL) == 0);
  EXT_ASSERT_TRUE(expect_cleared_failure(test_name, BSC_STATUS_INTERNAL_ERROR, NULL, &status_path, 1u,
                                         extended_token("overview"), NULL) == 0);
  EXT_ASSERT_TRUE(expect_cleared_failure(test_name, BSC_STATUS_INTERNAL_ERROR, &catalog, NULL, 1u,
                                         extended_token("overview"), NULL) == 0);
  EXT_ASSERT_STATUS(BSC_STATUS_INTERNAL_ERROR,
                    bsc_help_find_topic(&catalog, &status_path, 1u, extended_token("overview"), NULL, NULL));
  EXT_ASSERT_TRUE(expect_cleared_failure(test_name, BSC_STATUS_INTERNAL_ERROR, &catalog, &status_path, 1u, null_view,
                                         NULL) == 0);
  result.topic = &extended_topics[0];
  result.topic_index = 44u;
  result.parent_command_index = 55u;
  EXT_ASSERT_STATUS(BSC_STATUS_UNKNOWN_TOPIC,
                    bsc_help_find_topic(&catalog,
                                        &status_path,
                                        1u,
                                        bsc_string_view_from_parts("overview-extra-long-token-that-does-not-match", 45u),
                                        NULL,
                                        &result));
  EXT_ASSERT_TRUE(result.topic == NULL);
  EXT_ASSERT_TRUE(result.topic_index == 0u);
  EXT_ASSERT_TRUE(result.parent_command_index == 0u);
  return 0;
}

/** @brief Verify validation failures precede parent lookup and topic scanning. */
static int test_topic_lookup_validation_failures(const char *test_name) {
  bsc_help_catalog_t catalog = extended_valid_catalog();
  bsc_string_view_t status_path = extended_token("status");
  bsc_help_topic_t duplicate_topics[2];
  const bsc_command_t invalid_help_commands[] = {
      {path_status, 1u, BSC_NODE_COMMAND, NULL, 0u, extended_forbidden_handler, NULL, BSC_ACCESS_NORMAL,
       BSC_COMMAND_FLAG_NONE, extended_forbidden_access, "Status", NULL},
  };

  catalog.commands = NULL;
  EXT_ASSERT_TRUE(expect_cleared_failure(test_name, BSC_STATUS_INVALID_DESCRIPTOR, &catalog, &status_path, 1u,
                                         extended_token("overview"), NULL) == 0);

  catalog = extended_valid_catalog();
  duplicate_topics[0] = extended_topics[0];
  duplicate_topics[1] = extended_topics[0];
  duplicate_topics[1].id = "Overview";
  catalog.topics = duplicate_topics;
  catalog.topic_count = 2u;
  EXT_ASSERT_TRUE(expect_cleared_failure(test_name, BSC_STATUS_INVALID_DESCRIPTOR, &catalog, &status_path, 1u,
                                         extended_token("overview"), NULL) == 0);

  catalog = extended_valid_catalog();
  catalog.commands = invalid_help_commands;
  catalog.command_count = sizeof(invalid_help_commands) / sizeof(invalid_help_commands[0]);
  catalog.topics = NULL;
  catalog.topic_count = 0u;
  EXT_ASSERT_TRUE(expect_cleared_failure(test_name, BSC_STATUS_INVALID_DESCRIPTOR, &catalog, &status_path, 1u,
                                         extended_token("overview"), NULL) == 0);
  return 0;
}


/** @brief Compare extended renderer outputs with approved byte-exact fixtures. */
static int test_extended_render_golden_outputs(const char *test_name) {
#if BSC_ENABLE_FLOAT && BSC_MAX_FLOAT_FRACTION_DIGITS == 6u
  EXT_ASSERT_TRUE(ext_assert_golden(test_name, "extended_command_wifi_set_ssid.txt", ext_render_command_full) == 0);
  EXT_ASSERT_TRUE(ext_assert_golden(test_name, "extended_group_settings.txt", ext_render_group_full) == 0);
  EXT_ASSERT_TRUE(ext_assert_golden(test_name, "topic_settings_wifi_set_ssid_security.txt", ext_render_topic_full) == 0);
  EXT_ASSERT_TRUE(ext_assert_golden(test_name, "extended_command_wifi_set_ssid_no_metadata.txt",
                                    ext_render_command_no_metadata) == 0);
#else
  ext_capture_t capture;
  bsc_output_t output = {ext_capture_write, &capture};
  ext_capture_init(&capture);
  EXT_ASSERT_STATUS(BSC_STATUS_OK, ext_render_command_full(&output));
  EXT_ASSERT_TRUE(capture.used > 0u);
#endif
  EXT_ASSERT_TRUE(extended_handler_calls == 0);
  EXT_ASSERT_TRUE(extended_access_calls == 0);
  return 0;
}

/** @brief Prove no-metadata catalog-aware output is byte-identical to ordinary help rendering. */
static int test_extended_no_metadata_matches_ordinary(const char *test_name) {
  ext_capture_t catalog_capture;
  ext_capture_t ordinary_capture;
  bsc_output_t catalog_output = {ext_capture_write, &catalog_capture};
  bsc_output_t ordinary_output = {ext_capture_write, &ordinary_capture};
  ext_capture_init(&catalog_capture);
  ext_capture_init(&ordinary_capture);
  EXT_ASSERT_STATUS(BSC_STATUS_OK, ext_render_command_no_metadata(&catalog_output));
  EXT_ASSERT_STATUS(BSC_STATUS_OK, ext_render_command_ordinary(&ordinary_output));
  EXT_ASSERT_TRUE(catalog_capture.used == ordinary_capture.used);
  EXT_ASSERT_TRUE(memcmp(catalog_capture.buffer, ordinary_capture.buffer, catalog_capture.used) == 0);
  return 0;
}

/** @brief Verify invalid inputs, lookup failures, and sink precedence before output. */
static int test_extended_render_precedence_and_failures(const char *test_name) {
  bsc_help_catalog_t catalog = ext2_render_catalog();
  bsc_string_view_t path[] = {extended_token("settings"), extended_token("wifi"), extended_token("set"),
                              extended_token("ssid")};
  bsc_string_view_t unknown[] = {extended_token("settings"), extended_token("wifi"), extended_token("missing")};
  bsc_output_t invalid_output = {NULL, NULL};
  ext_capture_t capture;
  bsc_output_t output = {ext_capture_write, &capture};
  bsc_help_topic_t duplicate_topics[2];

  EXT_ASSERT_STATUS(BSC_STATUS_NO_INPUT, bsc_help_render_catalog_path(&catalog, path, 0u, NULL, &output));
  EXT_ASSERT_STATUS(BSC_STATUS_INTERNAL_ERROR, bsc_help_render_catalog_path(NULL, path, 4u, NULL, &output));
  EXT_ASSERT_STATUS(BSC_STATUS_INTERNAL_ERROR, bsc_help_render_catalog_path(&catalog, NULL, 4u, NULL, &output));
  EXT_ASSERT_STATUS(BSC_STATUS_UNKNOWN_COMMAND, bsc_help_render_catalog_path(&catalog, unknown, 3u, NULL, &output));
  EXT_ASSERT_STATUS(BSC_STATUS_NO_INPUT, bsc_help_render_topic(&catalog, path, 0u, extended_token("security"), NULL, &output));
  EXT_ASSERT_STATUS(BSC_STATUS_NO_INPUT, bsc_help_render_topic(&catalog, path, 4u, bsc_string_view_from_parts(NULL, 0u),
                                                               NULL, &output));
  EXT_ASSERT_STATUS(BSC_STATUS_INTERNAL_ERROR, bsc_help_render_topic(NULL, path, 4u, extended_token("security"), NULL,
                                                                    &output));
  EXT_ASSERT_STATUS(BSC_STATUS_INTERNAL_ERROR, bsc_help_render_topic(&catalog, NULL, 4u, extended_token("security"), NULL,
                                                                    &output));
  EXT_ASSERT_STATUS(BSC_STATUS_INTERNAL_ERROR,
                    bsc_help_render_topic(&catalog, path, 4u, bsc_string_view_from_parts(NULL, 3u), NULL, &output));
  EXT_ASSERT_STATUS(BSC_STATUS_UNKNOWN_TOPIC, bsc_help_render_topic(&catalog, path, 4u, extended_token("missing"), NULL,
                                                                   &output));
  EXT_ASSERT_STATUS(BSC_STATUS_INTERNAL_ERROR, bsc_help_render_catalog_path(&catalog, path, 4u, NULL, &invalid_output));
  EXT_ASSERT_STATUS(BSC_STATUS_INTERNAL_ERROR, bsc_help_render_topic(&catalog, path, 4u, extended_token("security"), NULL,
                                                                    &invalid_output));

  catalog.commands = NULL;
  ext_capture_init(&capture);
  EXT_ASSERT_STATUS(BSC_STATUS_INVALID_DESCRIPTOR, bsc_help_render_catalog_path(&catalog, path, 4u, NULL, &output));
  EXT_ASSERT_TRUE(capture.calls == 0u);
  catalog = ext2_render_catalog();
  duplicate_topics[0] = ext2_topics[0];
  duplicate_topics[1] = ext2_topics[0];
  duplicate_topics[1].id = "Security";
  catalog.topics = duplicate_topics;
  catalog.topic_count = 2u;
  ext_capture_init(&capture);
  EXT_ASSERT_STATUS(BSC_STATUS_INVALID_DESCRIPTOR, bsc_help_render_topic(&catalog, path, 4u, extended_token("security"),
                                                                        NULL, &output));
  EXT_ASSERT_TRUE(capture.calls == 0u);
  catalog = ext2_render_catalog();
  ext2_commands[5].description = NULL;
  ext_capture_init(&capture);
  EXT_ASSERT_STATUS(BSC_STATUS_INVALID_DESCRIPTOR, bsc_help_render_catalog_path(&catalog, path, 4u, NULL, &output));
  EXT_ASSERT_TRUE(capture.calls == 0u);
  ext2_commands[5].description = "WiFi set";
  EXT_ASSERT_TRUE(extended_handler_calls == 0);
  EXT_ASSERT_TRUE(extended_access_calls == 0);
  return 0;
}

/** @brief Verify all-filtered RELATED is omitted while visible topic pages still render. */
static int test_extended_all_filtered_related_omission(const char *test_name) {
  bsc_help_catalog_t catalog = ext2_render_catalog();
  bsc_help_related_t filtered_related[] = {{&ext2_commands[6]}};
  bsc_help_topic_t topic = ext2_topics[0];
  bsc_string_view_t path[] = {extended_token("settings"), extended_token("wifi"), extended_token("set"),
                              extended_token("ssid")};
  ext_capture_t capture;
  bsc_output_t output = {ext_capture_write, &capture};
  topic.related = filtered_related;
  topic.related_count = 1u;
  catalog.topics = &topic;
  catalog.topic_count = 1u;
  ext_capture_init(&capture);
  EXT_ASSERT_STATUS(BSC_STATUS_OK, bsc_help_render_topic(&catalog, path, 4u, extended_token("security"), NULL, &output));
  EXT_ASSERT_TRUE(!ext_bytes_find(capture.buffer, capture.used, "RELATED", strlen("RELATED")));
  return 0;
}

/** @brief Exhaustively verify short-write termination for every new successful page. */
static int test_extended_render_short_writes(const char *test_name) {
#if BSC_ENABLE_FLOAT && BSC_MAX_FLOAT_FRACTION_DIGITS == 6u
  EXT_ASSERT_TRUE(ext_assert_all_short_writes(test_name, ext_render_command_full) == 0);
  EXT_ASSERT_TRUE(ext_assert_all_short_writes(test_name, ext_render_group_full) == 0);
  EXT_ASSERT_TRUE(ext_assert_all_short_writes(test_name, ext_render_topic_full) == 0);
  EXT_ASSERT_TRUE(ext_assert_all_short_writes(test_name, ext_render_command_no_metadata) == 0);
#endif
  return 0;
}
/** @brief Aggregate pure extended-help topic lookup tests. */
int bsc_run_help_extended_tests(void) {
  int failures = 0;
  EXT_RUN_TEST(test_topic_result_clear);
  EXT_RUN_TEST(test_topic_lookup_success_and_identity);
  EXT_RUN_TEST(test_topic_lookup_same_id_under_different_parents);
  EXT_RUN_TEST(test_topic_lookup_visibility_options);
  EXT_RUN_TEST(test_topic_lookup_failure_statuses_and_precedence);
  EXT_RUN_TEST(test_topic_lookup_validation_failures);
  EXT_RUN_TEST(test_extended_render_golden_outputs);
  EXT_RUN_TEST(test_extended_no_metadata_matches_ordinary);
  EXT_RUN_TEST(test_extended_render_precedence_and_failures);
  EXT_RUN_TEST(test_extended_all_filtered_related_omission);
  EXT_RUN_TEST(test_extended_render_short_writes);
  return failures;
}
