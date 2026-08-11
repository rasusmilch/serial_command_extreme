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

#define EXT_ARRAY_FLOOR(value) ((value) == 0u ? 1u : (value))
#define EXT_LOOKUP_FIXTURE_SUPPORTED (BSC_MAX_HELP_TOPICS >= 8u)
#define EXT_RENDER_FIXTURE_SUPPORTED                                                                 \
  (BSC_MAX_HELP_TEXT_ITEMS >= 2u && BSC_MAX_HELP_EXAMPLES >= 2u && BSC_MAX_HELP_RELATED >= 3u &&    \
   BSC_MAX_HELP_TOPICS >= 3u)
#define EXT_CAPACITY_LINE_LEN                                                                        \
  ((size_t)BSC_MAX_LINE_LEN + ((size_t)BSC_MAX_PATH_TOKENS * ((size_t)BSC_MAX_TOKEN_LEN + 1u)) +     \
   (size_t)BSC_MAX_HELP_TEXT_LEN + 32u)

#if BSC_MAX_HELP_RELATED > BSC_MAX_COMMANDS
#error "extended-help related capacity tests require BSC_MAX_HELP_RELATED <= BSC_MAX_COMMANDS"
#endif

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
  size_t available;
  capture->calls += 1u;
  if (data == NULL || length == 0u) {
    return length;
  }
  available = sizeof(capture->buffer) - capture->used;
  if (length > available) {
    if (available > 0u) {
      memcpy(&capture->buffer[capture->used], data, available);
      capture->used += available;
    }
    return available;
  }
  memcpy(&capture->buffer[capture->used], data, length);
  capture->used += length;
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

/** @brief Render one explicit descriptor path from a catalog while capturing bytes. */
static bsc_status_t ext_render_catalog_capture(const bsc_help_catalog_t *catalog,
                                               const char *const *tokens,
                                               size_t token_count,
                                               const bsc_help_options_t *options,
                                               ext_capture_t *capture) {
  bsc_string_view_t path[BSC_MAX_PATH_TOKENS];
  size_t index;
  bsc_output_t output = {ext_capture_write, capture};
  for (index = 0u; index < token_count; ++index) {
    path[index] = extended_token(tokens[index]);
  }
  ext_capture_init(capture);
  return bsc_help_render_catalog_path(catalog, path, token_count, options, &output);
}

/** @brief Render one explicit topic path from a catalog while capturing bytes. */
static bsc_status_t ext_render_topic_capture(const bsc_help_catalog_t *catalog,
                                             const char *const *tokens,
                                             size_t token_count,
                                             const char *topic_id,
                                             const bsc_help_options_t *options,
                                             ext_capture_t *capture) {
  bsc_string_view_t path[BSC_MAX_PATH_TOKENS];
  size_t index;
  bsc_output_t output = {ext_capture_write, capture};
  for (index = 0u; index < token_count; ++index) {
    path[index] = extended_token(tokens[index]);
  }
  ext_capture_init(capture);
  return bsc_help_render_topic(catalog, path, token_count, extended_token(topic_id), options, &output);
}

/** @brief Return the first byte offset of a needle or SIZE_MAX when absent. */
static size_t ext_find_offset(const char *haystack, size_t haystack_len, const char *needle) {
  size_t needle_len = strlen(needle);
  size_t index;
  if (needle_len == 0u) return 0u;
  if (haystack_len < needle_len) return (size_t)-1;
  for (index = 0u; index <= haystack_len - needle_len; ++index) {
    if (memcmp(&haystack[index], needle, needle_len) == 0) return index;
  }
  return (size_t)-1;
}

/** @brief Assert a captured output contains one expected byte sequence. */
static int ext_assert_contains(const char *test_name, const ext_capture_t *capture, const char *needle) {
  EXT_ASSERT_TRUE(ext_find_offset(capture->buffer, capture->used, needle) != (size_t)-1);
  return 0;
}

/** @brief Assert a captured output omits one byte sequence. */
static int ext_assert_omits(const char *test_name, const ext_capture_t *capture, const char *needle) {
  EXT_ASSERT_TRUE(ext_find_offset(capture->buffer, capture->used, needle) == (size_t)-1);
  return 0;
}


/** @brief Count non-overlapping occurrences of a byte sequence in captured output. */
static size_t ext_count_occurrences(const char *haystack, size_t haystack_len, const char *needle) {
  size_t needle_len = strlen(needle);
  size_t count = 0u;
  size_t offset = 0u;
  if (needle_len == 0u) return 0u;
  while (offset + needle_len <= haystack_len) {
    if (memcmp(&haystack[offset], needle, needle_len) == 0) {
      count += 1u;
      offset += needle_len;
    } else {
      offset += 1u;
    }
  }
  return count;
}

/** @brief Assert one byte sequence appears before another in captured output. */
static int ext_assert_order(const char *test_name, const ext_capture_t *capture, const char *first, const char *second) {
  size_t first_offset = ext_find_offset(capture->buffer, capture->used, first);
  size_t second_offset = ext_find_offset(capture->buffer, capture->used, second);
  EXT_ASSERT_TRUE(first_offset != (size_t)-1);
  EXT_ASSERT_TRUE(second_offset != (size_t)-1);
  EXT_ASSERT_TRUE(first_offset < second_offset);
  return 0;
}

/** @brief Assert a catalog path renderer returns a status before any output callback. */
static int ext_expect_catalog_no_output(const char *test_name,
                                        bsc_status_t expected,
                                        const bsc_help_catalog_t *catalog,
                                        const char *const *tokens,
                                        size_t token_count,
                                        const bsc_help_options_t *options) {
  ext_capture_t capture;
  EXT_ASSERT_STATUS(expected, ext_render_catalog_capture(catalog, tokens, token_count, options, &capture));
  EXT_ASSERT_TRUE(capture.calls == 0u);
  return 0;
}

