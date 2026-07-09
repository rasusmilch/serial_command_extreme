#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "bsc_config.h"
#include "bsc_matcher.h"
#include "bsc_output.h"
#include "bsc_status.h"
#include "bsc_string_view.h"
#include "bsc_types.h"

/**
 * @brief Fail the current matcher test when a condition is false.
 */
#define MATCH_TEST_ASSERT_TRUE(condition)                                                           \
  do {                                                                                              \
    if (!(condition)) {                                                                             \
      printf("FAIL: %s: %s:%d: %s\n", test_name, __FILE__, __LINE__, #condition);                  \
      return 1;                                                                                     \
    }                                                                                               \
  } while (0)

/**
 * @brief Compare an expected matcher status against the actual status.
 */
#define MATCH_TEST_ASSERT_STATUS(expected, actual) MATCH_TEST_ASSERT_TRUE((expected) == (actual))

/**
 * @brief Run one matcher test case and accumulate failures.
 */
#define RUN_MATCH_TEST(fn)                                                                          \
  do {                                                                                              \
    int result;                                                                                     \
    const char *test_name = #fn;                                                                    \
    result = fn(test_name);                                                                         \
    if (result != 0) {                                                                              \
      failures += 1;                                                                                \
    } else {                                                                                        \
      printf("PASS: %s\n", test_name);                                                            \
    }                                                                                               \
  } while (0)

static const char *const k_status_path[] = {"status"};
static const char *const k_flags_path[] = {"flags"};
static const char *const k_settings_path[] = {"settings"};
static const char *const k_settings_wifi_path[] = {"settings", "wifi"};
static const char *const k_settings_wifi_status_path[] = {"settings", "wifi", "status"};
static const char *const k_settings_wifi_set_ssid_path[] = {"settings", "wifi", "set", "ssid"};
static const char *const k_settings_wifi_set_ssid_case_path[] = {"Settings", "WIFI", "Set", "SSID"};
static const char *const k_deep_path[] = {"a", "b", "c", "d", "e", "f", "g"};
static const char *const k_null_token_path[] = {"settings", NULL};
static const char *const k_status_upper_path[] = {"STATUS"};

static int g_matcher_handler_calls;
static int g_matcher_access_calls;

/**
 * @brief Test handler that increments a counter if matcher accidentally dispatches.
 */
static bsc_status_t matcher_test_handler(void *app_context,
                                         const struct bsc_command *command,
                                         const struct bsc_parsed_args *args,
                                         bsc_output_t *output) {
  (void)app_context;
  (void)command;
  (void)args;
  (void)output;
  g_matcher_handler_calls += 1;
  return BSC_STATUS_OK;
}

/**
 * @brief Test access callback that records accidental access checks.
 */
static bool matcher_test_access(void *app_context,
                                const struct bsc_command *command,
                                bsc_access_level_t required_access) {
  (void)app_context;
  (void)command;
  (void)required_access;
  g_matcher_access_calls += 1;
  return true;
}

/**
 * @brief Build a minimal executable command descriptor for matcher fixtures.
 */
static bsc_command_t make_match_command(const char *const *path, size_t path_len) {
  bsc_command_t command;

  memset(&command, 0, sizeof(command));
  command.path = path;
  command.path_len = path_len;
  command.node_type = BSC_NODE_COMMAND;
  command.handler = matcher_test_handler;
  command.command_context = NULL;
  command.access = BSC_ACCESS_NORMAL;
  command.flags = BSC_COMMAND_FLAG_NONE;
  command.access_fn = matcher_test_access;
  command.summary = "summary";
  command.description = "description";
  return command;
}

/**
 * @brief Build a minimal group descriptor for matcher fixtures.
 */
static bsc_command_t make_match_group(const char *const *path, size_t path_len) {
  bsc_command_t group;

  memset(&group, 0, sizeof(group));
  group.path = path;
  group.path_len = path_len;
  group.node_type = BSC_NODE_GROUP;
  group.access = BSC_ACCESS_NORMAL;
  group.flags = BSC_COMMAND_FLAG_NONE;
  group.summary = "group";
  group.description = "group description";
  return group;
}

/**
 * @brief Create a borrowed token view from a null-terminated test literal.
 */
static bsc_string_view_t token_from_cstr(const char *text) {
  return bsc_string_view_from_cstr(text);
}

/**
 * @brief Return whether a matcher result is in the documented cleared state.
 */
static int result_is_cleared(const bsc_match_result_t *result) {
  return result->command == NULL && result->command_index == 0u &&
         result->consumed_token_count == 0u && result->remaining_token_index == 0u &&
         result->remaining_token_count == 0u && result->group == NULL && result->group_index == 0u;
}

/**
 * @brief Build a deliberately non-cleared result for failure-path tests.
 */
static bsc_match_result_t dirty_result(void) {
  bsc_match_result_t result;

  result.command = (const bsc_command_t *)1;
  result.command_index = 2u;
  result.consumed_token_count = 3u;
  result.remaining_token_index = 4u;
  result.remaining_token_count = 5u;
  result.group = (const bsc_command_t *)6;
  result.group_index = 7u;
  return result;
}

/**
 * @brief Verify result clearing handles NULL and resets every result field.
 */
static int test_match_result_clear(const char *test_name) {
  bsc_match_result_t result = dirty_result();

  bsc_match_result_clear(NULL);
  bsc_match_result_clear(&result);
  MATCH_TEST_ASSERT_TRUE(result_is_cleared(&result));
  return 0;
}

/**
 * @brief Verify a simple executable path match populates command result fields.
 */
static int test_simple_executable_match(const char *test_name) {
  bsc_command_t commands[] = {make_match_command(k_status_path, 1u)};
  bsc_string_view_t tokens[] = {token_from_cstr("status")};
  bsc_match_result_t result;

  MATCH_TEST_ASSERT_STATUS(BSC_STATUS_OK,
                           bsc_match_command(commands, 1u, tokens, 1u, &result));
  MATCH_TEST_ASSERT_TRUE(result.command == &commands[0]);
  MATCH_TEST_ASSERT_TRUE(result.command_index == 0u);
  MATCH_TEST_ASSERT_TRUE(result.consumed_token_count == 1u);
  MATCH_TEST_ASSERT_TRUE(result.remaining_token_index == 1u);
  MATCH_TEST_ASSERT_TRUE(result.remaining_token_count == 0u);
  MATCH_TEST_ASSERT_TRUE(result.group == NULL);
  MATCH_TEST_ASSERT_TRUE(result.group_index == 0u);
  return 0;
}

/**
 * @brief Verify command_index identifies a matched descriptor after index zero.
 */
static int test_match_command_at_nonzero_index(const char *test_name) {
  bsc_command_t commands[] = {
      make_match_command(k_flags_path, 1u),
      make_match_command(k_status_path, 1u),
  };
  bsc_string_view_t tokens[] = {token_from_cstr("status")};
  bsc_match_result_t result;

  MATCH_TEST_ASSERT_STATUS(BSC_STATUS_OK,
                           bsc_match_command(commands, 2u, tokens, 1u, &result));
  MATCH_TEST_ASSERT_TRUE(result.command == &commands[1]);
  MATCH_TEST_ASSERT_TRUE(result.command_index == 1u);
  return 0;
}

/**
 * @brief Verify the longest executable path wins and leaves remaining args.
 */
static int test_longest_executable_match_leaves_remaining_arg(const char *test_name) {
  bsc_command_t commands[] = {
      make_match_group(k_settings_path, 1u),
      make_match_group(k_settings_wifi_path, 2u),
      make_match_command(k_settings_wifi_status_path, 3u),
      make_match_command(k_settings_wifi_set_ssid_path, 4u),
  };
  bsc_string_view_t tokens[] = {
      token_from_cstr("settings"), token_from_cstr("wifi"), token_from_cstr("set"),
      token_from_cstr("ssid"), token_from_cstr("Shop_AP"),
  };
  bsc_match_result_t result;

  MATCH_TEST_ASSERT_STATUS(BSC_STATUS_OK,
                           bsc_match_command(commands, 4u, tokens, 5u, &result));
  MATCH_TEST_ASSERT_TRUE(result.command == &commands[3]);
  MATCH_TEST_ASSERT_TRUE(result.command_index == 3u);
  MATCH_TEST_ASSERT_TRUE(result.consumed_token_count == 4u);
  MATCH_TEST_ASSERT_TRUE(result.remaining_token_index == 4u);
  MATCH_TEST_ASSERT_TRUE(result.remaining_token_count == 1u);
  return 0;
}

/**
 * @brief Verify extra tokens after a command remain for future argument parsing.
 */
static int test_extra_token_is_remaining_arg(const char *test_name) {
  bsc_command_t commands[] = {make_match_command(k_status_path, 1u)};
  bsc_string_view_t tokens[] = {token_from_cstr("status"), token_from_cstr("extra")};
  bsc_match_result_t result;

  MATCH_TEST_ASSERT_STATUS(BSC_STATUS_OK,
                           bsc_match_command(commands, 1u, tokens, 2u, &result));
  MATCH_TEST_ASSERT_TRUE(result.command == &commands[0]);
  MATCH_TEST_ASSERT_TRUE(result.consumed_token_count == 1u);
  MATCH_TEST_ASSERT_TRUE(result.remaining_token_index == 1u);
  MATCH_TEST_ASSERT_TRUE(result.remaining_token_count == 1u);
  return 0;
}

/**
 * @brief Verify command path matching is ASCII case-insensitive.
 */
static int test_case_insensitive_matching(const char *test_name) {
  bsc_command_t commands[] = {
      make_match_command(k_status_path, 1u),
      make_match_command(k_settings_wifi_set_ssid_path, 4u),
  };
  bsc_string_view_t status_tokens[] = {token_from_cstr("STATUS")};
  bsc_string_view_t nested_tokens[] = {
      token_from_cstr("Settings"), token_from_cstr("WIFI"), token_from_cstr("Set"),
      token_from_cstr("SSID"),
  };
  bsc_match_result_t result;

  MATCH_TEST_ASSERT_STATUS(BSC_STATUS_OK,
                           bsc_match_command(commands, 2u, status_tokens, 1u, &result));
  MATCH_TEST_ASSERT_TRUE(result.command == &commands[0]);

  MATCH_TEST_ASSERT_STATUS(BSC_STATUS_OK,
                           bsc_match_command(commands, 2u, nested_tokens, 4u, &result));
  MATCH_TEST_ASSERT_TRUE(result.command == &commands[1]);
  return 0;
}

/**
 * @brief Verify matching does not require token views to be null-terminated.
 */
static int test_non_null_terminated_token_view(const char *test_name) {
  bsc_command_t commands[] = {make_match_command(k_status_path, 1u)};
  const char text[] = "status-extra";
  bsc_string_view_t tokens[] = {bsc_string_view_from_parts(text, 6u)};
  bsc_match_result_t result;

  MATCH_TEST_ASSERT_STATUS(BSC_STATUS_OK,
                           bsc_match_command(commands, 1u, tokens, 1u, &result));
  MATCH_TEST_ASSERT_TRUE(result.command == &commands[0]);
  return 0;
}

/**
 * @brief Verify no-input, unknown-command, and empty-table statuses.
 */
static int test_no_input_and_unknown(const char *test_name) {
  bsc_command_t commands[] = {make_match_command(k_status_path, 1u)};
  bsc_string_view_t tokens[] = {token_from_cstr("unknown")};
  bsc_match_result_t result = dirty_result();

  MATCH_TEST_ASSERT_STATUS(BSC_STATUS_NO_INPUT,
                           bsc_match_command(commands, 1u, tokens, 0u, &result));
  MATCH_TEST_ASSERT_TRUE(result_is_cleared(&result));

  result = dirty_result();
  MATCH_TEST_ASSERT_STATUS(BSC_STATUS_NO_INPUT,
                           bsc_match_command(commands, 1u, NULL, 0u, &result));
  MATCH_TEST_ASSERT_TRUE(result_is_cleared(&result));

  result = dirty_result();
  MATCH_TEST_ASSERT_STATUS(BSC_STATUS_UNKNOWN_COMMAND,
                           bsc_match_command(commands, 1u, tokens, 1u, &result));
  MATCH_TEST_ASSERT_TRUE(result_is_cleared(&result));

  result = dirty_result();
  MATCH_TEST_ASSERT_STATUS(BSC_STATUS_UNKNOWN_COMMAND,
                           bsc_match_command(NULL, 0u, tokens, 1u, &result));
  MATCH_TEST_ASSERT_TRUE(result_is_cleared(&result));
  return 0;
}

/**
 * @brief Verify exact group paths return group-required result fields.
 */
static int test_group_requires_subcommand(const char *test_name) {
  bsc_command_t commands[] = {
      make_match_group(k_settings_path, 1u),
      make_match_group(k_settings_wifi_path, 2u),
      make_match_command(k_settings_wifi_status_path, 3u),
  };
  bsc_string_view_t settings_tokens[] = {token_from_cstr("settings")};
  bsc_string_view_t wifi_tokens[] = {token_from_cstr("settings"), token_from_cstr("wifi")};
  bsc_match_result_t result;

  MATCH_TEST_ASSERT_STATUS(BSC_STATUS_GROUP_REQUIRES_SUBCOMMAND,
                           bsc_match_command(commands, 3u, settings_tokens, 1u, &result));
  MATCH_TEST_ASSERT_TRUE(result.command == NULL);
  MATCH_TEST_ASSERT_TRUE(result.command_index == 0u);
  MATCH_TEST_ASSERT_TRUE(result.group == &commands[0]);
  MATCH_TEST_ASSERT_TRUE(result.group_index == 0u);
  MATCH_TEST_ASSERT_TRUE(result.consumed_token_count == 1u);
  MATCH_TEST_ASSERT_TRUE(result.remaining_token_index == 1u);
  MATCH_TEST_ASSERT_TRUE(result.remaining_token_count == 0u);

  MATCH_TEST_ASSERT_STATUS(BSC_STATUS_GROUP_REQUIRES_SUBCOMMAND,
                           bsc_match_command(commands, 3u, wifi_tokens, 2u, &result));
  MATCH_TEST_ASSERT_TRUE(result.group == &commands[1]);
  MATCH_TEST_ASSERT_TRUE(result.group_index == 1u);
  MATCH_TEST_ASSERT_TRUE(result.consumed_token_count == 2u);
  MATCH_TEST_ASSERT_TRUE(result.remaining_token_index == 2u);
  MATCH_TEST_ASSERT_TRUE(result.remaining_token_count == 0u);
  return 0;
}

/**
 * @brief Verify known group prefixes with bad children remain unknown commands.
 */
static int test_group_prefix_bad_child_is_unknown(const char *test_name) {
  bsc_command_t commands[] = {
      make_match_group(k_settings_path, 1u),
      make_match_group(k_settings_wifi_path, 2u),
  };
  bsc_string_view_t tokens[] = {
      token_from_cstr("settings"), token_from_cstr("wifi"), token_from_cstr("nope"),
  };
  bsc_match_result_t result = dirty_result();

  MATCH_TEST_ASSERT_STATUS(BSC_STATUS_UNKNOWN_COMMAND,
                           bsc_match_command(commands, 2u, tokens, 3u, &result));
  MATCH_TEST_ASSERT_TRUE(result_is_cleared(&result));
  return 0;
}

/**
 * @brief Verify prefixes of longer commands do not fabricate group matches.
 */
static int test_input_prefix_without_group_is_unknown(const char *test_name) {
  bsc_command_t commands[] = {make_match_command(k_settings_wifi_set_ssid_path, 4u)};
  bsc_string_view_t tokens[] = {token_from_cstr("settings"), token_from_cstr("wifi")};
  bsc_match_result_t result = dirty_result();

  MATCH_TEST_ASSERT_STATUS(BSC_STATUS_UNKNOWN_COMMAND,
                           bsc_match_command(commands, 1u, tokens, 2u, &result));
  MATCH_TEST_ASSERT_TRUE(result_is_cleared(&result));
  return 0;
}

/**
 * @brief Verify duplicate and group/command path conflicts are ambiguous.
 */
static int test_ambiguity_detection(const char *test_name) {
  bsc_command_t duplicate_commands[] = {
      make_match_command(k_status_path, 1u),
      make_match_command(k_status_path, 1u),
  };
  bsc_command_t case_duplicate_commands[] = {
      make_match_command(k_status_path, 1u),
      make_match_command(k_status_upper_path, 1u),
  };
  bsc_command_t group_and_command[] = {
      make_match_group(k_settings_path, 1u),
      make_match_command(k_settings_path, 1u),
  };
  bsc_command_t nested_duplicate_commands[] = {
      make_match_command(k_settings_wifi_set_ssid_path, 4u),
      make_match_command(k_settings_wifi_set_ssid_case_path, 4u),
  };
  bsc_string_view_t status_tokens[] = {token_from_cstr("status")};
  bsc_string_view_t settings_tokens[] = {token_from_cstr("settings")};
  bsc_string_view_t nested_tokens[] = {
      token_from_cstr("settings"), token_from_cstr("wifi"), token_from_cstr("set"),
      token_from_cstr("ssid"),
  };
  bsc_match_result_t result = dirty_result();

  MATCH_TEST_ASSERT_STATUS(BSC_STATUS_AMBIGUOUS_COMMAND,
                           bsc_match_command(duplicate_commands, 2u, status_tokens, 1u, &result));
  MATCH_TEST_ASSERT_TRUE(result_is_cleared(&result));

  result = dirty_result();
  MATCH_TEST_ASSERT_STATUS(
      BSC_STATUS_AMBIGUOUS_COMMAND,
      bsc_match_command(case_duplicate_commands, 2u, status_tokens, 1u, &result));
  MATCH_TEST_ASSERT_TRUE(result_is_cleared(&result));

  result = dirty_result();
  MATCH_TEST_ASSERT_STATUS(BSC_STATUS_AMBIGUOUS_COMMAND,
                           bsc_match_command(group_and_command, 2u, settings_tokens, 1u, &result));
  MATCH_TEST_ASSERT_TRUE(result_is_cleared(&result));

  result = dirty_result();
  MATCH_TEST_ASSERT_STATUS(
      BSC_STATUS_AMBIGUOUS_COMMAND,
      bsc_match_command(nested_duplicate_commands, 2u, nested_tokens, 4u, &result));
  MATCH_TEST_ASSERT_TRUE(result_is_cleared(&result));
  return 0;
}

/**
 * @brief Verify invalid API pointers and configured count bounds fail internally.
 */
static int test_invalid_api_and_count_bounds(const char *test_name) {
  bsc_command_t commands[] = {make_match_command(k_status_path, 1u)};
  bsc_string_view_t tokens[] = {token_from_cstr("status")};
  bsc_match_result_t result = dirty_result();

  MATCH_TEST_ASSERT_STATUS(BSC_STATUS_INTERNAL_ERROR,
                           bsc_match_command(commands, 1u, NULL, 1u, &result));
  MATCH_TEST_ASSERT_TRUE(result_is_cleared(&result));

  result = dirty_result();
  MATCH_TEST_ASSERT_STATUS(BSC_STATUS_INTERNAL_ERROR,
                           bsc_match_command(NULL, 1u, tokens, 1u, &result));
  MATCH_TEST_ASSERT_TRUE(result_is_cleared(&result));

  result = dirty_result();
  MATCH_TEST_ASSERT_STATUS(
      BSC_STATUS_INTERNAL_ERROR,
      bsc_match_command(commands, 1u, tokens, (size_t)BSC_MAX_TOKENS + 1u, &result));
  MATCH_TEST_ASSERT_TRUE(result_is_cleared(&result));

  result = dirty_result();
  MATCH_TEST_ASSERT_STATUS(
      BSC_STATUS_INTERNAL_ERROR,
      bsc_match_command(commands, (size_t)BSC_MAX_COMMANDS + 1u, tokens, 1u, &result));
  MATCH_TEST_ASSERT_TRUE(result_is_cleared(&result));
  return 0;
}

/**
 * @brief Verify malformed path/node descriptor fields are rejected defensively.
 */
static int test_invalid_descriptor_defensive_checks(const char *test_name) {
  bsc_command_t commands[] = {make_match_command(k_status_path, 1u)};
  bsc_string_view_t tokens[] = {token_from_cstr("status")};
  bsc_match_result_t result = dirty_result();

  commands[0].path = NULL;
  MATCH_TEST_ASSERT_STATUS(BSC_STATUS_INVALID_DESCRIPTOR,
                           bsc_match_command(commands, 1u, tokens, 1u, &result));
  MATCH_TEST_ASSERT_TRUE(result_is_cleared(&result));

  commands[0] = make_match_command(k_status_path, 0u);
  result = dirty_result();
  MATCH_TEST_ASSERT_STATUS(BSC_STATUS_INVALID_DESCRIPTOR,
                           bsc_match_command(commands, 1u, tokens, 1u, &result));
  MATCH_TEST_ASSERT_TRUE(result_is_cleared(&result));

  commands[0] = make_match_command(k_deep_path, (size_t)BSC_MAX_PATH_TOKENS + 1u);
  result = dirty_result();
  MATCH_TEST_ASSERT_STATUS(BSC_STATUS_INVALID_DESCRIPTOR,
                           bsc_match_command(commands, 1u, tokens, 1u, &result));
  MATCH_TEST_ASSERT_TRUE(result_is_cleared(&result));

  commands[0] = make_match_command(k_null_token_path, 2u);
  result = dirty_result();
  MATCH_TEST_ASSERT_STATUS(BSC_STATUS_INVALID_DESCRIPTOR,
                           bsc_match_command(commands, 1u, tokens, 1u, &result));
  MATCH_TEST_ASSERT_TRUE(result_is_cleared(&result));

  commands[0] = make_match_command(k_status_path, 1u);
  commands[0].node_type = (bsc_node_type_t)99;
  result = dirty_result();
  MATCH_TEST_ASSERT_STATUS(BSC_STATUS_INVALID_DESCRIPTOR,
                           bsc_match_command(commands, 1u, tokens, 1u, &result));
  MATCH_TEST_ASSERT_TRUE(result_is_cleared(&result));
  return 0;
}

/**
 * @brief Verify callers may omit the optional match result pointer.
 */
static int test_optional_result_pointer(const char *test_name) {
  bsc_command_t commands[] = {make_match_command(k_status_path, 1u)};
  bsc_string_view_t tokens[] = {token_from_cstr("status")};

  MATCH_TEST_ASSERT_STATUS(BSC_STATUS_OK, bsc_match_command(commands, 1u, tokens, 1u, NULL));
  return 0;
}

/**
 * @brief Verify matching performs no parsing, dispatch, access checks, or mutation.
 */
static int test_no_parser_dispatch_access_or_mutation(const char *test_name) {
  bsc_command_t commands[] = {make_match_command(k_status_path, 1u)};
  bsc_command_t before;
  bsc_string_view_t tokens[] = {token_from_cstr("status"), token_from_cstr("extra")};
  bsc_match_result_t result;

  g_matcher_handler_calls = 0;
  g_matcher_access_calls = 0;
  before = commands[0];

  MATCH_TEST_ASSERT_STATUS(BSC_STATUS_OK,
                           bsc_match_command(commands, 1u, tokens, 2u, &result));
  MATCH_TEST_ASSERT_TRUE(result.remaining_token_count == 1u);
  MATCH_TEST_ASSERT_TRUE(g_matcher_handler_calls == 0);
  MATCH_TEST_ASSERT_TRUE(g_matcher_access_calls == 0);
  MATCH_TEST_ASSERT_TRUE(memcmp(&before, &commands[0], sizeof(before)) == 0);
  return 0;
}

/**
 * @brief Run all matcher unit tests and return the number of failed cases.
 */
int bsc_run_matcher_tests(void) {
  int failures = 0;

  RUN_MATCH_TEST(test_match_result_clear);
  RUN_MATCH_TEST(test_simple_executable_match);
  RUN_MATCH_TEST(test_match_command_at_nonzero_index);
  RUN_MATCH_TEST(test_longest_executable_match_leaves_remaining_arg);
  RUN_MATCH_TEST(test_extra_token_is_remaining_arg);
  RUN_MATCH_TEST(test_case_insensitive_matching);
  RUN_MATCH_TEST(test_non_null_terminated_token_view);
  RUN_MATCH_TEST(test_no_input_and_unknown);
  RUN_MATCH_TEST(test_group_requires_subcommand);
  RUN_MATCH_TEST(test_group_prefix_bad_child_is_unknown);
  RUN_MATCH_TEST(test_input_prefix_without_group_is_unknown);
  RUN_MATCH_TEST(test_ambiguity_detection);
  RUN_MATCH_TEST(test_invalid_api_and_count_bounds);
  RUN_MATCH_TEST(test_invalid_descriptor_defensive_checks);
  RUN_MATCH_TEST(test_optional_result_pointer);
  RUN_MATCH_TEST(test_no_parser_dispatch_access_or_mutation);

  return failures;
}
