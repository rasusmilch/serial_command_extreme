#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bsc_args.h"
#include "bsc_config.h"
#include "bsc_output.h"
#include "bsc_registry.h"
#include "bsc_status.h"

#define ARG_TEST_ASSERT_TRUE(condition)                                                            \
  do {                                                                                             \
    if (!(condition)) {                                                                            \
      printf("FAIL: %s: %s:%d: %s\n", test_name, __FILE__, __LINE__, #condition);                \
      return 1;                                                                                    \
    }                                                                                              \
  } while (0)
#define ARG_TEST_ASSERT_STATUS(expected, actual) ARG_TEST_ASSERT_TRUE((expected) == (actual))
#define ARG_TEST_ASSERT_STR(expected, actual) ARG_TEST_ASSERT_TRUE(strcmp((expected), (actual)) == 0)
#define BSC_STRINGIFY_VALUE(value) #value
#define BSC_STRINGIFY(value) BSC_STRINGIFY_VALUE(value)
#define RUN_ARG_TEST(fn)                                                                           \
  do {                                                                                             \
    int result;                                                                                    \
    const char *test_name = #fn;                                                                   \
    result = fn(test_name);                                                                        \
    if (result != 0) {                                                                             \
      failures += 1;                                                                               \
    } else {                                                                                       \
      printf("PASS: %s\n", test_name);                                                           \
    }                                                                                              \
  } while (0)

#if BSC_MAX_FLOAT_FRACTION_DIGITS == 1u
#define ARG_FLOAT_ACCEPTED_FRACTION "1.1"
#define ARG_FLOAT_REJECTED_FRACTION "1.12"
#define ARG_FLOAT_FRACTION_LIMIT_TEXT "1"
#elif BSC_MAX_FLOAT_FRACTION_DIGITS == 2u
#define ARG_FLOAT_ACCEPTED_FRACTION "1.12"
#define ARG_FLOAT_REJECTED_FRACTION "1.123"
#define ARG_FLOAT_FRACTION_LIMIT_TEXT "2"
#elif BSC_MAX_FLOAT_FRACTION_DIGITS == 3u
#define ARG_FLOAT_ACCEPTED_FRACTION "1.123"
#define ARG_FLOAT_REJECTED_FRACTION "1.1234"
#define ARG_FLOAT_FRACTION_LIMIT_TEXT "3"
#elif BSC_MAX_FLOAT_FRACTION_DIGITS == 4u
#define ARG_FLOAT_ACCEPTED_FRACTION "1.1234"
#define ARG_FLOAT_REJECTED_FRACTION "1.12345"
#define ARG_FLOAT_FRACTION_LIMIT_TEXT "4"
#elif BSC_MAX_FLOAT_FRACTION_DIGITS == 5u
#define ARG_FLOAT_ACCEPTED_FRACTION "1.12345"
#define ARG_FLOAT_REJECTED_FRACTION "1.123456"
#define ARG_FLOAT_FRACTION_LIMIT_TEXT "5"
#elif BSC_MAX_FLOAT_FRACTION_DIGITS == 6u
#define ARG_FLOAT_ACCEPTED_FRACTION "1.123456"
#define ARG_FLOAT_REJECTED_FRACTION "1.1234567"
#define ARG_FLOAT_FRACTION_LIMIT_TEXT "6"
#else
#error "BSC_MAX_FLOAT_FRACTION_DIGITS must be in the documented range 1 through 6"
#endif
#define ARG_FLOAT_FRACTION_MESSAGE                                                                 \
  "argument 'ratio': expected at most " ARG_FLOAT_FRACTION_LIMIT_TEXT " digits after the decimal point"

/** @brief Shared one-token command path borrowed by command fixtures for the duration of each test. */
static const char *const k_path[] = {"set"};

/** @brief Stable enum choices used by enum parser tests without copying names or help text. */
static const bsc_enum_choice_t k_choices[] = {{"low", 10, NULL}, {"HIGH", 20, NULL}, {"auto", 30, NULL}};

/**
 * @brief No-op handler used only to make executable command descriptors registry-compatible.
 *
 * The parser tests never invoke handlers; this callback exists so command descriptors can model a normal
 * executable command while proving parser calls retain no command, argument, output, or application pointers.
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
 * @brief Build a default argument descriptor for one parser test.
 *
 * The returned descriptor borrows the caller-provided name and shared enum fixture storage. Bounds are initialized
 * to the broad approved runtime domain so individual tests can narrow only the fields relevant to their case.
 */
static bsc_arg_def_t make_arg(const char *name, bsc_arg_type_t type) {
  bsc_arg_def_t arg;
  memset(&arg, 0, sizeof(arg));
  arg.name = name;
  arg.type = type;
  arg.min_int = INT32_MIN;
  arg.max_int = INT32_MAX;
  arg.max_uint = UINT32_MAX;
  arg.min_float = -1000000000.0f;
  arg.max_float = 1000000000.0f;
  arg.max_length = BSC_MAX_TOKEN_LEN;
  if (type == BSC_ARG_ENUM) {
    arg.enum_choices = k_choices;
    arg.enum_choice_count = sizeof(k_choices) / sizeof(k_choices[0]);
  }
  return arg;
}

/**
 * @brief Build a command descriptor around caller-owned positional argument metadata.
 *
 * The descriptor borrows the supplied argument array for the immediate parser or registry call, uses the shared path
 * fixture, and does not allocate or retain storage beyond the returned by-value descriptor.
 */
static bsc_command_t make_command(const bsc_arg_def_t *args, size_t arg_count) {
  bsc_command_t command;
  memset(&command, 0, sizeof(command));
  command.path = k_path;
  command.path_len = 1u;
  command.node_type = BSC_NODE_COMMAND;
  command.args = args;
  command.arg_count = arg_count;
  command.handler = test_handler;
  command.access = BSC_ACCESS_NORMAL;
  return command;
}

/**
 * @brief Create a string view over a NUL-terminated test literal without transferring ownership.
 *
 * This helper is used only for test data that intentionally has no embedded NUL; tests that need explicit-length
 * slices or embedded bytes call bsc_string_view_from_parts() directly.
 */
static bsc_string_view_t view(const char *text) { return bsc_string_view_from_parts(text, strlen(text)); }

/**
 * @brief Parse one token against one descriptor and return the parser status.
 *
 * The helper constructs a temporary command descriptor that borrows the caller-owned descriptor and token view for
 * the synchronous call. Parsed output and diagnostics remain caller-owned and are not retained.
 */
static bsc_status_t parse_one(const bsc_arg_def_t *arg,
                              bsc_string_view_t token,
                              bsc_parsed_args_t *out,
                              bsc_arg_parse_error_t *error) {
  bsc_command_t command = make_command(arg, 1u);
  return bsc_parse_command_args(&command, &token, 1u, out, error);
}

/**
 * @brief Caller-owned capture buffer used to test bounded diagnostic output callbacks.
 *
 * buffer stores copied diagnostic bytes, used is the number of bytes accepted, and limit allows tests to simulate
 * short writes without heap allocation or hidden static state.
 */
typedef struct capture_sink {
  char buffer[256];
  size_t used;
  size_t limit;
} capture_sink_t;

/**
 * @brief Copy diagnostic output into a caller-owned capture sink.
 *
 * The callback retains no data pointers, copies at most the configured sink capacity, and returns a short byte count
 * when the output writer should observe BSC_STATUS_OUTPUT_TRUNCATED.
 */
static size_t capture_write(void *user, const char *data, size_t length) {
  capture_sink_t *sink = (capture_sink_t *)user;
  size_t available;
  size_t to_copy;
  if (sink == NULL || data == NULL) {
    return 0u;
  }
  available = sink->limit - sink->used;
  to_copy = length < available ? length : available;
  if (to_copy > 0u) {
    memcpy(&sink->buffer[sink->used], data, to_copy);
    sink->used += to_copy;
  }
  return to_copy;
}

/**
 * @brief Render a parse diagnostic into a caller-owned NUL-terminated test buffer.
 *
 * The diagnostic writer owns no storage, so this helper supplies a capture sink, asserts successful rendering, copies
 * the bounded output into buffer, and appends the terminator expected by string assertions.
 */
static int write_error(const char *test_name,
                       const bsc_command_t *command,
                       const bsc_arg_parse_error_t *error,
                       char *buffer,
                       size_t buffer_size) {
  capture_sink_t sink;
  bsc_output_t out;
  memset(&sink, 0, sizeof(sink));
  sink.limit = sizeof(sink.buffer);
  out.write = capture_write;
  out.user = &sink;
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_OK, bsc_arg_parse_error_write(command, error, &out));
  ARG_TEST_ASSERT_TRUE(sink.used + 1u <= buffer_size);
  memcpy(buffer, sink.buffer, sink.used);
  buffer[sink.used] = '\0';
  return 0;
}

