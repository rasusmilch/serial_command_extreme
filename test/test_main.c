#include <stdio.h>
#include <string.h>

#include "bsc_output.h"
#include "bsc_status.h"
#include "bsc_string_view.h"

int bsc_run_tokenizer_tests(void);

#define TEST_ASSERT_TRUE(condition)                                                                \
  do {                                                                                             \
    if (!(condition)) {                                                                            \
      printf("FAIL: %s: %s:%d: %s\n", test_name, __FILE__, __LINE__, #condition);                  \
      return 1;                                                                                    \
    }                                                                                              \
  } while (0)

#define TEST_ASSERT_STATUS(expected, actual) TEST_ASSERT_TRUE((expected) == (actual))
#define TEST_ASSERT_STR_EQ(expected, actual) TEST_ASSERT_TRUE(strcmp((expected), (actual)) == 0)

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

typedef struct capture_sink {
  char buffer[16];
  size_t used;
} capture_sink_t;

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

static int test_status_names(const char *test_name) {
  TEST_ASSERT_STR_EQ("BSC_STATUS_OK", bsc_status_name(BSC_STATUS_OK));
  TEST_ASSERT_STR_EQ("BSC_STATUS_LINE_TOO_LONG", bsc_status_name(BSC_STATUS_LINE_TOO_LONG));
  TEST_ASSERT_STR_EQ("BSC_STATUS_INVALID_SYNTAX", bsc_status_name(BSC_STATUS_INVALID_SYNTAX));
  TEST_ASSERT_STR_EQ("BSC_STATUS_UNKNOWN", bsc_status_name((bsc_status_t)999));
  return 0;
}

static int test_string_view_exact_match(const char *test_name) {
  bsc_string_view_t view = bsc_string_view_from_cstr("status");
  TEST_ASSERT_TRUE(bsc_string_view_equals_cstr(view, "status"));
  return 0;
}

static int test_string_view_mismatch(const char *test_name) {
  bsc_string_view_t view = bsc_string_view_from_cstr("status");
  TEST_ASSERT_TRUE(!bsc_string_view_equals_cstr(view, "stats"));
  TEST_ASSERT_TRUE(!bsc_string_view_equals_cstr(view, "status-now"));
  return 0;
}

static int test_string_view_non_null_terminated_slice(const char *test_name) {
  const char text[] = "xxGain=2048";
  bsc_string_view_t view = bsc_string_view_from_parts(&text[2], 4u);
  TEST_ASSERT_TRUE(bsc_string_view_equals_cstr(view, "Gain"));
  TEST_ASSERT_TRUE(bsc_string_view_equals_cstr_ignore_case(view, "gain"));
  TEST_ASSERT_TRUE(!bsc_string_view_equals_cstr(view, "Gain=2048"));
  return 0;
}

static int test_string_view_empty_behavior(const char *test_name) {
  bsc_string_view_t empty = bsc_string_view_from_parts(NULL, 0u);
  TEST_ASSERT_TRUE(bsc_string_view_is_empty(empty));
  TEST_ASSERT_TRUE(bsc_string_view_equals_cstr(empty, ""));
  TEST_ASSERT_TRUE(bsc_string_view_equals_cstr(empty, NULL));
  TEST_ASSERT_TRUE(!bsc_string_view_equals_cstr(empty, "x"));
  return 0;
}

static int test_output_write_and_writeln_capture(const char *test_name) {
  capture_sink_t sink = {{0}, 0u};
  bsc_output_t out = {capture_write, &sink};

  TEST_ASSERT_STATUS(BSC_STATUS_OK, bsc_out_write(&out, "OK"));
  TEST_ASSERT_STATUS(BSC_STATUS_OK, bsc_out_writeln(&out, ": ready"));
  TEST_ASSERT_TRUE(sink.used == 10u);
  TEST_ASSERT_TRUE(memcmp(sink.buffer, "OK: ready\n", sink.used) == 0);
  return 0;
}

static int test_output_truncation(const char *test_name) {
  capture_sink_t sink = {{0}, 0u};
  bsc_output_t out = {capture_write, &sink};

  TEST_ASSERT_STATUS(BSC_STATUS_OUTPUT_TRUNCATED, bsc_out_write(&out, "0123456789abcdef-overflow"));
  TEST_ASSERT_TRUE(sink.used == sizeof(sink.buffer));
  TEST_ASSERT_TRUE(memcmp(sink.buffer, "0123456789abcdef", sizeof(sink.buffer)) == 0);
  return 0;
}

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

  if (failures != 0) {
    printf("FAIL: %d test(s) failed\n", failures);
    return 1;
  }

  printf("PASS: all skeleton host tests passed\n");
  return 0;
}
