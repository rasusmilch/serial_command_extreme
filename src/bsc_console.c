#include "bsc_console.h"

#include "bsc_dispatch.h"
#include "bsc_help.h"
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

/** @brief Set the top-level phase in optional built-in-aware result storage. */
static void bsc_console_builtins_set_phase(bsc_console_builtins_result_t *result, bsc_console_phase_t phase) {
  if (result != NULL) {
    result->phase = phase;
  }
}

/** @brief Set the recognized built-in in optional built-in-aware result storage. */
static void bsc_console_builtins_set_builtin(bsc_console_builtins_result_t *result, bsc_console_builtin_t builtin) {
  if (result != NULL) {
    result->builtin = builtin;
  }
}

/** @brief Mirror an ordinary application result into the top-level built-in-aware phase. */
static void bsc_console_builtins_copy_application_phase(bsc_console_builtins_result_t *result) {
  if (result != NULL) {
    result->phase = result->application_result.phase;
  }
}

/** @brief Record the first descriptor that collides with an invoked built-in name. */
static void bsc_console_builtins_set_collision(bsc_console_builtins_result_t *result,
                                               const bsc_command_t *collision,
                                               size_t collision_index) {
  if (result != NULL) {
    result->collision = collision;
    result->collision_index = collision_index;
  }
}

/** @brief Return whether a token is one configured built-in literal, ignoring ASCII case. */
static bool bsc_console_token_equals_builtin(bsc_string_view_t token, const char *builtin_name) {
  return bsc_string_view_equals_cstr_ignore_case(token, builtin_name);
}

/**
 * @brief Find the first application descriptor whose first path token reserves a built-in.
 *
 * Collision detection is intentionally performed only after the first input token
 * has selected a built-in route. It scans descriptor-table order, reads only
 * borrowed immutable path metadata, invokes no matcher/access/handler code, and
 * retains no pointer unless optional result storage records the borrowed
 * descriptor for caller diagnostics.
 */
static bool bsc_console_find_builtin_collision(const bsc_console_t *console,
                                               const char *builtin_name,
                                               const bsc_command_t **collision,
                                               size_t *collision_index) {
  size_t index;
  for (index = 0u; index < console->command_count; ++index) {
    const bsc_command_t *candidate = &console->commands[index];
    if (candidate->path_len >= 1u && candidate->path != NULL && candidate->path[0] != NULL &&
        bsc_string_view_equals_cstr_ignore_case(bsc_string_view_from_cstr(candidate->path[0]), builtin_name)) {
      if (collision != NULL) {
        *collision = candidate;
      }
      if (collision_index != NULL) {
        *collision_index = index;
      }
      return true;
    }
  }
  return false;
}

/** @brief Copy console output by value for built-in rendering, or return NULL when absent. */
static bsc_output_t *bsc_console_builtin_output(const bsc_console_t *console, bsc_output_t *output_copy) {
  if (!console->has_output) {
    return NULL;
  }
  *output_copy = console->output;
  return output_copy;
}

/**
 * @brief Run the existing application matcher and dispatcher over already-tokenized input.
 *
 * The caller owns workspace activation, input copy, tokenization, and cleanup.
 * This helper preserves the original application-only command path used by
 * bsc_execute_line(), including longest-path matching, by-value output wrapper
 * copying, non-secret result population, parser diagnostics, and handler output
 * propagation.
 */