/** @brief Verify parsed-result and diagnostic clear helpers reset all public fields and tolerate NULL. */
static int test_parsed_result_and_error_clear(const char *test_name) {
  bsc_parsed_args_t args;
  bsc_arg_parse_error_t error;
  size_t index;
  args.count = BSC_MAX_ARGS;
  for (index = 0u; index < (size_t)BSC_MAX_ARGS; ++index) {
    args.values[index].type = BSC_ARG_SECRET;
    args.values[index].data.text_value = view("secret");
  }
  bsc_parsed_args_clear(&args);
  ARG_TEST_ASSERT_TRUE(args.count == 0u);
  for (index = 0u; index < (size_t)BSC_MAX_ARGS; ++index) {
    ARG_TEST_ASSERT_TRUE(args.values[index].type == BSC_ARG_NONE);
    ARG_TEST_ASSERT_TRUE(args.values[index].data.text_value.data == NULL);
    ARG_TEST_ASSERT_TRUE(args.values[index].data.text_value.length == 0u);
  }
  bsc_parsed_args_clear(NULL);
  error.reason = BSC_ARG_PARSE_ERROR_EXTRA_ARGUMENT;
  error.arg_index = 9u;
  error.token_offset = 3u;
  bsc_arg_parse_error_clear(&error);
  ARG_TEST_ASSERT_TRUE(error.reason == BSC_ARG_PARSE_ERROR_NONE);
  ARG_TEST_ASSERT_TRUE(error.arg_index == 0u);
  ARG_TEST_ASSERT_TRUE(error.token_offset == 0u);
  bsc_arg_parse_error_clear(NULL);
  return 0;
}

