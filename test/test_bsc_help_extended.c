#include "bsc_help.h"

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

/** @brief Aggregate pure extended-help topic lookup tests. */
int bsc_run_help_extended_tests(void) {
  int failures = 0;
  EXT_RUN_TEST(test_topic_result_clear);
  EXT_RUN_TEST(test_topic_lookup_success_and_identity);
  EXT_RUN_TEST(test_topic_lookup_same_id_under_different_parents);
  EXT_RUN_TEST(test_topic_lookup_visibility_options);
  EXT_RUN_TEST(test_topic_lookup_failure_statuses_and_precedence);
  EXT_RUN_TEST(test_topic_lookup_validation_failures);
  return failures;
}
