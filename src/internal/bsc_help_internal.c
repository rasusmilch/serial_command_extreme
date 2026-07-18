#include "internal/bsc_help_internal.h"

#include "bsc_config.h"
#include "bsc_output.h"
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

/**
 * @brief Return generated-help default visibility options by value.
 *
 * The helper owns no storage, reads no caller metadata, and returns the same
 * static presentation policy used by the public help APIs: normal and advanced
 * descriptors are visible, while factory, locked, and hidden descriptors are
 * filtered unless callers supply options that include them.
 */
bsc_help_options_t bsc_help_internal_default_options(void) {
  bsc_help_options_t options;
  options.include_advanced = true;
  options.include_factory = false;
  options.include_locked = false;
  options.include_hidden = false;
  return options;
}

/**
 * @brief Apply static generated-help visibility policy to one descriptor.
 *
 * @param command Borrowed descriptor to test; NULL is treated as not visible.
 * @param options Optional borrowed options; NULL selects default options.
 * @return Nonzero when the descriptor is presentable under static help policy.
 *
 * The helper never invokes execution access callbacks and retains no pointers.
 */
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

/**
 * @brief Find an exact visible descriptor path after caller validation.
 *
 * @param commands Borrowed authoritative descriptor table already validated by the caller.
 * @param command_count Number of descriptors in @p commands.
 * @param path_tokens Borrowed explicit-length path tokens.
 * @param path_token_count Token count; zero returns #BSC_STATUS_NO_INPUT.
 * @param options Optional borrowed visibility options; NULL selects defaults.
 * @param result Required caller-owned result storage, cleared on entry/failure.
 * @retval BSC_STATUS_OK One exact visible descriptor matched.
 * @retval BSC_STATUS_NO_INPUT No path tokens were supplied.
 * @retval BSC_STATUS_UNKNOWN_COMMAND No visible exact path matched.
 * @retval BSC_STATUS_AMBIGUOUS_COMMAND Defensive duplicate visible path match.
 * @retval BSC_STATUS_INTERNAL_ERROR Required pointers were invalid.
 *
 * The helper does not perform registry/help validation, does not call handlers
 * or access callbacks, and retains no borrowed pointers except through the
 * caller-owned result on success.
 */
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

/**
 * @brief Return the bounded byte length of validated help prose.
 *
 * @param text Required borrowed NUL-terminated prose accepted by prior help or
 * catalog validation. The helper reads at most BSC_MAX_HELP_TEXT_LEN + 1 bytes.
 * @return Byte length excluding the terminator.
 *
 * The helper owns no storage, retains no pointers, emits no output, and assumes
 * validation already proved the terminator appears within the configured bound.
 */
size_t bsc_help_internal_prose_length(const char *text) {
  size_t length = 0u;
  while (length <= (size_t)BSC_MAX_HELP_TEXT_LEN && text[length] != '\0') {
    length += 1u;
  }
  return length;
}

/**
 * @brief Return the bounded byte length of a validated rendered identifier.
 *
 * @param text Required borrowed NUL-terminated descriptor token, argument name,
 * enum choice name, or topic id accepted by prior validation.
 * @return Byte length excluding the terminator.
 *
 * The helper scans only within the token bound, owns no storage, retains no
 * pointers, and performs no output.
 */
size_t bsc_help_internal_identifier_length(const char *text) {
  size_t length = 0u;
  while (length <= (size_t)BSC_MAX_TOKEN_LEN && text[length] != '\0') {
    length += 1u;
  }
  return length;
}

/**
 * @brief Write an explicit byte span through a caller-owned help output sink.
 *
 * @param output Required caller-owned sink with non-NULL write callback.
 * @param data Required borrowed bytes when @p length is nonzero.
 * @param length Number of bytes to pass to the callback.
 * @retval BSC_STATUS_OK The sink accepted the full span.
 * @retval BSC_STATUS_OUTPUT_TRUNCATED The sink reported a short write.
 * @retval BSC_STATUS_INTERNAL_ERROR Sink or byte-span inputs were invalid.
 *
 * The helper performs exactly one callback per call, owns no storage, retains no
 * pointers, and leaves shared-sink serialization to callers.
 */
