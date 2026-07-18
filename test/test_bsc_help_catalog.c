#include "bsc_help.h"

#include "bsc_config.h"

#include <stdio.h>
#include <string.h>

#define CAT_ASSERT_TRUE(condition)                                                                \
  do {                                                                                            \
    if (!(condition)) {                                                                           \
      printf("FAIL: %s: %s:%d: %s\n", test_name, __FILE__, __LINE__, #condition);                \
      return 1;                                                                                   \
    }                                                                                             \
  } while (0)

#define CAT_ASSERT_STATUS(expected, actual) CAT_ASSERT_TRUE((expected) == (actual))

#define CAT_RUN_TEST(fn)                                                                          \
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
static const char *const path_settings[] = {"settings"};
static const char *const path_secret[] = {"secret", "set"};
static const char *const path_alpha[] = {"alpha"};
static const char *const path_beta[] = {"beta"};
static const char *const path_gamma[] = {"gamma"};
static int handler_calls;
static int access_calls;

/** @brief Handler sentinel that catalog validation must never invoke. */
static bsc_status_t catalog_forbidden_handler(void *app_context,
                                              const bsc_command_t *command,
                                              const struct bsc_parsed_args *args,
                                              bsc_output_t *output) {
  (void)app_context;
  (void)command;
  (void)args;
  (void)output;
  handler_calls += 1;
  return BSC_STATUS_OK;
}

/** @brief Access sentinel that catalog validation must never invoke. */
static bool catalog_forbidden_access(void *app_context,
                                     const bsc_command_t *command,
                                     bsc_access_level_t required_access) {
  (void)app_context;
  (void)command;
  (void)required_access;
  access_calls += 1;
  return true;
}

static const bsc_command_t catalog_commands[] = {
    {path_status, 1u, BSC_NODE_COMMAND, NULL, 0u, catalog_forbidden_handler, NULL, BSC_ACCESS_NORMAL,
     BSC_COMMAND_FLAG_NONE, catalog_forbidden_access, "Status", "Status."},
    {path_settings, 1u, BSC_NODE_GROUP, NULL, 0u, NULL, NULL, BSC_ACCESS_FACTORY,
     BSC_COMMAND_FLAG_HIDDEN, catalog_forbidden_access, "Settings", NULL},
    {path_secret, 2u, BSC_NODE_COMMAND, NULL, 0u, catalog_forbidden_handler, NULL, BSC_ACCESS_LOCKED,
     BSC_COMMAND_FLAG_NONE, catalog_forbidden_access, "Secret", "Secret."},
    {path_alpha, 1u, BSC_NODE_COMMAND, NULL, 0u, catalog_forbidden_handler, NULL, BSC_ACCESS_NORMAL,
     BSC_COMMAND_FLAG_NONE, catalog_forbidden_access, "Alpha", "Alpha."},
    {path_beta, 1u, BSC_NODE_COMMAND, NULL, 0u, catalog_forbidden_handler, NULL, BSC_ACCESS_NORMAL,
     BSC_COMMAND_FLAG_NONE, catalog_forbidden_access, "Beta", "Beta."},
    {path_gamma, 1u, BSC_NODE_COMMAND, NULL, 0u, catalog_forbidden_handler, NULL, BSC_ACCESS_NORMAL,
     BSC_COMMAND_FLAG_NONE, catalog_forbidden_access, "Gamma", "Gamma."},
};

static const bsc_command_t unrelated_command = {path_status, 1u, BSC_NODE_COMMAND, NULL, 0u,
                                                catalog_forbidden_handler, NULL, BSC_ACCESS_NORMAL,
                                                BSC_COMMAND_FLAG_NONE, NULL, "Other", "Other."};

/** @brief Return a minimal valid catalog around the shared descriptor table. */
static bsc_help_catalog_t valid_empty_catalog(void) {
  bsc_help_catalog_t catalog;
  catalog.commands = catalog_commands;
  catalog.command_count = sizeof(catalog_commands) / sizeof(catalog_commands[0]);
  catalog.targets = NULL;
  catalog.target_count = 0u;
  catalog.topics = NULL;
  catalog.topic_count = 0u;
  return catalog;
}

/** @brief Fill a string with repeated printable bytes and a trailing terminator. */
static void fill_string(char *buffer, size_t len) {
  size_t index;
  for (index = 0u; index < len; ++index) {
    buffer[index] = 'a';
  }
  buffer[len] = '\0';
}

/** @brief Assert that catalog validation returns one reason. */
static int expect_catalog_reason(const char *test_name,
                                 bsc_help_catalog_t *catalog,
                                 bsc_status_t expected_status,
                                 bsc_help_catalog_error_reason_t expected_reason) {
  bsc_help_catalog_validation_error_t error;
  bsc_help_catalog_validation_error_clear(&error);
  CAT_ASSERT_STATUS(expected_status, bsc_help_catalog_validate(catalog, &error));
  CAT_ASSERT_TRUE(error.reason == expected_reason);
  return 0;
}

/** @brief Assert status, reason, and selected diagnostic indexes while leaving unrelated indexes unconstrained. */
static int expect_catalog_diagnostic(const char *test_name,
                                     bsc_help_catalog_t *catalog,
                                     bsc_status_t expected_status,
                                     bsc_help_catalog_error_reason_t expected_reason,
                                     size_t expected_target_index,
                                     size_t expected_topic_index,
                                     size_t expected_example_index,
                                     size_t expected_related_index,
                                     size_t expected_command_index,
                                     size_t expected_duplicate_index) {
  const size_t ignored = (size_t)-1;
  bsc_help_catalog_validation_error_t error;
  bsc_help_catalog_validation_error_clear(&error);
  CAT_ASSERT_STATUS(expected_status, bsc_help_catalog_validate(catalog, &error));
  CAT_ASSERT_TRUE(error.reason == expected_reason);
  if (expected_target_index != ignored) CAT_ASSERT_TRUE(error.target_index == expected_target_index);
  if (expected_topic_index != ignored) CAT_ASSERT_TRUE(error.topic_index == expected_topic_index);
  if (expected_example_index != ignored) CAT_ASSERT_TRUE(error.example_index == expected_example_index);
  if (expected_related_index != ignored) CAT_ASSERT_TRUE(error.related_index == expected_related_index);
  if (expected_command_index != ignored) CAT_ASSERT_TRUE(error.command_index == expected_command_index);
  if (expected_duplicate_index != ignored) CAT_ASSERT_TRUE(error.duplicate_index == expected_duplicate_index);
  return 0;
}

/** @brief Write a small generated identifier into caller-owned storage and verify it stayed within the token bound. */
static int make_identifier(char *buffer, size_t capacity, const char *prefix, size_t index) {
  int written = snprintf(buffer, capacity, "%s%lu", prefix, (unsigned long)index);
  return written > 0 && (size_t)written < capacity && (size_t)written <= (size_t)BSC_MAX_TOKEN_LEN;
}

/** @brief Verify diagnostic clearing resets nested registry metadata and indexes. */
static int test_catalog_diagnostic_clear(const char *test_name) {
  bsc_help_catalog_validation_error_t error;
  error.reason = BSC_HELP_CATALOG_ERROR_DUPLICATE_TOPIC;
  error.target_index = 1u;
  error.topic_index = 2u;
  error.item_index = 3u;
  error.example_index = 4u;
  error.related_index = 5u;
  error.command_index = 6u;
  error.duplicate_index = 7u;
  error.registry_error.reason = BSC_REGISTRY_ERROR_ZERO_COMMANDS;
  bsc_help_catalog_validation_error_clear(&error);
  CAT_ASSERT_TRUE(error.reason == BSC_HELP_CATALOG_ERROR_NONE);
  CAT_ASSERT_TRUE(error.target_index == 0u && error.topic_index == 0u && error.item_index == 0u);
  CAT_ASSERT_TRUE(error.example_index == 0u && error.related_index == 0u && error.command_index == 0u);
  CAT_ASSERT_TRUE(error.duplicate_index == 0u);
  CAT_ASSERT_TRUE(error.registry_error.reason == BSC_REGISTRY_ERROR_NONE);
  bsc_help_catalog_validation_error_clear(NULL);
  return 0;
}

/** @brief Verify NULL catalog and nested registry diagnostics. */
static int test_catalog_api_and_registry_failures(const char *test_name) {
  bsc_help_catalog_t catalog = valid_empty_catalog();
  bsc_help_catalog_validation_error_t error;
  CAT_ASSERT_STATUS(BSC_STATUS_INTERNAL_ERROR, bsc_help_catalog_validate(NULL, &error));
  CAT_ASSERT_TRUE(error.reason == BSC_HELP_CATALOG_ERROR_NULL_CATALOG);
  catalog.commands = NULL;
  CAT_ASSERT_STATUS(BSC_STATUS_INVALID_DESCRIPTOR, bsc_help_catalog_validate(&catalog, &error));
  CAT_ASSERT_TRUE(error.reason == BSC_HELP_CATALOG_ERROR_REGISTRY_INVALID);
  CAT_ASSERT_TRUE(error.registry_error.reason == BSC_REGISTRY_ERROR_NULL_COMMANDS);
  catalog = valid_empty_catalog();
  catalog.command_count = 0u;
  CAT_ASSERT_STATUS(BSC_STATUS_INVALID_DESCRIPTOR, bsc_help_catalog_validate(&catalog, &error));
  CAT_ASSERT_TRUE(error.registry_error.reason == BSC_REGISTRY_ERROR_ZERO_COMMANDS);
  return 0;
}

/** @brief Verify empty catalogs and zero-count non-NULL arrays are accepted and ignored. */
static int test_catalog_empty_and_zero_count_pointer_policy(const char *test_name) {
  bsc_help_catalog_t catalog = valid_empty_catalog();
  bsc_help_target_t target = {&catalog_commands[0], {NULL, 0u}, {NULL, 0u}, NULL, 0u, NULL, 0u};
  bsc_help_topic_t topic = {&catalog_commands[0], "overview", "Overview", NULL, {NULL, 0u}, {NULL, 0u}, NULL, 0u, NULL, 0u};
  catalog.targets = &target;
  catalog.topics = &topic;
  CAT_ASSERT_STATUS(BSC_STATUS_OK, bsc_help_catalog_validate(&catalog, NULL));
  target.notes.items = (const char *const *)&path_status[0];
  target.examples = (const bsc_help_example_t *)&target;
  target.related = (const bsc_help_related_t *)&target;
  topic.warnings.items = (const char *const *)&path_status[0];
  topic.examples = (const bsc_help_example_t *)&topic;
  topic.related = (const bsc_help_related_t *)&topic;
  CAT_ASSERT_STATUS(BSC_STATUS_OK, bsc_help_catalog_validate(&catalog, NULL));
  return 0;
}

/** @brief Verify nonzero counts require non-NULL pointers and target_count is bounded by command_count. */
static int test_catalog_pointer_count_failures(const char *test_name) {
  bsc_help_catalog_t catalog = valid_empty_catalog();
  bsc_help_target_t target = {&catalog_commands[0], {NULL, 1u}, {NULL, 0u}, NULL, 0u, NULL, 0u};
  catalog.targets = NULL;
  catalog.target_count = 1u;
  CAT_ASSERT_TRUE(expect_catalog_reason(test_name, &catalog, BSC_STATUS_INVALID_DESCRIPTOR,
                                        BSC_HELP_CATALOG_ERROR_TARGETS_POINTER_COUNT) == 0);
  catalog = valid_empty_catalog();
  catalog.topics = NULL;
  catalog.topic_count = 1u;
  CAT_ASSERT_TRUE(expect_catalog_reason(test_name, &catalog, BSC_STATUS_INVALID_DESCRIPTOR,
                                        BSC_HELP_CATALOG_ERROR_TOPICS_POINTER_COUNT) == 0);
  catalog = valid_empty_catalog();
  catalog.targets = &target;
  catalog.target_count = 1u;
  CAT_ASSERT_TRUE(expect_catalog_reason(test_name, &catalog, BSC_STATUS_INVALID_DESCRIPTOR,
                                        BSC_HELP_CATALOG_ERROR_TEXT_LIST_POINTER_COUNT) == 0);
  catalog = valid_empty_catalog();
  target.notes.count = 0u;
  target.example_count = 1u;
  target.examples = NULL;
  catalog.targets = &target;
  catalog.target_count = 1u;
  CAT_ASSERT_TRUE(expect_catalog_reason(test_name, &catalog, BSC_STATUS_INVALID_DESCRIPTOR,
                                        BSC_HELP_CATALOG_ERROR_EXAMPLES_POINTER_COUNT) == 0);
  catalog = valid_empty_catalog();
  target.example_count = 0u;
  target.related_count = 1u;
  target.related = NULL;
  catalog.targets = &target;
  catalog.target_count = 1u;
  CAT_ASSERT_TRUE(expect_catalog_reason(test_name, &catalog, BSC_STATUS_INVALID_DESCRIPTOR,
                                        BSC_HELP_CATALOG_ERROR_RELATED_POINTER_COUNT) == 0);
  catalog = valid_empty_catalog();
  target.related_count = 0u;
  catalog.targets = &target;
  catalog.target_count = catalog.command_count + 1u;
  CAT_ASSERT_TRUE(expect_catalog_reason(test_name, &catalog, BSC_STATUS_INVALID_DESCRIPTOR,
                                        BSC_HELP_CATALOG_ERROR_TOO_MANY_TARGETS) == 0);
  return 0;
}

/** @brief Verify descriptor references are accepted only by exact element identity. */
static int test_catalog_descriptor_references(const char *test_name) {
  bsc_help_catalog_t catalog = valid_empty_catalog();
  bsc_help_target_t target = {&catalog_commands[0], {NULL, 0u}, {NULL, 0u}, NULL, 0u, NULL, 0u};
  bsc_help_catalog_validation_error_t error;
  catalog.targets = &target;
  catalog.target_count = 1u;
  CAT_ASSERT_STATUS(BSC_STATUS_OK, bsc_help_catalog_validate(&catalog, &error));
  target.target = &unrelated_command;
  CAT_ASSERT_STATUS(BSC_STATUS_INVALID_DESCRIPTOR, bsc_help_catalog_validate(&catalog, &error));
  CAT_ASSERT_TRUE(error.reason == BSC_HELP_CATALOG_ERROR_INVALID_DESCRIPTOR_REFERENCE);
  target.target = &catalog_commands[sizeof(catalog_commands) / sizeof(catalog_commands[0])];
  CAT_ASSERT_STATUS(BSC_STATUS_INVALID_DESCRIPTOR, bsc_help_catalog_validate(&catalog, &error));
  target.target = (const bsc_command_t *)((const char *)&catalog_commands[0] + 1u);
  CAT_ASSERT_STATUS(BSC_STATUS_INVALID_DESCRIPTOR, bsc_help_catalog_validate(&catalog, &error));
  return 0;
}

/** @brief Verify duplicate target metadata is rejected. */
static int test_catalog_duplicate_targets(const char *test_name) {
  bsc_help_catalog_t catalog = valid_empty_catalog();
  bsc_help_target_t targets[2] = {
      {&catalog_commands[0], {NULL, 0u}, {NULL, 0u}, NULL, 0u, NULL, 0u},
      {&catalog_commands[0], {NULL, 0u}, {NULL, 0u}, NULL, 0u, NULL, 0u},
  };
  bsc_help_catalog_validation_error_t error;
  catalog.targets = targets;
  catalog.target_count = 2u;
  CAT_ASSERT_STATUS(BSC_STATUS_INVALID_DESCRIPTOR, bsc_help_catalog_validate(&catalog, &error));
  CAT_ASSERT_TRUE(error.reason == BSC_HELP_CATALOG_ERROR_DUPLICATE_TARGET);
  CAT_ASSERT_TRUE(error.duplicate_index == 0u);
  return 0;
}

/** @brief Verify text-list bounds and text validation rules. */
static int test_catalog_text_lists(const char *test_name) {
  bsc_help_catalog_t catalog = valid_empty_catalog();
  const char *items[BSC_MAX_HELP_TEXT_ITEMS == 0 ? 1 : BSC_MAX_HELP_TEXT_ITEMS];
  bsc_help_target_t target = {&catalog_commands[0], {items, 0u}, {NULL, 0u}, NULL, 0u, NULL, 0u};
  char exact[BSC_MAX_HELP_TEXT_LEN + 1u];
  char over[BSC_MAX_HELP_TEXT_LEN + 2u];
  size_t index;
  fill_string(exact, (size_t)BSC_MAX_HELP_TEXT_LEN);
  fill_string(over, (size_t)BSC_MAX_HELP_TEXT_LEN + 1u);
  for (index = 0u; index < (size_t)BSC_MAX_HELP_TEXT_ITEMS; ++index) {
    items[index] = "note";
  }
  catalog.targets = &target;
  catalog.target_count = 1u;
  target.notes.count = (size_t)BSC_MAX_HELP_TEXT_ITEMS;
  CAT_ASSERT_STATUS(BSC_STATUS_OK, bsc_help_catalog_validate(&catalog, NULL));
  target.notes.count = (size_t)BSC_MAX_HELP_TEXT_ITEMS + 1u;
  CAT_ASSERT_TRUE(expect_catalog_reason(test_name, &catalog, BSC_STATUS_INVALID_DESCRIPTOR,
                                        BSC_HELP_CATALOG_ERROR_TOO_MANY_TEXT_ITEMS) == 0);
  if ((size_t)BSC_MAX_HELP_TEXT_ITEMS == 0u) {
    return 0;
  }
  target.notes.count = 1u;
  items[0] = NULL;
  CAT_ASSERT_TRUE(expect_catalog_reason(test_name, &catalog, BSC_STATUS_INVALID_DESCRIPTOR,
                                        BSC_HELP_CATALOG_ERROR_MISSING_TEXT) == 0);
  items[0] = "";
  CAT_ASSERT_TRUE(expect_catalog_reason(test_name, &catalog, BSC_STATUS_INVALID_DESCRIPTOR,
                                        BSC_HELP_CATALOG_ERROR_EMPTY_TEXT) == 0);
  items[0] = exact;
  CAT_ASSERT_STATUS(BSC_STATUS_OK, bsc_help_catalog_validate(&catalog, NULL));
  items[0] = over;
  CAT_ASSERT_TRUE(expect_catalog_reason(test_name, &catalog, BSC_STATUS_INVALID_DESCRIPTOR,
                                        BSC_HELP_CATALOG_ERROR_TEXT_TOO_LONG) == 0);
  items[0] = "bad\r";
  CAT_ASSERT_TRUE(expect_catalog_reason(test_name, &catalog, BSC_STATUS_INVALID_DESCRIPTOR,
                                        BSC_HELP_CATALOG_ERROR_INVALID_TEXT_CONTROL_BYTE) == 0);
  items[0] = "bad\n";
  CAT_ASSERT_TRUE(expect_catalog_reason(test_name, &catalog, BSC_STATUS_INVALID_DESCRIPTOR,
                                        BSC_HELP_CATALOG_ERROR_INVALID_TEXT_CONTROL_BYTE) == 0);
  items[0] = "bad\x01";
  CAT_ASSERT_TRUE(expect_catalog_reason(test_name, &catalog, BSC_STATUS_INVALID_DESCRIPTOR,
                                        BSC_HELP_CATALOG_ERROR_INVALID_TEXT_CONTROL_BYTE) == 0);
  items[0] = "bad\x7f";
  CAT_ASSERT_TRUE(expect_catalog_reason(test_name, &catalog, BSC_STATUS_INVALID_DESCRIPTOR,
                                        BSC_HELP_CATALOG_ERROR_INVALID_TEXT_CONTROL_BYTE) == 0);
  return 0;
}

/** @brief Verify deterministic example validation including optional-description boundaries and placeholders. */
static int test_catalog_examples(const char *test_name) {
  const size_t ignored = (size_t)-1;
  bsc_help_catalog_t catalog = valid_empty_catalog();
  bsc_help_example_t examples[BSC_MAX_HELP_EXAMPLES == 0 ? 1 : BSC_MAX_HELP_EXAMPLES];
  bsc_help_target_t target = {&catalog_commands[2], {NULL, 0u}, {NULL, 0u}, examples, 0u, NULL, 0u};
  char line_exact[BSC_MAX_LINE_LEN + 1u];
  char line_over[BSC_MAX_LINE_LEN + 2u];
  char desc_exact[BSC_MAX_HELP_TEXT_LEN + 1u];
  char desc_over[BSC_MAX_HELP_TEXT_LEN + 2u];
  bsc_help_topic_t topic;
  size_t index;
  fill_string(line_exact, (size_t)BSC_MAX_LINE_LEN);
  fill_string(line_over, (size_t)BSC_MAX_LINE_LEN + 1u);
  fill_string(desc_exact, (size_t)BSC_MAX_HELP_TEXT_LEN);
  fill_string(desc_over, (size_t)BSC_MAX_HELP_TEXT_LEN + 1u);
  for (index = 0u; index < (size_t)BSC_MAX_HELP_EXAMPLES; ++index) {
    examples[index].line = "secret set <secret>";
    examples[index].description = "Use <new-password> or ******** placeholders; token and password words are allowed.";
  }
  catalog.targets = &target;
  catalog.target_count = 1u;
  target.example_count = (size_t)BSC_MAX_HELP_EXAMPLES;
  CAT_ASSERT_STATUS(BSC_STATUS_OK, bsc_help_catalog_validate(&catalog, NULL));
  target.example_count = (size_t)BSC_MAX_HELP_EXAMPLES + 1u;
  CAT_ASSERT_TRUE(expect_catalog_reason(test_name, &catalog, BSC_STATUS_INVALID_DESCRIPTOR,
                                        BSC_HELP_CATALOG_ERROR_TOO_MANY_EXAMPLES) == 0);
  if ((size_t)BSC_MAX_HELP_EXAMPLES == 0u) {
    return 0;
  }
  target.example_count = 1u;
  examples[0].line = NULL;
  CAT_ASSERT_TRUE(expect_catalog_reason(test_name, &catalog, BSC_STATUS_INVALID_DESCRIPTOR,
                                        BSC_HELP_CATALOG_ERROR_MISSING_EXAMPLE_LINE) == 0);
  examples[0].line = "";
  CAT_ASSERT_TRUE(expect_catalog_reason(test_name, &catalog, BSC_STATUS_INVALID_DESCRIPTOR,
                                        BSC_HELP_CATALOG_ERROR_EMPTY_EXAMPLE_LINE) == 0);
  examples[0].line = line_exact;
  examples[0].description = NULL;
  CAT_ASSERT_STATUS(BSC_STATUS_OK, bsc_help_catalog_validate(&catalog, NULL));
  examples[0].description = desc_exact;
  CAT_ASSERT_STATUS(BSC_STATUS_OK, bsc_help_catalog_validate(&catalog, NULL));
  examples[0].line = line_over;
  CAT_ASSERT_TRUE(expect_catalog_reason(test_name, &catalog, BSC_STATUS_INVALID_DESCRIPTOR,
                                        BSC_HELP_CATALOG_ERROR_EXAMPLE_LINE_TOO_LONG) == 0);
  examples[0].line = "bad\n";
  CAT_ASSERT_TRUE(expect_catalog_reason(test_name, &catalog, BSC_STATUS_INVALID_DESCRIPTOR,
                                        BSC_HELP_CATALOG_ERROR_INVALID_EXAMPLE_LINE_CONTROL_BYTE) == 0);
  examples[0].line = "secret set <new-password>";
  examples[0].description = "";
  CAT_ASSERT_TRUE(expect_catalog_diagnostic(test_name, &catalog, BSC_STATUS_INVALID_DESCRIPTOR,
                                            BSC_HELP_CATALOG_ERROR_INVALID_EXAMPLE_DESCRIPTION, 0u, ignored, 0u,
                                            ignored, ignored, ignored) == 0);
  examples[0].description = desc_over;
  CAT_ASSERT_TRUE(expect_catalog_diagnostic(test_name, &catalog, BSC_STATUS_INVALID_DESCRIPTOR,
                                            BSC_HELP_CATALOG_ERROR_INVALID_EXAMPLE_DESCRIPTION, 0u, ignored, 0u,
                                            ignored, ignored, ignored) == 0);
  examples[0].description = "bad\r";
  CAT_ASSERT_TRUE(expect_catalog_diagnostic(test_name, &catalog, BSC_STATUS_INVALID_DESCRIPTOR,
                                            BSC_HELP_CATALOG_ERROR_INVALID_EXAMPLE_DESCRIPTION, 0u, ignored, 0u,
                                            ignored, ignored, ignored) == 0);
  examples[0].description = "bad\n";
  CAT_ASSERT_TRUE(expect_catalog_diagnostic(test_name, &catalog, BSC_STATUS_INVALID_DESCRIPTOR,
                                            BSC_HELP_CATALOG_ERROR_INVALID_EXAMPLE_DESCRIPTION, 0u, ignored, 0u,
                                            ignored, ignored, ignored) == 0);
  examples[0].description = "bad\x01";
  CAT_ASSERT_TRUE(expect_catalog_diagnostic(test_name, &catalog, BSC_STATUS_INVALID_DESCRIPTOR,
                                            BSC_HELP_CATALOG_ERROR_INVALID_EXAMPLE_DESCRIPTION, 0u, ignored, 0u,
                                            ignored, ignored, ignored) == 0);
  examples[0].description = "bad\x7f";
  CAT_ASSERT_TRUE(expect_catalog_diagnostic(test_name, &catalog, BSC_STATUS_INVALID_DESCRIPTOR,
                                            BSC_HELP_CATALOG_ERROR_INVALID_EXAMPLE_DESCRIPTION, 0u, ignored, 0u,
                                            ignored, ignored, ignored) == 0);

  examples[0].description = "Topic example description";
  target.example_count = 0u;
  catalog.targets = NULL;
  catalog.target_count = 0u;
  topic = (bsc_help_topic_t){&catalog_commands[0], "overview", "Overview", NULL,
                             {NULL, 0u}, {NULL, 0u}, examples, 1u, NULL, 0u};
  catalog.topics = &topic;
  catalog.topic_count = 1u;
  CAT_ASSERT_STATUS(BSC_STATUS_OK, bsc_help_catalog_validate(&catalog, NULL));
  examples[0].description = "";
  CAT_ASSERT_TRUE(expect_catalog_diagnostic(test_name, &catalog, BSC_STATUS_INVALID_DESCRIPTOR,
                                            BSC_HELP_CATALOG_ERROR_INVALID_EXAMPLE_DESCRIPTION, ignored, 0u, 0u,
                                            ignored, ignored, ignored) == 0);
  return 0;
}

/** @brief Verify topic count, parent, identifier, summary, description, and duplicate rules. */
static int test_catalog_topics(const char *test_name) {
  const size_t ignored = (size_t)-1;
  bsc_help_catalog_t catalog = valid_empty_catalog();
  /**
   * Caller-owned topic records and generated identifiers cover the configured exact limit plus one
   * over-capacity record without heap allocation; the one-entry floor keeps zero-capacity builds valid.
   */
  bsc_help_topic_t topics[(BSC_MAX_HELP_TOPICS == 0 ? 1 : BSC_MAX_HELP_TOPICS) + 1u];
  char topic_ids[(BSC_MAX_HELP_TOPICS == 0 ? 1 : BSC_MAX_HELP_TOPICS) + 1u][BSC_MAX_TOKEN_LEN + 1u];
  char id_exact[BSC_MAX_TOKEN_LEN + 1u];
  char id_over[BSC_MAX_TOKEN_LEN + 2u];
  char summary_exact[BSC_MAX_HELP_TEXT_LEN + 1u];
  char summary_over[BSC_MAX_HELP_TEXT_LEN + 2u];
  size_t index;
  fill_string(id_exact, (size_t)BSC_MAX_TOKEN_LEN);
  fill_string(id_over, (size_t)BSC_MAX_TOKEN_LEN + 1u);
  fill_string(summary_exact, (size_t)BSC_MAX_HELP_TEXT_LEN);
  fill_string(summary_over, (size_t)BSC_MAX_HELP_TEXT_LEN + 1u);
  for (index = 0u; index <= (size_t)BSC_MAX_HELP_TOPICS; ++index) {
    CAT_ASSERT_TRUE(make_identifier(topic_ids[index], sizeof(topic_ids[index]), "topic", index));
    topics[index].parent = &catalog_commands[index % (sizeof(catalog_commands) / sizeof(catalog_commands[0]))];
    topics[index].id = index == 0u ? id_exact : topic_ids[index];
    topics[index].summary = summary_exact;
    topics[index].description = NULL;
    topics[index].notes.items = NULL;
    topics[index].notes.count = 0u;
    topics[index].warnings.items = NULL;
    topics[index].warnings.count = 0u;
    topics[index].examples = NULL;
    topics[index].example_count = 0u;
    topics[index].related = NULL;
    topics[index].related_count = 0u;
  }
  catalog.topics = topics;
  catalog.topic_count = (size_t)BSC_MAX_HELP_TOPICS;
  CAT_ASSERT_STATUS(BSC_STATUS_OK, bsc_help_catalog_validate(&catalog, NULL));
  catalog.topic_count = (size_t)BSC_MAX_HELP_TOPICS + 1u;
  CAT_ASSERT_TRUE(expect_catalog_reason(test_name, &catalog, BSC_STATUS_INVALID_DESCRIPTOR,
                                        BSC_HELP_CATALOG_ERROR_TOO_MANY_TOPICS) == 0);
  if ((size_t)BSC_MAX_HELP_TOPICS == 0u) {
    return 0;
  }
  catalog.topic_count = 1u;
  topics[0].parent = &unrelated_command;
  CAT_ASSERT_TRUE(expect_catalog_reason(test_name, &catalog, BSC_STATUS_INVALID_DESCRIPTOR,
                                        BSC_HELP_CATALOG_ERROR_INVALID_DESCRIPTOR_REFERENCE) == 0);
  topics[0].parent = &catalog_commands[0];
  topics[0].id = NULL;
  CAT_ASSERT_TRUE(expect_catalog_reason(test_name, &catalog, BSC_STATUS_INVALID_DESCRIPTOR,
                                        BSC_HELP_CATALOG_ERROR_MISSING_TOPIC_ID) == 0);
  topics[0].id = "";
  CAT_ASSERT_TRUE(expect_catalog_reason(test_name, &catalog, BSC_STATUS_INVALID_DESCRIPTOR,
                                        BSC_HELP_CATALOG_ERROR_EMPTY_TOPIC_ID) == 0);
  topics[0].id = id_over;
  CAT_ASSERT_TRUE(expect_catalog_reason(test_name, &catalog, BSC_STATUS_INVALID_DESCRIPTOR,
                                        BSC_HELP_CATALOG_ERROR_TOPIC_ID_TOO_LONG) == 0);
  topics[0].id = "bad\x7f";
  CAT_ASSERT_TRUE(expect_catalog_reason(test_name, &catalog, BSC_STATUS_INVALID_DESCRIPTOR,
                                        BSC_HELP_CATALOG_ERROR_INVALID_TOPIC_ID_CONTROL_BYTE) == 0);
  topics[0].id = "overview";
  topics[0].summary = NULL;
  CAT_ASSERT_TRUE(expect_catalog_diagnostic(test_name, &catalog, BSC_STATUS_INVALID_DESCRIPTOR,
                                            BSC_HELP_CATALOG_ERROR_MISSING_TOPIC_SUMMARY, ignored, 0u, ignored,
                                            ignored, ignored, ignored) == 0);
  topics[0].summary = "";
  CAT_ASSERT_TRUE(expect_catalog_diagnostic(test_name, &catalog, BSC_STATUS_INVALID_DESCRIPTOR,
                                            BSC_HELP_CATALOG_ERROR_EMPTY_TOPIC_SUMMARY, ignored, 0u, ignored,
                                            ignored, ignored, ignored) == 0);
  topics[0].summary = summary_exact;
  topics[0].description = NULL;
  CAT_ASSERT_STATUS(BSC_STATUS_OK, bsc_help_catalog_validate(&catalog, NULL));
  topics[0].summary = summary_over;
  CAT_ASSERT_TRUE(expect_catalog_diagnostic(test_name, &catalog, BSC_STATUS_INVALID_DESCRIPTOR,
                                            BSC_HELP_CATALOG_ERROR_TOPIC_SUMMARY_TOO_LONG, ignored, 0u, ignored,
                                            ignored, ignored, ignored) == 0);
  topics[0].summary = "bad\r";
  CAT_ASSERT_TRUE(expect_catalog_diagnostic(test_name, &catalog, BSC_STATUS_INVALID_DESCRIPTOR,
                                            BSC_HELP_CATALOG_ERROR_INVALID_TOPIC_SUMMARY_CONTROL_BYTE, ignored, 0u,
                                            ignored, ignored, ignored, ignored) == 0);
  topics[0].summary = "bad\n";
  CAT_ASSERT_TRUE(expect_catalog_diagnostic(test_name, &catalog, BSC_STATUS_INVALID_DESCRIPTOR,
                                            BSC_HELP_CATALOG_ERROR_INVALID_TOPIC_SUMMARY_CONTROL_BYTE, ignored, 0u,
                                            ignored, ignored, ignored, ignored) == 0);
  topics[0].summary = "bad\x01";
  CAT_ASSERT_TRUE(expect_catalog_diagnostic(test_name, &catalog, BSC_STATUS_INVALID_DESCRIPTOR,
                                            BSC_HELP_CATALOG_ERROR_INVALID_TOPIC_SUMMARY_CONTROL_BYTE, ignored, 0u,
                                            ignored, ignored, ignored, ignored) == 0);
  topics[0].summary = "bad\x7f";
  CAT_ASSERT_TRUE(expect_catalog_diagnostic(test_name, &catalog, BSC_STATUS_INVALID_DESCRIPTOR,
                                            BSC_HELP_CATALOG_ERROR_INVALID_TOPIC_SUMMARY_CONTROL_BYTE, ignored, 0u,
                                            ignored, ignored, ignored, ignored) == 0);
  topics[0].summary = "Overview";
  topics[0].description = NULL;
  CAT_ASSERT_STATUS(BSC_STATUS_OK, bsc_help_catalog_validate(&catalog, NULL));
  topics[0].description = summary_exact;
  CAT_ASSERT_STATUS(BSC_STATUS_OK, bsc_help_catalog_validate(&catalog, NULL));
  topics[0].description = "";
  CAT_ASSERT_TRUE(expect_catalog_diagnostic(test_name, &catalog, BSC_STATUS_INVALID_DESCRIPTOR,
                                            BSC_HELP_CATALOG_ERROR_INVALID_TOPIC_DESCRIPTION, ignored, 0u, ignored,
                                            ignored, ignored, ignored) == 0);
  topics[0].description = summary_over;
  CAT_ASSERT_TRUE(expect_catalog_diagnostic(test_name, &catalog, BSC_STATUS_INVALID_DESCRIPTOR,
                                            BSC_HELP_CATALOG_ERROR_INVALID_TOPIC_DESCRIPTION, ignored, 0u, ignored,
                                            ignored, ignored, ignored) == 0);
  topics[0].description = "bad\r";
  CAT_ASSERT_TRUE(expect_catalog_diagnostic(test_name, &catalog, BSC_STATUS_INVALID_DESCRIPTOR,
                                            BSC_HELP_CATALOG_ERROR_INVALID_TOPIC_DESCRIPTION, ignored, 0u, ignored,
                                            ignored, ignored, ignored) == 0);
  topics[0].description = "bad\n";
  CAT_ASSERT_TRUE(expect_catalog_diagnostic(test_name, &catalog, BSC_STATUS_INVALID_DESCRIPTOR,
                                            BSC_HELP_CATALOG_ERROR_INVALID_TOPIC_DESCRIPTION, ignored, 0u, ignored,
                                            ignored, ignored, ignored) == 0);
  topics[0].description = "bad\x01";
  CAT_ASSERT_TRUE(expect_catalog_diagnostic(test_name, &catalog, BSC_STATUS_INVALID_DESCRIPTOR,
                                            BSC_HELP_CATALOG_ERROR_INVALID_TOPIC_DESCRIPTION, ignored, 0u, ignored,
                                            ignored, ignored, ignored) == 0);
  topics[0].description = "bad\x7f";
  CAT_ASSERT_TRUE(expect_catalog_diagnostic(test_name, &catalog, BSC_STATUS_INVALID_DESCRIPTOR,
                                            BSC_HELP_CATALOG_ERROR_INVALID_TOPIC_DESCRIPTION, ignored, 0u, ignored,
                                            ignored, ignored, ignored) == 0);
  topics[0].description = "Optional.";
  CAT_ASSERT_STATUS(BSC_STATUS_OK, bsc_help_catalog_validate(&catalog, NULL));
  catalog.topic_count = 2u;
  topics[1] = topics[0];
  topics[1].id = "OVERVIEW";
  CAT_ASSERT_TRUE(expect_catalog_diagnostic(test_name, &catalog, BSC_STATUS_INVALID_DESCRIPTOR,
                                            BSC_HELP_CATALOG_ERROR_DUPLICATE_TOPIC, ignored, 1u, ignored, ignored,
                                            ignored, 0u) == 0);
  topics[1].parent = &catalog_commands[1];
  CAT_ASSERT_STATUS(BSC_STATUS_OK, bsc_help_catalog_validate(&catalog, NULL));
  return 0;
}

/** @brief Verify related-command validation, self-reference policy, parent relation, and cycles. */
static int test_catalog_related(const char *test_name) {
  const size_t ignored = (size_t)-1;
  bsc_help_catalog_t catalog = valid_empty_catalog();
  bsc_help_related_t related[BSC_MAX_HELP_RELATED == 0 ? 2 : BSC_MAX_HELP_RELATED + 1u];
  bsc_help_target_t targets[2];
  bsc_help_topic_t topic;
  size_t index;

  if ((size_t)BSC_MAX_HELP_RELATED > (size_t)BSC_MAX_COMMANDS - 1u) {
    return 0;
  }
  {
    const size_t generated_count = (size_t)BSC_MAX_HELP_RELATED + 1u;
    /**
     * Generated descriptors provide one target plus one unique command per configured related entry.
     * All path token storage is automatic and outlives the synchronous validation calls in this block.
     */
    bsc_command_t generated_commands[BSC_MAX_HELP_RELATED + 1u];
    const char *generated_paths[BSC_MAX_HELP_RELATED + 1u][1];
    char generated_tokens[BSC_MAX_HELP_RELATED + 1u][BSC_MAX_TOKEN_LEN + 1u];
    bsc_help_target_t generated_target;

    for (index = 0u; index < generated_count; ++index) {
      CAT_ASSERT_TRUE(make_identifier(generated_tokens[index], sizeof(generated_tokens[index]), "rel", index));
      generated_paths[index][0] = generated_tokens[index];
      generated_commands[index] = (bsc_command_t){generated_paths[index], 1u, BSC_NODE_COMMAND, NULL, 0u,
                                                  catalog_forbidden_handler, NULL, BSC_ACCESS_NORMAL,
                                                  BSC_COMMAND_FLAG_NONE, catalog_forbidden_access, "Related", "Related."};
    }
    for (index = 0u; index < (size_t)BSC_MAX_HELP_RELATED; ++index) {
      related[index].target = &generated_commands[index + 1u];
    }
    generated_target = (bsc_help_target_t){&generated_commands[0], {NULL, 0u}, {NULL, 0u}, NULL, 0u,
                                           related, (size_t)BSC_MAX_HELP_RELATED};
    catalog.commands = generated_commands;
    catalog.command_count = generated_count;
    catalog.targets = &generated_target;
    catalog.target_count = 1u;
    catalog.topics = NULL;
    catalog.topic_count = 0u;
    CAT_ASSERT_STATUS(BSC_STATUS_OK, bsc_help_catalog_validate(&catalog, NULL));
    generated_target.related_count = (size_t)BSC_MAX_HELP_RELATED + 1u;
    CAT_ASSERT_TRUE(expect_catalog_reason(test_name, &catalog, BSC_STATUS_INVALID_DESCRIPTOR,
                                          BSC_HELP_CATALOG_ERROR_TOO_MANY_RELATED) == 0);
  }

  catalog = valid_empty_catalog();
  targets[0] = (bsc_help_target_t){&catalog_commands[0], {NULL, 0u}, {NULL, 0u}, NULL, 0u, related, 0u};
  targets[1] = (bsc_help_target_t){&catalog_commands[1], {NULL, 0u}, {NULL, 0u}, NULL, 0u, related, 1u};
  catalog.targets = targets;
  catalog.target_count = 1u;
  if ((size_t)BSC_MAX_HELP_RELATED == 0u) {
    return 0;
  }
  targets[0].related_count = 1u;
  related[0].target = NULL;
  CAT_ASSERT_TRUE(expect_catalog_diagnostic(test_name, &catalog, BSC_STATUS_INVALID_DESCRIPTOR,
                                            BSC_HELP_CATALOG_ERROR_INVALID_RELATED_DESCRIPTOR, 0u, ignored, ignored,
                                            0u, ignored, ignored) == 0);
  related[0].target = &unrelated_command;
  CAT_ASSERT_TRUE(expect_catalog_diagnostic(test_name, &catalog, BSC_STATUS_INVALID_DESCRIPTOR,
                                            BSC_HELP_CATALOG_ERROR_INVALID_RELATED_DESCRIPTOR, 0u, ignored, ignored,
                                            0u, ignored, ignored) == 0);
  related[0].target = &catalog_commands[0];
  CAT_ASSERT_TRUE(expect_catalog_diagnostic(test_name, &catalog, BSC_STATUS_INVALID_DESCRIPTOR,
                                            BSC_HELP_CATALOG_ERROR_TARGET_RELATED_SELF_REFERENCE, 0u, ignored, ignored,
                                            0u, 0u, ignored) == 0);
  related[0].target = &catalog_commands[1];
  related[1].target = &catalog_commands[1];
  targets[0].related_count = 2u;
  CAT_ASSERT_TRUE(expect_catalog_diagnostic(test_name, &catalog, BSC_STATUS_INVALID_DESCRIPTOR,
                                            BSC_HELP_CATALOG_ERROR_DUPLICATE_RELATED_DESCRIPTOR, 0u, ignored, ignored,
                                            1u, 1u, 0u) == 0);
  related[0].target = &catalog_commands[1];
  related[1].target = &catalog_commands[0];
  targets[0].related_count = 1u;
  targets[1].related_count = 0u;
  catalog.target_count = 2u;
  CAT_ASSERT_STATUS(BSC_STATUS_OK, bsc_help_catalog_validate(&catalog, NULL));
  topic = (bsc_help_topic_t){&catalog_commands[0], "overview", "Overview", NULL,
                             {NULL, 0u}, {NULL, 0u}, NULL, 0u, related, 1u};
  related[0].target = &catalog_commands[0];
  catalog.targets = NULL;
  catalog.target_count = 0u;
  catalog.topics = &topic;
  catalog.topic_count = 1u;
  CAT_ASSERT_STATUS(BSC_STATUS_OK, bsc_help_catalog_validate(&catalog, NULL));
  return 0;
}

/** @brief Verify structural validation ignores descriptor visibility and never invokes callbacks. */
static int test_catalog_visibility_and_callbacks_are_ignored(const char *test_name) {
  bsc_help_catalog_t catalog = valid_empty_catalog();
  bsc_help_target_t target = {&catalog_commands[1], {NULL, 0u}, {NULL, 0u}, NULL, 0u, NULL, 0u};
  handler_calls = 0;
  access_calls = 0;
  catalog.targets = &target;
  catalog.target_count = 1u;
  CAT_ASSERT_STATUS(BSC_STATUS_OK, bsc_help_catalog_validate(&catalog, NULL));
  CAT_ASSERT_TRUE(handler_calls == 0);
  CAT_ASSERT_TRUE(access_calls == 0);
  return 0;
}

/** @brief Run extended-help catalog structural validation tests. */
int bsc_run_help_catalog_tests(void) {
  int failures = 0;
  CAT_RUN_TEST(test_catalog_diagnostic_clear);
  CAT_RUN_TEST(test_catalog_api_and_registry_failures);
  CAT_RUN_TEST(test_catalog_empty_and_zero_count_pointer_policy);
  CAT_RUN_TEST(test_catalog_pointer_count_failures);
  CAT_RUN_TEST(test_catalog_descriptor_references);
  CAT_RUN_TEST(test_catalog_duplicate_targets);
  CAT_RUN_TEST(test_catalog_text_lists);
  CAT_RUN_TEST(test_catalog_examples);
  CAT_RUN_TEST(test_catalog_topics);
  CAT_RUN_TEST(test_catalog_related);
  CAT_RUN_TEST(test_catalog_visibility_and_callbacks_are_ignored);
  return failures;
}