/** @brief Assert a topic renderer returns a status before any output callback. */
static int ext_expect_topic_no_output(const char *test_name,
                                      bsc_status_t expected,
                                      const bsc_help_catalog_t *catalog,
                                      const char *const *tokens,
                                      size_t token_count,
                                      const char *topic_id,
                                      const bsc_help_options_t *options) {
  ext_capture_t capture;
  EXT_ASSERT_STATUS(expected, ext_render_topic_capture(catalog, tokens, token_count, topic_id, options, &capture));
  EXT_ASSERT_TRUE(capture.calls == 0u);
  return 0;
}

/** @brief Verify catalog and topic renderers obey all static visibility combinations without access callbacks. */
static int test_extended_render_visibility_options(const char *test_name) {
  bsc_help_catalog_t catalog = extended_valid_catalog();
  bsc_help_options_t options;
  ext_capture_t capture;
  extended_handler_calls = 0;
  extended_access_calls = 0;

  bsc_help_options_init(&options);
  EXT_ASSERT_STATUS(BSC_STATUS_OK, ext_render_catalog_capture(&catalog, path_status, 1u, NULL, &capture));
  EXT_ASSERT_STATUS(BSC_STATUS_OK, ext_render_topic_capture(&catalog, path_status, 1u, "overview", NULL, &capture));
  EXT_ASSERT_STATUS(BSC_STATUS_OK, ext_render_catalog_capture(&catalog, path_advanced, 1u, NULL, &capture));
  EXT_ASSERT_STATUS(BSC_STATUS_OK, ext_render_topic_capture(&catalog, path_advanced, 1u, "advanced", NULL, &capture));
  options.include_advanced = false;
  EXT_ASSERT_TRUE(ext_expect_catalog_no_output(test_name, BSC_STATUS_UNKNOWN_COMMAND, &catalog, path_advanced, 1u,
                                               &options) == 0);
  EXT_ASSERT_TRUE(ext_expect_topic_no_output(test_name, BSC_STATUS_UNKNOWN_COMMAND, &catalog, path_advanced, 1u,
                                             "advanced", &options) == 0);

  bsc_help_options_init(&options);
  EXT_ASSERT_TRUE(ext_expect_catalog_no_output(test_name, BSC_STATUS_UNKNOWN_COMMAND, &catalog, path_factory, 1u,
                                               &options) == 0);
  EXT_ASSERT_TRUE(ext_expect_topic_no_output(test_name, BSC_STATUS_UNKNOWN_COMMAND, &catalog, path_factory, 1u,
                                             "factory", &options) == 0);
  options.include_factory = true;
  EXT_ASSERT_STATUS(BSC_STATUS_OK, ext_render_catalog_capture(&catalog, path_factory, 1u, &options, &capture));
  EXT_ASSERT_STATUS(BSC_STATUS_OK, ext_render_topic_capture(&catalog, path_factory, 1u, "factory", &options, &capture));

  bsc_help_options_init(&options);
  EXT_ASSERT_TRUE(ext_expect_catalog_no_output(test_name, BSC_STATUS_UNKNOWN_COMMAND, &catalog, path_locked, 1u,
                                               &options) == 0);
  EXT_ASSERT_TRUE(ext_expect_topic_no_output(test_name, BSC_STATUS_UNKNOWN_COMMAND, &catalog, path_locked, 1u,
                                             "locked", &options) == 0);
  options.include_locked = true;
  EXT_ASSERT_STATUS(BSC_STATUS_OK, ext_render_catalog_capture(&catalog, path_locked, 1u, &options, &capture));
  EXT_ASSERT_STATUS(BSC_STATUS_OK, ext_render_topic_capture(&catalog, path_locked, 1u, "locked", &options, &capture));

  bsc_help_options_init(&options);
  EXT_ASSERT_TRUE(ext_expect_catalog_no_output(test_name, BSC_STATUS_UNKNOWN_COMMAND, &catalog, path_hidden, 1u,
                                               &options) == 0);
  EXT_ASSERT_TRUE(ext_expect_topic_no_output(test_name, BSC_STATUS_UNKNOWN_COMMAND, &catalog, path_hidden, 1u,
                                             "hidden", &options) == 0);
  options.include_hidden = true;
  EXT_ASSERT_STATUS(BSC_STATUS_OK, ext_render_catalog_capture(&catalog, path_hidden, 1u, &options, &capture));
  EXT_ASSERT_STATUS(BSC_STATUS_OK, ext_render_topic_capture(&catalog, path_hidden, 1u, "hidden", &options, &capture));

  bsc_help_options_init(&options);
  options.include_hidden = true;
  EXT_ASSERT_TRUE(ext_expect_catalog_no_output(test_name, BSC_STATUS_UNKNOWN_COMMAND, &catalog, path_hidden_factory, 1u,
                                               &options) == 0);
  EXT_ASSERT_TRUE(ext_expect_topic_no_output(test_name, BSC_STATUS_UNKNOWN_COMMAND, &catalog, path_hidden_factory, 1u,
                                             "service", &options) == 0);
  options.include_factory = true;
  EXT_ASSERT_STATUS(BSC_STATUS_OK, ext_render_catalog_capture(&catalog, path_hidden_factory, 1u, &options, &capture));
  EXT_ASSERT_STATUS(BSC_STATUS_OK, ext_render_topic_capture(&catalog, path_hidden_factory, 1u, "service", &options,
                                                            &capture));
  EXT_ASSERT_TRUE(extended_handler_calls == 0);
  EXT_ASSERT_TRUE(extended_access_calls == 0);
  return 0;
}

