#ifndef BSC_INTERNAL_BSC_FLOAT_PARSE_H
#define BSC_INTERNAL_BSC_FLOAT_PARSE_H

#include "bsc_config.h"
#include "bsc_status.h"
#include "bsc_string_view.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file bsc_float_parse.h
 * @brief Internal compact decimal float parser for typed argument parsing.
 *
 * This internal module is compiled only when BSC_ENABLE_FLOAT is enabled. It
 * parses a deliberately small operator-facing decimal grammar directly from a
 * borrowed string view without locale, libc conversion, heap allocation, hidden
 * scratch, token-sized buffers, or 64-bit integer arithmetic.
 */

/** Compact float parser failure category used by bsc_args diagnostics. */
typedef enum bsc_float_parse_error {
  /** No float conversion error occurred. */
  BSC_FLOAT_PARSE_OK = 0,
  /** Token bytes did not match the compact decimal grammar. */
  BSC_FLOAT_PARSE_INVALID_SYNTAX,
  /** The token contains an embedded NUL byte. */
  BSC_FLOAT_PARSE_EMBEDDED_NUL,
  /** More than BSC_MAX_FLOAT_FRACTION_DIGITS appeared after the decimal point. */
  BSC_FLOAT_PARSE_TOO_MANY_FRACTION_DIGITS,
  /** Positive compact decimal text exceeded BSC_COMPACT_FLOAT_MAX_MAGNITUDE. */
  BSC_FLOAT_PARSE_ABOVE_SUPPORTED_RANGE,
  /** Negative compact decimal text was below -BSC_COMPACT_FLOAT_MAX_MAGNITUDE. */
  BSC_FLOAT_PARSE_BELOW_SUPPORTED_RANGE
} bsc_float_parse_error_t;

#if BSC_ENABLE_FLOAT
/**
 * @brief Parse a compact decimal token into a finite float.
 *
 * @param token Borrowed token view. It is read by explicit length and may be
 *   non-null-terminated. The function does not mutate or retain the view.
 * @param out_value Caller-owned output float. Must not be NULL.
 * @param error_offset Optional caller-owned byte offset of a syntax error.
 * @return Compact parser status describing conversion success or failure.
 *
 * Accepted grammar is an optional '-' followed by one or more ASCII digits and
 * an optional fractional part containing '.' plus one to
 * BSC_MAX_FLOAT_FRACTION_DIGITS ASCII digits within the inclusive domain
 * -1000000000.0 through 1000000000.0. No exponent, leading plus,
 * leading decimal point, trailing decimal point, NaN, infinity, hexadecimal
 * syntax, whitespace, or embedded NUL is accepted. Textual negative zero is
 * normalized to positive 0.0f.
 */
bsc_float_parse_error_t bsc_float_parse_compact(bsc_string_view_t token,
                                                float *out_value,
                                                size_t *error_offset);
#endif

#ifdef __cplusplus
}
#endif

#endif /* BSC_INTERNAL_BSC_FLOAT_PARSE_H */
