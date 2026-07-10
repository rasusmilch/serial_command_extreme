#include <stdio.h>
#include <string.h>

#include "bsc_config.h"
#include "bsc_status.h"
#include "bsc_string_view.h"
#include "bsc_tokenizer.h"

/**
 * @brief Fail the current tokenizer test when a condition is false.
 */
#define TOKEN_TEST_ASSERT_TRUE(condition)                                                            \
  do {                                                                                                \
    if (!(condition)) {                                                                               \
      printf("FAIL: %s: %s:%d: %s\n", test_name, __FILE__, __LINE__, #condition);                    \
      return 1;                                                                                       \
    }                                                                                                 \
  } while (0)

/**
 * @brief Assert that tokenizer status matches the expected status code.
 */
#define TOKEN_TEST_ASSERT_STATUS(expected, actual) TOKEN_TEST_ASSERT_TRUE((expected) == (actual))

/**
 * @brief Run one tokenizer test and accumulate failures for the module runner.
 */
#define RUN_TOKEN_TEST(fn)                                                                            \
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

/**
 * @brief Compare a borrowed token view with expected C-string text by length.
 */
static int token_equals(bsc_string_view_t token, const char *expected) {
  size_t expected_length = strlen(expected);
  if (token.length != expected_length) {
    return 0;
  }
  if (expected_length == 0u) {
    return 1;
  }
  if (token.data == NULL) {
    return 0;
  }
  return memcmp(token.data, expected, expected_length) == 0;
}

/**
 * @brief Verify a token view still points inside the caller-owned line buffer.
 */
static int token_points_into_line(bsc_string_view_t token, const char *line, size_t storage_length) {
  return token.data >= line && token.data <= line + storage_length;
}

/**
 * @brief Forward a mutable C string to bsc_tokenize_line using strlen for length.
 */
static int tokenize_text(char *line,
                         bsc_string_view_t *tokens,
                         size_t token_capacity,
                         size_t *token_count) {
  return bsc_tokenize_line(line, strlen(line), tokens, token_capacity, token_count);
}

/**
 * @brief Protect invalid-argument handling and token-count clearing behavior.
 */
static int test_tokenizer_invalid_arguments(const char *test_name) {
  bsc_string_view_t tokens[2];
  char line[] = "status";
  size_t count = 123u;

  TOKEN_TEST_ASSERT_STATUS(BSC_STATUS_INTERNAL_ERROR,
                           bsc_tokenize_line(NULL, 0u, tokens, 2u, &count));
  TOKEN_TEST_ASSERT_TRUE(count == 0u);

  count = 123u;
  TOKEN_TEST_ASSERT_STATUS(BSC_STATUS_INTERNAL_ERROR,
                           bsc_tokenize_line(line, strlen(line), tokens, 2u, NULL));

  count = 123u;
  TOKEN_TEST_ASSERT_STATUS(BSC_STATUS_INTERNAL_ERROR,
                           bsc_tokenize_line(line, strlen(line), NULL, 2u, &count));
  TOKEN_TEST_ASSERT_TRUE(count == 0u);

  count = 123u;
  TOKEN_TEST_ASSERT_STATUS(BSC_STATUS_TOO_MANY_TOKENS,
                           bsc_tokenize_line(line, strlen(line), NULL, 0u, &count));
  TOKEN_TEST_ASSERT_TRUE(count == 0u);

  count = 123u;
  TOKEN_TEST_ASSERT_STATUS(BSC_STATUS_NO_INPUT, bsc_tokenize_line("", 0u, NULL, 0u, &count));
  TOKEN_TEST_ASSERT_TRUE(count == 0u);
  return 0;
}

/**
 * @brief Verify blank input returns NO_INPUT and leaves no borrowed tokens.
 */
static int test_tokenizer_no_input(const char *test_name) {
  bsc_string_view_t tokens[1];
  char empty[] = "";
  char spaces[] = "   ";
  char tabs[] = "\t\t";
  size_t count = 99u;

  TOKEN_TEST_ASSERT_STATUS(BSC_STATUS_NO_INPUT, tokenize_text(empty, tokens, 1u, &count));
  TOKEN_TEST_ASSERT_TRUE(count == 0u);
  count = 99u;
  TOKEN_TEST_ASSERT_STATUS(BSC_STATUS_NO_INPUT, tokenize_text(spaces, tokens, 1u, &count));
  TOKEN_TEST_ASSERT_TRUE(count == 0u);
  count = 99u;
  TOKEN_TEST_ASSERT_STATUS(BSC_STATUS_NO_INPUT, tokenize_text(tabs, tokens, 1u, &count));
  TOKEN_TEST_ASSERT_TRUE(count == 0u);
  return 0;
}

/**
 * @brief Verify basic space and tab token splitting into borrowed views.
 */
static int test_tokenizer_basic_tokens(const char *test_name) {
  bsc_string_view_t tokens[4];
  char one[] = "status";
  char two[] = "gain 2048";
  char spaced[] = "  gain   2048  ";
  char tabbed[] = "gain\t2048\tstatus";
  size_t count = 0u;

  TOKEN_TEST_ASSERT_STATUS(BSC_STATUS_OK, tokenize_text(one, tokens, 4u, &count));
  TOKEN_TEST_ASSERT_TRUE(count == 1u);
  TOKEN_TEST_ASSERT_TRUE(token_equals(tokens[0], "status"));
  TOKEN_TEST_ASSERT_TRUE(token_points_into_line(tokens[0], one, sizeof(one)));

  TOKEN_TEST_ASSERT_STATUS(BSC_STATUS_OK, tokenize_text(two, tokens, 4u, &count));
  TOKEN_TEST_ASSERT_TRUE(count == 2u);
  TOKEN_TEST_ASSERT_TRUE(token_equals(tokens[0], "gain"));
  TOKEN_TEST_ASSERT_TRUE(token_equals(tokens[1], "2048"));

  TOKEN_TEST_ASSERT_STATUS(BSC_STATUS_OK, tokenize_text(spaced, tokens, 4u, &count));
  TOKEN_TEST_ASSERT_TRUE(count == 2u);
  TOKEN_TEST_ASSERT_TRUE(token_equals(tokens[0], "gain"));
  TOKEN_TEST_ASSERT_TRUE(token_equals(tokens[1], "2048"));

  TOKEN_TEST_ASSERT_STATUS(BSC_STATUS_OK, tokenize_text(tabbed, tokens, 4u, &count));
  TOKEN_TEST_ASSERT_TRUE(count == 3u);
  TOKEN_TEST_ASSERT_TRUE(token_equals(tokens[0], "gain"));
  TOKEN_TEST_ASSERT_TRUE(token_equals(tokens[1], "2048"));
  TOKEN_TEST_ASSERT_TRUE(token_equals(tokens[2], "status"));
  return 0;
}

/**
 * @brief Protect literal treatment for punctuation and shell-like bytes.
 */
static int test_tokenizer_literal_punctuation_and_non_shell_bytes(const char *test_name) {
  bsc_string_view_t tokens[6];
  char line[] = "a=b,c:1 #tag //path path\\part 'single'";
  size_t count = 0u;

  TOKEN_TEST_ASSERT_STATUS(BSC_STATUS_OK, tokenize_text(line, tokens, 6u, &count));
  TOKEN_TEST_ASSERT_TRUE(count == 5u);
  TOKEN_TEST_ASSERT_TRUE(token_equals(tokens[0], "a=b,c:1"));
  TOKEN_TEST_ASSERT_TRUE(token_equals(tokens[1], "#tag"));
  TOKEN_TEST_ASSERT_TRUE(token_equals(tokens[2], "//path"));
  TOKEN_TEST_ASSERT_TRUE(token_equals(tokens[3], "path\\part"));
  TOKEN_TEST_ASSERT_TRUE(token_equals(tokens[4], "'single'"));
  return 0;
}

/**
 * @brief Verify quoted tokens, including empty quoted tokens, exclude quote bytes.
 */
static int test_tokenizer_quoted_tokens(const char *test_name) {
  bsc_string_view_t tokens[4];
  char line[] = "set name \"shop floor\"";
  char empty[] = "set \"\" done";
  size_t count = 0u;

  TOKEN_TEST_ASSERT_STATUS(BSC_STATUS_OK, tokenize_text(line, tokens, 4u, &count));
  TOKEN_TEST_ASSERT_TRUE(count == 3u);
  TOKEN_TEST_ASSERT_TRUE(token_equals(tokens[0], "set"));
  TOKEN_TEST_ASSERT_TRUE(token_equals(tokens[1], "name"));
  TOKEN_TEST_ASSERT_TRUE(token_equals(tokens[2], "shop floor"));
  TOKEN_TEST_ASSERT_TRUE(token_points_into_line(tokens[2], line, sizeof(line)));
  TOKEN_TEST_ASSERT_TRUE(tokens[2].data[0] != '"');
  TOKEN_TEST_ASSERT_TRUE(tokens[2].data[tokens[2].length - 1u] != '"');

  TOKEN_TEST_ASSERT_STATUS(BSC_STATUS_OK, tokenize_text(empty, tokens, 4u, &count));
  TOKEN_TEST_ASSERT_TRUE(count == 3u);
  TOKEN_TEST_ASSERT_TRUE(token_equals(tokens[0], "set"));
  TOKEN_TEST_ASSERT_TRUE(tokens[1].length == 0u);
  TOKEN_TEST_ASSERT_TRUE(token_points_into_line(tokens[1], empty, sizeof(empty)));
  TOKEN_TEST_ASSERT_TRUE(token_equals(tokens[2], "done"));
  return 0;
}

/**
 * @brief Verify supported quoted escapes are compacted in place in the line buffer.
 */
static int test_tokenizer_quoted_escapes_compact_in_place(const char *test_name) {
  bsc_string_view_t tokens[3];
  char line[] = "say \"a\\\"b\" \"c\\\\d\"";
  size_t count = 0u;

  TOKEN_TEST_ASSERT_STATUS(BSC_STATUS_OK, tokenize_text(line, tokens, 3u, &count));
  TOKEN_TEST_ASSERT_TRUE(count == 3u);
  TOKEN_TEST_ASSERT_TRUE(token_equals(tokens[0], "say"));
  TOKEN_TEST_ASSERT_TRUE(token_equals(tokens[1], "a\"b"));
  TOKEN_TEST_ASSERT_TRUE(token_equals(tokens[2], "c\\d"));
  TOKEN_TEST_ASSERT_TRUE(token_points_into_line(tokens[1], line, sizeof(line)));
  TOKEN_TEST_ASSERT_TRUE(token_points_into_line(tokens[2], line, sizeof(line)));
  TOKEN_TEST_ASSERT_TRUE(memcmp(tokens[1].data, "a\"b", tokens[1].length) == 0);
  TOKEN_TEST_ASSERT_TRUE(memcmp(tokens[2].data, "c\\d", tokens[2].length) == 0);
  return 0;
}

/**
 * @brief Verify unsupported or incomplete escape sequences fail with invalid syntax.
 */
static int test_tokenizer_escape_rejections(const char *test_name) {
  bsc_string_view_t tokens[2];
  char unsupported[] = "\"bad\\n\"";
  char escape_end[] = "\"bad\\";
  size_t count = 77u;

  TOKEN_TEST_ASSERT_STATUS(BSC_STATUS_INVALID_SYNTAX, tokenize_text(unsupported, tokens, 2u, &count));
  TOKEN_TEST_ASSERT_TRUE(count == 0u);
  count = 77u;
  TOKEN_TEST_ASSERT_STATUS(BSC_STATUS_INVALID_SYNTAX, tokenize_text(escape_end, tokens, 2u, &count));
  TOKEN_TEST_ASSERT_TRUE(count == 0u);
  return 0;
}

/**
 * @brief Verify unterminated and adjacent quote forms return the documented errors.
 */
static int test_tokenizer_quote_rejections(const char *test_name) {
  bsc_string_view_t tokens[2];
  char unterminated[] = "set \"name";
  char bare_adjacent[] = "abc\"def";
  char quoted_adjacent[] = "\"abc\"def";
  size_t count = 55u;

  TOKEN_TEST_ASSERT_STATUS(BSC_STATUS_UNTERMINATED_QUOTE, tokenize_text(unterminated, tokens, 2u, &count));
  TOKEN_TEST_ASSERT_TRUE(count == 0u);
  count = 55u;
  TOKEN_TEST_ASSERT_STATUS(BSC_STATUS_INVALID_SYNTAX, tokenize_text(bare_adjacent, tokens, 2u, &count));
  TOKEN_TEST_ASSERT_TRUE(count == 0u);
  count = 55u;
  TOKEN_TEST_ASSERT_STATUS(BSC_STATUS_INVALID_SYNTAX, tokenize_text(quoted_adjacent, tokens, 2u, &count));
  TOKEN_TEST_ASSERT_TRUE(count == 0u);
  return 0;
}

/**
 * @brief Verify carriage return and line feed bytes are rejected by the tokenizer.
 */
static int test_tokenizer_cr_lf_rejection(const char *test_name) {
  bsc_string_view_t tokens[2];
  char cr[] = "status\r";
  char lf[] = "status\n";
  char quoted_lf[] = "\"bad\nline\"";
  size_t count = 33u;

  TOKEN_TEST_ASSERT_STATUS(BSC_STATUS_INVALID_SYNTAX, tokenize_text(cr, tokens, 2u, &count));
  TOKEN_TEST_ASSERT_TRUE(count == 0u);
  count = 33u;
  TOKEN_TEST_ASSERT_STATUS(BSC_STATUS_INVALID_SYNTAX, tokenize_text(lf, tokens, 2u, &count));
  TOKEN_TEST_ASSERT_TRUE(count == 0u);
  count = 33u;
  TOKEN_TEST_ASSERT_STATUS(BSC_STATUS_INVALID_SYNTAX, tokenize_text(quoted_lf, tokens, 2u, &count));
  TOKEN_TEST_ASSERT_TRUE(count == 0u);
  return 0;
}

/**
 * @brief Verify tokenizer line, token length, token count, and caller capacity bounds.
 *
 * Large local arrays in this test are intentional boundary fixtures sized from
 * BSC_MAX_* constants; they are not reusable core scratch buffers.
 */
static int test_tokenizer_capacity_limits(const char *test_name) {
  bsc_string_view_t tokens[BSC_MAX_TOKENS];
  char long_line[BSC_MAX_LINE_LEN + 2u];
  char long_token[BSC_MAX_TOKEN_LEN + 2u];
  char many_tokens[(BSC_MAX_TOKENS * 2u) + 2u];
  char small_capacity[] = "one two";
  size_t count = 11u;
  size_t index;
  size_t cursor = 0u;

  for (index = 0u; index < sizeof(long_line); ++index) {
    long_line[index] = 'x';
  }
  TOKEN_TEST_ASSERT_STATUS(BSC_STATUS_LINE_TOO_LONG,
                           bsc_tokenize_line(long_line, BSC_MAX_LINE_LEN + 1u, tokens, BSC_MAX_TOKENS, &count));
  TOKEN_TEST_ASSERT_TRUE(count == 0u);

  for (index = 0u; index < BSC_MAX_TOKEN_LEN + 1u; ++index) {
    long_token[index] = 'x';
  }
  long_token[BSC_MAX_TOKEN_LEN + 1u] = '\0';
  count = 11u;
  TOKEN_TEST_ASSERT_STATUS(BSC_STATUS_TOKEN_TOO_LONG, tokenize_text(long_token, tokens, BSC_MAX_TOKENS, &count));
  TOKEN_TEST_ASSERT_TRUE(count == 0u);

  for (index = 0u; index < BSC_MAX_TOKENS + 1u; ++index) {
    many_tokens[cursor] = 'x';
    cursor += 1u;
    if (index != BSC_MAX_TOKENS) {
      many_tokens[cursor] = ' ';
      cursor += 1u;
    }
  }
  many_tokens[cursor] = '\0';
  count = 11u;
  TOKEN_TEST_ASSERT_STATUS(BSC_STATUS_TOO_MANY_TOKENS, tokenize_text(many_tokens, tokens, BSC_MAX_TOKENS, &count));
  TOKEN_TEST_ASSERT_TRUE(count == 0u);

  count = 11u;
  TOKEN_TEST_ASSERT_STATUS(BSC_STATUS_TOO_MANY_TOKENS, tokenize_text(small_capacity, tokens, 1u, &count));
  TOKEN_TEST_ASSERT_TRUE(count == 0u);
  return 0;
}

/**
 * @brief Verify token views can describe a slice that is not null-terminated.
 */
static int test_tokenizer_non_null_terminated_view(const char *test_name) {
  bsc_string_view_t tokens[1];
  char line[] = {'a', 'b', 'c', 'X', '\0'};
  size_t count = 0u;

  TOKEN_TEST_ASSERT_STATUS(BSC_STATUS_OK, bsc_tokenize_line(line, 3u, tokens, 1u, &count));
  TOKEN_TEST_ASSERT_TRUE(count == 1u);
  TOKEN_TEST_ASSERT_TRUE(tokens[0].length == 3u);
  TOKEN_TEST_ASSERT_TRUE(memcmp(tokens[0].data, "abc", tokens[0].length) == 0);
  TOKEN_TEST_ASSERT_TRUE(tokens[0].data[3] == 'X');
  TOKEN_TEST_ASSERT_TRUE(token_points_into_line(tokens[0], line, sizeof(line)));
  return 0;
}

/**
 * @brief Verify non-ASCII bytes are preserved literally in token views.
 */
static int test_tokenizer_non_ascii_bytes_are_literal(const char *test_name) {
  bsc_string_view_t tokens[1];
  char line[] = {(char)0xc3, (char)0xa9, 'x', '\0'};
  size_t count = 0u;

  TOKEN_TEST_ASSERT_STATUS(BSC_STATUS_OK, bsc_tokenize_line(line, 3u, tokens, 1u, &count));
  TOKEN_TEST_ASSERT_TRUE(count == 1u);
  TOKEN_TEST_ASSERT_TRUE(tokens[0].length == 3u);
  TOKEN_TEST_ASSERT_TRUE((unsigned char)tokens[0].data[0] == 0xc3u);
  TOKEN_TEST_ASSERT_TRUE((unsigned char)tokens[0].data[1] == 0xa9u);
  TOKEN_TEST_ASSERT_TRUE(tokens[0].data[2] == 'x');
  TOKEN_TEST_ASSERT_TRUE(token_points_into_line(tokens[0], line, sizeof(line)));
  return 0;
}

/**
 * @brief Run all tokenizer host tests and return the accumulated failure count.
 */
int bsc_run_tokenizer_tests(void) {
  int failures = 0;

  RUN_TOKEN_TEST(test_tokenizer_invalid_arguments);
  RUN_TOKEN_TEST(test_tokenizer_no_input);
  RUN_TOKEN_TEST(test_tokenizer_basic_tokens);
  RUN_TOKEN_TEST(test_tokenizer_literal_punctuation_and_non_shell_bytes);
  RUN_TOKEN_TEST(test_tokenizer_quoted_tokens);
  RUN_TOKEN_TEST(test_tokenizer_quoted_escapes_compact_in_place);
  RUN_TOKEN_TEST(test_tokenizer_escape_rejections);
  RUN_TOKEN_TEST(test_tokenizer_quote_rejections);
  RUN_TOKEN_TEST(test_tokenizer_cr_lf_rejection);
  RUN_TOKEN_TEST(test_tokenizer_capacity_limits);
  RUN_TOKEN_TEST(test_tokenizer_non_null_terminated_view);
  RUN_TOKEN_TEST(test_tokenizer_non_ascii_bytes_are_literal);

  return failures;
}