/** @brief Verify validation failures, including target and related metadata defects, occur before output. */
static int test_extended_render_validation_before_output_gaps(const char *test_name) {
  bsc_help_catalog_t catalog = ext2_render_catalog();
  bsc_command_t outside_command = ext2_commands[0];
  bsc_help_target_t target;
  bsc_help_related_t related;
  bsc_help_topic_t topic;
  bsc_help_topic_t duplicate_topics[2];
  const char *const unknown_path[] = {"settings", "wifi", "missing"};

  catalog.commands = NULL;
  EXT_ASSERT_TRUE(ext_expect_catalog_no_output(test_name, BSC_STATUS_INVALID_DESCRIPTOR, &catalog,
                                               ext2_path_settings_wifi_set_ssid, 4u, NULL) == 0);

  catalog = ext2_render_catalog();
  target = ext2_targets[0];
  target.target = &outside_command;
  catalog.targets = &target;
  catalog.target_count = 1u;
  EXT_ASSERT_TRUE(ext_expect_catalog_no_output(test_name, BSC_STATUS_INVALID_DESCRIPTOR, &catalog,
                                               ext2_path_settings_wifi_set_ssid, 4u, NULL) == 0);

  catalog = ext2_render_catalog();
  related.target = &outside_command;
  target = ext2_targets[0];
  target.related = &related;
  target.related_count = 1u;
  catalog.targets = &target;
  catalog.target_count = 1u;
  EXT_ASSERT_TRUE(ext_expect_catalog_no_output(test_name, BSC_STATUS_INVALID_DESCRIPTOR, &catalog,
                                               ext2_path_settings_wifi_set_ssid, 4u, NULL) == 0);

  catalog = ext2_render_catalog();
  related.target = &outside_command;
  topic = ext2_topics[0];
  topic.related = &related;
  topic.related_count = 1u;
  catalog.topics = &topic;
  catalog.topic_count = 1u;
  EXT_ASSERT_TRUE(ext_expect_topic_no_output(test_name, BSC_STATUS_INVALID_DESCRIPTOR, &catalog,
                                             ext2_path_settings_wifi_set_ssid, 4u, "security", NULL) == 0);

  catalog = ext2_render_catalog();
  duplicate_topics[0] = ext2_topics[0];
  duplicate_topics[1] = ext2_topics[0];
  duplicate_topics[1].id = "Security";
  catalog.topics = duplicate_topics;
  catalog.topic_count = 2u;
  EXT_ASSERT_TRUE(ext_expect_catalog_no_output(test_name, BSC_STATUS_INVALID_DESCRIPTOR, &catalog,
                                               ext2_path_settings_wifi_set_ssid, 4u, NULL) == 0);

  catalog = ext2_render_catalog();
  ext2_commands[5].description = NULL;
  EXT_ASSERT_TRUE(ext_expect_catalog_no_output(test_name, BSC_STATUS_INVALID_DESCRIPTOR, &catalog,
                                               ext2_path_settings_wifi_set_ssid, 4u, NULL) == 0);
  ext2_commands[5].description = "WiFi set";

  catalog = ext2_render_catalog();
  EXT_ASSERT_TRUE(ext_expect_catalog_no_output(test_name, BSC_STATUS_UNKNOWN_COMMAND, &catalog, unknown_path, 3u,
                                               NULL) == 0);
  EXT_ASSERT_TRUE(ext_expect_catalog_no_output(test_name, BSC_STATUS_UNKNOWN_COMMAND, &catalog, ext2_path_factory_reset,
                                               3u, NULL) == 0);
  EXT_ASSERT_TRUE(ext_expect_topic_no_output(test_name, BSC_STATUS_UNKNOWN_TOPIC, &catalog,
                                             ext2_path_settings_wifi_set_ssid, 4u, "missing", NULL) == 0);
  EXT_ASSERT_TRUE(extended_handler_calls == 0);
  EXT_ASSERT_TRUE(extended_access_calls == 0);
  return 0;
}

/** @brief Verify invalid sinks lose to prior metadata/lookup failures but fail for valid render requests. */
static int test_extended_render_invalid_sink_precedence(const char *test_name) {
  bsc_help_catalog_t catalog = ext2_render_catalog();
  bsc_output_t invalid_output = {NULL, NULL};
  bsc_string_view_t path[] = {extended_token("settings"), extended_token("wifi"), extended_token("set"),
                              extended_token("ssid")};
  bsc_string_view_t unknown[] = {extended_token("settings"), extended_token("wifi"), extended_token("missing")};
  bsc_string_view_t factory[] = {extended_token("factory"), extended_token("wifi"), extended_token("reset")};

  catalog.commands = NULL;
  EXT_ASSERT_STATUS(BSC_STATUS_INVALID_DESCRIPTOR, bsc_help_render_catalog_path(&catalog, path, 4u, NULL, NULL));
  catalog = ext2_render_catalog();
  EXT_ASSERT_STATUS(BSC_STATUS_UNKNOWN_COMMAND, bsc_help_render_catalog_path(&catalog, unknown, 3u, NULL, NULL));
  EXT_ASSERT_STATUS(BSC_STATUS_UNKNOWN_COMMAND, bsc_help_render_catalog_path(&catalog, factory, 3u, NULL, NULL));
  EXT_ASSERT_STATUS(BSC_STATUS_UNKNOWN_TOPIC,
                    bsc_help_render_topic(&catalog, path, 4u, extended_token("missing"), NULL, NULL));
  EXT_ASSERT_STATUS(BSC_STATUS_INTERNAL_ERROR, bsc_help_render_catalog_path(&catalog, path, 4u, NULL, NULL));
  EXT_ASSERT_STATUS(BSC_STATUS_INTERNAL_ERROR, bsc_help_render_catalog_path(&catalog, path, 4u, NULL, &invalid_output));
  EXT_ASSERT_STATUS(BSC_STATUS_INTERNAL_ERROR,
                    bsc_help_render_topic(&catalog, path, 4u, extended_token("security"), NULL, NULL));
  EXT_ASSERT_STATUS(BSC_STATUS_INTERNAL_ERROR,
                    bsc_help_render_topic(&catalog, path, 4u, extended_token("security"), NULL, &invalid_output));
  EXT_ASSERT_TRUE(extended_handler_calls == 0);
  EXT_ASSERT_TRUE(extended_access_calls == 0);
  return 0;
}

