#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bsc_config.h"
#include "bsc_dispatch.h"

/** @brief Fail the current dispatch test when a condition is false. */
#define DISPATCH_TEST_ASSERT_TRUE(condition)                                                        \
  do {                                                                                              \
    if (!(condition)) {                                                                             \
      printf("FAIL: %s: %s:%d: %s\n", test_name, __FILE__, __LINE__, #condition);                  \
      return 1;                                                                                     \
    }                                                                                               \
  } while (0)

/** @brief Assert that a dispatch call returns the expected status. */
#define DISPATCH_TEST_ASSERT_STATUS(expected, actual) DISPATCH_TEST_ASSERT_TRUE((expected) == (actual))

/** @brief Run one dispatch test and accumulate failures for the module runner. */
#define RUN_DISPATCH_TEST(fn)                                                                       \
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

/** @brief Mutable fixture state observed by access callbacks and handlers. */
typedef struct dispatch_fixture {
  int handler_calls;
  int access_calls;
  int allow_access;
  bsc_status_t handler_status;
  void *expected_app_context;
  const bsc_command_t *access_command;
  const bsc_command_t *handler_command;
  const bsc_parsed_args_t *handler_args;
  bsc_output_t *handler_output;
  bsc_access_level_t observed_access;
  int command_context_value;
  int saw_command_context;
  int wrote_output;
} dispatch_fixture_t;

/** @brief Bounded output sink used to prove output is passed to handlers only. */
typedef struct dispatch_sink {
  char bytes[4];
  size_t used;
} dispatch_sink_t;

/** @brief Caller-owned record for observing NULL app_context callback delivery. */
typedef struct dispatch_null_context_record {
  int access_calls;
  int handler_calls;
  void *access_app_context;
  void *handler_app_context;
  const bsc_command_t *access_command;
  const bsc_command_t *handler_command;
  int command_context_was_usable;
} dispatch_null_context_record_t;

/** Command path used by executable dispatch fixtures. */
static const char *const k_dispatch_path[] = {"dispatch"};
/** Command path used by group-descriptor rejection fixtures. */
static const char *const k_group_path[] = {"group"};
/** Enum choices used by dispatch parser-integration fixtures. */
static const bsc_enum_choice_t k_mode_choices[] = {{"low", 11, "Low"}, {"high", 22, "High"}};
/** Signed-integer descriptor constrained to the dispatch test range. */
static const bsc_arg_def_t k_int_arg[] = {{"value", BSC_ARG_INT, -5, 5, 0u, 0u, 0.0f, 0.0f, 0u, 0u, NULL, 0u, "int"}};
/** Unsigned-integer descriptor constrained to the dispatch test range. */
static const bsc_arg_def_t k_uint_arg[] = {{"count", BSC_ARG_UINT, 0, 0, 1u, 5u, 0.0f, 0.0f, 0u, 0u, NULL, 0u, "uint"}};
/** Boolean descriptor used to prove dispatch preserves bool parser failures. */
static const bsc_arg_def_t k_bool_arg[] = {{"enabled", BSC_ARG_BOOL, 0, 0, 0u, 0u, 0.0f, 0.0f, 0u, 0u, NULL, 0u, "bool"}};
/** Enum descriptor used to verify semantic choice values survive dispatch. */
static const bsc_arg_def_t k_enum_arg[] = {{"mode", BSC_ARG_ENUM, 0, 0, 0u, 0u, 0.0f, 0.0f, 0u, 0u, k_mode_choices, 2u, "enum"}};
/** Bounded string descriptor used to verify borrowed text views. */
static const bsc_arg_def_t k_string_arg[] = {{"name", BSC_ARG_STRING, 0, 0, 0u, 0u, 0.0f, 0.0f, 2u, 4u, NULL, 0u, "string"}};
/** Bounded secret descriptor used to verify borrowed secret views. */
static const bsc_arg_def_t k_secret_arg[] = {{"secret", BSC_ARG_SECRET, 0, 0, 0u, 0u, 0.0f, 0.0f, 3u, 5u, NULL, 0u, "secret"}};
/** Float descriptor used by enabled/disabled dispatch build tests. */
static const bsc_arg_def_t k_float_arg[] = {{"ratio", BSC_ARG_FLOAT, 0, 0, 0u, 0u, -2.0f, 2.0f, 0u, 0u, NULL, 0u, "float"}};

/** @brief Create one explicit-length test token from a C string. */
static bsc_string_view_t token_from_cstr(const char *text) { return bsc_string_view_from_cstr(text); }

/** @brief Create one explicit-length test token from arbitrary bytes. */
static bsc_string_view_t token_from_parts(const char *text, size_t length) {
  return bsc_string_view_from_parts(text, length);
}

/** @brief Capture bytes and intentionally report truncation when the sink fills. */
static size_t dispatch_sink_write(void *user, const char *data, size_t length) {
  dispatch_sink_t *sink = (dispatch_sink_t *)user;
  size_t available;
  size_t to_copy;
  if (sink == NULL || data == NULL) {
    return 0u;
  }
  available = sizeof(sink->bytes) - sink->used;
  to_copy = length < available ? length : available;
  if (to_copy > 0u) {
    memcpy(&sink->bytes[sink->used], data, to_copy);
    sink->used += to_copy;
  }
  return to_copy;
}

/** @brief Access callback that records its arguments and permits fixture-controlled denial. */
static bool dispatch_access_callback(void *app_context,
                                     const struct bsc_command *command,
                                     bsc_access_level_t required_access) {
  dispatch_fixture_t *fixture = (dispatch_fixture_t *)app_context;
  if (fixture != NULL) {
    fixture->access_calls += 1;
    fixture->access_command = command;
    fixture->observed_access = required_access;
    if (fixture->expected_app_context != NULL && fixture->expected_app_context != app_context) {
      fixture->allow_access = 0;
    }
  }
  return fixture != NULL && fixture->allow_access != 0;
}

/** @brief Generic handler that records all arguments passed by dispatch. */
static bsc_status_t dispatch_recording_handler(void *app_context,
                                               const struct bsc_command *command,
                                               const struct bsc_parsed_args *args,
                                               bsc_output_t *output) {
  dispatch_fixture_t *fixture = (dispatch_fixture_t *)app_context;
  if (fixture != NULL) {
    fixture->handler_calls += 1;
    fixture->handler_command = command;
    fixture->handler_args = args;
    fixture->handler_output = output;
    if (command != NULL && command->command_context == &fixture->command_context_value) {
      fixture->saw_command_context = 1;
    }
    return fixture->handler_status;
  }
  return BSC_STATUS_OK;
}

/** @brief Handler that writes through output only when invoked by dispatch. */
static bsc_status_t dispatch_output_handler(void *app_context,
                                            const struct bsc_command *command,
                                            const struct bsc_parsed_args *args,
                                            bsc_output_t *output) {
  dispatch_fixture_t *fixture = (dispatch_fixture_t *)app_context;
  (void)command;
  (void)args;
  if (fixture != NULL) {
    fixture->handler_calls += 1;
    fixture->handler_output = output;
    fixture->wrote_output = 1;
  }
  return bsc_out_write(output, "overflow");
}

/** @brief Access callback that records a NULL app_context through command_context. */
static bool dispatch_null_context_access(void *app_context,
                                         const struct bsc_command *command,
                                         bsc_access_level_t required_access) {
  dispatch_null_context_record_t *record;
  (void)required_access;
  if (command == NULL || command->command_context == NULL) {
    return false;
  }
  record = (dispatch_null_context_record_t *)command->command_context;
  record->access_calls += 1;
  record->access_app_context = app_context;
  record->access_command = command;
  record->command_context_was_usable = 1;
  return true;
}

/** @brief Handler that records a NULL app_context through command_context. */
static bsc_status_t dispatch_null_context_handler(void *app_context,
                                                  const struct bsc_command *command,
                                                  const struct bsc_parsed_args *args,
                                                  bsc_output_t *output) {
  dispatch_null_context_record_t *record;
  (void)args;
  (void)output;
  if (command == NULL || command->command_context == NULL) {
    return BSC_STATUS_APP_ERROR;
  }
  record = (dispatch_null_context_record_t *)command->command_context;
  record->handler_calls += 1;
  record->handler_app_context = app_context;
  record->handler_command = command;
  record->command_context_was_usable = 1;
  return BSC_STATUS_OK;
}

/** @brief Reset one dispatch fixture to default allowing/success behavior. */
static void fixture_init(dispatch_fixture_t *fixture) {
  memset(fixture, 0, sizeof(*fixture));
  fixture->allow_access = 1;
  fixture->handler_status = BSC_STATUS_OK;
  fixture->expected_app_context = fixture;
  fixture->command_context_value = 123;
}

/** @brief Initialize one executable command descriptor fixture. */
static bsc_command_t make_command(dispatch_fixture_t *fixture,
                                  bsc_access_level_t access,
                                  bsc_command_flags_t flags,
                                  const bsc_arg_def_t *args,
                                  size_t arg_count,
                                  bsc_command_access_fn_t access_fn) {
  bsc_command_t command;
  command.path = k_dispatch_path;
  command.path_len = 1u;
  command.node_type = BSC_NODE_COMMAND;
  command.args = args;
  command.arg_count = arg_count;
  command.handler = dispatch_recording_handler;
  command.command_context = fixture == NULL ? NULL : &fixture->command_context_value;
  command.access = access;
  command.flags = flags;
  command.access_fn = access_fn;
  command.summary = "Dispatch";
  command.description = "Dispatch test command.";
  return command;
}

/** @brief Fill parsed storage with a stale secret-like borrowed pointer. */
static void poison_parsed_args(bsc_parsed_args_t *parsed) {
  parsed->count = 1u;
  parsed->values[0].type = BSC_ARG_SECRET;
  parsed->values[0].data.text_value = bsc_string_view_from_cstr("stale-secret");
}

/** @brief Assert parsed storage contains no active borrowed values. */
static int parsed_is_cleared(const bsc_parsed_args_t *parsed) {
  return parsed->count == 0u && parsed->values[0].type == BSC_ARG_NONE &&
         parsed->values[0].data.text_value.data == NULL && parsed->values[0].data.text_value.length == 0u;
}

/** @brief Verify the no-callback default access matrix for every access level. */
static int test_dispatch_access_defaults_without_callback(const char *test_name) {
  bsc_access_level_t levels[] = {BSC_ACCESS_NORMAL, BSC_ACCESS_ADVANCED, BSC_ACCESS_FACTORY, BSC_ACCESS_LOCKED};
  bsc_status_t expected[] = {BSC_STATUS_OK, BSC_STATUS_OK, BSC_STATUS_ACCESS_DENIED, BSC_STATUS_ACCESS_DENIED};
  size_t index;
  for (index = 0u; index < 4u; ++index) {
    dispatch_fixture_t fixture;
    bsc_parsed_args_t parsed;
    bsc_arg_parse_error_t error;
    bsc_command_t command;
    fixture_init(&fixture);
    command = make_command(&fixture, levels[index], BSC_COMMAND_FLAG_NONE, NULL, 0u, NULL);
    DISPATCH_TEST_ASSERT_STATUS(expected[index], bsc_dispatch_command(&fixture, &command, NULL, 0u, &parsed, &error, NULL));
    DISPATCH_TEST_ASSERT_TRUE(fixture.access_calls == 0);
    DISPATCH_TEST_ASSERT_TRUE(fixture.handler_calls == (expected[index] == BSC_STATUS_OK ? 1 : 0));
    DISPATCH_TEST_ASSERT_TRUE(error.reason == BSC_ARG_PARSE_ERROR_NONE);
  }
  return 0;
}

/** @brief Verify an allowing access callback authorizes every access level. */
static int test_dispatch_access_callback_allows_all_levels(const char *test_name) {
  bsc_access_level_t levels[] = {BSC_ACCESS_NORMAL, BSC_ACCESS_ADVANCED, BSC_ACCESS_FACTORY, BSC_ACCESS_LOCKED};
  size_t index;
  for (index = 0u; index < 4u; ++index) {
    dispatch_fixture_t fixture;
    bsc_parsed_args_t parsed;
    bsc_arg_parse_error_t error;
    bsc_command_t command;
    fixture_init(&fixture);
    command = make_command(&fixture, levels[index], BSC_COMMAND_FLAG_NONE, NULL, 0u, dispatch_access_callback);
    DISPATCH_TEST_ASSERT_STATUS(BSC_STATUS_OK, bsc_dispatch_command(&fixture, &command, NULL, 0u, &parsed, &error, NULL));
    DISPATCH_TEST_ASSERT_TRUE(fixture.access_calls == 1);
    DISPATCH_TEST_ASSERT_TRUE(fixture.access_command == &command);
    DISPATCH_TEST_ASSERT_TRUE(fixture.observed_access == levels[index]);
    DISPATCH_TEST_ASSERT_TRUE(fixture.handler_calls == 1);
  }
  return 0;
}

/** @brief Verify a denying access callback blocks every access level before parsing. */
static int test_dispatch_access_callback_denies_all_levels(const char *test_name) {
  bsc_access_level_t levels[] = {BSC_ACCESS_NORMAL, BSC_ACCESS_ADVANCED, BSC_ACCESS_FACTORY, BSC_ACCESS_LOCKED};
  size_t index;
  for (index = 0u; index < 4u; ++index) {
    dispatch_fixture_t fixture;
    bsc_parsed_args_t parsed;
    bsc_arg_parse_error_t error;
    bsc_command_t command;
    bsc_string_view_t malformed = token_from_cstr("not-an-int");
    fixture_init(&fixture);
    fixture.allow_access = 0;
    poison_parsed_args(&parsed);
    command = make_command(&fixture, levels[index], BSC_COMMAND_FLAG_NONE, k_int_arg, 1u, dispatch_access_callback);
    DISPATCH_TEST_ASSERT_STATUS(BSC_STATUS_ACCESS_DENIED,
                                bsc_dispatch_command(&fixture, &command, &malformed, 1u, &parsed, &error, NULL));
    DISPATCH_TEST_ASSERT_TRUE(fixture.access_calls == 1);
    DISPATCH_TEST_ASSERT_TRUE(fixture.handler_calls == 0);
    DISPATCH_TEST_ASSERT_TRUE(error.reason == BSC_ARG_PARSE_ERROR_NONE);
    DISPATCH_TEST_ASSERT_TRUE(parsed_is_cleared(&parsed));
  }
  return 0;
}

/** @brief Verify hidden commands follow access policy without extra denial. */
static int test_dispatch_hidden_flag_follows_access_policy(const char *test_name) {
  dispatch_fixture_t fixture;
  bsc_parsed_args_t parsed;
  bsc_arg_parse_error_t error;
  bsc_command_t command;

  fixture_init(&fixture);
  command = make_command(&fixture, BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_HIDDEN, NULL, 0u, NULL);
  DISPATCH_TEST_ASSERT_STATUS(BSC_STATUS_OK, bsc_dispatch_command(&fixture, &command, NULL, 0u, &parsed, &error, NULL));
  DISPATCH_TEST_ASSERT_TRUE(fixture.handler_calls == 1);

  fixture_init(&fixture);
  command = make_command(&fixture, BSC_ACCESS_ADVANCED, BSC_COMMAND_FLAG_HIDDEN, NULL, 0u, NULL);
  DISPATCH_TEST_ASSERT_STATUS(BSC_STATUS_OK, bsc_dispatch_command(&fixture, &command, NULL, 0u, &parsed, &error, NULL));

  fixture_init(&fixture);
  command = make_command(&fixture, BSC_ACCESS_FACTORY, BSC_COMMAND_FLAG_HIDDEN, NULL, 0u, NULL);
  DISPATCH_TEST_ASSERT_STATUS(BSC_STATUS_ACCESS_DENIED,
                              bsc_dispatch_command(&fixture, &command, NULL, 0u, &parsed, &error, NULL));

  fixture_init(&fixture);
  command = make_command(&fixture, BSC_ACCESS_FACTORY, BSC_COMMAND_FLAG_HIDDEN, NULL, 0u, dispatch_access_callback);
  DISPATCH_TEST_ASSERT_STATUS(BSC_STATUS_OK, bsc_dispatch_command(&fixture, &command, NULL, 0u, &parsed, &error, NULL));

  fixture_init(&fixture);
  fixture.allow_access = 0;
  command = make_command(&fixture, BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_HIDDEN, NULL, 0u, dispatch_access_callback);
  DISPATCH_TEST_ASSERT_STATUS(BSC_STATUS_ACCESS_DENIED,
                              bsc_dispatch_command(&fixture, &command, NULL, 0u, &parsed, &error, NULL));
  return 0;
}

/** @brief Verify dispatch API validation clears usable caller-owned diagnostics. */
static int test_dispatch_api_validation_and_clearing(const char *test_name) {
  dispatch_fixture_t fixture;
  bsc_parsed_args_t parsed;
  bsc_arg_parse_error_t error;
  bsc_command_t command;
  bsc_string_view_t token = token_from_cstr("1");
  fixture_init(&fixture);
  command = make_command(&fixture, BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_NONE, NULL, 0u, dispatch_access_callback);

  poison_parsed_args(&parsed);
  error.reason = BSC_ARG_PARSE_ERROR_INVALID_VALUE;
  DISPATCH_TEST_ASSERT_STATUS(BSC_STATUS_INTERNAL_ERROR,
                              bsc_dispatch_command(&fixture, NULL, NULL, 0u, &parsed, &error, NULL));
  DISPATCH_TEST_ASSERT_TRUE(parsed_is_cleared(&parsed));
  DISPATCH_TEST_ASSERT_TRUE(error.reason == BSC_ARG_PARSE_ERROR_NONE);

  error.reason = BSC_ARG_PARSE_ERROR_INVALID_VALUE;
  DISPATCH_TEST_ASSERT_STATUS(BSC_STATUS_INTERNAL_ERROR,
                              bsc_dispatch_command(&fixture, &command, NULL, 0u, NULL, &error, NULL));
  DISPATCH_TEST_ASSERT_TRUE(error.reason == BSC_ARG_PARSE_ERROR_NONE);

  poison_parsed_args(&parsed);
  DISPATCH_TEST_ASSERT_STATUS(BSC_STATUS_INTERNAL_ERROR,
                              bsc_dispatch_command(&fixture, &command, NULL, 0u, &parsed, NULL, NULL));
  DISPATCH_TEST_ASSERT_TRUE(parsed_is_cleared(&parsed));

  poison_parsed_args(&parsed);
  error.reason = BSC_ARG_PARSE_ERROR_INVALID_VALUE;
  DISPATCH_TEST_ASSERT_STATUS(BSC_STATUS_INTERNAL_ERROR,
                              bsc_dispatch_command(&fixture, &command, NULL, 1u, &parsed, &error, NULL));
  DISPATCH_TEST_ASSERT_TRUE(parsed_is_cleared(&parsed));
  DISPATCH_TEST_ASSERT_TRUE(error.reason == BSC_ARG_PARSE_ERROR_NONE);
  DISPATCH_TEST_ASSERT_TRUE(fixture.access_calls == 0);

  DISPATCH_TEST_ASSERT_STATUS(BSC_STATUS_EXTRA_ARGUMENT,
                              bsc_dispatch_command(&fixture, &command, &token, 1u, &parsed, &error, NULL));
  DISPATCH_TEST_ASSERT_TRUE(fixture.access_calls == 1);
  return 0;
}

/** @brief Verify shallow descriptor failures occur before access callbacks. */
static int test_dispatch_descriptor_validation_before_access(const char *test_name) {
  dispatch_fixture_t fixture;
  bsc_parsed_args_t parsed;
  bsc_arg_parse_error_t error;
  bsc_command_t command;
  size_t too_many = (size_t)BSC_MAX_ARGS + 1u;
  fixture_init(&fixture);

  command = make_command(&fixture, BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_NONE, NULL, 0u, dispatch_access_callback);
  command.node_type = BSC_NODE_GROUP;
  command.path = k_group_path;
  DISPATCH_TEST_ASSERT_STATUS(BSC_STATUS_INVALID_DESCRIPTOR,
                              bsc_dispatch_command(&fixture, &command, NULL, 0u, &parsed, &error, NULL));
  DISPATCH_TEST_ASSERT_TRUE(fixture.access_calls == 0);
  DISPATCH_TEST_ASSERT_TRUE(error.reason == BSC_ARG_PARSE_ERROR_NONE);

  command = make_command(&fixture, BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_NONE, NULL, 0u, dispatch_access_callback);
  command.handler = NULL;
  DISPATCH_TEST_ASSERT_STATUS(BSC_STATUS_INVALID_DESCRIPTOR,
                              bsc_dispatch_command(&fixture, &command, NULL, 0u, &parsed, &error, NULL));

  command = make_command(&fixture, (bsc_access_level_t)99, BSC_COMMAND_FLAG_NONE, NULL, 0u, dispatch_access_callback);
  DISPATCH_TEST_ASSERT_STATUS(BSC_STATUS_INVALID_DESCRIPTOR,
                              bsc_dispatch_command(&fixture, &command, NULL, 0u, &parsed, &error, NULL));

  command = make_command(&fixture, BSC_ACCESS_NORMAL, (bsc_command_flags_t)(1u << 4u), NULL, 0u, dispatch_access_callback);
  DISPATCH_TEST_ASSERT_STATUS(BSC_STATUS_INVALID_DESCRIPTOR,
                              bsc_dispatch_command(&fixture, &command, NULL, 0u, &parsed, &error, NULL));

  command = make_command(&fixture, BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_NONE, k_int_arg, too_many, dispatch_access_callback);
  DISPATCH_TEST_ASSERT_STATUS(BSC_STATUS_INVALID_DESCRIPTOR,
                              bsc_dispatch_command(&fixture, &command, NULL, 0u, &parsed, &error, NULL));

  command = make_command(&fixture, BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_NONE, NULL, 1u, dispatch_access_callback);
  DISPATCH_TEST_ASSERT_STATUS(BSC_STATUS_INVALID_DESCRIPTOR,
                              bsc_dispatch_command(&fixture, &command, NULL, 0u, &parsed, &error, NULL));
  DISPATCH_TEST_ASSERT_TRUE(fixture.access_calls == 0);
  return 0;
}

/** @brief Assert one parser failure preserves status/reason and skips handler. */
static int expect_parse_failure(const char *test_name,
                                const bsc_arg_def_t *args,
                                size_t arg_count,
                                const bsc_string_view_t *tokens,
                                size_t token_count,
                                bsc_status_t expected_status,
                                bsc_arg_parse_error_reason_t expected_reason) {
  dispatch_fixture_t fixture;
  bsc_parsed_args_t parsed;
  bsc_arg_parse_error_t error;
  bsc_command_t command;
  fixture_init(&fixture);
  poison_parsed_args(&parsed);
  command = make_command(&fixture, BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_NONE, args, arg_count, NULL);
  DISPATCH_TEST_ASSERT_STATUS(expected_status,
                              bsc_dispatch_command(&fixture, &command, tokens, token_count, &parsed, &error, NULL));
  DISPATCH_TEST_ASSERT_TRUE(error.reason == expected_reason);
  DISPATCH_TEST_ASSERT_TRUE(fixture.handler_calls == 0);
  DISPATCH_TEST_ASSERT_TRUE(parsed_is_cleared(&parsed));
  return 0;
}

/** @brief Verify parser failures propagate status and structured diagnostics. */
static int test_dispatch_parser_failures_preserve_diagnostics(const char *test_name) {
  bsc_string_view_t int_bad = token_from_cstr("x");
  bsc_string_view_t int_low = token_from_cstr("-6");
  bsc_string_view_t int_high = token_from_cstr("6");
  bsc_string_view_t uint_bad = token_from_cstr("-1");
  bsc_string_view_t bool_bad = token_from_cstr("maybe");
  bsc_string_view_t enum_bad = token_from_cstr("medium");
  bsc_string_view_t text_short = token_from_cstr("a");
  bsc_string_view_t text_long = token_from_cstr("abcde");
  const char nul_bytes[] = {'a', '\0', 'b'};
  bsc_string_view_t embedded_nul = token_from_parts(nul_bytes, sizeof(nul_bytes));
  bsc_string_view_t extra_tokens[2] = {token_from_cstr("1"), token_from_cstr("2")};
  bsc_string_view_t too_many[(size_t)BSC_MAX_ARGS + 1u];
  size_t index;

  DISPATCH_TEST_ASSERT_STATUS(0, expect_parse_failure(test_name, k_int_arg, 1u, NULL, 0u,
                                                       BSC_STATUS_MISSING_ARGUMENT,
                                                       BSC_ARG_PARSE_ERROR_MISSING_ARGUMENT));
  DISPATCH_TEST_ASSERT_STATUS(0, expect_parse_failure(test_name, k_int_arg, 1u, extra_tokens, 2u,
                                                       BSC_STATUS_EXTRA_ARGUMENT,
                                                       BSC_ARG_PARSE_ERROR_EXTRA_ARGUMENT));
  DISPATCH_TEST_ASSERT_STATUS(0, expect_parse_failure(test_name, k_int_arg, 1u, &int_bad, 1u,
                                                       BSC_STATUS_INVALID_ARGUMENT_TYPE,
                                                       BSC_ARG_PARSE_ERROR_INVALID_VALUE));
  DISPATCH_TEST_ASSERT_STATUS(0, expect_parse_failure(test_name, k_uint_arg, 1u, &uint_bad, 1u,
                                                       BSC_STATUS_INVALID_ARGUMENT_TYPE,
                                                       BSC_ARG_PARSE_ERROR_INVALID_VALUE));
  DISPATCH_TEST_ASSERT_STATUS(0, expect_parse_failure(test_name, k_bool_arg, 1u, &bool_bad, 1u,
                                                       BSC_STATUS_INVALID_ARGUMENT_TYPE,
                                                       BSC_ARG_PARSE_ERROR_INVALID_VALUE));
  DISPATCH_TEST_ASSERT_STATUS(0, expect_parse_failure(test_name, k_enum_arg, 1u, &enum_bad, 1u,
                                                       BSC_STATUS_INVALID_ENUM_VALUE,
                                                       BSC_ARG_PARSE_ERROR_INVALID_ENUM_CHOICE));
  DISPATCH_TEST_ASSERT_STATUS(0, expect_parse_failure(test_name, k_int_arg, 1u, &int_low, 1u,
                                                       BSC_STATUS_ARGUMENT_OUT_OF_RANGE,
                                                       BSC_ARG_PARSE_ERROR_BELOW_MINIMUM));
  DISPATCH_TEST_ASSERT_STATUS(0, expect_parse_failure(test_name, k_int_arg, 1u, &int_high, 1u,
                                                       BSC_STATUS_ARGUMENT_OUT_OF_RANGE,
                                                       BSC_ARG_PARSE_ERROR_ABOVE_MAXIMUM));
  DISPATCH_TEST_ASSERT_STATUS(0, expect_parse_failure(test_name, k_string_arg, 1u, &text_short, 1u,
                                                       BSC_STATUS_ARGUMENT_TOO_SHORT,
                                                       BSC_ARG_PARSE_ERROR_TEXT_TOO_SHORT));
  DISPATCH_TEST_ASSERT_STATUS(0, expect_parse_failure(test_name, k_string_arg, 1u, &text_long, 1u,
                                                       BSC_STATUS_ARGUMENT_TOO_LONG,
                                                       BSC_ARG_PARSE_ERROR_TEXT_TOO_LONG));
  DISPATCH_TEST_ASSERT_STATUS(0, expect_parse_failure(test_name, k_string_arg, 1u, &embedded_nul, 1u,
                                                       BSC_STATUS_INVALID_ARGUMENT_TYPE,
                                                       BSC_ARG_PARSE_ERROR_EMBEDDED_NUL));
  for (index = 0u; index < (size_t)BSC_MAX_ARGS + 1u; ++index) {
    too_many[index] = token_from_cstr("1");
  }
  DISPATCH_TEST_ASSERT_STATUS(0, expect_parse_failure(test_name, k_int_arg, 1u, too_many,
                                                       (size_t)BSC_MAX_ARGS + 1u,
                                                       BSC_STATUS_EXTRA_ARGUMENT,
                                                       BSC_ARG_PARSE_ERROR_EXTRA_ARGUMENT));
  return 0;
}

/** @brief Verify successful dispatch passes context, output, and parsed values. */
static int test_dispatch_success_passes_context_output_and_values(const char *test_name) {
  dispatch_fixture_t fixture;
  bsc_parsed_args_t parsed;
  bsc_arg_parse_error_t error;
  bsc_command_t command;
  bsc_output_t output = {dispatch_sink_write, NULL};
  bsc_string_view_t int_token = token_from_cstr("-3");
  bsc_string_view_t uint_token = token_from_cstr("4");
  bsc_string_view_t bool_token = token_from_cstr("true");
  bsc_string_view_t enum_token = token_from_cstr("high");
  char text_storage[] = "xxabcdyy";
  bsc_string_view_t string_token = token_from_parts(&text_storage[2], 4u);
  char secret_storage[] = "01234";
  bsc_string_view_t secret_token = token_from_parts(secret_storage, 5u);

  fixture_init(&fixture);
  command = make_command(&fixture, BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_NONE, k_int_arg, 1u, NULL);
  DISPATCH_TEST_ASSERT_STATUS(BSC_STATUS_OK, bsc_dispatch_command(&fixture, &command, &int_token, 1u, &parsed, &error, &output));
  DISPATCH_TEST_ASSERT_TRUE(fixture.handler_calls == 1);
  DISPATCH_TEST_ASSERT_TRUE(fixture.handler_command == &command);
  DISPATCH_TEST_ASSERT_TRUE(fixture.handler_args == &parsed);
  DISPATCH_TEST_ASSERT_TRUE(fixture.handler_output == &output);
  DISPATCH_TEST_ASSERT_TRUE(fixture.saw_command_context == 1);
  DISPATCH_TEST_ASSERT_TRUE(parsed.count == 1u && parsed.values[0].data.int_value == -3);
  DISPATCH_TEST_ASSERT_TRUE(error.reason == BSC_ARG_PARSE_ERROR_NONE);

  fixture_init(&fixture);
  command = make_command(&fixture, BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_NONE, k_uint_arg, 1u, NULL);
  DISPATCH_TEST_ASSERT_STATUS(BSC_STATUS_OK, bsc_dispatch_command(&fixture, &command, &uint_token, 1u, &parsed, &error, NULL));
  DISPATCH_TEST_ASSERT_TRUE(fixture.handler_output == NULL);
  DISPATCH_TEST_ASSERT_TRUE(parsed.values[0].data.uint_value == 4u);

  fixture_init(&fixture);
  command = make_command(&fixture, BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_NONE, k_bool_arg, 1u, NULL);
  DISPATCH_TEST_ASSERT_STATUS(BSC_STATUS_OK, bsc_dispatch_command(&fixture, &command, &bool_token, 1u, &parsed, &error, NULL));
  DISPATCH_TEST_ASSERT_TRUE(parsed.values[0].data.bool_value);

  fixture_init(&fixture);
  command = make_command(&fixture, BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_NONE, k_enum_arg, 1u, NULL);
  DISPATCH_TEST_ASSERT_STATUS(BSC_STATUS_OK, bsc_dispatch_command(&fixture, &command, &enum_token, 1u, &parsed, &error, NULL));
  DISPATCH_TEST_ASSERT_TRUE(parsed.values[0].data.enum_value == 22);

  fixture_init(&fixture);
  command = make_command(&fixture, BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_NONE, k_string_arg, 1u, NULL);
  DISPATCH_TEST_ASSERT_STATUS(BSC_STATUS_OK, bsc_dispatch_command(&fixture, &command, &string_token, 1u, &parsed, &error, NULL));
  DISPATCH_TEST_ASSERT_TRUE(parsed.values[0].data.text_value.data == &text_storage[2]);
  DISPATCH_TEST_ASSERT_TRUE(parsed.values[0].data.text_value.length == 4u);

  fixture_init(&fixture);
  command = make_command(&fixture, BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_NONE, k_secret_arg, 1u, NULL);
  DISPATCH_TEST_ASSERT_STATUS(BSC_STATUS_OK, bsc_dispatch_command(&fixture, &command, &secret_token, 1u, &parsed, &error, NULL));
  DISPATCH_TEST_ASSERT_TRUE(parsed.values[0].data.text_value.data == secret_storage);
  DISPATCH_TEST_ASSERT_TRUE(parsed.values[0].data.text_value.length == 5u);
  return 0;
}

/** @brief Verify valid handler statuses propagate and invalid statuses map to app error. */
static int test_dispatch_handler_status_policy(const char *test_name) {
  bsc_status_t valid_statuses[] = {
      BSC_STATUS_OK,
      BSC_STATUS_NO_INPUT,
      BSC_STATUS_LINE_TOO_LONG,
      BSC_STATUS_TOKEN_TOO_LONG,
      BSC_STATUS_TOO_MANY_TOKENS,
      BSC_STATUS_UNTERMINATED_QUOTE,
      BSC_STATUS_INVALID_SYNTAX,
      BSC_STATUS_UNKNOWN_COMMAND,
      BSC_STATUS_AMBIGUOUS_COMMAND,
      BSC_STATUS_GROUP_REQUIRES_SUBCOMMAND,
      BSC_STATUS_MISSING_ARGUMENT,
      BSC_STATUS_EXTRA_ARGUMENT,
      BSC_STATUS_INVALID_ARGUMENT_TYPE,
      BSC_STATUS_ARGUMENT_OUT_OF_RANGE,
      BSC_STATUS_ARGUMENT_TOO_LONG,
      BSC_STATUS_INVALID_ENUM_VALUE,
      BSC_STATUS_INVALID_DESCRIPTOR,
      BSC_STATUS_ACCESS_DENIED,
      BSC_STATUS_OUTPUT_TRUNCATED,
      BSC_STATUS_APP_ERROR,
      BSC_STATUS_INTERNAL_ERROR,
      BSC_STATUS_ARGUMENT_TOO_SHORT,
  };
  size_t index;
  bsc_string_view_t token = token_from_cstr("1");
  for (index = 0u; index < sizeof(valid_statuses) / sizeof(valid_statuses[0]); ++index) {
    dispatch_fixture_t fixture;
    bsc_parsed_args_t parsed;
    bsc_arg_parse_error_t error;
    bsc_command_t command;
    fixture_init(&fixture);
    fixture.handler_status = valid_statuses[index];
    command = make_command(&fixture, BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_NONE, k_int_arg, 1u, NULL);
    DISPATCH_TEST_ASSERT_STATUS(valid_statuses[index],
                                bsc_dispatch_command(&fixture, &command, &token, 1u, &parsed, &error, NULL));
    DISPATCH_TEST_ASSERT_TRUE(fixture.handler_calls == 1);
    DISPATCH_TEST_ASSERT_TRUE(parsed.count == 1u);
    DISPATCH_TEST_ASSERT_TRUE(error.reason == BSC_ARG_PARSE_ERROR_NONE);
  }

  {
    dispatch_fixture_t fixture;
    bsc_parsed_args_t parsed;
    bsc_arg_parse_error_t error;
    bsc_command_t command;
    fixture_init(&fixture);
    fixture.handler_status = (bsc_status_t)-1;
    command = make_command(&fixture, BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_NONE, k_int_arg, 1u, NULL);
    DISPATCH_TEST_ASSERT_STATUS(BSC_STATUS_APP_ERROR,
                                bsc_dispatch_command(&fixture, &command, &token, 1u, &parsed, &error, NULL));
  }
  {
    dispatch_fixture_t fixture;
    bsc_parsed_args_t parsed;
    bsc_arg_parse_error_t error;
    bsc_command_t command;
    fixture_init(&fixture);
    fixture.handler_status = (bsc_status_t)9999;
    command = make_command(&fixture, BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_NONE, k_int_arg, 1u, NULL);
    DISPATCH_TEST_ASSERT_STATUS(BSC_STATUS_APP_ERROR,
                                bsc_dispatch_command(&fixture, &command, &token, 1u, &parsed, &error, NULL));
  }
  return 0;
}

/** @brief Verify output truncation returned by a handler propagates unchanged. */
static int test_dispatch_output_truncation_propagates_from_handler(const char *test_name) {
  dispatch_fixture_t fixture;
  bsc_parsed_args_t parsed;
  bsc_arg_parse_error_t error;
  bsc_command_t command;
  dispatch_sink_t sink = {{0}, 0u};
  bsc_output_t output = {dispatch_sink_write, &sink};
  fixture_init(&fixture);
  command = make_command(&fixture, BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_NONE, NULL, 0u, NULL);
  command.handler = dispatch_output_handler;
  DISPATCH_TEST_ASSERT_STATUS(BSC_STATUS_OUTPUT_TRUNCATED,
                              bsc_dispatch_command(&fixture, &command, NULL, 0u, &parsed, &error, &output));
  DISPATCH_TEST_ASSERT_TRUE(fixture.handler_calls == 1);
  DISPATCH_TEST_ASSERT_TRUE(fixture.wrote_output == 1);
  DISPATCH_TEST_ASSERT_TRUE(sink.used == sizeof(sink.bytes));
  return 0;
}

/** @brief Verify failure clearing and handler-failure parsed-result retention. */
static int test_dispatch_state_clearing_and_handler_failure_keeps_parsed(const char *test_name) {
  dispatch_fixture_t fixture;
  bsc_parsed_args_t parsed;
  bsc_arg_parse_error_t error;
  bsc_command_t command;
  bsc_string_view_t bad = token_from_cstr("bad");
  bsc_string_view_t good = token_from_cstr("2");

  fixture_init(&fixture);
  fixture.allow_access = 0;
  command = make_command(&fixture, BSC_ACCESS_LOCKED, BSC_COMMAND_FLAG_NONE, k_int_arg, 1u, dispatch_access_callback);
  poison_parsed_args(&parsed);
  DISPATCH_TEST_ASSERT_STATUS(BSC_STATUS_ACCESS_DENIED,
                              bsc_dispatch_command(&fixture, &command, &bad, 1u, &parsed, &error, NULL));
  DISPATCH_TEST_ASSERT_TRUE(parsed_is_cleared(&parsed));
  DISPATCH_TEST_ASSERT_TRUE(error.reason == BSC_ARG_PARSE_ERROR_NONE);

  fixture_init(&fixture);
  command = make_command(&fixture, BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_NONE, k_int_arg, 1u, NULL);
  poison_parsed_args(&parsed);
  DISPATCH_TEST_ASSERT_STATUS(BSC_STATUS_INVALID_ARGUMENT_TYPE,
                              bsc_dispatch_command(&fixture, &command, &bad, 1u, &parsed, &error, NULL));
  DISPATCH_TEST_ASSERT_TRUE(parsed_is_cleared(&parsed));

  fixture_init(&fixture);
  fixture.handler_status = BSC_STATUS_APP_ERROR;
  command = make_command(&fixture, BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_NONE, k_int_arg, 1u, NULL);
  DISPATCH_TEST_ASSERT_STATUS(BSC_STATUS_APP_ERROR,
                              bsc_dispatch_command(&fixture, &command, &good, 1u, &parsed, &error, NULL));
  DISPATCH_TEST_ASSERT_TRUE(parsed.count == 1u);
  DISPATCH_TEST_ASSERT_TRUE(parsed.values[0].data.int_value == 2);
  DISPATCH_TEST_ASSERT_TRUE(error.reason == BSC_ARG_PARSE_ERROR_NONE);
  return 0;
}

/** @brief Verify independent dispatch calls do not share mutable state. */
static int test_dispatch_reentrant_independent_storage(const char *test_name) {
  dispatch_fixture_t fixture_a;
  dispatch_fixture_t fixture_b;
  bsc_parsed_args_t parsed_a;
  bsc_parsed_args_t parsed_b;
  bsc_arg_parse_error_t error_a;
  bsc_arg_parse_error_t error_b;
  bsc_command_t command_a;
  bsc_command_t command_b;
  bsc_string_view_t token_a = token_from_cstr("1");
  bsc_string_view_t token_b = token_from_cstr("2");
  fixture_init(&fixture_a);
  fixture_init(&fixture_b);
  command_a = make_command(&fixture_a, BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_NONE, k_int_arg, 1u, dispatch_access_callback);
  command_b = make_command(&fixture_b, BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_NONE, k_int_arg, 1u, dispatch_access_callback);
  DISPATCH_TEST_ASSERT_STATUS(BSC_STATUS_OK,
                              bsc_dispatch_command(&fixture_a, &command_a, &token_a, 1u, &parsed_a, &error_a, NULL));
  DISPATCH_TEST_ASSERT_STATUS(BSC_STATUS_OK,
                              bsc_dispatch_command(&fixture_b, &command_b, &token_b, 1u, &parsed_b, &error_b, NULL));
  DISPATCH_TEST_ASSERT_TRUE(fixture_a.access_calls == 1 && fixture_b.access_calls == 1);
  DISPATCH_TEST_ASSERT_TRUE(fixture_a.handler_calls == 1 && fixture_b.handler_calls == 1);
  DISPATCH_TEST_ASSERT_TRUE(fixture_a.handler_command == &command_a);
  DISPATCH_TEST_ASSERT_TRUE(fixture_b.handler_command == &command_b);
  DISPATCH_TEST_ASSERT_TRUE(parsed_a.values[0].data.int_value == 1);
  DISPATCH_TEST_ASSERT_TRUE(parsed_b.values[0].data.int_value == 2);
  DISPATCH_TEST_ASSERT_TRUE(error_a.reason == BSC_ARG_PARSE_ERROR_NONE);
  DISPATCH_TEST_ASSERT_TRUE(error_b.reason == BSC_ARG_PARSE_ERROR_NONE);
  return 0;
}

/** @brief Verify NULL app_context is passed unchanged to access and handler callbacks. */
static int test_dispatch_null_app_context_reaches_callbacks(const char *test_name) {
  dispatch_null_context_record_t record;
  bsc_parsed_args_t parsed;
  bsc_arg_parse_error_t error;
  bsc_command_t command;

  memset(&record, 0, sizeof(record));
  command.path = k_dispatch_path;
  command.path_len = 1u;
  command.node_type = BSC_NODE_COMMAND;
  command.args = NULL;
  command.arg_count = 0u;
  command.handler = dispatch_null_context_handler;
  command.command_context = &record;
  command.access = BSC_ACCESS_FACTORY;
  command.flags = BSC_COMMAND_FLAG_NONE;
  command.access_fn = dispatch_null_context_access;
  command.summary = "NULL context";
  command.description = "Verify NULL application context propagation.";

  DISPATCH_TEST_ASSERT_STATUS(BSC_STATUS_OK, bsc_dispatch_command(NULL, &command, NULL, 0u, &parsed, &error, NULL));
  DISPATCH_TEST_ASSERT_TRUE(record.access_calls == 1);
  DISPATCH_TEST_ASSERT_TRUE(record.handler_calls == 1);
  DISPATCH_TEST_ASSERT_TRUE(record.access_app_context == NULL);
  DISPATCH_TEST_ASSERT_TRUE(record.handler_app_context == NULL);
  DISPATCH_TEST_ASSERT_TRUE(record.access_command == &command);
  DISPATCH_TEST_ASSERT_TRUE(record.handler_command == &command);
  DISPATCH_TEST_ASSERT_TRUE(record.command_context_was_usable == 1);
  DISPATCH_TEST_ASSERT_TRUE(parsed.count == 0u);
  DISPATCH_TEST_ASSERT_TRUE(error.reason == BSC_ARG_PARSE_ERROR_NONE);
  return 0;
}

/** @brief Verify dispatch behavior in float-enabled and float-disabled builds. */
static int test_dispatch_float_configuration_behavior(const char *test_name) {
  dispatch_fixture_t fixture;
  bsc_parsed_args_t parsed;
  bsc_arg_parse_error_t error;
  bsc_command_t command;
  bsc_string_view_t float_token = token_from_cstr("1.25");
  bsc_string_view_t int_token = token_from_cstr("1");

  fixture_init(&fixture);
  command = make_command(&fixture, BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_NONE, k_int_arg, 1u, NULL);
  DISPATCH_TEST_ASSERT_STATUS(BSC_STATUS_OK,
                              bsc_dispatch_command(&fixture, &command, &int_token, 1u, &parsed, &error, NULL));

  fixture_init(&fixture);
  command = make_command(&fixture, BSC_ACCESS_NORMAL, BSC_COMMAND_FLAG_NONE, k_float_arg, 1u, NULL);
#if BSC_ENABLE_FLOAT
  DISPATCH_TEST_ASSERT_STATUS(BSC_STATUS_OK,
                              bsc_dispatch_command(&fixture, &command, &float_token, 1u, &parsed, &error, NULL));
  DISPATCH_TEST_ASSERT_TRUE(parsed.count == 1u);
  DISPATCH_TEST_ASSERT_TRUE(parsed.values[0].type == BSC_ARG_FLOAT);
  DISPATCH_TEST_ASSERT_TRUE(parsed.values[0].data.float_value > 1.24f && parsed.values[0].data.float_value < 1.26f);
#else
  DISPATCH_TEST_ASSERT_STATUS(BSC_STATUS_INVALID_DESCRIPTOR,
                              bsc_dispatch_command(&fixture, &command, &float_token, 1u, &parsed, &error, NULL));
  DISPATCH_TEST_ASSERT_TRUE(error.reason == BSC_ARG_PARSE_ERROR_FLOAT_DISABLED);
  DISPATCH_TEST_ASSERT_TRUE(fixture.handler_calls == 0);
#endif
  return 0;
}

/** @brief Aggregate selected-command dispatch and access-enforcement tests. */
int bsc_run_dispatch_tests(void) {
  int failures = 0;
  RUN_DISPATCH_TEST(test_dispatch_access_defaults_without_callback);
  RUN_DISPATCH_TEST(test_dispatch_access_callback_allows_all_levels);
  RUN_DISPATCH_TEST(test_dispatch_access_callback_denies_all_levels);
  RUN_DISPATCH_TEST(test_dispatch_hidden_flag_follows_access_policy);
  RUN_DISPATCH_TEST(test_dispatch_api_validation_and_clearing);
  RUN_DISPATCH_TEST(test_dispatch_descriptor_validation_before_access);
  RUN_DISPATCH_TEST(test_dispatch_parser_failures_preserve_diagnostics);
  RUN_DISPATCH_TEST(test_dispatch_success_passes_context_output_and_values);
  RUN_DISPATCH_TEST(test_dispatch_handler_status_policy);
  RUN_DISPATCH_TEST(test_dispatch_output_truncation_propagates_from_handler);
  RUN_DISPATCH_TEST(test_dispatch_state_clearing_and_handler_failure_keeps_parsed);
  RUN_DISPATCH_TEST(test_dispatch_reentrant_independent_storage);
  RUN_DISPATCH_TEST(test_dispatch_null_app_context_reaches_callbacks);
  RUN_DISPATCH_TEST(test_dispatch_float_configuration_behavior);
  return failures;
}
