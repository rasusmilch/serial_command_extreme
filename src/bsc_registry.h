#ifndef BSC_REGISTRY_H
#define BSC_REGISTRY_H

#include <stddef.h>

#include "bsc_status.h"
#include "bsc_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file bsc_registry.h
 * @brief Static command descriptor table validation for Serial Command Extreme.
 *
 * The registry validator checks caller-owned #bsc_command_t metadata before
 * later matcher, argument parser, dispatch, and help modules consume it. It
 * validates descriptor shape only: it performs no token matching, argument
 * parsing, command dispatch, help rendering, runtime registration, allocation,
 * handler invocation, or access-callback invocation.
 *
 * Descriptor tables, path arrays, path strings, argument arrays, enum choice
 * arrays, help strings, callbacks, command contexts, and optional diagnostic
 * output remain caller-owned. Validation borrows them only for the duration of
 * the call, does not mutate descriptor metadata, and retains no pointers after
 * returning. The implementation is synchronous, uses no platform I/O, and owns
 * no storage. It is reentrant for independent immutable descriptor tables. ISR
 * suitability depends on the caller's tolerance for bounded
 * O(command_count^2 + enum_choice_count^2) validation time.
 */

/**
 * @brief Specific descriptor validation failure reason.
 *
 * Reasons are reported through #bsc_registry_validation_error_t when a caller
 * provides diagnostic storage to #bsc_registry_validate. Invalid descriptors
 * return #BSC_STATUS_INVALID_DESCRIPTOR; the reason identifies the first
 * failing metadata rule found by the validator.
 */
typedef enum bsc_registry_error_reason {
  /** No registry validation error. */
  BSC_REGISTRY_ERROR_NONE = 0,
  /** The command descriptor table pointer was NULL. */
  BSC_REGISTRY_ERROR_NULL_COMMANDS,
  /** The command descriptor table contained zero entries. */
  BSC_REGISTRY_ERROR_ZERO_COMMANDS,
  /** The command count exceeded #BSC_MAX_COMMANDS. */
  BSC_REGISTRY_ERROR_TOO_MANY_COMMANDS,
  /** A command path pointer was NULL. */
  BSC_REGISTRY_ERROR_NULL_PATH,
  /** A command path length was zero. */
  BSC_REGISTRY_ERROR_EMPTY_PATH,
  /** A command path length exceeded #BSC_MAX_PATH_TOKENS. */
  BSC_REGISTRY_ERROR_PATH_TOO_DEEP,
  /** An active command path token pointer was NULL. */
  BSC_REGISTRY_ERROR_NULL_PATH_TOKEN,
  /** An active command path token was an empty string. */
  BSC_REGISTRY_ERROR_EMPTY_PATH_TOKEN,
  /** An active command path token exceeded #BSC_MAX_TOKEN_LEN bytes. */
  BSC_REGISTRY_ERROR_PATH_TOKEN_TOO_LONG,
  /** A command node type was not a supported #bsc_node_type_t value. */
  BSC_REGISTRY_ERROR_INVALID_NODE_TYPE,
  /** An executable command descriptor had no handler. */
  BSC_REGISTRY_ERROR_COMMAND_MISSING_HANDLER,
  /** A group descriptor had a handler, which is not supported in the MVP. */
  BSC_REGISTRY_ERROR_GROUP_HAS_HANDLER,
  /** A command argument count exceeded #BSC_MAX_ARGS. */
  BSC_REGISTRY_ERROR_ARG_COUNT_TOO_HIGH,
  /** A command had active arguments but a NULL args pointer. */
  BSC_REGISTRY_ERROR_NULL_ARGS_WITH_COUNT,
  /** An active argument name was NULL or empty. */
  BSC_REGISTRY_ERROR_INVALID_ARG_NAME,
  /** An active argument name exceeded #BSC_MAX_TOKEN_LEN bytes. */
  BSC_REGISTRY_ERROR_ARG_NAME_TOO_LONG,
  /** An active argument type was not a supported #bsc_arg_type_t value. */
  BSC_REGISTRY_ERROR_INVALID_ARG_TYPE,
  /** #BSC_ARG_NONE appeared in an active positional argument descriptor. */
  BSC_REGISTRY_ERROR_ARG_NONE_NOT_ALLOWED,
  /** Numeric range metadata was invalid for an argument descriptor. */
  BSC_REGISTRY_ERROR_INVALID_ARG_RANGE,
  /** An enum argument had a NULL choice table. */
  BSC_REGISTRY_ERROR_ENUM_MISSING_CHOICES,
  /** An enum argument had zero choices. */
  BSC_REGISTRY_ERROR_ENUM_CHOICE_COUNT_ZERO,
  /** An enum argument choice count exceeded #BSC_MAX_ENUM_CHOICES. */
  BSC_REGISTRY_ERROR_ENUM_CHOICE_COUNT_TOO_HIGH,
  /** An active enum choice name pointer was NULL. */
  BSC_REGISTRY_ERROR_NULL_ENUM_CHOICE_NAME,
  /** An active enum choice name was empty. */
  BSC_REGISTRY_ERROR_EMPTY_ENUM_CHOICE_NAME,
  /** An active enum choice name exceeded #BSC_MAX_TOKEN_LEN bytes. */
  BSC_REGISTRY_ERROR_ENUM_CHOICE_NAME_TOO_LONG,
  /** An enum argument had duplicate choice names under ASCII case folding. */
  BSC_REGISTRY_ERROR_DUPLICATE_ENUM_CHOICE_NAME,
  /** An enum argument had duplicate semantic choice values. */
  BSC_REGISTRY_ERROR_DUPLICATE_ENUM_CHOICE_VALUE,
  /** String or secret length metadata was invalid. */
  BSC_REGISTRY_ERROR_INVALID_STRING_LENGTH_RANGE,
  /** A command access level was not a supported #bsc_access_level_t value. */
  BSC_REGISTRY_ERROR_INVALID_COMMAND_ACCESS,
  /** A command flags bitmask used unknown bits. */
  BSC_REGISTRY_ERROR_INVALID_COMMAND_FLAGS,
  /** Two command descriptors had the same path under ASCII case folding. */
  BSC_REGISTRY_ERROR_DUPLICATE_COMMAND_PATH
} bsc_registry_error_reason_t;

