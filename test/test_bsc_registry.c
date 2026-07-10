#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bsc_config.h"
#include "bsc_output.h"
#include "bsc_registry.h"
#include "bsc_status.h"
#include "bsc_types.h"

/**
 * @brief Fail the current registry test when a condition is false.
 */
#define REG_TEST_ASSERT_TRUE(condition)                                                             \
  do {                                                                                              \
    if (!(condition)) {                                                                             \
      printf("FAIL: %s: %s:%d: %s\n", test_name, __FILE__, __LINE__, #condition);                  \
      return 1;                                                                                     \
    }                                                                                               \
  } while (0)

/**
 * @brief Assert that registry validation returns the expected status code.
 */
#define REG_TEST_ASSERT_STATUS(expected, actual) REG_TEST_ASSERT_TRUE((expected) == (actual))
/**
 * @brief Assert that registry diagnostics report the expected reason.
 */
#define REG_TEST_ASSERT_REASON(expected, actual) REG_TEST_ASSERT_TRUE((expected) == (actual).reason)

/**
 * @brief Run one registry test and accumulate failures for the module runner.
 */
#define RUN_REG_TEST(fn)                                                                            \
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

static const char *const k_group_path[] = {"settings"};
static const char *const k_status_path[] = {"status"};
static const char *const k_hidden_path[] = {"secret"};
static const char *const k_args_path[] = {"set", "all"};
static const char *const k_other_path[] = {"other"};
static const char *const k_case_path[] = {"STATUS"};
static const char *const k_deep_path[] = {"a", "b", "c", "d", "e", "f", "g"};
static const char *const k_null_token_path[] = {"wifi", NULL, "ssid"};
static const char *const k_empty_token_path[] = {"wifi", ""};
static const char k_long_name[] =
    "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
static const char *const k_long_token_path[] = {k_long_name};

static const bsc_enum_choice_t k_enum_choices[] = {
    {"low", 1, NULL},
    {"high", 2, "High choice"},
};
static const bsc_enum_choice_t k_enum_duplicate_name[] = {
    {"low", 1, NULL},
    {"LOW", 2, NULL},
};
static const bsc_enum_choice_t k_enum_duplicate_value[] = {
    {"low", 1, NULL},
    {"high", 1, NULL},
};
static const bsc_enum_choice_t k_enum_null_name[] = {{NULL, 1, NULL}};
static const bsc_enum_choice_t k_enum_empty_name[] = {{"", 1, NULL}};
static const bsc_enum_choice_t k_enum_long_name[] = {{k_long_name, 1, NULL}};
static const bsc_enum_choice_t k_many_choices[BSC_MAX_ENUM_CHOICES + 1u] = {
    {"c0", 0, NULL},   {"c1", 1, NULL},   {"c2", 2, NULL},   {"c3", 3, NULL},
    {"c4", 4, NULL},   {"c5", 5, NULL},   {"c6", 6, NULL},   {"c7", 7, NULL},
    {"c8", 8, NULL},   {"c9", 9, NULL},   {"c10", 10, NULL}, {"c11", 11, NULL},
    {"c12", 12, NULL}, {"c13", 13, NULL}, {"c14", 14, NULL}, {"c15", 15, NULL},
    {"c16", 16, NULL},
};

/**
 * @brief Stub handler used by command descriptor fixtures.
 *
 * The registry tests only validate descriptor shape and callback presence; this
 * stub is never used for real command dispatch.
 */
static bsc_status_t test_handler(void *app_context,
                                 const struct bsc_command *command,
                                 const struct bsc_parsed_args *args,
                                 bsc_output_t *output) {
  (void)app_context;
  (void)command;
  (void)args;
  (void)output;
  return BSC_STATUS_OK;
}

/**
 * @brief Stub access callback used by registry descriptor fixtures.
 */
static bool test_access(void *app_context,
                        const struct bsc_command *command,
                        bsc_access_level_t required_access) {
  (void)app_context;
  (void)command;
  return required_access == BSC_ACCESS_NORMAL;
}

/**
 * @brief Build a minimally valid argument descriptor fixture for validation tests.
 */
static bsc_arg_def_t make_arg(const char *name, bsc_arg_type_t type) {
  bsc_arg_def_t arg;

  memset(&arg, 0, sizeof(arg));
  arg.name = name;
  arg.type = type;
  arg.max_int = 1;
  arg.max_uint = 1u;
  arg.max_float = 1.0f;
  arg.max_length = 1u;
  if (type == BSC_ARG_ENUM) {
    arg.enum_choices = k_enum_choices;
    arg.enum_choice_count = sizeof(k_enum_choices) / sizeof(k_enum_choices[0]);
  }
  return arg;
}

/**
 * @brief Build a minimally valid executable command descriptor fixture.
 */
static bsc_command_t make_command(const char *const *path, size_t path_len) {
  bsc_command_t command;

  memset(&command, 0, sizeof(command));
  command.path = path;
  command.path_len = path_len;
  command.node_type = BSC_NODE_COMMAND;
  command.handler = test_handler;
  command.access = BSC_ACCESS_NORMAL;
  command.flags = BSC_COMMAND_FLAG_NONE;
  return command;
}

/**
 * @brief Build a group descriptor fixture by removing executable handler wiring.
 */
static bsc_command_t make_group(const char *const *path, size_t path_len) {
  bsc_command_t command = make_command(path, path_len);

  command.node_type = BSC_NODE_GROUP;
  command.handler = NULL;
  return command;
}

/**
 * @brief Verify invalid descriptor status and the expected diagnostic reason.
 */
static int expect_invalid(const char *test_name,
                          const bsc_command_t *commands,
                          size_t command_count,
                          bsc_registry_error_reason_t reason,
                          bsc_registry_validation_error_t *error) {
  REG_TEST_ASSERT_STATUS(BSC_STATUS_INVALID_DESCRIPTOR,
                         bsc_registry_validate(commands, command_count, error));
  REG_TEST_ASSERT_REASON(reason, *error);
  return 0;
}

/**
 * @brief Verify representative valid command tables pass and clear diagnostics.
 */
static int test_registry_valid_tables(const char *test_name) {
#if BSC_ENABLE_FLOAT
  bsc_arg_def_t args[7];
#else
  bsc_arg_def_t args[6];
#endif
  bsc_command_t commands[4];
  bsc_registry_validation_error_t error;
  size_t index;

  args[0] = make_arg("offset", BSC_ARG_INT);
  args[0].min_int = -10;
  args[0].max_int = 10;
  args[1] = make_arg("count", BSC_ARG_UINT);
  args[1].min_uint = 1u;
  args[1].max_uint = 10u;
#if BSC_ENABLE_FLOAT
  args[2] = make_arg("ratio", BSC_ARG_FLOAT);
  args[2].min_float = -1.0f;
  args[2].max_float = 1.0f;
  args[3] = make_arg("enabled", BSC_ARG_BOOL);
  args[4] = make_arg("mode", BSC_ARG_ENUM);
  args[5] = make_arg("ssid", BSC_ARG_STRING);
  args[5].min_length = 1u;
  args[5].max_length = 32u;
  args[6] = make_arg("password", BSC_ARG_SECRET);
  args[6].min_length = 8u;
  args[6].max_length = 64u;
#else
  args[2] = make_arg("enabled", BSC_ARG_BOOL);
  args[3] = make_arg("mode", BSC_ARG_ENUM);
  args[4] = make_arg("ssid", BSC_ARG_STRING);
  args[4].min_length = 1u;
  args[4].max_length = 32u;
  args[5] = make_arg("password", BSC_ARG_SECRET);
  args[5].min_length = 8u;
  args[5].max_length = 64u;
#endif

  commands[0] = make_group(k_group_path, 1u);
  commands[1] = make_command(k_status_path, 1u);
  commands[1].summary = NULL;
  commands[1].description = NULL;
  commands[2] = make_command(k_args_path, 2u);
  commands[2].args = args;
  commands[2].arg_count = sizeof(args) / sizeof(args[0]);
  commands[2].access_fn = test_access;
  commands[3] = make_command(k_hidden_path, 1u);
  commands[3].flags = BSC_COMMAND_FLAG_HIDDEN;

  error.reason = BSC_REGISTRY_ERROR_NULL_COMMANDS;
  error.command_index = 99u;
  REG_TEST_ASSERT_STATUS(BSC_STATUS_OK,
                         bsc_registry_validate(commands, sizeof(commands) / sizeof(commands[0]), &error));
  REG_TEST_ASSERT_REASON(BSC_REGISTRY_ERROR_NONE, error);
  REG_TEST_ASSERT_TRUE(error.command_index == 0u);
  REG_TEST_ASSERT_STATUS(BSC_STATUS_OK,
                         bsc_registry_validate(commands, sizeof(commands) / sizeof(commands[0]), NULL));

  for (index = 0u; index < sizeof(commands) / sizeof(commands[0]); ++index) {
    REG_TEST_ASSERT_TRUE(commands[index].path != NULL);
  }
  return 0;
}

/**
 * @brief Verify command table pointer, zero-count, and max-count validation.
 */
static int test_registry_table_pointer_and_count(const char *test_name) {
  bsc_command_t command = make_command(k_status_path, 1u);
  bsc_registry_validation_error_t error;

  REG_TEST_ASSERT_STATUS(BSC_STATUS_INVALID_DESCRIPTOR, bsc_registry_validate(NULL, 1u, &error));
  REG_TEST_ASSERT_REASON(BSC_REGISTRY_ERROR_NULL_COMMANDS, error);

  REG_TEST_ASSERT_STATUS(BSC_STATUS_INVALID_DESCRIPTOR, bsc_registry_validate(&command, 0u, &error));
  REG_TEST_ASSERT_REASON(BSC_REGISTRY_ERROR_ZERO_COMMANDS, error);

  REG_TEST_ASSERT_STATUS(BSC_STATUS_INVALID_DESCRIPTOR,
                         bsc_registry_validate(&command, BSC_MAX_COMMANDS + 1u, &error));
  REG_TEST_ASSERT_REASON(BSC_REGISTRY_ERROR_TOO_MANY_COMMANDS, error);
  return 0;
}

/**
 * @brief Verify path pointer, depth, token, and token-length validation errors.
 */
static int test_registry_path_validation(const char *test_name) {
  bsc_command_t command = make_command(k_status_path, 1u);
  bsc_registry_validation_error_t error;

  command.path = NULL;
  REG_TEST_ASSERT_TRUE(expect_invalid(test_name, &command, 1u, BSC_REGISTRY_ERROR_NULL_PATH, &error) == 0);
  REG_TEST_ASSERT_TRUE(error.command_index == 0u);

  command = make_command(k_status_path, 0u);
  REG_TEST_ASSERT_TRUE(expect_invalid(test_name, &command, 1u, BSC_REGISTRY_ERROR_EMPTY_PATH, &error) == 0);

  command = make_command(k_deep_path, BSC_MAX_PATH_TOKENS + 1u);
  REG_TEST_ASSERT_TRUE(expect_invalid(test_name, &command, 1u, BSC_REGISTRY_ERROR_PATH_TOO_DEEP, &error) == 0);

  command = make_command(k_null_token_path, 3u);
  REG_TEST_ASSERT_TRUE(expect_invalid(test_name, &command, 1u, BSC_REGISTRY_ERROR_NULL_PATH_TOKEN, &error) == 0);
  REG_TEST_ASSERT_TRUE(error.path_token_index == 1u);

  command = make_command(k_empty_token_path, 2u);
  REG_TEST_ASSERT_TRUE(expect_invalid(test_name, &command, 1u, BSC_REGISTRY_ERROR_EMPTY_PATH_TOKEN, &error) == 0);
  REG_TEST_ASSERT_TRUE(error.path_token_index == 1u);

  command = make_command(k_long_token_path, 1u);
  REG_TEST_ASSERT_TRUE(expect_invalid(test_name, &command, 1u, BSC_REGISTRY_ERROR_PATH_TOKEN_TOO_LONG, &error) == 0);
  REG_TEST_ASSERT_TRUE(error.path_token_index == 0u);
  return 0;
}

/**
 * @brief Verify node type and group/command handler presence rules.
 */
static int test_registry_node_and_handler_policy(const char *test_name) {
  bsc_command_t command = make_command(k_status_path, 1u);
  bsc_registry_validation_error_t error;

  command.node_type = (bsc_node_type_t)99;
  REG_TEST_ASSERT_TRUE(expect_invalid(test_name, &command, 1u, BSC_REGISTRY_ERROR_INVALID_NODE_TYPE, &error) == 0);

  command = make_command(k_status_path, 1u);
  command.handler = NULL;
  REG_TEST_ASSERT_TRUE(expect_invalid(test_name, &command, 1u, BSC_REGISTRY_ERROR_COMMAND_MISSING_HANDLER, &error) == 0);

  command = make_group(k_group_path, 1u);
  command.handler = test_handler;
  REG_TEST_ASSERT_TRUE(expect_invalid(test_name, &command, 1u, BSC_REGISTRY_ERROR_GROUP_HAS_HANDLER, &error) == 0);

  command = make_group(k_group_path, 1u);
  REG_TEST_ASSERT_STATUS(BSC_STATUS_OK, bsc_registry_validate(&command, 1u, &error));
  command = make_command(k_status_path, 1u);
  REG_TEST_ASSERT_STATUS(BSC_STATUS_OK, bsc_registry_validate(&command, 1u, &error));
  return 0;
}

/**
 * @brief Verify argument count, pointer, name, type, and NONE rejection diagnostics.
 */
static int test_registry_argument_metadata(const char *test_name) {
  bsc_command_t command = make_command(k_status_path, 1u);
  bsc_arg_def_t arg = make_arg("value", BSC_ARG_INT);
  bsc_registry_validation_error_t error;

  command.arg_count = BSC_MAX_ARGS + 1u;
  REG_TEST_ASSERT_TRUE(expect_invalid(test_name, &command, 1u, BSC_REGISTRY_ERROR_ARG_COUNT_TOO_HIGH, &error) == 0);

  command = make_command(k_status_path, 1u);
  command.arg_count = 1u;
  command.args = NULL;
  REG_TEST_ASSERT_TRUE(expect_invalid(test_name, &command, 1u, BSC_REGISTRY_ERROR_NULL_ARGS_WITH_COUNT, &error) == 0);

  command = make_command(k_status_path, 1u);
  command.args = &arg;
  command.arg_count = 0u;
  REG_TEST_ASSERT_STATUS(BSC_STATUS_OK, bsc_registry_validate(&command, 1u, &error));

  command.arg_count = 1u;
  arg.name = NULL;
  REG_TEST_ASSERT_TRUE(expect_invalid(test_name, &command, 1u, BSC_REGISTRY_ERROR_INVALID_ARG_NAME, &error) == 0);
  REG_TEST_ASSERT_TRUE(error.arg_index == 0u);

  arg = make_arg("", BSC_ARG_INT);
  REG_TEST_ASSERT_TRUE(expect_invalid(test_name, &command, 1u, BSC_REGISTRY_ERROR_INVALID_ARG_NAME, &error) == 0);

  arg = make_arg(k_long_name, BSC_ARG_INT);
  REG_TEST_ASSERT_TRUE(expect_invalid(test_name, &command, 1u, BSC_REGISTRY_ERROR_ARG_NAME_TOO_LONG, &error) == 0);

  arg = make_arg("value", (bsc_arg_type_t)99);
  REG_TEST_ASSERT_TRUE(expect_invalid(test_name, &command, 1u, BSC_REGISTRY_ERROR_INVALID_ARG_TYPE, &error) == 0);

  arg = make_arg("value", BSC_ARG_NONE);
  REG_TEST_ASSERT_TRUE(expect_invalid(test_name, &command, 1u, BSC_REGISTRY_ERROR_ARG_NONE_NOT_ALLOWED, &error) == 0);
  return 0;
}

/**
 * @brief Verify numeric and string/secret range validation for argument metadata.
 */
static int test_registry_argument_ranges(const char *test_name) {
  bsc_command_t command = make_command(k_status_path, 1u);
  bsc_arg_def_t arg = make_arg("value", BSC_ARG_INT);
  bsc_registry_validation_error_t error;

  command.args = &arg;
  command.arg_count = 1u;

  arg.min_int = 2;
  arg.max_int = 1;
  REG_TEST_ASSERT_TRUE(expect_invalid(test_name, &command, 1u, BSC_REGISTRY_ERROR_INVALID_ARG_RANGE, &error) == 0);

  arg = make_arg("value", BSC_ARG_UINT);
  arg.min_uint = 2u;
  arg.max_uint = 1u;
  REG_TEST_ASSERT_TRUE(expect_invalid(test_name, &command, 1u, BSC_REGISTRY_ERROR_INVALID_ARG_RANGE, &error) == 0);

  arg = make_arg("value", BSC_ARG_FLOAT);
#if BSC_ENABLE_FLOAT
  arg.min_float = -(float)BSC_COMPACT_FLOAT_MAX_MAGNITUDE;
  arg.max_float = (float)BSC_COMPACT_FLOAT_MAX_MAGNITUDE;
  REG_TEST_ASSERT_STATUS(BSC_STATUS_OK, bsc_registry_validate(&command, 1u, &error));
  arg.min_float = 2.0f;
  arg.max_float = 1.0f;
  REG_TEST_ASSERT_TRUE(expect_invalid(test_name, &command, 1u, BSC_REGISTRY_ERROR_INVALID_ARG_RANGE, &error) == 0);
  arg = make_arg("value", BSC_ARG_FLOAT);
  arg.min_float = -((float)BSC_COMPACT_FLOAT_MAX_MAGNITUDE + 1024.0f);
  REG_TEST_ASSERT_TRUE(expect_invalid(test_name, &command, 1u, BSC_REGISTRY_ERROR_INVALID_ARG_RANGE, &error) == 0);
  arg = make_arg("value", BSC_ARG_FLOAT);
  arg.max_float = ((float)BSC_COMPACT_FLOAT_MAX_MAGNITUDE + 1024.0f);
  REG_TEST_ASSERT_TRUE(expect_invalid(test_name, &command, 1u, BSC_REGISTRY_ERROR_INVALID_ARG_RANGE, &error) == 0);
  arg = make_arg("value", BSC_ARG_FLOAT);
  arg.min_float = NAN;
  REG_TEST_ASSERT_TRUE(expect_invalid(test_name, &command, 1u, BSC_REGISTRY_ERROR_INVALID_ARG_RANGE, &error) == 0);
  arg = make_arg("value", BSC_ARG_FLOAT);
  arg.max_float = NAN;
  REG_TEST_ASSERT_TRUE(expect_invalid(test_name, &command, 1u, BSC_REGISTRY_ERROR_INVALID_ARG_RANGE, &error) == 0);
  arg = make_arg("value", BSC_ARG_FLOAT);
  arg.min_float = INFINITY;
  REG_TEST_ASSERT_TRUE(expect_invalid(test_name, &command, 1u, BSC_REGISTRY_ERROR_INVALID_ARG_RANGE, &error) == 0);
  arg = make_arg("value", BSC_ARG_FLOAT);
  arg.max_float = INFINITY;
  REG_TEST_ASSERT_TRUE(expect_invalid(test_name, &command, 1u, BSC_REGISTRY_ERROR_INVALID_ARG_RANGE, &error) == 0);
  arg = make_arg("value", BSC_ARG_FLOAT);
  arg.min_float = -INFINITY;
  REG_TEST_ASSERT_TRUE(expect_invalid(test_name, &command, 1u, BSC_REGISTRY_ERROR_INVALID_ARG_RANGE, &error) == 0);
  arg = make_arg("value", BSC_ARG_FLOAT);
  arg.max_float = -INFINITY;
  REG_TEST_ASSERT_TRUE(expect_invalid(test_name, &command, 1u, BSC_REGISTRY_ERROR_INVALID_ARG_RANGE, &error) == 0);
#else
  REG_TEST_ASSERT_TRUE(expect_invalid(test_name, &command, 1u, BSC_REGISTRY_ERROR_FLOAT_DISABLED, &error) == 0);
#endif

  arg = make_arg("value", BSC_ARG_BOOL);
  arg.min_int = 2;
  arg.max_int = 1;
  REG_TEST_ASSERT_STATUS(BSC_STATUS_OK, bsc_registry_validate(&command, 1u, &error));

  arg = make_arg("value", BSC_ARG_STRING);
  arg.min_length = 2u;
  arg.max_length = 1u;
  REG_TEST_ASSERT_TRUE(expect_invalid(test_name, &command, 1u, BSC_REGISTRY_ERROR_INVALID_STRING_LENGTH_RANGE, &error) == 0);

  arg = make_arg("value", BSC_ARG_SECRET);
  arg.min_length = 2u;
  arg.max_length = 1u;
  REG_TEST_ASSERT_TRUE(expect_invalid(test_name, &command, 1u, BSC_REGISTRY_ERROR_INVALID_STRING_LENGTH_RANGE, &error) == 0);

  arg = make_arg("value", BSC_ARG_STRING);
  arg.max_length = BSC_MAX_TOKEN_LEN + 1u;
  REG_TEST_ASSERT_TRUE(expect_invalid(test_name, &command, 1u, BSC_REGISTRY_ERROR_INVALID_STRING_LENGTH_RANGE, &error) == 0);

  arg = make_arg("value", BSC_ARG_SECRET);
  arg.max_length = BSC_MAX_TOKEN_LEN + 1u;
  REG_TEST_ASSERT_TRUE(expect_invalid(test_name, &command, 1u, BSC_REGISTRY_ERROR_INVALID_STRING_LENGTH_RANGE, &error) == 0);
  return 0;
}

/**
 * @brief Verify enum choice table presence, count, name, and duplicate validation.
 */
static int test_registry_enum_metadata(const char *test_name) {
  bsc_command_t command = make_command(k_status_path, 1u);
  bsc_arg_def_t arg = make_arg("mode", BSC_ARG_ENUM);
  bsc_registry_validation_error_t error;

  command.args = &arg;
  command.arg_count = 1u;

  arg.enum_choices = NULL;
  REG_TEST_ASSERT_TRUE(expect_invalid(test_name, &command, 1u, BSC_REGISTRY_ERROR_ENUM_MISSING_CHOICES, &error) == 0);

  arg = make_arg("mode", BSC_ARG_ENUM);
  arg.enum_choice_count = 0u;
  REG_TEST_ASSERT_TRUE(expect_invalid(test_name, &command, 1u, BSC_REGISTRY_ERROR_ENUM_CHOICE_COUNT_ZERO, &error) == 0);

  arg = make_arg("mode", BSC_ARG_ENUM);
  arg.enum_choices = k_many_choices;
  arg.enum_choice_count = BSC_MAX_ENUM_CHOICES + 1u;
  REG_TEST_ASSERT_TRUE(expect_invalid(test_name, &command, 1u, BSC_REGISTRY_ERROR_ENUM_CHOICE_COUNT_TOO_HIGH, &error) == 0);

  arg = make_arg("mode", BSC_ARG_ENUM);
  arg.enum_choices = k_enum_null_name;
  arg.enum_choice_count = 1u;
  REG_TEST_ASSERT_TRUE(expect_invalid(test_name, &command, 1u, BSC_REGISTRY_ERROR_NULL_ENUM_CHOICE_NAME, &error) == 0);
  REG_TEST_ASSERT_TRUE(error.enum_choice_index == 0u);

  arg.enum_choices = k_enum_empty_name;
  REG_TEST_ASSERT_TRUE(expect_invalid(test_name, &command, 1u, BSC_REGISTRY_ERROR_EMPTY_ENUM_CHOICE_NAME, &error) == 0);

  arg.enum_choices = k_enum_long_name;
  REG_TEST_ASSERT_TRUE(expect_invalid(test_name, &command, 1u, BSC_REGISTRY_ERROR_ENUM_CHOICE_NAME_TOO_LONG, &error) == 0);

  arg.enum_choices = k_enum_duplicate_name;
  arg.enum_choice_count = 2u;
  REG_TEST_ASSERT_TRUE(expect_invalid(test_name, &command, 1u, BSC_REGISTRY_ERROR_DUPLICATE_ENUM_CHOICE_NAME, &error) == 0);
  REG_TEST_ASSERT_TRUE(error.enum_choice_index == 1u);

  arg.enum_choices = k_enum_duplicate_value;
  REG_TEST_ASSERT_TRUE(expect_invalid(test_name, &command, 1u, BSC_REGISTRY_ERROR_DUPLICATE_ENUM_CHOICE_VALUE, &error) == 0);

  arg = make_arg("mode", BSC_ARG_ENUM);
  REG_TEST_ASSERT_STATUS(BSC_STATUS_OK, bsc_registry_validate(&command, 1u, &error));
  return 0;
}

/**
 * @brief Verify access-level and command-flag validation rules.
 */
static int test_registry_access_and_flags(const char *test_name) {
  bsc_command_t command = make_command(k_status_path, 1u);
  bsc_registry_validation_error_t error;

  command.access = (bsc_access_level_t)99;
  REG_TEST_ASSERT_TRUE(expect_invalid(test_name, &command, 1u, BSC_REGISTRY_ERROR_INVALID_COMMAND_ACCESS, &error) == 0);

  command.access = BSC_ACCESS_NORMAL;
  command.flags = (bsc_command_flags_t)(1u << 3u);
  REG_TEST_ASSERT_TRUE(expect_invalid(test_name, &command, 1u, BSC_REGISTRY_ERROR_INVALID_COMMAND_FLAGS, &error) == 0);

  command.flags = BSC_COMMAND_FLAG_NONE;
  REG_TEST_ASSERT_STATUS(BSC_STATUS_OK, bsc_registry_validate(&command, 1u, &error));
  command.flags = BSC_COMMAND_FLAG_HIDDEN;
  REG_TEST_ASSERT_STATUS(BSC_STATUS_OK, bsc_registry_validate(&command, 1u, &error));

  command.flags = BSC_COMMAND_FLAG_NONE;
  command.access = BSC_ACCESS_ADVANCED;
  REG_TEST_ASSERT_STATUS(BSC_STATUS_OK, bsc_registry_validate(&command, 1u, &error));
  command.access = BSC_ACCESS_FACTORY;
  REG_TEST_ASSERT_STATUS(BSC_STATUS_OK, bsc_registry_validate(&command, 1u, &error));
  command.access = BSC_ACCESS_LOCKED;
  REG_TEST_ASSERT_STATUS(BSC_STATUS_OK, bsc_registry_validate(&command, 1u, &error));
  return 0;
}

/**
 * @brief Verify duplicate command paths are rejected with ASCII case-insensitive matching.
 */
static int test_registry_duplicate_paths(const char *test_name) {
  bsc_command_t commands[2];
  bsc_registry_validation_error_t error;

  commands[0] = make_command(k_status_path, 1u);
  commands[1] = make_command(k_status_path, 1u);
  REG_TEST_ASSERT_TRUE(expect_invalid(test_name, commands, 2u, BSC_REGISTRY_ERROR_DUPLICATE_COMMAND_PATH, &error) == 0);
  REG_TEST_ASSERT_TRUE(error.command_index == 1u);
  REG_TEST_ASSERT_TRUE(error.duplicate_command_index == 0u);

  commands[1] = make_command(k_case_path, 1u);
  REG_TEST_ASSERT_TRUE(expect_invalid(test_name, commands, 2u, BSC_REGISTRY_ERROR_DUPLICATE_COMMAND_PATH, &error) == 0);
  REG_TEST_ASSERT_TRUE(error.command_index == 1u);
  REG_TEST_ASSERT_TRUE(error.duplicate_command_index == 0u);

  commands[1] = make_command(k_other_path, 1u);
  REG_TEST_ASSERT_STATUS(BSC_STATUS_OK, bsc_registry_validate(commands, 2u, &error));
  return 0;
}

/**
 * @brief Verify diagnostic clear resets reason and index fields and accepts NULL.
 */
static int test_registry_diagnostic_clear(const char *test_name) {
  bsc_registry_validation_error_t error;

  bsc_registry_validation_error_clear(NULL);
  error.reason = BSC_REGISTRY_ERROR_DUPLICATE_COMMAND_PATH;
  error.command_index = 1u;
  error.path_token_index = 2u;
  error.arg_index = 3u;
  error.enum_choice_index = 4u;
  error.duplicate_command_index = 5u;
  bsc_registry_validation_error_clear(&error);
  REG_TEST_ASSERT_REASON(BSC_REGISTRY_ERROR_NONE, error);
  REG_TEST_ASSERT_TRUE(error.command_index == 0u);
  REG_TEST_ASSERT_TRUE(error.path_token_index == 0u);
  REG_TEST_ASSERT_TRUE(error.arg_index == 0u);
  REG_TEST_ASSERT_TRUE(error.enum_choice_index == 0u);
  REG_TEST_ASSERT_TRUE(error.duplicate_command_index == 0u);
  return 0;
}

/**
 * @brief Run all registry validation host tests and return accumulated failures.
 */
int bsc_run_registry_tests(void) {
  int failures = 0;

  RUN_REG_TEST(test_registry_valid_tables);
  RUN_REG_TEST(test_registry_table_pointer_and_count);
  RUN_REG_TEST(test_registry_path_validation);
  RUN_REG_TEST(test_registry_node_and_handler_policy);
  RUN_REG_TEST(test_registry_argument_metadata);
  RUN_REG_TEST(test_registry_argument_ranges);
  RUN_REG_TEST(test_registry_enum_metadata);
  RUN_REG_TEST(test_registry_access_and_flags);
  RUN_REG_TEST(test_registry_duplicate_paths);
  RUN_REG_TEST(test_registry_diagnostic_clear);

  return failures;
}
