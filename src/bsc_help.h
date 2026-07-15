#ifndef BSC_HELP_H
#define BSC_HELP_H

#include <stdbool.h>
#include <stddef.h>

#include "bsc_registry.h"
#include "bsc_string_view.h"
#include "bsc_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file bsc_help.h
 * @brief Pure bounded generated help validation, lookup, and rendering.
 *
 * The help module consumes caller-owned immutable command descriptors and emits
 * deterministic LF-only text through caller-owned #bsc_output_t sinks. It owns
 * no descriptor, option, result, output, or string storage; retains no pointers
 * after returning; allocates no heap memory; and does not invoke command
 * handlers or execution access callbacks. Help metadata remains borrowed
 * NUL-terminated C-string data and is validated with #BSC_MAX_HELP_TEXT_LEN
 * bounds before rendering so rendering performs no unbounded help-prose scans.
 */

typedef struct bsc_help_options {
  /** Include advanced descriptors; default true. */
  bool include_advanced;
  /** Include factory descriptors; default false. */
  bool include_factory;
  /** Include locked descriptors; default false. */
  bool include_locked;
  /** Include descriptors flagged hidden; default false. */
  bool include_hidden;
} bsc_help_options_t;

typedef enum bsc_help_error_reason {
  /** No help validation error is present. */
  BSC_HELP_ERROR_NONE = 0,
  /** Ordinary registry validation failed; inspect the nested registry diagnostic. */
  BSC_HELP_ERROR_REGISTRY_INVALID,
  /** A visible descriptor had a NULL required summary. */
  BSC_HELP_ERROR_MISSING_SUMMARY,
  /** A visible descriptor had an empty required summary. */
  BSC_HELP_ERROR_EMPTY_SUMMARY,
  /** A visible descriptor summary was unterminated within #BSC_MAX_HELP_TEXT_LEN. */
  BSC_HELP_ERROR_SUMMARY_TOO_LONG,
  /** A visible executable command had a NULL required description. */
  BSC_HELP_ERROR_MISSING_EXECUTABLE_DESCRIPTION,
  /** A visible executable command had an empty required description. */
  BSC_HELP_ERROR_EMPTY_DESCRIPTION,
  /** A visible descriptor description was unterminated within #BSC_MAX_HELP_TEXT_LEN. */
  BSC_HELP_ERROR_DESCRIPTION_TOO_LONG,
  /** A non-NULL optional argument help string was empty. */
  BSC_HELP_ERROR_EMPTY_ARGUMENT_HELP,
  /** An argument help string was unterminated within #BSC_MAX_HELP_TEXT_LEN. */
  BSC_HELP_ERROR_ARGUMENT_HELP_TOO_LONG,
  /** A non-NULL optional enum-choice help string was empty. */
  BSC_HELP_ERROR_EMPTY_ENUM_CHOICE_HELP,
  /** An enum-choice help string was unterminated within #BSC_MAX_HELP_TEXT_LEN. */
  BSC_HELP_ERROR_ENUM_CHOICE_HELP_TOO_LONG,
  /** Help prose contained CR, LF, another ASCII control byte, or DEL. */
  BSC_HELP_ERROR_INVALID_HELP_TEXT_CONTROL_BYTE,
  /** A visible command path token emitted by help contained a presentation control byte. */
  BSC_HELP_ERROR_INVALID_PATH_TOKEN_CONTROL_BYTE,
  /** A visible argument name emitted by help contained a presentation control byte. */
  BSC_HELP_ERROR_INVALID_ARGUMENT_NAME_CONTROL_BYTE,
  /** A visible enum-choice name emitted by help contained a presentation control byte. */
  BSC_HELP_ERROR_INVALID_ENUM_CHOICE_NAME_CONTROL_BYTE,
  /** A visible nested descriptor lacked a visible explicit group at the recorded prefix depth. */
  BSC_HELP_ERROR_MISSING_VISIBLE_PARENT_GROUP
} bsc_help_error_reason_t;