/** @brief Verify optional extended sections and structural edge cases render or omit deterministically. */
static int test_extended_render_optional_sections_and_edges(const char *test_name) {
  bsc_help_catalog_t catalog = ext2_render_catalog();
  ext_capture_t capture;
  bsc_help_target_t target;
  bsc_help_topic_t topic;
  bsc_help_topic_t topics[1];
  bsc_help_target_t cycle_targets[2];
  bsc_help_related_t status_to_wifi[] = {{&extended_commands[1]}};
  bsc_help_related_t wifi_to_status[] = {{&extended_commands[0]}};
  bsc_help_related_t self_parent[] = {{&extended_commands[0]}};
  const char *const solo_path[] = {"solo"};
  static const char *const solo_group_path[] = {"solo"};
  static const bsc_command_t solo_group[] = {
      {solo_group_path, 1u, BSC_NODE_GROUP, NULL, 0u, NULL, NULL, BSC_ACCESS_NORMAL,
       BSC_COMMAND_FLAG_NONE, NULL, "Solo", NULL},
  };

  catalog.targets = NULL;
  catalog.target_count = 0u;
  catalog.topics = &ext2_topics[0];
  catalog.topic_count = 1u;
  EXT_ASSERT_STATUS(BSC_STATUS_OK, ext_render_catalog_capture(&catalog, ext2_path_settings_wifi_set_ssid, 4u, NULL,
                                                              &capture));
  EXT_ASSERT_TRUE(ext_assert_contains(test_name, &capture, "TOPICS\n") == 0);
  EXT_ASSERT_TRUE(ext_assert_omits(test_name, &capture, "NOTES\n") == 0);

  catalog = ext2_render_catalog();
  target = ext2_targets[0];
  catalog.targets = &target;
  catalog.target_count = 1u;
  catalog.topics = NULL;
  catalog.topic_count = 0u;
  EXT_ASSERT_STATUS(BSC_STATUS_OK, ext_render_catalog_capture(&catalog, ext2_path_settings_wifi_set_ssid, 4u, NULL,
                                                              &capture));
  EXT_ASSERT_TRUE(ext_assert_contains(test_name, &capture, "NOTES\n") == 0);
  EXT_ASSERT_TRUE(ext_assert_omits(test_name, &capture, "TOPICS\n") == 0);
  EXT_ASSERT_TRUE(ext_assert_order(test_name, &capture, "NOTES\n", "WARNINGS\n") == 0);
  EXT_ASSERT_TRUE(ext_assert_order(test_name, &capture, "WARNINGS\n", "EXAMPLES\n") == 0);
  EXT_ASSERT_TRUE(ext_assert_order(test_name, &capture, "EXAMPLES\n", "RELATED\n") == 0);

  catalog = ext2_render_catalog();
  catalog.targets = NULL;
  catalog.target_count = 0u;
  catalog.topics = NULL;
  catalog.topic_count = 0u;
  EXT_ASSERT_STATUS(BSC_STATUS_OK, ext_render_catalog_capture(&catalog, ext2_path_status, 1u, NULL, &capture));
  EXT_ASSERT_TRUE(ext_assert_omits(test_name, &capture, "ARGUMENTS\n") == 0);
  EXT_ASSERT_TRUE(ext_assert_omits(test_name, &capture, "VALID VALUES\n") == 0);
  EXT_ASSERT_TRUE(ext_assert_omits(test_name, &capture, "NOTES\n") == 0);

  catalog = ext2_render_catalog();
  topics[0] = ext2_topics[1];
  catalog.topics = topics;
  catalog.topic_count = 1u;
  EXT_ASSERT_STATUS(BSC_STATUS_OK, ext_render_topic_capture(&catalog, ext2_path_settings_wifi_set_ssid, 4u, "reconnect",
                                                            NULL, &capture));
  EXT_ASSERT_TRUE(ext_assert_omits(test_name, &capture, "SYNOPSIS\n") == 0);
  EXT_ASSERT_TRUE(ext_assert_omits(test_name, &capture, "DESCRIPTION\n") == 0);

  catalog = ext2_render_catalog();
  EXT_ASSERT_STATUS(BSC_STATUS_OK, ext_render_catalog_capture(&catalog, ext2_path_settings_wifi, 2u, NULL, &capture));
  EXT_ASSERT_TRUE(ext_assert_omits(test_name, &capture, "DESCRIPTION\n") == 0);

  catalog.commands = solo_group;
  catalog.command_count = sizeof(solo_group) / sizeof(solo_group[0]);
  catalog.targets = NULL;
  catalog.target_count = 0u;
  catalog.topics = NULL;
  catalog.topic_count = 0u;
  EXT_ASSERT_STATUS(BSC_STATUS_OK, ext_render_catalog_capture(&catalog, solo_path, 1u, NULL, &capture));
  EXT_ASSERT_TRUE(ext_assert_omits(test_name, &capture, "COMMANDS\n") == 0);

  cycle_targets[0].target = &extended_commands[0];
  cycle_targets[0].notes.items = NULL;
  cycle_targets[0].notes.count = 0u;
  cycle_targets[0].warnings.items = NULL;
  cycle_targets[0].warnings.count = 0u;
  cycle_targets[0].examples = NULL;
  cycle_targets[0].example_count = 0u;
  cycle_targets[0].related = status_to_wifi;
  cycle_targets[0].related_count = 1u;
  cycle_targets[1].target = &extended_commands[1];
  cycle_targets[1].notes.items = NULL;
  cycle_targets[1].notes.count = 0u;
  cycle_targets[1].warnings.items = NULL;
  cycle_targets[1].warnings.count = 0u;
  cycle_targets[1].examples = NULL;
  cycle_targets[1].example_count = 0u;
  cycle_targets[1].related = wifi_to_status;
  cycle_targets[1].related_count = 1u;
  catalog = extended_valid_catalog();
  catalog.targets = cycle_targets;
  catalog.target_count = 2u;
  catalog.topics = NULL;
  catalog.topic_count = 0u;
  EXT_ASSERT_STATUS(BSC_STATUS_OK, ext_render_catalog_capture(&catalog, path_status, 1u, NULL, &capture));
  EXT_ASSERT_TRUE(ext_assert_contains(test_name, &capture, "RELATED\n") == 0);
  EXT_ASSERT_TRUE(ext_assert_contains(test_name, &capture, "  wifi - WiFi\n") == 0);
  EXT_ASSERT_TRUE(ext_count_occurrences(capture.buffer, capture.used, "RELATED\n") == 1u);

  topic = extended_topics[0];
  topic.related = self_parent;
  topic.related_count = 1u;
  catalog = extended_valid_catalog();
  catalog.topics = &topic;
  catalog.topic_count = 1u;
  EXT_ASSERT_STATUS(BSC_STATUS_OK, ext_render_topic_capture(&catalog, path_status, 1u, "overview", NULL, &capture));
  EXT_ASSERT_TRUE(ext_assert_contains(test_name, &capture, "RELATED\n") == 0);
  EXT_ASSERT_TRUE(ext_assert_contains(test_name, &capture, "  status - Status\n") == 0);
  return 0;
}

