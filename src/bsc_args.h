#ifndef BSC_ARGS_H
#define BSC_ARGS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "bsc_config.h"
#include "bsc_output.h"
#include "bsc_status.h"
#include "bsc_string_view.h"
#include "bsc_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file bsc_args.h
 * @brief Typed positional argument parsing for matched command descriptors.
 *
 * The parser consumes a caller-selected executable command descriptor and the
 * remaining positional token slice produced by the tokenizer/matcher pipeline.
 * It performs no matching, dispatch, access checks, console orchestration, help
 * rendering, or automatic output. Parsed string and secret values are borrowed
 * views into the active mutable line buffer and must not escape that lifetime.
 * Normal use expects descriptors that passed bsc_registry_validate(); defensive
 * parser checks cover ordinary malformed metadata but cannot make arbitrary
 * invalid pointers or unterminated descriptor strings safe. Compact float input
 * deliberately excludes scientific notation and is limited to the inclusive
 * domain -1000000000.0 through 1000000000.0.
 */

/** Parsed value for one positional command argument. */
typedef struct bsc_arg_value {
  /** Runtime value tag; BSC_ARG_STRING and BSC_ARG_SECRET share text storage. */
  bsc_arg_type_t type;
  /** Type-specific parsed scalar or borrowed text view. */
  union {
    /** Parsed BSC_ARG_INT value. */
    int32_t int_value;
    /** Parsed BSC_ARG_UINT value. */
    uint32_t uint_value;
    /** Parsed BSC_ARG_FLOAT value when BSC_ENABLE_FLOAT is enabled. */
    float float_value;
    /** Parsed BSC_ARG_BOOL value. */
    bool bool_value;
    /** Stable semantic value from the matched enum choice descriptor. */
    int32_t enum_value;
    /** Borrowed string or secret token view. */
    bsc_string_view_t text_value;
  } data;
} bsc_arg_value_t;

/** Caller-owned fixed parsed-argument collection. */
typedef struct bsc_parsed_args {
  /** Parsed values for the active command, bounded by BSC_MAX_ARGS. */
  bsc_arg_value_t values[BSC_MAX_ARGS];
  /** Number of valid values; zero after clear and after every parse failure. */
  size_t count;
} bsc_parsed_args_t;

/** Operator-relevant parse diagnostic reason. */
typedef enum bsc_arg_parse_error_reason {
  /** No parse error. */
  BSC_ARG_PARSE_ERROR_NONE = 0,
  /** A required positional argument was absent. */
  BSC_ARG_PARSE_ERROR_MISSING_ARGUMENT,
  /** One or more unexpected positional tokens remained. */
  BSC_ARG_PARSE_ERROR_EXTRA_ARGUMENT,
  /** Token syntax did not match the descriptor type. */
  BSC_ARG_PARSE_ERROR_INVALID_VALUE,
  /** Numeric value was below descriptor or representable minimum. */
  BSC_ARG_PARSE_ERROR_BELOW_MINIMUM,
  /** Numeric value was above descriptor or representable maximum. */
  BSC_ARG_PARSE_ERROR_ABOVE_MAXIMUM,
  /** Text/secret byte length was below descriptor minimum. */
  BSC_ARG_PARSE_ERROR_TEXT_TOO_SHORT,
  /** Text/secret byte length was above descriptor maximum. */
  BSC_ARG_PARSE_ERROR_TEXT_TOO_LONG,
  /** Enum token did not match any allowed choice. */
  BSC_ARG_PARSE_ERROR_INVALID_ENUM_CHOICE,
  /** Explicit-length token contained an embedded NUL byte. */
  BSC_ARG_PARSE_ERROR_EMBEDDED_NUL,
  /** Compact float token exceeded the fractional digit limit. */
  BSC_ARG_PARSE_ERROR_TOO_MANY_FLOAT_FRACTION_DIGITS,
  /** Float argument support is disabled in this build. */
  BSC_ARG_PARSE_ERROR_FLOAT_DISABLED,
  /** Command argument descriptor metadata is malformed. */
  BSC_ARG_PARSE_ERROR_INVALID_DESCRIPTOR,
  /** Parser API was called with invalid pointers or inconsistent counts. */
  BSC_ARG_PARSE_ERROR_INVALID_API
} bsc_arg_parse_error_reason_t;

/** Structured parse diagnostic populated for every rejection. */
typedef struct bsc_arg_parse_error {
  /** Operator-relevant reason for the parse failure. */
  bsc_arg_parse_error_reason_t reason;
  /** Positional argument index when meaningful; otherwise zero. */
  size_t arg_index;
  /** Byte offset inside the failing token when meaningful; otherwise zero. */
  size_t token_offset;
} bsc_arg_parse_error_t;

/**
 * @brief Clear a caller-owned parsed result and remove all borrowed views.
 *
 * @param args Optional caller-owned parsed-argument object. NULL is accepted.
 * @post Non-NULL results have count zero and every fixed value slot reset to
 *   BSC_ARG_NONE with an empty borrowed text view, so no previous secret/string
 *   pointer remains visible.
 * @note The function is synchronous, uses no heap or static scratch, retains no
 *   pointers, performs no I/O, and is reentrant for independent caller storage.
 */