typedef struct bsc_help_validation_error {
  /** Help failure reason, or #BSC_HELP_ERROR_NONE after clear/success. */
  bsc_help_error_reason_t reason;
  /** Failing descriptor index when applicable; zero when cleared or not applicable. */
  size_t command_index;
  /** Failing command path token index for path-token presentation failures; zero otherwise. */
  size_t path_token_index;
  /** Failing argument index for argument or enum-choice failures; zero otherwise. */
  size_t arg_index;
  /** Failing enum-choice index for enum-choice failures; zero otherwise. */
  size_t enum_choice_index;
  /** Missing parent prefix depth for parent-group failures; zero otherwise. */
  size_t required_parent_depth;
  /** Nested ordinary registry diagnostic for #BSC_HELP_ERROR_REGISTRY_INVALID; cleared otherwise. */
  bsc_registry_validation_error_t registry_error;
} bsc_help_validation_error_t;

typedef struct bsc_help_lookup_result {
  /** Borrowed matched descriptor pointer, or NULL after clear/failure; never retained by the core. */
  const bsc_command_t *command;
  /** Matched descriptor index, or zero after clear/failure. */
  size_t command_index;
} bsc_help_lookup_result_t;

/**
 * @brief Initialize help visibility options to defaults.
 * @param options Caller-owned options storage, or NULL for no effect.
 * Defaults include advanced descriptors and exclude factory, locked, and hidden descriptors.
 */
void bsc_help_options_init(bsc_help_options_t *options);

/**
 * @brief Clear a caller-owned help validation diagnostic.
 * @param error Optional diagnostic storage. NULL is accepted. All scalar indexes and nested registry data are reset.
 */
void bsc_help_validation_error_clear(bsc_help_validation_error_t *error);

/**
 * @brief Clear a caller-owned lookup result.
 * @param result Optional result storage. NULL is accepted. Any borrowed descriptor pointer is discarded.
 */
void bsc_help_lookup_result_clear(bsc_help_lookup_result_t *result);

/**
 * @brief Validate registry shape, help-visible metadata, emitted identifiers, and visible parent groups.
 * @param commands Borrowed descriptor table; ordinary registry rules define pointer/count validity.
 * @param command_count Number of descriptors.
 * @param options Optional borrowed visibility options; NULL means defaults.
 * @param error Optional caller-owned diagnostic cleared on entry and filled on failure.
 * @retval BSC_STATUS_OK The table is valid for generated help under the supplied visibility options.
 * @retval BSC_STATUS_INVALID_DESCRIPTOR Registry, help prose, identifier, or parent-group validation failed.
 * The function emits no output, invokes no handlers/access callbacks, retains no pointers, and is reentrant for
 * immutable tables with independent diagnostics. It is task-context oriented; ISR suitability depends on bounded scan tolerance.
 */
bsc_status_t bsc_help_validate(const bsc_command_t *commands,
                               size_t command_count,
                               const bsc_help_options_t *options,
                               bsc_help_validation_error_t *error);

/**
 * @brief Find one visible descriptor by exact ASCII case-insensitive path tokens.
 * @param commands Borrowed descriptor table.
 * @param command_count Number of descriptors.
 * @param path_tokens Borrowed explicit-length path tokens; required when @p path_token_count is nonzero.
 * @param path_token_count Number of path tokens; zero returns #BSC_STATUS_NO_INPUT.
 * @param options Optional borrowed visibility options; NULL means defaults.
 * @param result Required caller-owned result, cleared on entry and failure.
 * @retval BSC_STATUS_OK Exact visible command or group found; result borrows the descriptor until caller metadata changes.
 * @retval BSC_STATUS_NO_INPUT No path tokens were supplied.
 * @retval BSC_STATUS_UNKNOWN_COMMAND No exact visible target matched, including filtered targets.
 * @retval BSC_STATUS_AMBIGUOUS_COMMAND Defensive duplicate visible match was detected.
 * @retval BSC_STATUS_INVALID_DESCRIPTOR Validation failed before lookup.
 * @retval BSC_STATUS_INTERNAL_ERROR Required pointers were invalid.
 * The function emits no output, invokes no handlers/access callbacks, treats no token as an argument or subtopic,
 * retains no pointers, and is reentrant for independent result storage.
 */
bsc_status_t bsc_help_find_path(const bsc_command_t *commands,
                                size_t command_count,
                                const bsc_string_view_t *path_tokens,
                                size_t path_token_count,
                                const bsc_help_options_t *options,
                                bsc_help_lookup_result_t *result);

