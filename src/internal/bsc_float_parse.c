#include "internal/bsc_float_parse.h"

#if BSC_ENABLE_FLOAT

#include <stdint.h>

/**
 * @brief Return whether a byte is an ASCII decimal digit.
 */
static int bsc_float_is_digit(char value) {
  return value >= '0' && value <= '9';
}

/**
 * @brief Store an optional parse-error offset.
 */
static bsc_float_parse_error_t bsc_float_fail(bsc_float_parse_error_t error,
                                              size_t offset,
                                              size_t *error_offset) {
  if (error_offset != NULL) {
    *error_offset = offset;
  }
  return error;
}

/**
 * @brief Return 10^-digits for the approved zero-through-six digit range.
 *
 * The table is immutable static const storage, has program lifetime, and is not
 * scratch. It avoids general math-library exponent helpers for code-size and
 * portability reasons.
 */
static float bsc_float_fraction_scale(size_t digits) {
  static const float k_scale[] = {1.0f, 0.1f, 0.01f, 0.001f, 0.0001f, 0.00001f, 0.000001f};
  return k_scale[digits];
}

bsc_float_parse_error_t bsc_float_parse_compact(bsc_string_view_t token,
                                                float *out_value,
                                                size_t *error_offset) {
  size_t index = 0u;
  int negative = 0;
  int saw_dot = 0;
  size_t whole_digits = 0u;
  size_t frac_digits = 0u;
  uint32_t whole = 0u;
  uint32_t frac = 0u;
  float value;

  if (error_offset != NULL) {
    *error_offset = 0u;
  }
  if (out_value == NULL || (token.data == NULL && token.length > 0u) || token.length == 0u) {
    return bsc_float_fail(BSC_FLOAT_PARSE_INVALID_SYNTAX, 0u, error_offset);
  }

  if (token.data[index] == '-') {
    negative = 1;
    index += 1u;
    if (index == token.length) {
      return bsc_float_fail(BSC_FLOAT_PARSE_INVALID_SYNTAX, index - 1u, error_offset);
    }
  } else if (token.data[index] == '+') {
    return bsc_float_fail(BSC_FLOAT_PARSE_INVALID_SYNTAX, index, error_offset);
  }

  for (; index < token.length; ++index) {
    char current = token.data[index];
    uint32_t digit;

    if (current == '\0') {
      return bsc_float_fail(BSC_FLOAT_PARSE_EMBEDDED_NUL, index, error_offset);
    }
    if (current == '.') {
      if (saw_dot || whole_digits == 0u) {
        return bsc_float_fail(BSC_FLOAT_PARSE_INVALID_SYNTAX, index, error_offset);
      }
      saw_dot = 1;
      continue;
    }
    if (!bsc_float_is_digit(current)) {
      return bsc_float_fail(BSC_FLOAT_PARSE_INVALID_SYNTAX, index, error_offset);
    }

    digit = (uint32_t)(current - '0');
    if (saw_dot) {
      if (frac_digits >= (size_t)BSC_MAX_FLOAT_FRACTION_DIGITS) {
        return bsc_float_fail(BSC_FLOAT_PARSE_TOO_MANY_FRACTION_DIGITS, index, error_offset);
      }
      frac = (uint32_t)(frac * 10u + digit);
      frac_digits += 1u;
    } else {
      if (whole > (uint32_t)(((uint32_t)BSC_COMPACT_FLOAT_MAX_MAGNITUDE - digit) / 10u)) {
        return bsc_float_fail(negative ? BSC_FLOAT_PARSE_BELOW_SUPPORTED_RANGE
                                       : BSC_FLOAT_PARSE_ABOVE_SUPPORTED_RANGE,
                              index, error_offset);
      }
      whole = (uint32_t)(whole * 10u + digit);
      whole_digits += 1u;
    }
  }

  if (whole_digits == 0u || (saw_dot && frac_digits == 0u)) {
    return bsc_float_fail(BSC_FLOAT_PARSE_INVALID_SYNTAX, token.length == 0u ? 0u : token.length - 1u,
                          error_offset);
  }
  if (whole == (uint32_t)BSC_COMPACT_FLOAT_MAX_MAGNITUDE && frac != 0u) {
    size_t fail_offset = token.length == 0u ? 0u : token.length - 1u;
    return bsc_float_fail(negative ? BSC_FLOAT_PARSE_BELOW_SUPPORTED_RANGE
                                   : BSC_FLOAT_PARSE_ABOVE_SUPPORTED_RANGE,
                          fail_offset, error_offset);
  }

  value = (float)whole;
  if (frac_digits > 0u) {
    value += ((float)frac) * bsc_float_fraction_scale(frac_digits);
  }
  if (value != 0.0f && negative) {
    value = -value;
  }
  *out_value = value;
  return BSC_FLOAT_PARSE_OK;
}

#endif /* BSC_ENABLE_FLOAT */
