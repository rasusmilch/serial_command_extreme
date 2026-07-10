#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "bsc_output.h"
#include "bsc_status.h"
#include "bsc_types.h"

/**
 * @brief Fail the current type-layout test when a condition is false.
 */
#define TYPE_TEST_ASSERT_TRUE(condition)                                                             \
  do {                                                                                                \
    if (!(condition)) {                                                                               \
      printf("FAIL: %s: %s:%d: %s\n", test_name, __FILE__, __LINE__, #condition);                    \
      return 1;                                                                                       \
    }                                                                                                 \
  } while (0)

/**
 * @brief Run one type test and accumulate failures for the module runner.
 */
#define RUN_TYPE_TEST(fn)                                                                             \
  do {                                                                                                \
    int result;                                                                                       \
    const char *test_name = #fn;                                                                      \
    result = fn(test_name);                                                                           \
    if (result != 0) {                                                                                \
      failures += 1;                                                                                  \
    } else {                                                                                          \
      printf("PASS: %s\n", test_name);                                                               \
    }                                                                                                 \
  } while (0)

enum test_gain_value {
  TEST_GAIN_LOW = 1,
  TEST_GAIN_HIGH = 2
};

static int g_command_context = 17;

static const bsc_enum_choice_t k_gain_choices[] = {
    {"low", TEST_GAIN_LOW, "Low gain range"},
    {"high", TEST_GAIN_HIGH, "High gain range"},
};


static const bsc_arg_def_t k_gain_args[] = {
    {
        "level",
        BSC_ARG_ENUM,
        0,
        0,
        0u,
        0u,
        0.0f,
        0.0f,
        0u,
        0u,
        k_gain_choices,
        sizeof(k_gain_choices) / sizeof(k_gain_choices[0]),
        "Gain level",
    },
};

static const char *const k_status_path[] = {"status"};
static const char *const k_settings_path[] = {"settings"};
static const char *const k_wifi_ssid_path[] = {"settings", "wifi", "set", "ssid"};
static const char *const k_wifi_password_path[] = {"settings", "wifi", "set", "password"};

static const bsc_arg_def_t k_representative_args[] = {
    {"offset", BSC_ARG_INT, -100, 100, 0u, 0u, 0.0f, 0.0f, 0u, 0u, NULL, 0u, "Signed offset"},
    {"count", BSC_ARG_UINT, 0, 0, 0u, 1000u, 0.0f, 0.0f, 0u, 0u, NULL, 0u, "Unsigned count"},
    {"ratio", BSC_ARG_FLOAT, 0, 0, 0u, 0u, -1.0f, 1.0f, 0u, 0u, NULL, 0u, "Float ratio"},
    {"enabled", BSC_ARG_BOOL, 0, 0, 0u, 0u, 0.0f, 0.0f, 0u, 0u, NULL, 0u, "Boolean flag"},
    {
        "mode",
        BSC_ARG_ENUM,
        0,
        0,
        0u,
        0u,
        0.0f,
        0.0f,
        0u,
        0u,
        k_gain_choices,
        sizeof(k_gain_choices) / sizeof(k_gain_choices[0]),
        "Choice value",
    },
    {"ssid", BSC_ARG_STRING, 0, 0, 0u, 0u, 0.0f, 0.0f, 1u, 32u, NULL, 0u, "SSID text"},
    {"password", BSC_ARG_SECRET, 0, 0, 0u, 0u, 0.0f, 0.0f, 8u, 64u, NULL, 0u, "Secret text"},
};

/**
 * @brief Stub command handler used to verify callback typedef compatibility.
 *
 * This is a descriptor-wiring fixture only; it does not perform real dispatch.
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
 * @brief Stub access callback used to verify access typedef and descriptor wiring.
 */
static bool test_access(void *app_context,
                        const struct bsc_command *command,
                        bsc_access_level_t required_access) {
  (void)app_context;
  (void)command;
  return required_access == BSC_ACCESS_NORMAL || required_access == BSC_ACCESS_ADVANCED;
}

static const bsc_command_t k_commands[] = {
    {
        k_settings_path,
        sizeof(k_settings_path) / sizeof(k_settings_path[0]),
        BSC_NODE_GROUP,
        NULL,
        0u,
        NULL,
        NULL,
        BSC_ACCESS_NORMAL,
        BSC_COMMAND_FLAG_NONE,
        NULL,
        "Settings",
        "Settings command namespace.",
    },
    {
        k_status_path,
        sizeof(k_status_path) / sizeof(k_status_path[0]),
        BSC_NODE_COMMAND,
        NULL,
        0u,
        test_handler,
        &g_command_context,
        BSC_ACCESS_NORMAL,
        BSC_COMMAND_FLAG_NONE,
        test_access,
        "Show status",
        "Show current device status.",
    },
    {
        k_wifi_ssid_path,
        sizeof(k_wifi_ssid_path) / sizeof(k_wifi_ssid_path[0]),
        BSC_NODE_COMMAND,
        &k_representative_args[5],
        1u,
        test_handler,
        &g_command_context,
        BSC_ACCESS_ADVANCED,
        BSC_COMMAND_FLAG_NONE,
        test_access,
        "Set SSID",
        "Set the Wi-Fi network name.",
    },
    {
        k_wifi_password_path,
        sizeof(k_wifi_password_path) / sizeof(k_wifi_password_path[0]),
        BSC_NODE_COMMAND,
        &k_representative_args[6],
        1u,
        test_handler,
        &g_command_context,
        BSC_ACCESS_LOCKED,
        BSC_COMMAND_FLAG_HIDDEN,
        test_access,
        "Set password",
        "Set the Wi-Fi password.",
    },
};

/**
 * @brief Verify enum choice fixtures preserve names, values, and help text.
 */
static int test_enum_choice_initialization(const char *test_name) {
  TYPE_TEST_ASSERT_TRUE(k_gain_choices[0].name != NULL);
  TYPE_TEST_ASSERT_TRUE(k_gain_choices[0].value == TEST_GAIN_LOW);
  TYPE_TEST_ASSERT_TRUE(k_gain_choices[0].help != NULL);
  TYPE_TEST_ASSERT_TRUE(k_gain_choices[1].value == TEST_GAIN_HIGH);
  return 0;
}

/**
 * @brief Verify static command path arrays keep expected token layout.
 */
static int test_static_path_arrays(const char *test_name) {
  TYPE_TEST_ASSERT_TRUE(k_status_path[0][0] == 's');
  TYPE_TEST_ASSERT_TRUE((sizeof(k_wifi_ssid_path) / sizeof(k_wifi_ssid_path[0])) == 4u);
  TYPE_TEST_ASSERT_TRUE(k_wifi_ssid_path[0] != NULL);
  TYPE_TEST_ASSERT_TRUE(k_wifi_ssid_path[3] != NULL);
  return 0;
}

/**
 * @brief Verify representative argument descriptors initialize each supported type.
 */
static int test_arg_def_initialization(const char *test_name) {
  TYPE_TEST_ASSERT_TRUE(k_representative_args[0].type == BSC_ARG_INT);
  TYPE_TEST_ASSERT_TRUE(k_representative_args[0].min_int == -100);
  TYPE_TEST_ASSERT_TRUE(k_representative_args[1].type == BSC_ARG_UINT);
  TYPE_TEST_ASSERT_TRUE(k_representative_args[1].max_uint == 1000u);
  TYPE_TEST_ASSERT_TRUE(k_representative_args[2].type == BSC_ARG_FLOAT);
  TYPE_TEST_ASSERT_TRUE(k_representative_args[2].min_float < 0.0f);
  TYPE_TEST_ASSERT_TRUE(k_representative_args[3].type == BSC_ARG_BOOL);
  TYPE_TEST_ASSERT_TRUE(k_representative_args[4].type == BSC_ARG_ENUM);
  TYPE_TEST_ASSERT_TRUE(k_representative_args[4].enum_choices == k_gain_choices);
  TYPE_TEST_ASSERT_TRUE(k_representative_args[4].enum_choice_count == 2u);
  TYPE_TEST_ASSERT_TRUE(k_gain_args[0].type == BSC_ARG_ENUM);
  TYPE_TEST_ASSERT_TRUE(k_gain_args[0].enum_choices == k_gain_choices);
  TYPE_TEST_ASSERT_TRUE(k_gain_args[0].enum_choice_count == 2u);
  return 0;
}

/**
 * @brief Protect size_t metadata fields for string and secret argument lengths.
 */
static int test_string_and_secret_metadata_use_size_t(const char *test_name) {
  size_t ssid_min = k_representative_args[5].min_length;
  size_t ssid_max = k_representative_args[5].max_length;
  size_t secret_min = k_representative_args[6].min_length;
  size_t secret_max = k_representative_args[6].max_length;

  TYPE_TEST_ASSERT_TRUE(k_representative_args[5].type == BSC_ARG_STRING);
  TYPE_TEST_ASSERT_TRUE(ssid_min == 1u);
  TYPE_TEST_ASSERT_TRUE(ssid_max == 32u);
  TYPE_TEST_ASSERT_TRUE(k_representative_args[6].type == BSC_ARG_SECRET);
  TYPE_TEST_ASSERT_TRUE(secret_min == 8u);
  TYPE_TEST_ASSERT_TRUE(secret_max == 64u);
  return 0;
}

/**
 * @brief Verify group descriptors have namespace shape and no executable handler.
 */
static int test_group_descriptor_initialization(const char *test_name) {
  const bsc_command_t *group = &k_commands[0];

  TYPE_TEST_ASSERT_TRUE(group->node_type == BSC_NODE_GROUP);
  TYPE_TEST_ASSERT_TRUE(group->path == k_settings_path);
  TYPE_TEST_ASSERT_TRUE(group->path_len == 1u);
  TYPE_TEST_ASSERT_TRUE(group->args == NULL);
  TYPE_TEST_ASSERT_TRUE(group->arg_count == 0u);
  TYPE_TEST_ASSERT_TRUE(group->handler == NULL);
  TYPE_TEST_ASSERT_TRUE(group->flags == BSC_COMMAND_FLAG_NONE);
  return 0;
}

/**
 * @brief Verify executable command descriptors wire path, args, callbacks, and help.
 */
static int test_executable_descriptor_initialization(const char *test_name) {
  const bsc_command_t *command = &k_commands[2];

  TYPE_TEST_ASSERT_TRUE(command->node_type == BSC_NODE_COMMAND);
  TYPE_TEST_ASSERT_TRUE(command->path == k_wifi_ssid_path);
  TYPE_TEST_ASSERT_TRUE(command->path_len == 4u);
  TYPE_TEST_ASSERT_TRUE(command->args == &k_representative_args[5]);
  TYPE_TEST_ASSERT_TRUE(command->arg_count == 1u);
  TYPE_TEST_ASSERT_TRUE(command->handler == test_handler);
  TYPE_TEST_ASSERT_TRUE(command->command_context == &g_command_context);
  TYPE_TEST_ASSERT_TRUE(command->access == BSC_ACCESS_ADVANCED);
  TYPE_TEST_ASSERT_TRUE(command->access_fn == test_access);
  TYPE_TEST_ASSERT_TRUE(command->summary != NULL);
  TYPE_TEST_ASSERT_TRUE(command->description != NULL);
  return 0;
}

/**
 * @brief Verify handler and access stubs can be assigned to public typedefs.
 */
static int test_callback_typedef_assignments(const char *test_name) {
  bsc_command_handler_t handler = test_handler;
  bsc_command_access_fn_t access = test_access;

  TYPE_TEST_ASSERT_TRUE(handler == k_commands[1].handler);
  TYPE_TEST_ASSERT_TRUE(access == k_commands[1].access_fn);
  return 0;
}

/**
 * @brief Verify hidden command flags remain independent from access levels.
 */
static int test_hidden_flag_is_separate_from_access(const char *test_name) {
  const bsc_command_t *command = &k_commands[3];

  TYPE_TEST_ASSERT_TRUE(command->flags == BSC_COMMAND_FLAG_HIDDEN);
  TYPE_TEST_ASSERT_TRUE((command->flags & BSC_COMMAND_FLAG_HIDDEN) != 0u);
  TYPE_TEST_ASSERT_TRUE(command->access == BSC_ACCESS_LOCKED);
  TYPE_TEST_ASSERT_TRUE(command->access != BSC_ACCESS_NORMAL);
  return 0;
}

/**
 * @brief Verify BSC_ARG_NONE exists even though registry validation rejects it as a concrete argument.
 */
static int test_none_argument_type_is_available_without_optional_semantics(const char *test_name) {
  static const bsc_arg_def_t none_arg = {
      "none", BSC_ARG_NONE, 0, 0, 0u, 0u, 0.0f, 0.0f, 0u, 0u, NULL, 0u, NULL,
  };

  TYPE_TEST_ASSERT_TRUE(none_arg.type == BSC_ARG_NONE);
  TYPE_TEST_ASSERT_TRUE(none_arg.enum_choices == NULL);
  TYPE_TEST_ASSERT_TRUE(none_arg.enum_choice_count == 0u);
  return 0;
}

/**
 * @brief Run all type and descriptor-layout host tests.
 */
int bsc_run_types_tests(void) {
  int failures = 0;

  RUN_TYPE_TEST(test_enum_choice_initialization);
  RUN_TYPE_TEST(test_static_path_arrays);
  RUN_TYPE_TEST(test_arg_def_initialization);
  RUN_TYPE_TEST(test_string_and_secret_metadata_use_size_t);
  RUN_TYPE_TEST(test_group_descriptor_initialization);
  RUN_TYPE_TEST(test_executable_descriptor_initialization);
  RUN_TYPE_TEST(test_callback_typedef_assignments);
  RUN_TYPE_TEST(test_hidden_flag_is_separate_from_access);
  RUN_TYPE_TEST(test_none_argument_type_is_available_without_optional_semantics);

  return failures;
}