static char cap_note_storage[EXT_ARRAY_FLOOR(BSC_MAX_HELP_TEXT_ITEMS)][16u];
static const char *cap_note_items[EXT_ARRAY_FLOOR(BSC_MAX_HELP_TEXT_ITEMS)];
static char cap_warning_storage[EXT_ARRAY_FLOOR(BSC_MAX_HELP_TEXT_ITEMS)][16u];
static const char *cap_warning_items[EXT_ARRAY_FLOOR(BSC_MAX_HELP_TEXT_ITEMS)];
static char cap_example_line_storage[EXT_ARRAY_FLOOR(BSC_MAX_HELP_EXAMPLES)][16u];
static char cap_example_desc_storage[EXT_ARRAY_FLOOR(BSC_MAX_HELP_EXAMPLES)][16u];
static bsc_help_example_t cap_examples[EXT_ARRAY_FLOOR(BSC_MAX_HELP_EXAMPLES)];
static char cap_related_path_storage[EXT_ARRAY_FLOOR(BSC_MAX_HELP_RELATED)][16u];
static const char *cap_related_paths[EXT_ARRAY_FLOOR(BSC_MAX_HELP_RELATED)][1u];
static char cap_related_summary_storage[EXT_ARRAY_FLOOR(BSC_MAX_HELP_RELATED)][24u];
static bsc_command_t cap_related_commands[EXT_ARRAY_FLOOR(BSC_MAX_HELP_RELATED)];
static bsc_help_related_t cap_related[EXT_ARRAY_FLOOR(BSC_MAX_HELP_RELATED)];
static char cap_topic_id_storage[EXT_ARRAY_FLOOR(BSC_MAX_HELP_TOPICS)][16u];
static char cap_topic_summary_storage[EXT_ARRAY_FLOOR(BSC_MAX_HELP_TOPICS)][24u];
static bsc_help_topic_t cap_topics[EXT_ARRAY_FLOOR(BSC_MAX_HELP_TOPICS)];

typedef enum ext_capacity_section {
  EXT_CAP_SECTION_NONE = 0,
  EXT_CAP_SECTION_NOTES,
  EXT_CAP_SECTION_WARNINGS,
  EXT_CAP_SECTION_EXAMPLES,
  EXT_CAP_SECTION_TOPICS,
  EXT_CAP_SECTION_RELATED
} ext_capacity_section_t;

typedef struct ext_capacity_sink {
  char line[EXT_CAPACITY_LINE_LEN];
  size_t line_used;
  int overflow;
  ext_capacity_section_t section;
  size_t notes;
  size_t warnings;
  size_t examples;
  size_t topics;
  size_t related;
  int saw_first_note;
  int saw_last_note;
  int saw_first_warning;
  int saw_last_warning;
  int saw_first_example;
  int saw_last_example;
  int saw_first_topic;
  int saw_last_topic;
  int saw_first_related;
  int saw_last_related;
} ext_capacity_sink_t;

static ext_capacity_sink_t cap_sink;

/** @brief Append an unsigned decimal index to a bounded test prefix. */
static int ext_make_indexed_text(char *buffer, size_t capacity, const char *prefix, size_t index) {
  char digits[24];
  size_t prefix_len = strlen(prefix);
  size_t digit_count = 0u;
  size_t value = index;
  if (capacity == 0u || prefix_len + 1u >= capacity) return 0;
  do {
    digits[digit_count] = (char)('0' + (value % 10u));
    digit_count += 1u;
    value /= 10u;
  } while (value != 0u && digit_count < sizeof(digits));
  if (value != 0u || prefix_len + digit_count + 1u > capacity) return 0;
  memcpy(buffer, prefix, prefix_len);
  while (digit_count > 0u) {
    digit_count -= 1u;
    buffer[prefix_len] = digits[digit_count];
    prefix_len += 1u;
  }
  buffer[prefix_len] = '\0';
  return 1;
}

/** @brief Initialize generated prose lists for the configured text-item capacity. */
static int ext_capacity_init_text_items(void) {
  size_t index;
  for (index = 0u; index < (size_t)BSC_MAX_HELP_TEXT_ITEMS; ++index) {
    if (!ext_make_indexed_text(cap_note_storage[index], sizeof(cap_note_storage[index]), "note", index)) return 0;
    if (!ext_make_indexed_text(cap_warning_storage[index], sizeof(cap_warning_storage[index]), "warning", index)) return 0;
    cap_note_items[index] = cap_note_storage[index];
    cap_warning_items[index] = cap_warning_storage[index];
  }
  return 1;
}

/** @brief Initialize generated examples for the configured example capacity. */
static int ext_capacity_init_examples(void) {
  size_t index;
  for (index = 0u; index < (size_t)BSC_MAX_HELP_EXAMPLES; ++index) {
    if (!ext_make_indexed_text(cap_example_line_storage[index], sizeof(cap_example_line_storage[index]), "ex", index)) {
      return 0;
    }
    cap_examples[index].line = cap_example_line_storage[index];
    if ((index % 2u) == 1u) {
      if (!ext_make_indexed_text(cap_example_desc_storage[index], sizeof(cap_example_desc_storage[index]), "desc", index)) {
        return 0;
      }
      cap_examples[index].description = cap_example_desc_storage[index];
    } else {
      cap_examples[index].description = NULL;
    }
  }
  return 1;
}