/**
 * @brief Optional structured descriptor validation diagnostic.
 *
 * All fields are caller-owned scalar output values. On success the reason is
 * #BSC_REGISTRY_ERROR_NONE and every index is zero. On failure, non-applicable
 * indexes remain zero. `command_index` identifies the failing command for
 * command-level, path, argument, enum, and duplicate-path failures.
 * `path_token_index` is meaningful for path-token failures. `arg_index` is
 * meaningful for argument and enum failures. `enum_choice_index` is meaningful
 * for enum-choice failures. For duplicate command paths, `command_index` is the
 * later duplicate descriptor and `duplicate_command_index` is the earlier
 * matching descriptor.
 */
typedef struct bsc_registry_validation_error {
  /** Specific validation failure reason, or #BSC_REGISTRY_ERROR_NONE. */
  bsc_registry_error_reason_t reason;
  /** Failing command descriptor index when applicable. */
  size_t command_index;
  /** Failing path token index when applicable. */
  size_t path_token_index;
  /** Failing argument descriptor index when applicable. */
  size_t arg_index;
  /** Failing enum choice descriptor index when applicable. */
  size_t enum_choice_index;
  /** Earlier duplicate command index for duplicate-path failures. */
  size_t duplicate_command_index;
} bsc_registry_validation_error_t;

/**
 * @brief Clear a registry validation diagnostic structure.
 *
 * @param error Optional caller-owned diagnostic output. NULL is accepted and
 *   leaves no state to clear. Non-NULL storage is overwritten with
 *   #BSC_REGISTRY_ERROR_NONE and zero indexes.
 * @note The function performs no allocation, no I/O, and retains no pointer to
 *   @p error after returning.
 */
void bsc_registry_validation_error_clear(bsc_registry_validation_error_t *error);

/**
 * @brief Validate a caller-owned static command descriptor table.
 *
 * @param commands Caller-owned descriptor array with @p command_count entries.
 *   The pointer must not be NULL, even when @p command_count is zero. Nested
 *   path, argument, enum choice, help, callback, and context pointers remain
 *   caller-owned and are borrowed only during validation.
 * @param command_count Number of command descriptors in @p commands. Values of
 *   zero and values greater than #BSC_MAX_COMMANDS are invalid.
 * @param error Optional caller-owned diagnostic output. When non-NULL, it is
 *   cleared at entry and filled with the first validation failure detail.
 *
 * @retval BSC_STATUS_OK The descriptor table passed validation.
 * @retval BSC_STATUS_INVALID_DESCRIPTOR The descriptor table or nested metadata
 *   failed validation; inspect @p error when provided for the reason and indexes.
 *
 * The validator does not mutate descriptors, retain pointers, allocate memory,
 * call command handlers, call access callbacks, match input tokens, parse
 * arguments, dispatch commands, render help, perform runtime registration, or
 * use platform I/O. It is synchronous and reentrant for independent immutable
 * descriptor tables. ISR suitability depends on the caller's tolerance for the
 * bounded validation loops.
 */
bsc_status_t bsc_registry_validate(const bsc_command_t *commands,
                                   size_t command_count,
                                   bsc_registry_validation_error_t *error);

#ifdef __cplusplus
}
#endif

#endif /* BSC_REGISTRY_H */