/** @brief Verify argument-count failures and required parser pointers produce deterministic diagnostics. */
static int test_argument_counts_and_api(const char *test_name) {
  bsc_arg_def_t arg = make_arg("value", BSC_ARG_INT);
  bsc_command_t command = make_command(&arg, 1u);
  bsc_command_t no_args = make_command(NULL, 0u);
  bsc_string_view_t token = view("1");
  bsc_string_view_t two[2] = {view("1"), view("2")};
  bsc_parsed_args_t out;
  bsc_arg_parse_error_t error;
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_OK, bsc_parse_command_args(&no_args, NULL, 0u, &out, &error));
  ARG_TEST_ASSERT_TRUE(out.count == 0u);
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_MISSING_ARGUMENT, bsc_parse_command_args(&command, NULL, 0u, &out, &error));
  ARG_TEST_ASSERT_TRUE(error.reason == BSC_ARG_PARSE_ERROR_MISSING_ARGUMENT);
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_EXTRA_ARGUMENT, bsc_parse_command_args(&command, two, 2u, &out, &error));
  ARG_TEST_ASSERT_TRUE(error.reason == BSC_ARG_PARSE_ERROR_EXTRA_ARGUMENT);
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_INTERNAL_ERROR, bsc_parse_command_args(&command, &token, 1u, NULL, &error));
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_INTERNAL_ERROR, bsc_parse_command_args(&command, &token, 1u, &out, NULL));
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_INTERNAL_ERROR, bsc_parse_command_args(&command, NULL, 1u, &out, &error));
  return 0;
}

/** @brief Verify the fixed parsed-result storage accepts exactly BSC_MAX_ARGS positional values. */
static int test_exact_max_args(const char *test_name) {
  bsc_arg_def_t args[BSC_MAX_ARGS];
  bsc_string_view_t tokens[BSC_MAX_ARGS];
  bsc_command_t command;
  bsc_parsed_args_t out;
  bsc_arg_parse_error_t error;
  size_t index;
  for (index = 0u; index < (size_t)BSC_MAX_ARGS; ++index) {
    args[index] = make_arg("flag", BSC_ARG_BOOL);
    tokens[index] = view(index % 2u == 0u ? "on" : "0");
  }
  command = make_command(args, BSC_MAX_ARGS);
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_OK, bsc_parse_command_args(&command, tokens, BSC_MAX_ARGS, &out, &error));
  ARG_TEST_ASSERT_TRUE(out.count == BSC_MAX_ARGS);
  return 0;
}

/** @brief Verify signed decimal parsing, representable bounds, syntax rejection, and overflow direction. */
static int test_signed_integer_boundaries(const char *test_name) {
  bsc_arg_def_t arg = make_arg("i", BSC_ARG_INT);
  bsc_parsed_args_t out;
  bsc_arg_parse_error_t error;
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_OK, parse_one(&arg, view("0"), &out, &error));
  ARG_TEST_ASSERT_TRUE(out.values[0].data.int_value == 0);
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_OK, parse_one(&arg, view("00042"), &out, &error));
  ARG_TEST_ASSERT_TRUE(out.values[0].data.int_value == 42);
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_OK, parse_one(&arg, view("-42"), &out, &error));
  ARG_TEST_ASSERT_TRUE(out.values[0].data.int_value == -42);
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_OK, parse_one(&arg, view("-2147483648"), &out, &error));
  ARG_TEST_ASSERT_TRUE(out.values[0].data.int_value == INT32_MIN);
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_OK, parse_one(&arg, view("2147483647"), &out, &error));
  ARG_TEST_ASSERT_TRUE(out.values[0].data.int_value == INT32_MAX);
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_ARGUMENT_OUT_OF_RANGE, parse_one(&arg, view("-2147483649"), &out, &error));
  ARG_TEST_ASSERT_TRUE(error.reason == BSC_ARG_PARSE_ERROR_BELOW_MINIMUM);
  ARG_TEST_ASSERT_TRUE(out.count == 0u);
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_ARGUMENT_OUT_OF_RANGE, parse_one(&arg, view("2147483648"), &out, &error));
  ARG_TEST_ASSERT_TRUE(error.reason == BSC_ARG_PARSE_ERROR_ABOVE_MAXIMUM);
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_INVALID_ARGUMENT_TYPE, parse_one(&arg, view("+1"), &out, &error));
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_INVALID_ARGUMENT_TYPE, parse_one(&arg, view("-"), &out, &error));
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_INVALID_ARGUMENT_TYPE, parse_one(&arg, view("12x"), &out, &error));
  return 0;
}

/** @brief Verify signed descriptor ranges and exact non-NUL-terminated token-slice consumption. */
static int test_signed_integer_descriptor_ranges_and_slices(const char *test_name) {
  bsc_arg_def_t arg = make_arg("i", BSC_ARG_INT);
  char text[] = "123x";
  bsc_parsed_args_t out;
  bsc_arg_parse_error_t error;
  arg.min_int = -200;
  arg.max_int = 200;
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_OK, parse_one(&arg, view("5"), &out, &error));
  arg.min_int = -5;
  arg.max_int = 5;
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_ARGUMENT_OUT_OF_RANGE, parse_one(&arg, view("6"), &out, &error));
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_ARGUMENT_OUT_OF_RANGE, parse_one(&arg, view("-6"), &out, &error));
  arg.min_int = -200;
  arg.max_int = 200;
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_OK,
                         parse_one(&arg, bsc_string_view_from_parts(text, 3u), &out, &error));
  ARG_TEST_ASSERT_TRUE(out.count == 1u);
  ARG_TEST_ASSERT_TRUE(out.values[0].data.int_value == 123);
  return 0;
}