/** @brief Initialize a generated visible command table and exact related references. */
static int ext_capacity_init_related(void) {
  size_t index;
  for (index = 0u; index < (size_t)BSC_MAX_HELP_RELATED; ++index) {
    if (!ext_make_indexed_text(cap_related_path_storage[index], sizeof(cap_related_path_storage[index]), "rel", index)) {
      return 0;
    }
    if (!ext_make_indexed_text(cap_related_summary_storage[index], sizeof(cap_related_summary_storage[index]), "Related ", index)) {
      return 0;
    }
    cap_related_paths[index][0] = cap_related_path_storage[index];
    cap_related_commands[index].path = cap_related_paths[index];
    cap_related_commands[index].path_len = 1u;
    cap_related_commands[index].node_type = BSC_NODE_COMMAND;
    cap_related_commands[index].args = NULL;
    cap_related_commands[index].arg_count = 0u;
    cap_related_commands[index].handler = extended_forbidden_handler;
    cap_related_commands[index].command_context = NULL;
    cap_related_commands[index].access = BSC_ACCESS_NORMAL;
    cap_related_commands[index].flags = BSC_COMMAND_FLAG_NONE;
    cap_related_commands[index].access_fn = extended_forbidden_access;
    cap_related_commands[index].summary = cap_related_summary_storage[index];
    cap_related_commands[index].description = "Related command.";
    cap_related[index].target = &cap_related_commands[index];
  }
  return 1;
}

/** @brief Initialize generated unique topics under one visible parent. */
static int ext_capacity_init_topics(const bsc_command_t *parent) {
  size_t index;
  for (index = 0u; index < (size_t)BSC_MAX_HELP_TOPICS; ++index) {
    if (!ext_make_indexed_text(cap_topic_id_storage[index], sizeof(cap_topic_id_storage[index]), "t", index)) return 0;
    if (!ext_make_indexed_text(cap_topic_summary_storage[index], sizeof(cap_topic_summary_storage[index]), "Topic ", index)) {
      return 0;
    }
    cap_topics[index].parent = parent;
    cap_topics[index].id = cap_topic_id_storage[index];
    cap_topics[index].summary = cap_topic_summary_storage[index];
    cap_topics[index].description = NULL;
    cap_topics[index].notes.items = NULL;
    cap_topics[index].notes.count = 0u;
    cap_topics[index].warnings.items = NULL;
    cap_topics[index].warnings.count = 0u;
    cap_topics[index].examples = NULL;
    cap_topics[index].example_count = 0u;
    cap_topics[index].related = NULL;
    cap_topics[index].related_count = 0u;
  }
  return 1;
}

/** @brief Reset the streaming capacity sink before one render. */
static void ext_capacity_sink_init(ext_capacity_sink_t *sink) {
  memset(sink, 0, sizeof(*sink));
}

/** @brief Return nonzero when the current line equals a generated two-space item line. */
static int ext_capacity_line_matches_item(const ext_capacity_sink_t *sink, const char *value) {
  size_t value_len = strlen(value);
  return sink->line_used == value_len + 2u && sink->line[0] == ' ' && sink->line[1] == ' ' &&
         memcmp(&sink->line[2], value, value_len) == 0;
}

/** @brief Return nonzero when the current line equals a generated bullet item line. */
static int ext_capacity_line_matches_bullet(const ext_capacity_sink_t *sink, const char *value) {
  size_t value_len = strlen(value);
  return sink->line_used == value_len + 4u && memcmp(sink->line, "  - ", 4u) == 0 &&
         memcmp(&sink->line[4], value, value_len) == 0;
}

/** @brief Return nonzero when the current line equals one generated descriptor entry. */
static int ext_capacity_line_matches_entry(const ext_capacity_sink_t *sink, size_t index) {
  size_t path_len = strlen(cap_related_path_storage[index]);
  size_t summary_len = strlen(cap_related_summary_storage[index]);
  return sink->line_used == 2u + path_len + 3u + summary_len && memcmp(sink->line, "  ", 2u) == 0 &&
         memcmp(&sink->line[2], cap_related_path_storage[index], path_len) == 0 &&
         memcmp(&sink->line[2u + path_len], " - ", 3u) == 0 &&
         memcmp(&sink->line[2u + path_len + 3u], cap_related_summary_storage[index], summary_len) == 0;
}

/** @brief Return nonzero when the current line equals one generated topic entry. */
static int ext_capacity_line_matches_topic(const ext_capacity_sink_t *sink, size_t index) {
  size_t id_len = strlen(cap_topic_id_storage[index]);
  size_t summary_len = strlen(cap_topic_summary_storage[index]);
  return sink->line_used == 2u + id_len + 3u + summary_len && memcmp(sink->line, "  ", 2u) == 0 &&
         memcmp(&sink->line[2], cap_topic_id_storage[index], id_len) == 0 &&
         memcmp(&sink->line[2u + id_len], " - ", 3u) == 0 &&
         memcmp(&sink->line[2u + id_len + 3u], cap_topic_summary_storage[index], summary_len) == 0;
}

