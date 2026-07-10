#include "bsc_tokenizer.h"

#include "bsc_config.h"

typedef enum bsc_tokenizer_state {
  BSC_TOKENIZER_IDLE = 0,
  BSC_TOKENIZER_BARE_TOKEN,
  BSC_TOKENIZER_QUOTED_TOKEN,
  BSC_TOKENIZER_QUOTED_ESCAPE,
  BSC_TOKENIZER_AFTER_QUOTED_TOKEN
} bsc_tokenizer_state_t;

/**
 * @brief Return whether a byte separates tokens outside quotes.
 */
static int bsc_tokenizer_is_separator(char value) {
  return value == ' ' || value == '\t';
}

/**
 * @brief Return whether a byte is a disallowed embedded line ending.
 */
static int bsc_tokenizer_is_line_ending(char value) {
  return value == '\r' || value == '\n';
}

/**
 * @brief Clamp caller token capacity to the core BSC_MAX_TOKENS limit.
 */
static size_t bsc_tokenizer_limit(size_t token_capacity) {
  return token_capacity < (size_t)BSC_MAX_TOKENS ? token_capacity : (size_t)BSC_MAX_TOKENS;
}

/**
 * @brief Append one borrowed token view when capacity and length allow it.
 *
 * The token array is caller-owned and bounded by the clamped token_limit. Each
 * token length must fit BSC_MAX_TOKEN_LEN. Stored views borrow slices of the
 * caller-owned mutable line buffer and must not outlive that buffer.
 */
static bsc_status_t bsc_tokenizer_add_token(bsc_string_view_t *tokens,
                                            size_t token_limit,
                                            size_t *count,
                                            const char *data,
                                            size_t length) {
  if (length > (size_t)BSC_MAX_TOKEN_LEN) {
    return BSC_STATUS_TOKEN_TOO_LONG;
  }
  if (*count >= token_limit) {
    return BSC_STATUS_TOO_MANY_TOKENS;
  }
  tokens[*count] = bsc_string_view_from_parts(data, length);
  *count += 1u;
  return BSC_STATUS_OK;
}

/**
 * @brief Tokenize a caller-owned mutable input line into borrowed token views.
 *
 * The tokenizer allocates no storage and mutates the line in place only to
 * compact quoted text and supported escapes. Token views borrow from that line
 * buffer, are bounded by BSC_MAX_TOKENS and BSC_MAX_TOKEN_LEN, and remain valid
 * only while the caller keeps the mutable line buffer alive and unchanged.
 * This step does not parse command paths, convert typed arguments, dispatch
 * handlers, or print output.
 */
bsc_status_t bsc_tokenize_line(char *line,
                               size_t line_length,
                               bsc_string_view_t *tokens,
                               size_t token_capacity,
                               size_t *token_count) {
  bsc_tokenizer_state_t state = BSC_TOKENIZER_IDLE;
  size_t read_index = 0u;
  size_t write_index = 0u;
  size_t token_start = 0u;
  size_t token_length = 0u;
  size_t count = 0u;
  size_t token_limit;
  bsc_status_t status;

  if (token_count != NULL) {
    *token_count = 0u;
  }

  if (line == NULL || token_count == NULL || (tokens == NULL && token_capacity > 0u)) {
    return BSC_STATUS_INTERNAL_ERROR;
  }
  if (line_length > (size_t)BSC_MAX_LINE_LEN) {
    return BSC_STATUS_LINE_TOO_LONG;
  }

  token_limit = bsc_tokenizer_limit(token_capacity);

  while (read_index < line_length) {
    char current = line[read_index];

    switch (state) {
    case BSC_TOKENIZER_IDLE:
      if (bsc_tokenizer_is_separator(current)) {
        read_index += 1u;
      } else if (bsc_tokenizer_is_line_ending(current)) {
        return BSC_STATUS_INVALID_SYNTAX;
      } else if (current == '"') {
        token_start = read_index + 1u;
        write_index = token_start;
        token_length = 0u;
        state = BSC_TOKENIZER_QUOTED_TOKEN;
        read_index += 1u;
      } else {
        token_start = read_index;
        token_length = 0u;
        state = BSC_TOKENIZER_BARE_TOKEN;
      }
      break;

    case BSC_TOKENIZER_BARE_TOKEN:
      if (bsc_tokenizer_is_separator(current)) {
        status = bsc_tokenizer_add_token(tokens, token_limit, &count, &line[token_start], token_length);
        if (status != BSC_STATUS_OK) {
          return status;
        }
        state = BSC_TOKENIZER_IDLE;
        read_index += 1u;
      } else if (bsc_tokenizer_is_line_ending(current) || current == '"') {
        return BSC_STATUS_INVALID_SYNTAX;
      } else {
        token_length += 1u;
        if (token_length > (size_t)BSC_MAX_TOKEN_LEN) {
          return BSC_STATUS_TOKEN_TOO_LONG;
        }
        read_index += 1u;
      }
      break;

    case BSC_TOKENIZER_QUOTED_TOKEN:
      if (current == '"') {
        status = bsc_tokenizer_add_token(tokens, token_limit, &count, &line[token_start], token_length);
        if (status != BSC_STATUS_OK) {
          return status;
        }
        state = BSC_TOKENIZER_AFTER_QUOTED_TOKEN;
        read_index += 1u;
      } else if (current == '\\') {
        state = BSC_TOKENIZER_QUOTED_ESCAPE;
        read_index += 1u;
      } else if (bsc_tokenizer_is_line_ending(current)) {
        return BSC_STATUS_INVALID_SYNTAX;
      } else {
        /* Quoted content is compacted into the original line buffer as it is read. */
        line[write_index] = current;
        write_index += 1u;
        token_length += 1u;
        if (token_length > (size_t)BSC_MAX_TOKEN_LEN) {
          return BSC_STATUS_TOKEN_TOO_LONG;
        }
        read_index += 1u;
      }
      break;

    case BSC_TOKENIZER_QUOTED_ESCAPE:
      if (current == '"' || current == '\\') {
        line[write_index] = current;
        write_index += 1u;
        token_length += 1u;
        if (token_length > (size_t)BSC_MAX_TOKEN_LEN) {
          return BSC_STATUS_TOKEN_TOO_LONG;
        }
        state = BSC_TOKENIZER_QUOTED_TOKEN;
        read_index += 1u;
      } else {
        return BSC_STATUS_INVALID_SYNTAX;
      }
      break;

    case BSC_TOKENIZER_AFTER_QUOTED_TOKEN:
      if (bsc_tokenizer_is_separator(current)) {
        state = BSC_TOKENIZER_IDLE;
        read_index += 1u;
      } else if (bsc_tokenizer_is_line_ending(current)) {
        return BSC_STATUS_INVALID_SYNTAX;
      } else {
        return BSC_STATUS_INVALID_SYNTAX;
      }
      break;
    }
  }

  if (state == BSC_TOKENIZER_BARE_TOKEN) {
    status = bsc_tokenizer_add_token(tokens, token_limit, &count, &line[token_start], token_length);
    if (status != BSC_STATUS_OK) {
      return status;
    }
  } else if (state == BSC_TOKENIZER_QUOTED_TOKEN || state == BSC_TOKENIZER_QUOTED_ESCAPE) {
    return state == BSC_TOKENIZER_QUOTED_ESCAPE ? BSC_STATUS_INVALID_SYNTAX : BSC_STATUS_UNTERMINATED_QUOTE;
  }

  if (count == 0u) {
    return BSC_STATUS_NO_INPUT;
  }

  *token_count = count;
  return BSC_STATUS_OK;
}