/**
 * @brief Render the top-level visible help index under a COMMANDS heading.
 * @param commands Borrowed descriptor table.
 * @param command_count Number of descriptors.
 * @param options Optional borrowed visibility options; NULL means defaults.
 * @param output Required caller-owned output sink; callback blocking/ISR behavior is sink-defined.
 * @retval BSC_STATUS_OK Output completed.
 * @retval BSC_STATUS_INVALID_DESCRIPTOR Validation failed before any output was emitted.
 * @retval BSC_STATUS_OUTPUT_TRUNCATED The first short write occurred and rendering stopped immediately.
 * @retval BSC_STATUS_INTERNAL_ERROR Output target or required API input was invalid.
 * Emits LF-only deterministic descriptor-order text, retains no pointers, invokes no handlers/access callbacks,
 * and never reads runtime parsed arguments or secret values.
 */
bsc_status_t bsc_help_render_index(const bsc_command_t *commands,
                                   size_t command_count,
                                   const bsc_help_options_t *options,
                                   bsc_output_t *output);

/**
 * @brief Render every visible executable command under a COMMANDS heading.
 * @param commands Borrowed descriptor table whose path tokens, argument metadata, summaries, and descriptions
 *        must remain valid only for the duration of the call; the renderer retains no descriptor pointers.
 * @param command_count Number of descriptors in @p commands. The same command/table validity rules used by
 *        #bsc_help_validate apply before any bytes are emitted.
 * @param options Optional borrowed static visibility options. NULL selects #bsc_help_options_init defaults:
 *        normal and advanced descriptors are visible, while factory, locked, and hidden descriptors are filtered.
 * @param output Required caller-owned output sink. The sink callback may block according to application policy;
 *        this API performs no serialization around a shared sink.
 * @retval BSC_STATUS_OK Output completed.
 * @retval BSC_STATUS_INVALID_DESCRIPTOR Registry or help metadata validation failed before any output was emitted.
 * @retval BSC_STATUS_OUTPUT_TRUNCATED The first partial/short sink write occurred; rendering stopped immediately
 *         and no fallback text or trailing newline was added.
 * @retval BSC_STATUS_INTERNAL_ERROR Required API inputs or output callback storage were invalid.
 *
 * The emitted document is LF-only, starts with a COMMANDS heading, lists only visible executable command descriptors
 * (groups are omitted), and preserves descriptor-table order without sorting or heap allocation. The function validates
 * descriptors before output, never invokes command handlers or command access callbacks, never reads parsed runtime
 * arguments or runtime secret values, and does not retain @p commands, @p options, or @p output after return. Calls are
 * reentrant for independent descriptor tables/options/output sinks; callers that share a sink across tasks must provide
 * external serialization. Because sink behavior may block and registry scans are bounded but nontrivial, this function is
 * intended for normal task/thread context rather than ISR context.
 */
bsc_status_t bsc_help_render_commands(const bsc_command_t *commands,
                                      size_t command_count,
                                      const bsc_help_options_t *options,
                                      bsc_output_t *output);

/**
 * @brief Render a visible exact group or executable-command help page.
 * @param commands Borrowed descriptor table.
 * @param command_count Number of descriptors.
 * @param path_tokens Borrowed explicit-length target path tokens.
 * @param path_token_count Number of target path tokens.
 * @param options Optional borrowed visibility options; NULL means defaults.
 * @param output Required caller-owned output sink.
 * @retval BSC_STATUS_OK Output completed.
 * @retval BSC_STATUS_NO_INPUT No path tokens were supplied.
 * @retval BSC_STATUS_UNKNOWN_COMMAND Target was absent or filtered.
 * @retval BSC_STATUS_INVALID_DESCRIPTOR Validation failed before output.
 * @retval BSC_STATUS_OUTPUT_TRUNCATED The first short write occurred and rendering stopped immediately.
 * @retval BSC_STATUS_INTERNAL_ERROR Required pointers or output target were invalid.
 * The renderer emits generated NAME/SYNOPSIS/DESCRIPTION/ARGUMENTS/VALID VALUES/COMMANDS sections as applicable,
 * invokes no handlers/access callbacks, retains no pointers, and never reads parsed arguments or runtime secrets.
 */
bsc_status_t bsc_help_render_path(const bsc_command_t *commands,
                                  size_t command_count,
                                  const bsc_string_view_t *path_tokens,
                                  size_t path_token_count,
                                  const bsc_help_options_t *options,
                                  bsc_output_t *output);

#ifdef __cplusplus
}
#endif

#endif /* BSC_HELP_H */