/** @brief Verify unsigned decimal parsing, uint32 boundary handling, syntax rejection, and descriptor ranges. */
static int test_unsigned_integer_boundaries(const char *test_name) {
  bsc_arg_def_t arg = make_arg("u", BSC_ARG_UINT);
  bsc_parsed_args_t out;
  bsc_arg_parse_error_t error;
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_OK, parse_one(&arg, view("0"), &out, &error));
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_OK, parse_one(&arg, view("00042"), &out, &error));
  ARG_TEST_ASSERT_TRUE(out.values[0].data.uint_value == 42u);
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_OK, parse_one(&arg, view("4294967295"), &out, &error));
  ARG_TEST_ASSERT_TRUE(out.values[0].data.uint_value == UINT32_MAX);
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_ARGUMENT_OUT_OF_RANGE, parse_one(&arg, view("4294967296"), &out, &error));
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_INVALID_ARGUMENT_TYPE, parse_one(&arg, view("-1"), &out, &error));
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_INVALID_ARGUMENT_TYPE, parse_one(&arg, view("+1"), &out, &error));
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_INVALID_ARGUMENT_TYPE, parse_one(&arg, view("12x"), &out, &error));
  arg.min_uint = 10u;
  arg.max_uint = 20u;
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_OK, parse_one(&arg, view("10"), &out, &error));
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_OK, parse_one(&arg, view("20"), &out, &error));
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_ARGUMENT_OUT_OF_RANGE, parse_one(&arg, view("9"), &out, &error));
  return 0;
}

/** @brief Verify all approved case-insensitive boolean spellings and rejected alternatives. */
static int test_boolean_spellings(const char *test_name) {
  bsc_arg_def_t arg = make_arg("enabled", BSC_ARG_BOOL);
  bsc_parsed_args_t out;
  bsc_arg_parse_error_t error;
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_OK, parse_one(&arg, view("on"), &out, &error));
  ARG_TEST_ASSERT_TRUE(out.values[0].data.bool_value);
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_OK, parse_one(&arg, view("TRUE"), &out, &error));
  ARG_TEST_ASSERT_TRUE(out.values[0].data.bool_value);
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_OK, parse_one(&arg, view("1"), &out, &error));
  ARG_TEST_ASSERT_TRUE(out.values[0].data.bool_value);
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_OK, parse_one(&arg, view("off"), &out, &error));
  ARG_TEST_ASSERT_TRUE(!out.values[0].data.bool_value);
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_OK, parse_one(&arg, view("False"), &out, &error));
  ARG_TEST_ASSERT_TRUE(!out.values[0].data.bool_value);
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_OK, parse_one(&arg, view("0"), &out, &error));
  ARG_TEST_ASSERT_TRUE(!out.values[0].data.bool_value);
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_INVALID_ARGUMENT_TYPE, parse_one(&arg, view("yes"), &out, &error));
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_INVALID_ARGUMENT_TYPE, parse_one(&arg, view("truex"), &out, &error));
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_INVALID_ARGUMENT_TYPE, parse_one(&arg, view("xtrue"), &out, &error));
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_INVALID_ARGUMENT_TYPE, parse_one(&arg, view("2"), &out, &error));
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_INVALID_ARGUMENT_TYPE, parse_one(&arg, view("00"), &out, &error));
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_INVALID_ARGUMENT_TYPE, parse_one(&arg, view("01"), &out, &error));
  return 0;
}

/** @brief Verify enum matching returns semantic values and diagnostics list choices in descriptor order. */
static int test_enum_values_and_diagnostics(const char *test_name) {
  bsc_arg_def_t arg = make_arg("mode", BSC_ARG_ENUM);
  bsc_command_t command = make_command(&arg, 1u);
  bsc_parsed_args_t out;
  bsc_arg_parse_error_t error;
  char message[128];
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_OK, parse_one(&arg, view("LOW"), &out, &error));
  ARG_TEST_ASSERT_TRUE(out.values[0].data.enum_value == 10);
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_OK, parse_one(&arg, view("high"), &out, &error));
  ARG_TEST_ASSERT_TRUE(out.values[0].data.enum_value == 20);
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_INVALID_ENUM_VALUE, parse_one(&arg, view("auto-x"), &out, &error));
  ARG_TEST_ASSERT_TRUE(error.reason == BSC_ARG_PARSE_ERROR_INVALID_ENUM_CHOICE);
  ARG_TEST_ASSERT_TRUE(write_error(test_name, &command, &error, message, sizeof(message)) == 0);
  ARG_TEST_ASSERT_STR("argument 'mode': expected one of: low, HIGH, auto", message);
  return 0;
}

/** @brief Verify string and secret values remain borrowed, bounded, NUL-free views and clear after failures. */
static int test_string_and_secret_lifetimes(const char *test_name) {
  bsc_arg_def_t args[2];
  bsc_command_t command;
  bsc_string_view_t tokens[2];
  char name[] = "caf\xc3\xa9";
  char secret[] = "sensitive";
  char bad_secret[] = {'b', 'a', 'd', '\0', 'x'};
  bsc_parsed_args_t out;
  bsc_arg_parse_error_t error;
  args[0] = make_arg("name", BSC_ARG_STRING);
  args[0].min_length = 0u;
  args[0].max_length = 8u;
  args[1] = make_arg("password", BSC_ARG_SECRET);
  args[1].min_length = 3u;
  args[1].max_length = 16u;
  command = make_command(args, 2u);
  tokens[0] = bsc_string_view_from_parts(name, strlen(name));
  tokens[1] = bsc_string_view_from_parts(secret, strlen(secret));
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_OK, bsc_parse_command_args(&command, tokens, 2u, &out, &error));
  ARG_TEST_ASSERT_TRUE(out.values[0].type == BSC_ARG_STRING);
  ARG_TEST_ASSERT_TRUE(out.values[0].data.text_value.data == name);
  ARG_TEST_ASSERT_TRUE(out.values[1].type == BSC_ARG_SECRET);
  ARG_TEST_ASSERT_TRUE(out.values[1].data.text_value.data == secret);
  tokens[1] = bsc_string_view_from_parts("xx", 2u);
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_ARGUMENT_TOO_SHORT, bsc_parse_command_args(&command, tokens, 2u, &out, &error));
  ARG_TEST_ASSERT_TRUE(out.count == 0u);
  ARG_TEST_ASSERT_TRUE(out.values[0].data.text_value.data == NULL);
  tokens[1] = bsc_string_view_from_parts("this-is-too-long-for-the-secret", 31u);
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_ARGUMENT_TOO_LONG, bsc_parse_command_args(&command, tokens, 2u, &out, &error));
  tokens[1] = bsc_string_view_from_parts(bad_secret, sizeof(bad_secret));
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_INVALID_ARGUMENT_TYPE, bsc_parse_command_args(&command, tokens, 2u, &out, &error));
  ARG_TEST_ASSERT_TRUE(error.reason == BSC_ARG_PARSE_ERROR_EMBEDDED_NUL);
  return 0;
}

