#ifndef BSC_STATUS_H
#define BSC_STATUS_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file bsc_status.h
 * @brief Status codes shared by the bounded serial command core.
 *
 * Status values are stable public API. Current tokenizer, registry-validation,
 * matcher, typed parser, selected-command dispatch/access enforcement, output,
 * console-context, generated-help, built-in-aware console integration, and foundational
 * helper functions return the applicable values from this enum.
 */

/**
 * @brief Result codes returned by Serial Command Extreme core functions.
 *
 * The core does not allocate or own caller buffers when returning these values.
 * Functions that borrow caller memory document the lifetime requirements in
 * their own headers. The enum is safe to inspect from host tests, polling code,
 * or task context; ISR suitability depends on the function returning it.
 */
typedef enum bsc_status {
  /** Operation completed successfully. */
  BSC_STATUS_OK = 0,
  /** No input was available or the provided input was empty. */
  BSC_STATUS_NO_INPUT,
  /** Input line exceeded the configured or caller-provided bound. */
  BSC_STATUS_LINE_TOO_LONG,
  /** A token exceeded the configured or caller-provided bound. */
  BSC_STATUS_TOKEN_TOO_LONG,
  /** Input contained more tokens than the configured or caller-provided bound. */
  BSC_STATUS_TOO_MANY_TOKENS,
  /** Input ended before a quoted string was closed. */
  BSC_STATUS_UNTERMINATED_QUOTE,
  /** Input contained malformed tokenizer or parser syntax. */
  BSC_STATUS_INVALID_SYNTAX,
  /** No command matched the provided path. */
  BSC_STATUS_UNKNOWN_COMMAND,
  /** More than one command matched and the input was ambiguous. */
  BSC_STATUS_AMBIGUOUS_COMMAND,
  /** A group command was provided without a required subcommand. */
  BSC_STATUS_GROUP_REQUIRES_SUBCOMMAND,
  /** A required positional argument was absent. */
  BSC_STATUS_MISSING_ARGUMENT,
  /** Input provided more arguments than the command accepts. */
  BSC_STATUS_EXTRA_ARGUMENT,
  /** An argument could not be parsed as the required type. */
  BSC_STATUS_INVALID_ARGUMENT_TYPE,
  /** A parsed argument was outside its accepted numeric range. */
  BSC_STATUS_ARGUMENT_OUT_OF_RANGE,
  /** A string argument exceeded its accepted length. */
  BSC_STATUS_ARGUMENT_TOO_LONG,
  /** An enum/string-choice argument was not in its allowed set. */
  BSC_STATUS_INVALID_ENUM_VALUE,
  /** A caller-provided static command descriptor table is invalid. */
  BSC_STATUS_INVALID_DESCRIPTOR,
  /** The caller or access policy denied command execution. */
  BSC_STATUS_ACCESS_DENIED,
  /** An output sink accepted only part of the requested bytes. */
  BSC_STATUS_OUTPUT_TRUNCATED,
  /** Application callback reported a failure. */
  BSC_STATUS_APP_ERROR,
  /** The core detected an invalid internal state or invalid API use. */
  BSC_STATUS_INTERNAL_ERROR,
  /** A string argument was shorter than its accepted length. */
  BSC_STATUS_ARGUMENT_TOO_SHORT
} bsc_status_t;

/**
 * @brief Return a stable symbolic name for a status code.
 *
 * @param status Status value to describe. Values outside #bsc_status_t return a
 *   stable fallback string.
 * @return Null-terminated static string. The caller must not modify or release
 *   the returned pointer. The returned storage is valid for the lifetime of the
 *   program and the function performs no blocking I/O.
 * @note This helper does not expose secrets and has no platform dependencies.
 */
const char *bsc_status_name(bsc_status_t status);

#ifdef __cplusplus
}
#endif

#endif /* BSC_STATUS_H */
