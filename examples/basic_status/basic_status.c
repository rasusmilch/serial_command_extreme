#include "basic_status.h"

#include <stdio.h>

#include "bsc_args.h"
#include "bsc_output.h"

/**
 * @brief Render the current application statistics without changing them.
 */
static bsc_status_t handle_status(void *app_context,
                                  const bsc_command_t *command,
                                  const bsc_parsed_args_t *args,
                                  bsc_output_t *output) {
  const basic_status_stats_t *stats = (const basic_status_stats_t *)app_context;
  char line[80];
  int length;
  (void)command;
  (void)args;

  if (stats == NULL || output == NULL) {
    return BSC_STATUS_INTERNAL_ERROR;
  }
  length = snprintf(line, sizeof(line), "commands_processed=%lu errors=%lu\n",
                    (unsigned long)stats->commands_processed, (unsigned long)stats->errors);
  if (length < 0 || (size_t)length >= sizeof(line)) {
    return BSC_STATUS_INTERNAL_ERROR;
  }
  return bsc_out_write_bytes(output, line, (size_t)length);
}

/**
 * @brief Clear all application statistics and report completion.
 */
static bsc_status_t handle_reset_stats(void *app_context,
                                       const bsc_command_t *command,
                                       const bsc_parsed_args_t *args,
                                       bsc_output_t *output) {
  basic_status_stats_t *stats = (basic_status_stats_t *)app_context;
  (void)command;
  (void)args;

  if (stats == NULL || output == NULL) {
    return BSC_STATUS_INTERNAL_ERROR;
  }
  stats->commands_processed = 0u;
  stats->errors = 0u;
  return bsc_out_writeln(output, "statistics reset");
}

static const char *const status_path[] = {"status"};
static const char *const reset_stats_path[] = {"reset_stats"};

static const bsc_command_t commands[] = {
    {status_path, 1u, BSC_NODE_COMMAND, NULL, 0u, handle_status, NULL, BSC_ACCESS_NORMAL,
     BSC_COMMAND_FLAG_NONE, NULL, "Show application statistics",
     "Reports command and error counters without changing application state."},
    {reset_stats_path, 1u, BSC_NODE_COMMAND, NULL, 0u, handle_reset_stats, NULL, BSC_ACCESS_NORMAL,
     BSC_COMMAND_FLAG_NONE, NULL, "Reset application statistics",
     "Clears both bounded application-owned counters."},
};

/** @copydoc basic_status_commands */
const bsc_command_t *basic_status_commands(size_t *command_count) {
  if (command_count == NULL) {
    return NULL;
  }
  *command_count = sizeof(commands) / sizeof(commands[0]);
  return commands;
}