#if BSC_ENABLE_FLOAT
/** @brief Verify compact-float accepted forms, active precision, boundary values, and negative-zero normalization. */
static int test_compact_float_enabled_successes(const char *test_name) {
  bsc_arg_def_t arg = make_arg("ratio", BSC_ARG_FLOAT);
  bsc_parsed_args_t out;
  bsc_arg_parse_error_t error;
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_OK, parse_one(&arg, view("0"), &out, &error));
  ARG_TEST_ASSERT_TRUE(out.values[0].data.float_value == 0.0f);
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_OK, parse_one(&arg, view("12"), &out, &error));
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_OK, parse_one(&arg, view("-12"), &out, &error));
#if BSC_MAX_FLOAT_FRACTION_DIGITS >= 2u
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_OK, parse_one(&arg, view("0.25"), &out, &error));
#endif
#if BSC_MAX_FLOAT_FRACTION_DIGITS >= 3u
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_OK, parse_one(&arg, view("-0.001"), &out, &error));
#endif
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_OK, parse_one(&arg, view("10.0"), &out, &error));
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_OK, parse_one(&arg, view(ARG_FLOAT_ACCEPTED_FRACTION), &out, &error));
  ARG_TEST_ASSERT_TRUE(out.count == 1u);
  ARG_TEST_ASSERT_TRUE(out.values[0].type == BSC_ARG_FLOAT);
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_OK, parse_one(&arg, view("1000000000"), &out, &error));
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_OK, parse_one(&arg, view("1000000000.0"), &out, &error));
#if BSC_MAX_FLOAT_FRACTION_DIGITS == 6u
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_OK, parse_one(&arg, view("1000000000.000000"), &out, &error));
#endif
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_OK, parse_one(&arg, view("-1000000000"), &out, &error));
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_OK, parse_one(&arg, view("-1000000000.0"), &out, &error));
#if BSC_MAX_FLOAT_FRACTION_DIGITS == 6u
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_OK, parse_one(&arg, view("-1000000000.000000"), &out, &error));
#endif
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_OK, parse_one(&arg, view("-0.0"), &out, &error));
  ARG_TEST_ASSERT_TRUE(out.values[0].data.float_value == 0.0f);
  return 0;
}

/** @brief Verify compact-float rejected grammar, active precision, range direction, token offsets, and NUL handling. */
static int test_compact_float_enabled_rejections(const char *test_name) {
  bsc_arg_def_t arg = make_arg("ratio", BSC_ARG_FLOAT);
  char text[] = "12.5x";
  char nul_text[] = {'1', '.', '\0', '2'};
  bsc_parsed_args_t out;
  bsc_arg_parse_error_t error;
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_INVALID_ARGUMENT_TYPE, parse_one(&arg, view("+12.5"), &out, &error));
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_INVALID_ARGUMENT_TYPE, parse_one(&arg, view(".5"), &out, &error));
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_INVALID_ARGUMENT_TYPE, parse_one(&arg, view("12."), &out, &error));
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_INVALID_ARGUMENT_TYPE, parse_one(&arg, view("1e-3"), &out, &error));
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_INVALID_ARGUMENT_TYPE, parse_one(&arg, view("NaN"), &out, &error));
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_INVALID_ARGUMENT_TYPE, parse_one(&arg, view("inf"), &out, &error));
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_INVALID_ARGUMENT_TYPE, parse_one(&arg, view("0x1.8p2"), &out, &error));
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_INVALID_ARGUMENT_TYPE, parse_one(&arg, view("12.3.4"), &out, &error));
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_INVALID_ARGUMENT_TYPE, parse_one(&arg, view("-"), &out, &error));
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_INVALID_ARGUMENT_TYPE, parse_one(&arg, view("."), &out, &error));
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_INVALID_ARGUMENT_TYPE, parse_one(&arg, view(ARG_FLOAT_REJECTED_FRACTION), &out, &error));
  ARG_TEST_ASSERT_TRUE(error.reason == BSC_ARG_PARSE_ERROR_TOO_MANY_FLOAT_FRACTION_DIGITS);
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_ARGUMENT_OUT_OF_RANGE, parse_one(&arg, view("1000000001"), &out, &error));
  ARG_TEST_ASSERT_TRUE(error.reason == BSC_ARG_PARSE_ERROR_ABOVE_MAXIMUM);
  ARG_TEST_ASSERT_TRUE(error.token_offset == 9u);
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_ARGUMENT_OUT_OF_RANGE, parse_one(&arg, view("-1000000001"), &out, &error));
  ARG_TEST_ASSERT_TRUE(error.reason == BSC_ARG_PARSE_ERROR_BELOW_MINIMUM);
  ARG_TEST_ASSERT_TRUE(error.token_offset == 10u);
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_ARGUMENT_OUT_OF_RANGE, parse_one(&arg, view("1000000000.1"), &out, &error));
  ARG_TEST_ASSERT_TRUE(error.reason == BSC_ARG_PARSE_ERROR_ABOVE_MAXIMUM);
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_ARGUMENT_OUT_OF_RANGE, parse_one(&arg, view("-1000000000.1"), &out, &error));
  ARG_TEST_ASSERT_TRUE(error.reason == BSC_ARG_PARSE_ERROR_BELOW_MINIMUM);
