#include <stdio.h>
#include <string.h>

#include "bsc_output.h"
#include "bsc_status.h"
#include "bsc_string_view.h"

/**
 * @brief Run tokenizer tests supplied by the tokenizer test module.
 */
int bsc_run_tokenizer_tests(void);
/**
 * @brief Run descriptor type tests supplied by the types test module.
 */
int bsc_run_types_tests(void);
/**
 * @brief Run registry validation tests supplied by the registry test module.
 */
int bsc_run_registry_tests(void);
/**
 * @brief Run matcher tests supplied by the matcher test module.
 */
int bsc_run_matcher_tests(void);
/**
 * @brief Run argument parser tests supplied by the args test module.
 */
int bsc_run_args_tests(void);
/**
 * @brief Run selected-command dispatch tests supplied by the dispatch module.
 */
int bsc_run_dispatch_tests(void);

/**
 * @brief Fail the current host test when a condition is false.
 */
#define TEST_ASSERT_TRUE(condition)                                                                \
  do {                                                                                             \
    if (!(condition)) {                                                                            \
      printf("FAIL: %s: %s:%d: %s\n", test_name, __FILE__, __LINE__, #condition);                  \
      return 1;                                                                                    \
    }                                                                                              \
  } while (0)

/**
 * @brief Assert that a status-producing call returns the expected status code.
 */
#define TEST_ASSERT_STATUS(expected, actual) TEST_ASSERT_TRUE((expected) == (actual))
/**
 * @brief Assert that two diagnostic strings are equal in the host harness.
 */
#define TEST_ASSERT_STR_EQ(expected, actual) TEST_ASSERT_TRUE(strcmp((expected), (actual)) == 0)

/**
 * @brief Run one local test function and accumulate failures without changing runner flow.
 */
#define RUN_TEST(fn)                                                                               \
  do {                                                                                             \
    int result;                                                                                    \
    const char *test_name = #fn;                                                                   \
    result = fn(test_name);                                                                        \
    if (result != 0) {                                                                             \
      failures += 1;                                                                               \
    } else {                                                                                       \
      printf("PASS: %s\n", test_name);                                                             \
    }                                                                                              \
  } while (0)

/**
 * @brief Small host fixture that captures bytes written through bsc_output_t.
 */
typedef struct capture_sink {
  char buffer[16];
  size_t used;
} capture_sink_t;

/**
 * @brief Simulate a bounded output sink for output helper tests.
 *
 * Copies only the bytes that fit in the fixture buffer and deliberately returns
 * a short write when full so tests can verify OUTPUT_TRUNCATED mapping.
 */
static size_t capture_write(void *user, const char *data, size_t length) {
  capture_sink_t *sink = (capture_sink_t *)user;
  size_t available;
  size_t to_copy;

  if (sink == NULL || data == NULL) {
    return 0u;
  }

  available = sizeof(sink->buffer) - sink->used;
  to_copy = length < available ? length : available;
  if (to_copy > 0u) {
    memcpy(&sink->buffer[sink->used], data, to_copy);
    sink->used += to_copy;
  }
  return to_copy;
}

/**
 * @brief Verify stable symbolic names for representative and unknown status values.
 */
static int test_status_names(const char *test_name) {
  TEST_ASSERT_STR_EQ("BSC_STATUS_OK", bsc_status_name(BSC_STATUS_OK));
  TEST_ASSERT_STR_EQ("BSC_STATUS_LINE_TOO_LONG", bsc_status_name(BSC_STATUS_LINE_TOO_LONG));
  TEST_ASSERT_STR_EQ("BSC_STATUS_INVALID_SYNTAX", bsc_status_name(BSC_STATUS_INVALID_SYNTAX));
  TEST_ASSERT_STR_EQ("BSC_STATUS_INVALID_DESCRIPTOR", bsc_status_name(BSC_STATUS_INVALID_DESCRIPTOR));
  TEST_ASSERT_STR_EQ("BSC_STATUS_ARGUMENT_TOO_SHORT", bsc_status_name(BSC_STATUS_ARGUMENT_TOO_SHORT));
  TEST_ASSERT_STR_EQ("BSC_STATUS_UNKNOWN", bsc_status_name((bsc_status_t)999));
  return 0;
}

/**
 * @brief Protect exact C-string equality for a borrowed string view.
 */
static int test_string_view_exact_match(const char *test_name) {
  bsc_string_view_t view = bsc_string_view_from_cstr("status");
  TEST_ASSERT_TRUE(bsc_string_view_equals_cstr(view, "status"));
  return 0;
}

/**
 * @brief Protect mismatch behavior for different-length and different-text strings.
 */
static int test_string_view_mismatch(const char *test_name) {
  bsc_string_view_t view = bsc_string_view_from_cstr("status");
  TEST_ASSERT_TRUE(!bsc_string_view_equals_cstr(view, "stats"));
  TEST_ASSERT_TRUE(!bsc_string_view_equals_cstr(view, "status-now"));
  return 0;
}

/**
 * @brief Verify length-based comparisons for a view into the middle of a buffer.
 */
static int test_string_view_non_null_terminated_slice(const char *test_name) {
  const char text[] = "xxGain=2048";
  bsc_string_view_t view = bsc_string_view_from_parts(&text[2], 4u);
  TEST_ASSERT_TRUE(bsc_string_view_equals_cstr(view, "Gain"));
  TEST_ASSERT_TRUE(bsc_string_view_equals_cstr_ignore_case(view, "gain"));
  TEST_ASSERT_TRUE(!bsc_string_view_equals_cstr(view, "Gain=2048"));
  return 0;
}

/**
 * @brief Verify empty borrowed views compare correctly against empty and NULL text.
 */
static int test_string_view_empty_behavior(const char *test_name) {
  bsc_string_view_t empty = bsc_string_view_from_parts(NULL, 0u);
  TEST_ASSERT_TRUE(bsc_string_view_is_empty(empty));
  TEST_ASSERT_TRUE(bsc_string_view_equals_cstr(empty, ""));
  TEST_ASSERT_TRUE(bsc_string_view_equals_cstr(empty, NULL));
  TEST_ASSERT_TRUE(!bsc_string_view_equals_cstr(empty, "x"));
  return 0;
}

/**
 * @brief Verify write and writeln append bytes to the bounded capture sink in order.
 */
static int test_output_write_and_writeln_capture(const char *test_name) {
  capture_sink_t sink = {{0}, 0u};
  bsc_output_t out = {capture_write, &sink};

  TEST_ASSERT_STATUS(BSC_STATUS_OK, bsc_out_write(&out, "OK"));
  TEST_ASSERT_STATUS(BSC_STATUS_OK, bsc_out_writeln(&out, ": ready"));
  TEST_ASSERT_TRUE(sink.used == 10u);
  TEST_ASSERT_TRUE(memcmp(sink.buffer, "OK: ready\n", sink.used) == 0);
  return 0;
}

/**
 * @brief Verify output helper short writes are reported as output truncation.
 */
static int test_output_truncation(const char *test_name) {
  capture_sink_t sink = {{0}, 0u};
  bsc_output_t out = {capture_write, &sink};

  TEST_ASSERT_STATUS(BSC_STATUS_OUTPUT_TRUNCATED, bsc_out_write(&out, "0123456789abcdef-overflow"));
  TEST_ASSERT_TRUE(sink.used == sizeof(sink.buffer));
  TEST_ASSERT_TRUE(memcmp(sink.buffer, "0123456789abcdef", sizeof(sink.buffer)) == 0);
  return 0;
}

/**
 * @brief Aggregate local host tests and module-specific suites.
 *
 * Returns nonzero if any local test or delegated module runner reports failure.
 */
int main(void) {
  int failures = 0;

  RUN_TEST(test_status_names);
  RUN_TEST(test_string_view_exact_match);
  RUN_TEST(test_string_view_mismatch);
  RUN_TEST(test_string_view_non_null_terminated_slice);
  RUN_TEST(test_string_view_empty_behavior);
  RUN_TEST(test_output_write_and_writeln_capture);
  RUN_TEST(test_output_truncation);

  failures += bsc_run_tokenizer_tests();
  failures += bsc_run_types_tests();
  failures += bsc_run_registry_tests();
  failures += bsc_run_matcher_tests();
  failures += bsc_run_args_tests();
  failures += bsc_run_dispatch_tests();

  if (failures != 0) {
    printf("FAIL: %d test(s) failed\n", failures);
    return 1;
  }

  printf("PASS: all host tests passed\n");
  return 0;
}
