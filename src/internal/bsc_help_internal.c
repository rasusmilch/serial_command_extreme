#include "internal/bsc_help_internal.h"

#include "bsc_string_view.h"

/** @brief Compare one validated descriptor path to explicit-length lookup tokens. */
static int bsc_help_internal_path_matches(const bsc_command_t *command,
                                          const bsc_string_view_t *path_tokens,
                                          size_t path_token_count) {
  size_t index;
  if (command->path_len != path_token_count) {
    return 0;
  }
  for (index = 0u; index < path_token_count; ++index) {
    if (!bsc_string_view_equals_cstr_ignore_case(path_tokens[index], command->path[index])) {
      return 0;
    }
  }
  return 1;
}

/** @brief Return generated-help default visibility options by value. */
bsc_help_options_t bsc_help_internal_default_options(void) {
  bsc_help_options_t options;
  options.include_advanced = true;
  options.include_factory = false;
  options.include_locked = false;
  options.include_hidden = false;
  return options;
}

/** @brief Apply static generated-help visibility policy to one descriptor. */
int bsc_help_internal_is_visible(const bsc_command_t *command, const bsc_help_options_t *options) {
  bsc_help_options_t effective;
  if (command == NULL) {
    return 0;
  }
  effective = options == NULL ? bsc_help_internal_default_options() : *options;
  if (command->access == BSC_ACCESS_ADVANCED && !effective.include_advanced) {
    return 0;
  }
  if (command->access == BSC_ACCESS_FACTORY && !effective.include_factory) {
    return 0;
  }
  if (command->access == BSC_ACCESS_LOCKED && !effective.include_locked) {
    return 0;
  }
  if ((command->flags & BSC_COMMAND_FLAG_HIDDEN) != 0u && !effective.include_hidden) {
    return 0;
  }
  return 1;
}

/** @brief Find an exact visible descriptor path after caller validation. */
bsc_status_t bsc_help_internal_find_path_validated(const bsc_command_t *commands,
                                                   size_t command_count,
                                                   const bsc_string_view_t *path_tokens,
                                                   size_t path_token_count,
                                                   const bsc_help_options_t *options,
                                                   bsc_help_lookup_result_t *result) {
  size_t command_index;
  const bsc_command_t *found = NULL;
  size_t found_index = 0u;

  bsc_help_lookup_result_clear(result);
  if (path_token_count == 0u) {
    return BSC_STATUS_NO_INPUT;
  }
  if (commands == NULL || path_tokens == NULL || result == NULL) {
    return BSC_STATUS_INTERNAL_ERROR;
  }
  for (command_index = 0u; command_index < command_count; ++command_index) {
    if (!bsc_help_internal_is_visible(&commands[command_index], options) ||
        !bsc_help_internal_path_matches(&commands[command_index], path_tokens, path_token_count)) {
      continue;
    }
    if (found != NULL) {
      bsc_help_lookup_result_clear(result);
      return BSC_STATUS_AMBIGUOUS_COMMAND;
    }
    found = &commands[command_index];
    found_index = command_index;
  }
  if (found == NULL) {
    return BSC_STATUS_UNKNOWN_COMMAND;
  }
  result->command = found;
  result->command_index = found_index;
  return BSC_STATUS_OK;
}