#if BSC_MAX_FLOAT_FRACTION_DIGITS == 6u
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_ARGUMENT_OUT_OF_RANGE, parse_one(&arg, view("1000000000.000001"), &out, &error));
  ARG_TEST_ASSERT_TRUE(error.reason == BSC_ARG_PARSE_ERROR_ABOVE_MAXIMUM);
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_ARGUMENT_OUT_OF_RANGE, parse_one(&arg, view("-1000000000.000001"), &out, &error));
  ARG_TEST_ASSERT_TRUE(error.reason == BSC_ARG_PARSE_ERROR_BELOW_MINIMUM);
#endif
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_INVALID_ARGUMENT_TYPE,
                         parse_one(&arg, bsc_string_view_from_parts(text, 5u), &out, &error));
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_INVALID_ARGUMENT_TYPE,
                         parse_one(&arg, bsc_string_view_from_parts(nul_text, sizeof(nul_text)), &out, &error));
  return 0;
}

/** @brief Verify compact-float descriptor ranges are enforced after successful restricted conversion. */
static int test_compact_float_descriptor_ranges(const char *test_name) {
  bsc_arg_def_t arg = make_arg("ratio", BSC_ARG_FLOAT);
  bsc_parsed_args_t out;
  bsc_arg_parse_error_t error;
  arg.min_float = -(float)BSC_COMPACT_FLOAT_MAX_MAGNITUDE;
  arg.max_float = (float)BSC_COMPACT_FLOAT_MAX_MAGNITUDE;
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_OK, parse_one(&arg, view("-1000000000"), &out, &error));
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_OK, parse_one(&arg, view("1000000000"), &out, &error));
  arg.min_float = -1.0f;
  arg.max_float = 1.0f;
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_OK, parse_one(&arg, view("-1.0"), &out, &error));
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_OK, parse_one(&arg, view("1.0"), &out, &error));
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_ARGUMENT_OUT_OF_RANGE, parse_one(&arg, view("-1.1"), &out, &error));
  ARG_TEST_ASSERT_TRUE(error.reason == BSC_ARG_PARSE_ERROR_BELOW_MINIMUM);
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_ARGUMENT_OUT_OF_RANGE, parse_one(&arg, view("1.1"), &out, &error));
  ARG_TEST_ASSERT_TRUE(error.reason == BSC_ARG_PARSE_ERROR_ABOVE_MAXIMUM);
  return 0;
}

/** @brief Verify every legal BSC_MAX_FLOAT_FRACTION_DIGITS value accepts exactly its configured precision. */
static int test_compact_float_configured_fraction_digits(const char *test_name) {
  bsc_arg_def_t arg = make_arg("ratio", BSC_ARG_FLOAT);
  bsc_command_t command = make_command(&arg, 1u);
  bsc_parsed_args_t out;
  bsc_arg_parse_error_t error;
  char message[128];
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_OK, parse_one(&arg, view(ARG_FLOAT_ACCEPTED_FRACTION), &out, &error));
  ARG_TEST_ASSERT_TRUE(out.count == 1u);
  ARG_TEST_ASSERT_TRUE(out.values[0].type == BSC_ARG_FLOAT);
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_INVALID_ARGUMENT_TYPE, parse_one(&arg, view(ARG_FLOAT_REJECTED_FRACTION), &out, &error));
  ARG_TEST_ASSERT_TRUE(error.reason == BSC_ARG_PARSE_ERROR_TOO_MANY_FLOAT_FRACTION_DIGITS);
  ARG_TEST_ASSERT_TRUE(write_error(test_name, &command, &error, message, sizeof(message)) == 0);
  ARG_TEST_ASSERT_STR(ARG_FLOAT_FRACTION_MESSAGE, message);
#if BSC_MAX_FLOAT_FRACTION_DIGITS != 1u
  ARG_TEST_ASSERT_TRUE(strstr(message, "1 digits") == NULL);
#endif
#if BSC_MAX_FLOAT_FRACTION_DIGITS != 2u
  ARG_TEST_ASSERT_TRUE(strstr(message, "2 digits") == NULL);
#endif
#if BSC_MAX_FLOAT_FRACTION_DIGITS != 3u
  ARG_TEST_ASSERT_TRUE(strstr(message, "3 digits") == NULL);
#endif
#if BSC_MAX_FLOAT_FRACTION_DIGITS != 4u
  ARG_TEST_ASSERT_TRUE(strstr(message, "4 digits") == NULL);
#endif
#if BSC_MAX_FLOAT_FRACTION_DIGITS != 5u
  ARG_TEST_ASSERT_TRUE(strstr(message, "5 digits") == NULL);
#endif
#if BSC_MAX_FLOAT_FRACTION_DIGITS != 6u
  ARG_TEST_ASSERT_TRUE(strstr(message, "6 digits") == NULL);
#endif
  ARG_TEST_ASSERT_TRUE(strstr(message, ARG_FLOAT_REJECTED_FRACTION) == NULL);
  return 0;
}
#endif

