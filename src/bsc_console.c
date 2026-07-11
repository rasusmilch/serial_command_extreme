#include "bsc_console.h"

#include "bsc_dispatch.h"
#include "bsc_tokenizer.h"

#include <string.h>

/** @brief Clear every token view slot in a console workspace. */
static void bsc_console_clear_tokens(bsc_console_workspace_t *workspace) {
  size_t index;
  for (index = 0u; index < (size_t)BSC_MAX_TOKENS; ++index) {
    workspace->tokens[index] = bsc_string_view_from_parts(NULL, 0u);
  }
  workspace->token_count = 0u;
}

/** @brief Clear transient execution metadata without touching active line bytes or guard state. */
static void bsc_console_clear_metadata(bsc_console_workspace_t *workspace) {
  bsc_console_clear_tokens(workspace);
  bsc_match_result_clear(&workspace->match_result);
  bsc_parsed_args_clear(&workspace->parsed_args);
  bsc_arg_parse_error_clear(&workspace->parse_error);
}

/** @brief Clear all transient workspace storage after an active execution finishes. */
static void bsc_console_cleanup_active_workspace(bsc_console_workspace_t *workspace) {
  memset(workspace->line_buffer, 0, sizeof(workspace->line_buffer));
  bsc_console_clear_metadata(workspace);
  workspace->execution_active = false;
}

/** @brief Return true when a dispatch parse diagnostic should be copied to the public result. */
static bool bsc_console_parse_error_is_populated(const bsc_arg_parse_error_t *parse_error) {
  return parse_error != NULL && parse_error->reason != BSC_ARG_PARSE_ERROR_NONE;
}

/** @brief Return whether the input span contains a NUL byte rejected by high-level execution. */
static bool bsc_console_contains_embedded_nul(const char *line, size_t line_length) {
  size_t index;
  for (index = 0u; index < line_length; ++index) {
    if (line[index] == '\0') {
      return true;
    }
  }
  return false;
}

/** @brief Validate matcher result indexes before deriving a dispatch argument slice. */
static bool bsc_console_match_handoff_is_valid(const bsc_console_workspace_t *workspace) {
  const bsc_match_result_t *match;
  size_t available;

  match = &workspace->match_result;
  if (match->command == NULL || match->remaining_token_index > workspace->token_count) {
    return false;
  }
  available = workspace->token_count - match->remaining_token_index;
  return match->remaining_token_count <= available;
}

/** @brief Set the public result phase when optional result storage exists. */
static void bsc_console_set_phase(bsc_console_result_t *result, bsc_console_phase_t phase) {
  if (result != NULL) {
    result->phase = phase;
  }
}

/** @brief Copy a successful command match into optional public result metadata. */
static void bsc_console_copy_command_result(bsc_console_result_t *result, const bsc_match_result_t *match) {
  if (result != NULL) {
    result->command = match->command;
    result->command_index = match->command_index;
  }
}

/** @brief Copy an exact group match into optional public result metadata. */
static void bsc_console_copy_group_result(bsc_console_result_t *result, const bsc_match_result_t *match) {
  if (result != NULL) {
    result->group = match->group;
    result->group_index = match->group_index;
  }
}

void bsc_console_workspace_init(bsc_console_workspace_t *workspace) {
  if (workspace == NULL) {
    return;
  }
  memset(workspace->line_buffer, 0, sizeof(workspace->line_buffer));
  bsc_console_clear_metadata(workspace);
  workspace->execution_active = false;
}

void bsc_console_result_clear(bsc_console_result_t *result) {
  if (result == NULL) {
    return;
  }
  result->phase = BSC_CONSOLE_PHASE_NONE;
  result->command = NULL;
  result->command_index = 0u;
  result->group = NULL;
  result->group_index = 0u;
  bsc_arg_parse_error_clear(&result->parse_error);
}

bsc_status_t bsc_console_init(bsc_console_t *console,
                              const bsc_console_config_t *config,
                              bsc_registry_validation_error_t *validation_error) {
  bsc_status_t status;

  if (validation_error != NULL) {
    bsc_registry_validation_error_clear(validation_error);
  }
  if (console == NULL) {
    return BSC_STATUS_INTERNAL_ERROR;
  }

  memset(console, 0, sizeof(*console));
  if (config == NULL) {
    return BSC_STATUS_INTERNAL_ERROR;
  }

  status = bsc_registry_validate(config->commands, config->command_count, validation_error);
  if (status != BSC_STATUS_OK) {
    return status;
  }

  console->commands = config->commands;
  console->command_count = config->command_count;
  console->app_context = config->app_context;
  if (config->output != NULL) {
    console->output = *config->output;
    console->has_output = true;
  }
  console->initialized = true;
  return BSC_STATUS_OK;
}