/** @brief Update capacity counters for one completed LF-terminated output line. */
static void ext_capacity_sink_process_line(ext_capacity_sink_t *sink) {
  sink->line[sink->line_used] = '\0';
  if (strcmp(sink->line, "NOTES") == 0) sink->section = EXT_CAP_SECTION_NOTES;
  else if (strcmp(sink->line, "WARNINGS") == 0) sink->section = EXT_CAP_SECTION_WARNINGS;
  else if (strcmp(sink->line, "EXAMPLES") == 0) sink->section = EXT_CAP_SECTION_EXAMPLES;
  else if (strcmp(sink->line, "TOPICS") == 0) sink->section = EXT_CAP_SECTION_TOPICS;
  else if (strcmp(sink->line, "RELATED") == 0) sink->section = EXT_CAP_SECTION_RELATED;
  else if (sink->line_used == 0u) sink->section = EXT_CAP_SECTION_NONE;
  else if (sink->section == EXT_CAP_SECTION_NOTES && memcmp(sink->line, "  - ", 4u) == 0) {
#if BSC_MAX_HELP_TEXT_ITEMS > 0u
    if (ext_capacity_line_matches_bullet(sink, cap_note_storage[0])) sink->saw_first_note = 1;
    if (ext_capacity_line_matches_bullet(sink, cap_note_storage[BSC_MAX_HELP_TEXT_ITEMS - 1u])) sink->saw_last_note = 1;
#endif
    sink->notes += 1u;
  } else if (sink->section == EXT_CAP_SECTION_WARNINGS && memcmp(sink->line, "  - ", 4u) == 0) {
#if BSC_MAX_HELP_TEXT_ITEMS > 0u
    if (ext_capacity_line_matches_bullet(sink, cap_warning_storage[0])) sink->saw_first_warning = 1;
    if (ext_capacity_line_matches_bullet(sink, cap_warning_storage[BSC_MAX_HELP_TEXT_ITEMS - 1u])) sink->saw_last_warning = 1;
#endif
    sink->warnings += 1u;
  } else if (sink->section == EXT_CAP_SECTION_EXAMPLES && sink->line_used >= 2u &&
             sink->line[0] == ' ' && sink->line[1] == ' ' && (sink->line_used < 4u || sink->line[2] != ' ' || sink->line[3] != ' ')) {
#if BSC_MAX_HELP_EXAMPLES > 0u
    if (ext_capacity_line_matches_item(sink, cap_example_line_storage[0])) sink->saw_first_example = 1;
    if (ext_capacity_line_matches_item(sink, cap_example_line_storage[BSC_MAX_HELP_EXAMPLES - 1u])) sink->saw_last_example = 1;
#endif
    sink->examples += 1u;
  } else if (sink->section == EXT_CAP_SECTION_TOPICS && sink->line_used >= 2u && sink->line[0] == ' ' && sink->line[1] == ' ') {
#if BSC_MAX_HELP_TOPICS > 0u
    if (ext_capacity_line_matches_topic(sink, 0u)) sink->saw_first_topic = 1;
    if (ext_capacity_line_matches_topic(sink, BSC_MAX_HELP_TOPICS - 1u)) sink->saw_last_topic = 1;
#endif
    sink->topics += 1u;
  } else if (sink->section == EXT_CAP_SECTION_RELATED && sink->line_used >= 2u && sink->line[0] == ' ' && sink->line[1] == ' ') {
#if BSC_MAX_HELP_RELATED > 0u
    if (ext_capacity_line_matches_entry(sink, 0u)) sink->saw_first_related = 1;
    if (ext_capacity_line_matches_entry(sink, BSC_MAX_HELP_RELATED - 1u)) sink->saw_last_related = 1;
#endif
    sink->related += 1u;
  }
  sink->line_used = 0u;
}

/** @brief Streaming sink that retains only one bounded line and capacity counters. */
static size_t ext_capacity_write(void *user, const char *data, size_t length) {
  ext_capacity_sink_t *sink = (ext_capacity_sink_t *)user;
  size_t index;
  if (data == NULL && length != 0u) return 0u;
  for (index = 0u; index < length; ++index) {
    char byte = data[index];
    if (byte == '\n') {
      ext_capacity_sink_process_line(sink);
    } else if (sink->line_used + 1u < sizeof(sink->line)) {
      sink->line[sink->line_used] = byte;
      sink->line_used += 1u;
    } else {
      sink->overflow = 1;
    }
  }
  return length;
}

/** @brief Render catalog path through the streaming capacity sink. */
static bsc_status_t ext_capacity_render_path(const bsc_help_catalog_t *catalog, const char *const *path, size_t path_len) {
  bsc_string_view_t tokens[BSC_MAX_PATH_TOKENS];
  bsc_output_t output = {ext_capacity_write, &cap_sink};
  size_t index;
  for (index = 0u; index < path_len; ++index) tokens[index] = extended_token(path[index]);
  ext_capacity_sink_init(&cap_sink);
  return bsc_help_render_catalog_path(catalog, tokens, path_len, NULL, &output);
}

/** @brief Render topic page through the streaming capacity sink. */
static bsc_status_t ext_capacity_render_topic(const bsc_help_catalog_t *catalog,
                                              const char *const *path,
                                              size_t path_len,
                                              const char *topic_id) {
  bsc_string_view_t tokens[BSC_MAX_PATH_TOKENS];
  bsc_output_t output = {ext_capacity_write, &cap_sink};
  size_t index;
  for (index = 0u; index < path_len; ++index) tokens[index] = extended_token(path[index]);
  ext_capacity_sink_init(&cap_sink);
  return bsc_help_render_topic(catalog, tokens, path_len, extended_token(topic_id), NULL, &output);
}

/** @brief Verify configured maximum note and warning counts render exactly. */
static int test_extended_render_maximum_text_items(const char *test_name) {
  bsc_help_catalog_t catalog = extended_valid_catalog();
  bsc_help_target_t target = {&extended_commands[0], {NULL, 0u}, {NULL, 0u}, NULL, 0u, NULL, 0u};
  EXT_ASSERT_TRUE(ext_capacity_init_text_items());
#if BSC_MAX_HELP_TEXT_ITEMS > 0u
  target.notes.items = cap_note_items;
  target.notes.count = BSC_MAX_HELP_TEXT_ITEMS;
  target.warnings.items = cap_warning_items;
  target.warnings.count = BSC_MAX_HELP_TEXT_ITEMS;
#endif
  catalog.targets = &target;
  catalog.target_count = 1u;
  catalog.topics = NULL;
  catalog.topic_count = 0u;
  EXT_ASSERT_STATUS(BSC_STATUS_OK, ext_capacity_render_path(&catalog, path_status, 1u));
  EXT_ASSERT_TRUE(cap_sink.overflow == 0);
  EXT_ASSERT_TRUE(cap_sink.notes == (size_t)BSC_MAX_HELP_TEXT_ITEMS);
  EXT_ASSERT_TRUE(cap_sink.warnings == (size_t)BSC_MAX_HELP_TEXT_ITEMS);
#if BSC_MAX_HELP_TEXT_ITEMS > 0u
  EXT_ASSERT_TRUE(cap_sink.saw_first_note && cap_sink.saw_last_note);
  EXT_ASSERT_TRUE(cap_sink.saw_first_warning && cap_sink.saw_last_warning);
#else
  EXT_ASSERT_TRUE(cap_sink.notes == 0u && cap_sink.warnings == 0u);
#endif
  return 0;
}