static bsc_status_t bsc_console_execute_application_tokens(const bsc_console_t *console,
                                                           bsc_console_workspace_t *workspace,
                                                           bsc_console_result_t *result) {
  bsc_status_t status;
  const bsc_string_view_t *arg_tokens = NULL;
  size_t arg_token_count = 0u;
  bsc_output_t output_copy;
  bsc_output_t *dispatch_output = NULL;

  status = bsc_match_command(console->commands,
                             console->command_count,
                             workspace->tokens,
                             workspace->token_count,
                             &workspace->match_result);
  if (status == BSC_STATUS_GROUP_REQUIRES_SUBCOMMAND) {
    bsc_console_set_phase(result, BSC_CONSOLE_PHASE_MATCH);
    bsc_console_copy_group_result(result, &workspace->match_result);
    return status;
  }
  if (status != BSC_STATUS_OK) {
    bsc_console_set_phase(result, BSC_CONSOLE_PHASE_MATCH);
    return status;
  }
  if (!bsc_console_match_handoff_is_valid(workspace)) {
    bsc_console_set_phase(result, BSC_CONSOLE_PHASE_MATCH);
    return BSC_STATUS_INTERNAL_ERROR;
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
  return status;
}

/**
 * @brief Route one already-tokenized built-in request through the pure help API.
 *
 * Built-in routing owns no large storage: it borrows path token views from the
 * active workspace, copies help options and output wrappers by value, scans only
 * for the invoked built-in collision, and invokes no application matcher,
 * parser, access callback, dispatcher, or handler.
 */
static bsc_status_t bsc_console_execute_builtin_tokens(const bsc_console_t *console,
                                                       bsc_console_workspace_t *workspace,
                                                       const bsc_help_options_t *help_options,
                                                       bsc_console_builtins_result_t *result,
                                                       const char *builtin_name,
                                                       bsc_console_builtin_t builtin) {
  const bsc_command_t *collision = NULL;
  size_t collision_index = 0u;
  bsc_help_options_t options_copy;
  bsc_output_t output_copy;
  bsc_output_t *output;

  bsc_console_builtins_set_phase(result, BSC_CONSOLE_PHASE_BUILTIN);
  bsc_console_builtins_set_builtin(result, builtin);

  if (bsc_console_find_builtin_collision(console, builtin_name, &collision, &collision_index)) {
    bsc_console_builtins_set_collision(result, collision, collision_index);
    return BSC_STATUS_INVALID_DESCRIPTOR;
  }

  if (help_options != NULL) {
    options_copy = *help_options;
  } else {
    bsc_help_options_init(&options_copy);
  }
  output = bsc_console_builtin_output(console, &output_copy);

  if (builtin == BSC_CONSOLE_BUILTIN_COMMANDS) {
    if (workspace->token_count != 1u) {
      return BSC_STATUS_EXTRA_ARGUMENT;
    }
    return bsc_help_render_commands(console->commands, console->command_count, &options_copy, output);
  }
  if (workspace->token_count == 1u) {
    return bsc_help_render_index(console->commands, console->command_count, &options_copy, output);
  }
  return bsc_help_render_path(console->commands,
                              console->command_count,
                              &workspace->tokens[1],
                              workspace->token_count - 1u,
                              &options_copy,
                              output);
}

/**
 * @brief Shared complete-line orchestration for application-only and built-in-aware APIs.
 *
 * This helper owns the one-copy, one-tokenization execution boundary. When
 * built-in result storage is supplied, it recognizes help/commands after
 * tokenization and before application matching; otherwise it runs the original
 * application path unchanged. It never cleans a workspace rejected before
 * activation, which protects an outer same-workspace execution from nested
 * misuse.
 */
static bsc_status_t bsc_console_execute_line_common(const bsc_console_t *console,
                                                    bsc_console_workspace_t *workspace,
                                                    bool builtins_enabled,
                                                    const bsc_help_options_t *help_options,
                                                    const char *line,
                                                    size_t line_length,
                                                    bsc_console_result_t *application_result,
                                                    bsc_console_builtins_result_t *builtins_result) {
  bsc_status_t status;

  if (console == NULL || workspace == NULL || !console->initialized || (line == NULL && line_length != 0u)) {
    bsc_console_set_phase(application_result, BSC_CONSOLE_PHASE_INPUT);
    bsc_console_builtins_set_phase(builtins_result, BSC_CONSOLE_PHASE_INPUT);
    return BSC_STATUS_INTERNAL_ERROR;
  }
  if (line_length > (size_t)BSC_MAX_LINE_LEN) {
    bsc_console_set_phase(application_result, BSC_CONSOLE_PHASE_INPUT);
    bsc_console_builtins_set_phase(builtins_result, BSC_CONSOLE_PHASE_INPUT);
    return BSC_STATUS_LINE_TOO_LONG;
  }
  if (workspace->execution_active) {
    bsc_console_set_phase(application_result, BSC_CONSOLE_PHASE_INPUT);
    bsc_console_builtins_set_phase(builtins_result, BSC_CONSOLE_PHASE_INPUT);
    return BSC_STATUS_INTERNAL_ERROR;
  }
  if (line_length != 0u && bsc_console_contains_embedded_nul(line, line_length)) {
    bsc_console_set_phase(application_result, BSC_CONSOLE_PHASE_INPUT);
    bsc_console_builtins_set_phase(builtins_result, BSC_CONSOLE_PHASE_INPUT);
    return BSC_STATUS_INVALID_SYNTAX;
  }

  workspace->execution_active = true;
  bsc_console_clear_metadata(workspace);

  if (line_length == 0u) {
    status = BSC_STATUS_NO_INPUT;
    bsc_console_set_phase(application_result, BSC_CONSOLE_PHASE_INPUT);
    bsc_console_builtins_set_phase(builtins_result, BSC_CONSOLE_PHASE_INPUT);
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
    bsc_console_set_phase(application_result, BSC_CONSOLE_PHASE_TOKENIZE);
    bsc_console_builtins_set_phase(builtins_result, BSC_CONSOLE_PHASE_TOKENIZE);
    goto cleanup;
  }

  if (builtins_enabled && bsc_console_token_equals_builtin(workspace->tokens[0], "help")) {
    status = bsc_console_execute_builtin_tokens(console,
                                                workspace,
                                                help_options,
                                                builtins_result,
                                                "help",
                                                workspace->token_count == 1u ? BSC_CONSOLE_BUILTIN_HELP_INDEX
                                                                            : BSC_CONSOLE_BUILTIN_HELP_PATH);
    goto cleanup;
  }
  if (builtins_enabled && bsc_console_token_equals_builtin(workspace->tokens[0], "commands")) {
    status = bsc_console_execute_builtin_tokens(console,
                                                workspace,
                                                help_options,
                                                builtins_result,
                                                "commands",
                                                BSC_CONSOLE_BUILTIN_COMMANDS);
    goto cleanup;
  }

  status = bsc_console_execute_application_tokens(console, workspace, application_result);
  bsc_console_builtins_copy_application_phase(builtins_result);

cleanup:
  bsc_console_cleanup_active_workspace(workspace);
  return status;
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

void bsc_console_builtins_result_clear(bsc_console_builtins_result_t *result) {
  if (result == NULL) {
    return;
  }
  result->phase = BSC_CONSOLE_PHASE_NONE;
  result->builtin = BSC_CONSOLE_BUILTIN_NONE;
  bsc_console_result_clear(&result->application_result);
  result->collision = NULL;
  result->collision_index = 0u;
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
  bsc_console_result_clear(result);
  return bsc_console_execute_line_common(console, workspace, false, NULL, line, line_length, result, NULL);
}

bsc_status_t bsc_execute_line_with_builtins(const bsc_console_t *console,
                                            bsc_console_workspace_t *workspace,
                                            const bsc_help_options_t *help_options,
                                            const char *line,
                                            size_t line_length,
                                            bsc_console_builtins_result_t *result) {
  bsc_console_builtins_result_clear(result);
  return bsc_console_execute_line_common(console,
                                         workspace,
                                         true,
                                         help_options,
                                         line,
                                         line_length,
                                         result != NULL ? &result->application_result : NULL,
                                         result);
}
