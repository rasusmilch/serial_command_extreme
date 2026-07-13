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
 * bounds before rendering so rendering performs no unbounded help-text scans.
 *
 * Visibility is static: normal and advanced descriptors are visible by default,
 * while factory, locked, and hidden descriptors require explicit options. Help
 * visibility is not execution authorization and filtered exact lookup behaves as
 * #BSC_STATUS_UNKNOWN_COMMAND rather than #BSC_STATUS_ACCESS_DENIED. APIs are
 * synchronous and reentrant for independent caller-owned result/output storage
 * and immutable descriptor tables. Callers must serialize shared descriptor,
 * output, and option state. ISR suitability depends on the caller's output sink
 * and tolerance for bounded descriptor-table scans; task-context use is
 * recommended.
 */

/** @brief Static visibility options for generated help. */
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

/** @brief Help-specific descriptor validation failure reason. */
typedef enum bsc_help_error_reason {
  BSC_HELP_ERROR_NONE = 0,
  BSC_HELP_ERROR_REGISTRY_INVALID,
  BSC_HELP_ERROR_MISSING_SUMMARY,
  BSC_HELP_ERROR_EMPTY_SUMMARY,
  BSC_HELP_ERROR_SUMMARY_TOO_LONG,
  BSC_HELP_ERROR_MISSING_EXECUTABLE_DESCRIPTION,
  BSC_HELP_ERROR_EMPTY_DESCRIPTION,
  BSC_HELP_ERROR_DESCRIPTION_TOO_LONG,
  BSC_HELP_ERROR_EMPTY_ARGUMENT_HELP,
  BSC_HELP_ERROR_ARGUMENT_HELP_TOO_LONG,
  BSC_HELP_ERROR_EMPTY_ENUM_CHOICE_HELP,
  BSC_HELP_ERROR_ENUM_CHOICE_HELP_TOO_LONG,
  BSC_HELP_ERROR_INVALID_HELP_TEXT_CONTROL_BYTE,
  BSC_HELP_ERROR_MISSING_VISIBLE_PARENT_GROUP
} bsc_help_error_reason_t;

/** @brief Structured diagnostic for help metadata validation. */
typedef struct bsc_help_validation_error {
  bsc_help_error_reason_t reason;
  size_t command_index;
  size_t arg_index;
  size_t enum_choice_index;
  size_t required_parent_depth;
  bsc_registry_validation_error_t registry_error;
} bsc_help_validation_error_t;

/** @brief Result for exact help-path lookup. */
typedef struct bsc_help_lookup_result {
  const bsc_command_t *command;
  size_t command_index;
} bsc_help_lookup_result_t;

void bsc_help_options_init(bsc_help_options_t *options);
void bsc_help_validation_error_clear(bsc_help_validation_error_t *error);
void bsc_help_lookup_result_clear(bsc_help_lookup_result_t *result);

bsc_status_t bsc_help_validate(const bsc_command_t *commands,
                               size_t command_count,
                               const bsc_help_options_t *options,
                               bsc_help_validation_error_t *error);

bsc_status_t bsc_help_find_path(const bsc_command_t *commands,
                                size_t command_count,
                                const bsc_string_view_t *path_tokens,
                                size_t path_token_count,
                                const bsc_help_options_t *options,
                                bsc_help_lookup_result_t *result);

bsc_status_t bsc_help_render_index(const bsc_command_t *commands,
                                   size_t command_count,
                                   const bsc_help_options_t *options,
                                   bsc_output_t *output);

bsc_status_t bsc_help_render_commands(const bsc_command_t *commands,
                                      size_t command_count,
                                      const bsc_help_options_t *options,
                                      bsc_output_t *output);

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