bsc_status_t bsc_help_internal_write(bsc_output_t *output, const char *data, size_t length) {
  return bsc_out_write_bytes(output, data, length);
}

/**
 * @brief Emit a validated descriptor path with one space between tokens.
 *
 * @param output Required caller-owned sink.
 * @param command Required borrowed descriptor whose path tokens were already validated.
 * @retval BSC_STATUS_OK The complete path was written.
 * @retval BSC_STATUS_OUTPUT_TRUNCATED The first short write occurred.
 * @retval BSC_STATUS_INTERNAL_ERROR Output inputs were invalid.
 *
 * The helper streams directly, performs no allocation, retains no pointers, and
 * stops immediately when a write helper returns a failure status.
 */
bsc_status_t bsc_help_internal_write_path(bsc_output_t *output, const bsc_command_t *command) {
  size_t index;
  bsc_status_t status;
  for (index = 0u; index < command->path_len; ++index) {
    if (index != 0u) {
      status = bsc_help_internal_write(output, " ", 1u);
      if (status != BSC_STATUS_OK) {
        return status;
      }
    }
    status = bsc_help_internal_write(output,
                                     command->path[index],
                                     bsc_help_internal_identifier_length(command->path[index]));
    if (status != BSC_STATUS_OK) {
      return status;
    }
  }
  return BSC_STATUS_OK;
}

/**
 * @brief Emit a section heading using the ordinary help blank-line policy.
 *
 * @param output Required caller-owned sink.
 * @param heading Required borrowed heading bytes.
 * @param heading_length Number of heading bytes to emit.
 * @param sections Required caller-owned emitted-section counter.
 * @retval BSC_STATUS_OK The heading and separator bytes were written.
 * @retval BSC_STATUS_OUTPUT_TRUNCATED The first short write occurred.
 * @retval BSC_STATUS_INTERNAL_ERROR Output or required helper inputs were invalid.
 *
 * The helper updates only caller-owned section state and retains no pointers.
 */
bsc_status_t bsc_help_internal_section(bsc_output_t *output, const char *heading, size_t heading_length, int *sections) {
  int section_started = sections != NULL && *sections != 0;
  bsc_status_t status;
  if (section_started) {
    status = bsc_help_internal_write(output, "\n", 1u);
    if (status != BSC_STATUS_OK) {
      return status;
    }
  }
  if (sections != NULL) {
    *sections = 1;
  }
  status = bsc_help_internal_write(output, heading, heading_length);
  if (status != BSC_STATUS_OK) {
    return status;
  }
  return bsc_help_internal_write(output, "\n", 1u);
}

/**
 * @brief Emit one two-space-indented descriptor path and summary entry.
 *
 * @param output Required caller-owned sink.
 * @param command Required borrowed descriptor with validated path and summary.
 * @retval BSC_STATUS_OK The entry was fully written.
 * @retval BSC_STATUS_OUTPUT_TRUNCATED The first short write occurred.
 * @retval BSC_STATUS_INTERNAL_ERROR Output inputs were invalid.
 *
 * Used by command lists and RELATED rendering; it streams directly and never
 * invokes command handlers or access callbacks.
 */
bsc_status_t bsc_help_internal_entry(bsc_output_t *output, const bsc_command_t *command) {
  bsc_status_t status;
  status = bsc_help_internal_write(output, "  ", 2u);
  if (status != BSC_STATUS_OK) {
    return status;
  }
  status = bsc_help_internal_write_path(output, command);
  if (status != BSC_STATUS_OK) {
    return status;
  }
  status = bsc_help_internal_write(output, " - ", 3u);
  if (status != BSC_STATUS_OK) {
    return status;
  }
  status = bsc_help_internal_write(output, command->summary, bsc_help_internal_prose_length(command->summary));
  if (status != BSC_STATUS_OK) {
    return status;
  }
  return bsc_help_internal_write(output, "\n", 1u);
}