/** @brief Verify configured maximum presentation example count renders exactly. */
static int test_extended_render_maximum_examples(const char *test_name) {
  bsc_help_catalog_t catalog = extended_valid_catalog();
  bsc_help_target_t target = {&extended_commands[0], {NULL, 0u}, {NULL, 0u}, NULL, 0u, NULL, 0u};
  EXT_ASSERT_TRUE(ext_capacity_init_examples());
#if BSC_MAX_HELP_EXAMPLES > 0u
  target.examples = cap_examples;
  target.example_count = BSC_MAX_HELP_EXAMPLES;
#endif
  catalog.targets = &target;
  catalog.target_count = 1u;
  catalog.topics = NULL;
  catalog.topic_count = 0u;
  EXT_ASSERT_STATUS(BSC_STATUS_OK, ext_capacity_render_path(&catalog, path_status, 1u));
  EXT_ASSERT_TRUE(cap_sink.overflow == 0);
  EXT_ASSERT_TRUE(cap_sink.examples == (size_t)BSC_MAX_HELP_EXAMPLES);
#if BSC_MAX_HELP_EXAMPLES > 0u
  EXT_ASSERT_TRUE(cap_sink.saw_first_example && cap_sink.saw_last_example);
#else
  EXT_ASSERT_TRUE(cap_sink.examples == 0u);
#endif
  return 0;
}

/** @brief Verify configured maximum related count renders exactly through topic metadata. */
static int test_extended_render_maximum_related(const char *test_name) {
  bsc_help_catalog_t catalog;
  bsc_help_topic_t topic;
  EXT_ASSERT_TRUE(ext_capacity_init_related());
  if (BSC_MAX_HELP_RELATED == 0u) {
    catalog = extended_valid_catalog();
    catalog.targets = NULL;
    catalog.target_count = 0u;
    catalog.topics = NULL;
    catalog.topic_count = 0u;
    EXT_ASSERT_STATUS(BSC_STATUS_OK, ext_capacity_render_path(&catalog, path_status, 1u));
    EXT_ASSERT_TRUE(cap_sink.related == 0u);
    return 0;
  }
  topic.parent = &cap_related_commands[0];
  topic.id = "rel";
  topic.summary = "Related topic";
  topic.description = NULL;
  topic.notes.items = NULL;
  topic.notes.count = 0u;
  topic.warnings.items = NULL;
  topic.warnings.count = 0u;
  topic.examples = NULL;
  topic.example_count = 0u;
  topic.related = cap_related;
  topic.related_count = BSC_MAX_HELP_RELATED;
  catalog.commands = cap_related_commands;
  catalog.command_count = BSC_MAX_HELP_RELATED;
  catalog.targets = NULL;
  catalog.target_count = 0u;
  catalog.topics = &topic;
  catalog.topic_count = 1u;
  EXT_ASSERT_STATUS(BSC_STATUS_OK, ext_capacity_render_topic(&catalog, cap_related_paths[0], 1u, "rel"));
  EXT_ASSERT_TRUE(cap_sink.overflow == 0);
  EXT_ASSERT_TRUE(cap_sink.related == (size_t)BSC_MAX_HELP_RELATED);
  EXT_ASSERT_TRUE(cap_sink.saw_first_related && cap_sink.saw_last_related);
  return 0;
}

/** @brief Verify configured maximum flat topic count renders exactly under one parent. */
static int test_extended_render_maximum_topics(const char *test_name) {
  bsc_help_catalog_t catalog = extended_valid_catalog();
  EXT_ASSERT_TRUE(ext_capacity_init_topics(&extended_commands[0]));
  catalog.targets = NULL;
  catalog.target_count = 0u;
#if BSC_MAX_HELP_TOPICS > 0u
  catalog.topics = cap_topics;
  catalog.topic_count = BSC_MAX_HELP_TOPICS;
#else
  catalog.topics = NULL;
  catalog.topic_count = 0u;
#endif
  EXT_ASSERT_STATUS(BSC_STATUS_OK, ext_capacity_render_path(&catalog, path_status, 1u));
  EXT_ASSERT_TRUE(cap_sink.overflow == 0);
  EXT_ASSERT_TRUE(cap_sink.topics == (size_t)BSC_MAX_HELP_TOPICS);
#if BSC_MAX_HELP_TOPICS > 0u
  EXT_ASSERT_TRUE(cap_sink.saw_first_topic && cap_sink.saw_last_topic);
#else
  EXT_ASSERT_TRUE(cap_sink.topics == 0u);
#endif
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
#if EXT_LOOKUP_FIXTURE_SUPPORTED
  EXT_RUN_TEST(test_topic_lookup_success_and_identity);
  EXT_RUN_TEST(test_topic_lookup_same_id_under_different_parents);
  EXT_RUN_TEST(test_topic_lookup_visibility_options);
  EXT_RUN_TEST(test_topic_lookup_failure_statuses_and_precedence);
  EXT_RUN_TEST(test_topic_lookup_validation_failures);
#endif
#if EXT_RENDER_FIXTURE_SUPPORTED
  EXT_RUN_TEST(test_extended_render_golden_outputs);
#endif
  EXT_RUN_TEST(test_extended_no_metadata_matches_ordinary);
#if EXT_RENDER_FIXTURE_SUPPORTED
  EXT_RUN_TEST(test_extended_render_precedence_and_failures);
  EXT_RUN_TEST(test_extended_render_visibility_options);
  EXT_RUN_TEST(test_extended_render_validation_before_output_gaps);
  EXT_RUN_TEST(test_extended_render_invalid_sink_precedence);
  EXT_RUN_TEST(test_extended_render_optional_sections_and_edges);
#endif
  EXT_RUN_TEST(test_extended_render_maximum_text_items);
  EXT_RUN_TEST(test_extended_render_maximum_examples);
  EXT_RUN_TEST(test_extended_render_maximum_related);
  EXT_RUN_TEST(test_extended_render_maximum_topics);
#if EXT_RENDER_FIXTURE_SUPPORTED
  EXT_RUN_TEST(test_extended_all_filtered_related_omission);
  EXT_RUN_TEST(test_extended_render_short_writes);
#endif
  return failures;
}