/** @brief Verify float registry policy in enabled builds and parser/diagnostic behavior in disabled builds. */
static int test_float_disabled_or_registry_validation(const char *test_name) {
  bsc_arg_def_t arg = make_arg("ratio", BSC_ARG_FLOAT);
  bsc_command_t command = make_command(&arg, 1u);
  bsc_registry_validation_error_t reg_error;
#if BSC_ENABLE_FLOAT
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_OK, bsc_registry_validate(&command, 1u, &reg_error));
  arg.min_float = NAN;
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_INVALID_DESCRIPTOR, bsc_registry_validate(&command, 1u, &reg_error));
  ARG_TEST_ASSERT_TRUE(reg_error.reason == BSC_REGISTRY_ERROR_INVALID_ARG_RANGE);
  arg.min_float = -1.0f;
  arg.max_float = INFINITY;
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_INVALID_DESCRIPTOR, bsc_registry_validate(&command, 1u, &reg_error));
#else
  bsc_parsed_args_t out;
  bsc_arg_parse_error_t error;
  bsc_string_view_t token = view("1.0");
  char message[128];
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_INVALID_DESCRIPTOR, bsc_registry_validate(&command, 1u, &reg_error));
  ARG_TEST_ASSERT_TRUE(reg_error.reason == BSC_REGISTRY_ERROR_FLOAT_DISABLED);
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_INVALID_DESCRIPTOR, bsc_parse_command_args(&command, &token, 1u, &out, &error));
  ARG_TEST_ASSERT_TRUE(error.reason == BSC_ARG_PARSE_ERROR_FLOAT_DISABLED);
  ARG_TEST_ASSERT_TRUE(write_error(test_name, &command, &error, message, sizeof(message)) == 0);
  ARG_TEST_ASSERT_STR("argument 'ratio': floating-point arguments are disabled in this build", message);
#endif
  return 0;
}

/**
 * @brief Assert that parsing one token produces an exact operator-facing diagnostic.
 *
 * The helper renders into a caller-owned fixed buffer, verifies the requested status and exact message, and protects
 * secret/raw-token non-disclosure by checking representative rejected tokens are absent from output.
 */
static int expect_message_for_parse(const char *test_name,
                                    const bsc_arg_def_t *arg,
                                    bsc_string_view_t token,
                                    bsc_status_t expected_status,
                                    const char *expected_message) {
  bsc_command_t command = make_command(arg, 1u);
  bsc_parsed_args_t out;
  bsc_arg_parse_error_t error;
  char message[256];
  ARG_TEST_ASSERT_STATUS(expected_status, bsc_parse_command_args(&command, &token, 1u, &out, &error));
  ARG_TEST_ASSERT_TRUE(error.reason != BSC_ARG_PARSE_ERROR_NONE);
  ARG_TEST_ASSERT_TRUE(write_error(test_name, &command, &error, message, sizeof(message)) == 0);
  ARG_TEST_ASSERT_STR(expected_message, message);
  ARG_TEST_ASSERT_TRUE(strstr(message, "xSECRETx") == NULL);
  ARG_TEST_ASSERT_TRUE(strstr(message, "1e-3") == NULL);
  return 0;
}

/** @brief Verify every public diagnostic category renders a deterministic non-generic operator message. */
static int test_operator_diagnostics(const char *test_name) {
  bsc_arg_def_t arg = make_arg("value", BSC_ARG_INT);
  bsc_arg_def_t uint_arg = make_arg("count", BSC_ARG_UINT);
  bsc_arg_def_t float_arg = make_arg("ratio", BSC_ARG_FLOAT);
  bsc_arg_def_t bool_arg = make_arg("enabled", BSC_ARG_BOOL);
  bsc_arg_def_t enum_arg = make_arg("mode", BSC_ARG_ENUM);
  bsc_arg_def_t text_arg = make_arg("name", BSC_ARG_STRING);
  bsc_command_t command = make_command(&arg, 1u);
  bsc_parsed_args_t out;
  bsc_arg_parse_error_t error;
  char message[256];
  bsc_string_view_t two[2] = {view("1"), view("2")};
  char nul_text[] = {'b', 'a', 'd', '\0', 'x'};

  ARG_TEST_ASSERT_STATUS(BSC_STATUS_MISSING_ARGUMENT, bsc_parse_command_args(&command, NULL, 0u, &out, &error));
  ARG_TEST_ASSERT_TRUE(write_error(test_name, &command, &error, message, sizeof(message)) == 0);
  ARG_TEST_ASSERT_STR("missing required argument 'value'", message);
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_EXTRA_ARGUMENT, bsc_parse_command_args(&command, two, 2u, &out, &error));
  ARG_TEST_ASSERT_TRUE(write_error(test_name, &command, &error, message, sizeof(message)) == 0);
  ARG_TEST_ASSERT_STR("unexpected extra argument", message);
  ARG_TEST_ASSERT_TRUE(expect_message_for_parse(test_name, &arg, view("xSECRETx"),
                                                BSC_STATUS_INVALID_ARGUMENT_TYPE,
                                                "argument 'value': expected a signed decimal integer") == 0);
  ARG_TEST_ASSERT_TRUE(expect_message_for_parse(test_name, &uint_arg, view("-1"),
                                                BSC_STATUS_INVALID_ARGUMENT_TYPE,
                                                "argument 'count': expected an unsigned decimal integer") == 0);
#if BSC_ENABLE_FLOAT
  ARG_TEST_ASSERT_TRUE(expect_message_for_parse(test_name, &float_arg, view("1e-3"),
                                                BSC_STATUS_INVALID_ARGUMENT_TYPE,
                                                "argument 'ratio': expected a decimal number such as -12.5") == 0);
  ARG_TEST_ASSERT_TRUE(expect_message_for_parse(test_name, &float_arg, view(ARG_FLOAT_REJECTED_FRACTION),
                                                BSC_STATUS_INVALID_ARGUMENT_TYPE,
                                                ARG_FLOAT_FRACTION_MESSAGE) == 0);
  ARG_TEST_ASSERT_TRUE(expect_message_for_parse(test_name, &float_arg, view("1000000001"),
                                                BSC_STATUS_ARGUMENT_OUT_OF_RANGE,
                                                "argument 'ratio': value is above the configured maximum") == 0);
  ARG_TEST_ASSERT_TRUE(expect_message_for_parse(test_name, &float_arg, view("-1000000001"),
                                                BSC_STATUS_ARGUMENT_OUT_OF_RANGE,
                                                "argument 'ratio': value is below the configured minimum") == 0);