bsc_status_t bsc_execute_line(const bsc_console_t *console,
                              bsc_console_workspace_t *workspace,
                              const char *line,
                              size_t line_length,
                              bsc_console_result_t *result) {
  bsc_status_t status = BSC_STATUS_OK;
  const bsc_string_view_t *arg_tokens = NULL;
  size_t arg_token_count = 0u;
  bsc_output_t output_copy;
  bsc_output_t *dispatch_output = NULL;

  bsc_console_result_clear(result);

  if (console == NULL || workspace == NULL || !console->initialized || (line == NULL && line_length != 0u)) {
    bsc_console_set_phase(result, BSC_CONSOLE_PHASE_INPUT);
    return BSC_STATUS_INTERNAL_ERROR;
  }
  if (line_length > (size_t)BSC_MAX_LINE_LEN) {
    bsc_console_set_phase(result, BSC_CONSOLE_PHASE_INPUT);
    return BSC_STATUS_LINE_TOO_LONG;
  }
  if (workspace->execution_active) {
    bsc_console_set_phase(result, BSC_CONSOLE_PHASE_INPUT);
    return BSC_STATUS_INTERNAL_ERROR;
  }
  if (line_length != 0u && bsc_console_contains_embedded_nul(line, line_length)) {
    bsc_console_set_phase(result, BSC_CONSOLE_PHASE_INPUT);
    return BSC_STATUS_INVALID_SYNTAX;
  }

  workspace->execution_active = true;
  bsc_console_clear_metadata(workspace);

  if (line_length == 0u) {
    status = BSC_STATUS_NO_INPUT;
    bsc_console_set_phase(result, BSC_CONSOLE_PHASE_INPUT);
    goto cleanup;
  }

  memmove(workspace->line_buffer, line, line_length);
  workspace->line_buffer[line_length] = '\0';

  status = bsc_tokenize_line(workspace->line_buffer,
                             line_length,
                             workspace->tokens,
                             (size_t)BSC_MAX_TOKENS,
                             &workspace->token_count);
  if (status != BSC_STATUS_OK) {
    bsc_console_set_phase(result, BSC_CONSOLE_PHASE_TOKENIZE);
    goto cleanup;
  }

  status = bsc_match_command(console->commands,
                             console->command_count,
                             workspace->tokens,
                             workspace->token_count,
                             &workspace->match_result);
  if (status == BSC_STATUS_GROUP_REQUIRES_SUBCOMMAND) {
    bsc_console_set_phase(result, BSC_CONSOLE_PHASE_MATCH);
    bsc_console_copy_group_result(result, &workspace->match_result);
    goto cleanup;
  }
  if (status != BSC_STATUS_OK) {
    bsc_console_set_phase(result, BSC_CONSOLE_PHASE_MATCH);
    goto cleanup;
  }
  if (!bsc_console_match_handoff_is_valid(workspace)) {
    status = BSC_STATUS_INTERNAL_ERROR;
    bsc_console_set_phase(result, BSC_CONSOLE_PHASE_MATCH);
    goto cleanup;
  }

  bsc_console_copy_command_result(result, &workspace->match_result);
  if (workspace->match_result.remaining_token_count != 0u) {
    arg_tokens = &workspace->tokens[workspace->match_result.remaining_token_index];
    arg_token_count = workspace->match_result.remaining_token_count;
  }
  if (console->has_output) {
    output_copy = console->output;
    dispatch_output = &output_copy;
  }

  status = bsc_dispatch_command(console->app_context,
                                workspace->match_result.command,
                                arg_tokens,
                                arg_token_count,
                                &workspace->parsed_args,
                                &workspace->parse_error,
                                dispatch_output);
  bsc_console_set_phase(result, BSC_CONSOLE_PHASE_DISPATCH);
  if (result != NULL && bsc_console_parse_error_is_populated(&workspace->parse_error)) {
    result->parse_error = workspace->parse_error;
  }

cleanup:
  bsc_console_cleanup_active_workspace(workspace);
  return status;
}
