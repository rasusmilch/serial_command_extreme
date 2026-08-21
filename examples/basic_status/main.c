#include <stdio.h>
#include <string.h>

#include "basic_status.h"
#include "bsc_console.h"

/** @brief Write example output synchronously to stdout. */
static size_t stdout_write(void *user, const char *data, size_t length) {
  (void)user;
  return fwrite(data, 1u, length, stdout);
}

/** @brief Execute the deterministic host demonstration sequence. */
int main(void) {
  basic_status_stats_t stats = {7u, 2u};
  bsc_console_t console;
  bsc_console_workspace_t workspace;
  bsc_console_builtins_result_t result;
  bsc_output_t output = {stdout_write, NULL};
  bsc_console_config_t config;
  const char *const lines[] = {"status", "help status", "reset_stats", "status"};
  size_t command_count;
  size_t index;

  config.commands = basic_status_commands(&command_count);
  config.command_count = command_count;
  config.help_catalog = NULL;
  config.app_context = &stats;
  config.output = &output;
  if (bsc_console_init(&console, &config, NULL) != BSC_STATUS_OK) {
    return 1;
  }
  bsc_console_workspace_init(&workspace);
  for (index = 0u; index < sizeof(lines) / sizeof(lines[0]); ++index) {
    printf("> %s\n", lines[index]);
    if (bsc_execute_line_with_builtins(&console, &workspace, NULL, lines[index], strlen(lines[index]), &result) !=
        BSC_STATUS_OK) {
      return 1;
    }
  }
  return 0;
}