#endif
  ARG_TEST_ASSERT_TRUE(expect_message_for_parse(test_name, &bool_arg, view("yes"),
                                                BSC_STATUS_INVALID_ARGUMENT_TYPE,
                                                "argument 'enabled': expected on, off, true, false, 1, or 0") == 0);
  ARG_TEST_ASSERT_TRUE(expect_message_for_parse(test_name, &enum_arg, view("other"),
                                                BSC_STATUS_INVALID_ENUM_VALUE,
                                                "argument 'mode': expected one of: low, HIGH, auto") == 0);
  arg.min_int = 10;
  arg.max_int = 20;
  ARG_TEST_ASSERT_TRUE(expect_message_for_parse(test_name, &arg, view("9"),
                                                BSC_STATUS_ARGUMENT_OUT_OF_RANGE,
                                                "argument 'value': value is below the configured minimum") == 0);
  ARG_TEST_ASSERT_TRUE(expect_message_for_parse(test_name, &arg, view("21"),
                                                BSC_STATUS_ARGUMENT_OUT_OF_RANGE,
                                                "argument 'value': value is above the configured maximum") == 0);
  text_arg.min_length = 3u;
  text_arg.max_length = 4u;
  ARG_TEST_ASSERT_TRUE(expect_message_for_parse(test_name, &text_arg, view("ab"),
                                                BSC_STATUS_ARGUMENT_TOO_SHORT,
                                                "argument 'name': text is shorter than the configured minimum length") == 0);
  ARG_TEST_ASSERT_TRUE(expect_message_for_parse(test_name, &text_arg, view("abcde"),
                                                BSC_STATUS_ARGUMENT_TOO_LONG,
                                                "argument 'name': text is longer than the configured maximum length") == 0);
  ARG_TEST_ASSERT_TRUE(expect_message_for_parse(test_name, &text_arg,
                                                bsc_string_view_from_parts(nul_text, sizeof(nul_text)),
                                                BSC_STATUS_INVALID_ARGUMENT_TYPE,
                                                "argument 'name': embedded NUL byte is not allowed") == 0);
  error.reason = BSC_ARG_PARSE_ERROR_INVALID_API;
  error.arg_index = 0u;
  ARG_TEST_ASSERT_TRUE(write_error(test_name, &command, &error, message, sizeof(message)) == 0);
  ARG_TEST_ASSERT_STR("command argument parser was called incorrectly", message);
  error.reason = BSC_ARG_PARSE_ERROR_INVALID_DESCRIPTOR;
  ARG_TEST_ASSERT_TRUE(write_error(test_name, &command, &error, message, sizeof(message)) == 0);
  ARG_TEST_ASSERT_STR("command argument configuration is invalid", message);
  error.reason = BSC_ARG_PARSE_ERROR_NONE;
  ARG_TEST_ASSERT_TRUE(write_error(test_name, &command, &error, message, sizeof(message)) == 0);
  ARG_TEST_ASSERT_STR("", message);
  return 0;
}

/** @brief Verify diagnostic writer propagates bounded output callback truncation. */
static int test_output_truncation_from_diagnostic_writer(const char *test_name) {
  bsc_arg_def_t arg = make_arg("value", BSC_ARG_INT);
  bsc_command_t command = make_command(&arg, 1u);
  bsc_arg_parse_error_t error;
  capture_sink_t sink;
  bsc_output_t out;
  memset(&sink, 0, sizeof(sink));
  sink.limit = 4u;
  out.write = capture_write;
  out.user = &sink;
  error.reason = BSC_ARG_PARSE_ERROR_INVALID_VALUE;
  error.arg_index = 0u;
  error.token_offset = 0u;
  ARG_TEST_ASSERT_STATUS(BSC_STATUS_OUTPUT_TRUNCATED, bsc_arg_parse_error_write(&command, &error, &out));
  return 0;
}

/** @brief Run all typed argument parser tests and return the number of failed cases. */
int bsc_run_args_tests(void) {
  int failures = 0;
  RUN_ARG_TEST(test_parsed_result_and_error_clear);
  RUN_ARG_TEST(test_argument_counts_and_api);
  RUN_ARG_TEST(test_exact_max_args);
  RUN_ARG_TEST(test_signed_integer_boundaries);
  RUN_ARG_TEST(test_signed_integer_descriptor_ranges_and_slices);
  RUN_ARG_TEST(test_unsigned_integer_boundaries);
  RUN_ARG_TEST(test_boolean_spellings);
  RUN_ARG_TEST(test_enum_values_and_diagnostics);
  RUN_ARG_TEST(test_string_and_secret_lifetimes);
#if BSC_ENABLE_FLOAT
  RUN_ARG_TEST(test_compact_float_enabled_successes);
  RUN_ARG_TEST(test_compact_float_enabled_rejections);
  RUN_ARG_TEST(test_compact_float_descriptor_ranges);
  RUN_ARG_TEST(test_compact_float_configured_fraction_digits);
#endif
  RUN_ARG_TEST(test_float_disabled_or_registry_validation);
  RUN_ARG_TEST(test_operator_diagnostics);
  RUN_ARG_TEST(test_output_truncation_from_diagnostic_writer);
  return failures;
}