void bsc_parsed_args_clear(bsc_parsed_args_t *args);

/**
 * @brief Clear a caller-owned structured parse diagnostic.
 *
 * @param error Optional caller-owned diagnostic object. NULL is accepted.
 * @post Non-NULL diagnostics report BSC_ARG_PARSE_ERROR_NONE with zero indexes.
 * @note The function performs no allocation, I/O, blocking work, or pointer
 *   retention and is reentrant for independent caller storage.
 */
void bsc_arg_parse_error_clear(bsc_arg_parse_error_t *error);

/**
 * @brief Parse positional argument tokens for a matched executable command.
 *
 * @param command Required borrowed executable descriptor. It and nested path,
 *   argument, enum, and name strings must remain valid for the call. Descriptor
 *   metadata is read but never mutated or retained.
 * @param arg_tokens Borrowed token slice for positional arguments. May be NULL
 *   only when arg_token_count is zero. Token views are explicit-length, are not
 *   assumed terminated, and may point into mutable command-line storage owned by
 *   the caller or future workspace.
 * @param arg_token_count Number of entries in arg_tokens.
 * @param out_args Required caller-owned fixed-capacity result with BSC_MAX_ARGS
 *   slots. It is cleared at entry whenever safely possible and contains no
 *   partial result after failure.
 * @param error Required caller-owned diagnostic. It is cleared at entry and
 *   receives a non-NONE reason for every rejection. Diagnostics store only
 *   indexes/offsets and never copy raw token text.
 * @retval BSC_STATUS_OK All arguments parsed and out_args->count is valid.
 * @retval BSC_STATUS_MISSING_ARGUMENT Required input was absent.
 * @retval BSC_STATUS_EXTRA_ARGUMENT Unexpected input remained.
 * @retval BSC_STATUS_INVALID_ARGUMENT_TYPE A token had invalid syntax/type.
 * @retval BSC_STATUS_ARGUMENT_OUT_OF_RANGE Numeric conversion or range failed.
 * @retval BSC_STATUS_INVALID_ENUM_VALUE Enum choice did not match.
 * @retval BSC_STATUS_ARGUMENT_TOO_SHORT Text was below descriptor minimum.
 * @retval BSC_STATUS_ARGUMENT_TOO_LONG Text was above descriptor maximum.
 * @retval BSC_STATUS_INVALID_DESCRIPTOR Descriptor metadata was malformed or a
 *   float descriptor was used when BSC_ENABLE_FLOAT is disabled.
 * @retval BSC_STATUS_INTERNAL_ERROR Required parser API pointers were invalid.
 * @note The parser is synchronous, performs no output, owns no storage, uses no
 *   heap or hidden mutable static scratch, calls no handler/access callback, and
 *   retains no pointers after return. Thread/task/ISR suitability depends on the
 *   caller owning and serializing descriptor, token, result, and diagnostic
 *   storage for the duration of the bounded call.
 */
bsc_status_t bsc_parse_command_args(const bsc_command_t *command,
                                    const bsc_string_view_t *arg_tokens,
                                    size_t arg_token_count,
                                    bsc_parsed_args_t *out_args,
                                    bsc_arg_parse_error_t *error);

/**
 * @brief Write a bounded operator-facing diagnostic for a parse failure.
 *
 * @param command Optional borrowed command descriptor used only to read argument
 *   and enum names for validated diagnostic indexes. May be NULL when only
 *   generic text can be rendered. When non-NULL and descriptor-derived names are
 *   needed, the command, argument array, enum arrays, and strings read by the
 *   writer must remain valid for the call; normal use passes the same
 *   registry-validated descriptor that produced the parse diagnostic. The writer
 *   can fall back for NULL commands, unavailable argument arrays, or diagnostic
 *   indexes outside `arg_count`, but it cannot make arbitrary invalid pointers,
 *   unterminated names, fabricated enum arrays, or invalid enum counts safe.
 * @param error Required borrowed diagnostic to render. NULL is treated as invalid
 *   parser/API use. The diagnostic is not retained.
 * @param output Required caller-owned output callback target. Its callback may
 *   block according to caller policy.
 * @retval BSC_STATUS_OK The message, if any, was accepted. NONE diagnostics
 *   intentionally render no bytes.
 * @retval BSC_STATUS_OUTPUT_TRUNCATED The output callback accepted a short write.
 * @retval BSC_STATUS_INTERNAL_ERROR The output target was invalid.
 * @note The writer emits no trailing newline, never writes raw input token text,
 *   never echoes string or secret values, allocates no memory, owns no output
 *   storage, and retains no pointers after return.
 */
bsc_status_t bsc_arg_parse_error_write(const bsc_command_t *command,
                                       const bsc_arg_parse_error_t *error,
                                       bsc_output_t *output);

#ifdef __cplusplus
}
#endif

#endif /* BSC_ARGS_H */
