#include "bsc_matcher.h"

#include "bsc_config.h"

/**
 * @brief Centralize the cleared-result invariant used by matcher failure paths.
 */
void bsc_match_result_clear(bsc_match_result_t *result) {
  if (result == NULL) {
    return;
  }

  result->command = NULL;
  result->command_index = 0u;
  result->consumed_token_count = 0u;
  result->remaining_token_index = 0u;
  result->remaining_token_count = 0u;
  result->group = NULL;
  result->group_index = 0u;
}

/**
 * @brief Return whether a node type can participate in command matching.
 */
static int bsc_matcher_node_type_is_valid(bsc_node_type_t node_type) {
  return node_type == BSC_NODE_GROUP || node_type == BSC_NODE_COMMAND;
}

/**
 * @brief Validate only descriptor path and node fields needed for safe matching.
 *
 * This helper intentionally does not duplicate full registry validation. It does
 * not inspect handlers, argument metadata, access levels, flags, help strings,
 * or command context because those fields are outside matcher responsibility.
 *
 * @retval BSC_STATUS_OK The path/node fields are safe to inspect.
 * @retval BSC_STATUS_INVALID_DESCRIPTOR Required path/node metadata was malformed.
 */
static bsc_status_t bsc_matcher_validate_path_fields(const bsc_command_t *command) {
  size_t token_index;

  if (command->path == NULL || command->path_len == 0u ||
      command->path_len > (size_t)BSC_MAX_PATH_TOKENS ||
      !bsc_matcher_node_type_is_valid(command->node_type)) {
    return BSC_STATUS_INVALID_DESCRIPTOR;
  }

  for (token_index = 0u; token_index < command->path_len; ++token_index) {
    if (command->path[token_index] == NULL) {
      return BSC_STATUS_INVALID_DESCRIPTOR;
    }
  }

  return BSC_STATUS_OK;
}

/**
 * @brief Return whether a descriptor path matches the leading input tokens.
 *
 * Descriptor path tokens are null-terminated C strings, while input tokens are
 * bounded borrowed views. Matching is ASCII case-insensitive through the
 * string-view helper, copies no token text, and never treats token views as C
 * strings.
 */
static int bsc_matcher_path_matches_tokens(const bsc_command_t *command,
                                           const bsc_string_view_t *tokens,
                                           size_t token_count) {
  size_t token_index;

  if (command->path_len > token_count) {
    return 0;
  }

  for (token_index = 0u; token_index < command->path_len; ++token_index) {
    if (!bsc_string_view_equals_cstr_ignore_case(tokens[token_index], command->path[token_index])) {
      return 0;
    }
  }

  return 1;
}

/**
 * @brief Populate the OK result shape for a matched executable command.
 *
 * The remaining-token fields define the future argument-parser handoff. The
 * result pointer is optional, and any stored command pointer borrows from the
 * caller-owned command table.
 */
static void bsc_matcher_set_command_result(bsc_match_result_t *result,
                                           const bsc_command_t *command,
                                           size_t command_index,
                                           size_t token_count) {
  if (result == NULL) {
    return;
  }

  result->command = command;
  result->command_index = command_index;
  result->consumed_token_count = command->path_len;
  result->remaining_token_index = command->path_len;
  result->remaining_token_count = token_count - command->path_len;
}

/**
 * @brief Populate the group-required result shape for an exact group match.
 *
 * Group results are produced only for exact-token-count matches, so the
 * remaining-token count is zero. The result pointer is optional, and any stored
 * group pointer borrows from the caller-owned command table.
 */
static void bsc_matcher_set_group_result(bsc_match_result_t *result,
                                         const bsc_command_t *group,
                                         size_t group_index) {
  if (result == NULL) {
    return;
  }

  result->group = group;
  result->group_index = group_index;
  result->consumed_token_count = group->path_len;
  result->remaining_token_index = group->path_len;
  result->remaining_token_count = 0u;
}

/**
 * @brief Scan descriptors with separate best-command and exact-group state.
 *
 * Longer executable paths supersede shorter command or group prefixes and
 * define the remaining-token argument handoff. Exact groups are retained only as
 * a fallback when no executable command wins. Ambiguity detection is defensive
 * for unvalidated or inconsistent descriptor tables.
 */
bsc_status_t bsc_match_command(const bsc_command_t *commands,
                               size_t command_count,
                               const bsc_string_view_t *tokens,
                               size_t token_count,
                               bsc_match_result_t *result) {
  const bsc_command_t *best_command = NULL;
  const bsc_command_t *exact_group = NULL;
  size_t best_command_index = 0u;
  size_t exact_group_index = 0u;
  int best_command_is_ambiguous = 0;
  size_t command_index;

  bsc_match_result_clear(result);

  if (token_count == 0u) {
    return BSC_STATUS_NO_INPUT;
  }
  if (tokens == NULL || token_count > (size_t)BSC_MAX_TOKENS ||
      (commands == NULL && command_count > 0u) || command_count > (size_t)BSC_MAX_COMMANDS) {
    return BSC_STATUS_INTERNAL_ERROR;
  }
  if (command_count == 0u) {
    return BSC_STATUS_UNKNOWN_COMMAND;
  }

  for (command_index = 0u; command_index < command_count; ++command_index) {
    const bsc_command_t *candidate = &commands[command_index];
    bsc_status_t status = bsc_matcher_validate_path_fields(candidate);
    int matched;

    if (status != BSC_STATUS_OK) {
      return status;
    }

    matched = bsc_matcher_path_matches_tokens(candidate, tokens, token_count);
    if (!matched) {
      continue;
    }

    if (candidate->node_type == BSC_NODE_COMMAND) {
      /* A longer executable path defines the argument boundary. Shorter command
       * or group prefixes remain valid metadata but must not win the match. */
      if (best_command == NULL || candidate->path_len > best_command->path_len) {
        best_command = candidate;
        best_command_index = command_index;
        best_command_is_ambiguous = 0;
      } else if (candidate->path_len == best_command->path_len) {
        best_command_is_ambiguous = 1;
      }

      if (exact_group != NULL && exact_group->path_len == candidate->path_len) {
        bsc_match_result_clear(result);
        return BSC_STATUS_AMBIGUOUS_COMMAND;
      }
    } else {
      if (candidate->path_len == token_count) {
        if (exact_group != NULL) {
          bsc_match_result_clear(result);
          return BSC_STATUS_AMBIGUOUS_COMMAND;
        }
        exact_group = candidate;
        exact_group_index = command_index;

        if (best_command != NULL && best_command->path_len == candidate->path_len) {
          bsc_match_result_clear(result);
          return BSC_STATUS_AMBIGUOUS_COMMAND;
        }
      }
    }
  }

  if (best_command != NULL) {
    if (best_command_is_ambiguous) {
      bsc_match_result_clear(result);
      return BSC_STATUS_AMBIGUOUS_COMMAND;
    }
    bsc_matcher_set_command_result(result, best_command, best_command_index, token_count);
    return BSC_STATUS_OK;
  }

  if (exact_group != NULL) {
    bsc_matcher_set_group_result(result, exact_group, exact_group_index);
    return BSC_STATUS_GROUP_REQUIRES_SUBCOMMAND;
  }

  /* Group prefixes with unknown children and prefixes of longer commands without
   * an explicit exact group are unknown commands; matcher does not fabricate a
   * group or parse argument tokens here. */
  return BSC_STATUS_UNKNOWN_COMMAND;
}
