#include <stdio.h>
#include <string.h>

#include "basic_status.h"
#include "bsc_console.h"

/** @brief Fixed caller-owned capture buffer for example integration output. */
typedef struct capture {
  char bytes[1024];
  size_t used;
} capture_t;

/** @brief Capture output without allocation, truncating when the fixed buffer fills. */
static size_t capture_write(void *user, const char *data, size_t length) {
  capture_t *capture = (capture_t *)user;
  size_t available = sizeof(capture->bytes) - capture->used;
  size_t count = length < available ? length : available;
  if (count > 0u) {
    memcpy(capture->bytes + capture->used, data, count);
    capture->used += count;
  }
  return count;
}

/** @brief Return whether captured bytes contain a public deterministic fragment. */
static int capture_contains(const capture_t *capture, const char *fragment) {
  size_t length = strlen(fragment);
  size_t index;
  if (length > capture->used) {
    return 0;
  }
  for (index = 0u; index + length <= capture->used; ++index) {
    if (memcmp(capture->bytes + index, fragment, length) == 0) {
      return 1;
    }
  }
  return 0;
}

/** @brief Clear only the bounded output fixture between complete-line executions. */
static void capture_clear(capture_t *capture) {
  memset(capture, 0, sizeof(*capture));
}

/** @brief Fail the integration test with its source location. */
#define CHECK(condition)                                                                           \
  do {                                                                                             \
    if (!(condition)) {                                                                            \
      fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition);                         \
      return 1;                                                                                    \
    }                                                                                              \
  } while (0)

/**
 * @brief Exercise the real example registry exclusively through complete-line public execution.
 *
 * The fixed buffers and application struct also guard the example's allocation-free ownership
 * model. A runtime-sensitive sentinel is kept adjacent to the statistics and must never appear
 * in output or result metadata.
 */
int main(void) {
  struct application_fixture {
    basic_status_stats_t stats;
    char runtime_secret[24];
  } app = {{7u, 2u}, "runtime-secret-sentinel"};
  capture_t capture;
  bsc_output_t output = {capture_write, &capture};
  bsc_console_t console;
  bsc_console_workspace_t workspace;
  bsc_console_builtins_result_t result;
  bsc_console_config_t config;
  const bsc_command_t *commands;
  size_t command_count;

  capture_clear(&capture);
  commands = basic_status_commands(&command_count);
  CHECK(commands != NULL && command_count == 2u);
  config.commands = commands;
  config.command_count = command_count;
  config.help_catalog = NULL;
  config.app_context = &app.stats;
  config.output = &output;
  CHECK(bsc_console_init(&console, &config, NULL) == BSC_STATUS_OK);
  bsc_console_workspace_init(&workspace);

  CHECK(bsc_execute_line_with_builtins(&console, &workspace, NULL, "status", 6u, &result) == BSC_STATUS_OK);
  CHECK(result.application_result.command == &commands[0]);
  CHECK(capture_contains(&capture, "commands_processed=7 errors=2\n"));
  CHECK(!capture_contains(&capture, app.runtime_secret));
  CHECK(app.stats.commands_processed == 7u && app.stats.errors == 2u);

  capture_clear(&capture);
  CHECK(bsc_execute_line_with_builtins(&console, &workspace, NULL, "reset_stats", 11u, &result) == BSC_STATUS_OK);
  CHECK(result.application_result.command == &commands[1]);
  CHECK(app.stats.commands_processed == 0u && app.stats.errors == 0u);
  CHECK(capture_contains(&capture, "statistics reset\n"));
  CHECK(!capture_contains(&capture, app.runtime_secret));

  capture_clear(&capture);
  CHECK(bsc_execute_line_with_builtins(&console, &workspace, NULL, "status", 6u, &result) == BSC_STATUS_OK);
  CHECK(capture_contains(&capture, "commands_processed=0 errors=0\n"));
  CHECK(!capture_contains(&capture, app.runtime_secret));

  capture_clear(&capture);
  CHECK(bsc_execute_line_with_builtins(&console, &workspace, NULL, "help status", 11u, &result) == BSC_STATUS_OK);
  CHECK(result.builtin == BSC_CONSOLE_BUILTIN_HELP_PATH);
  CHECK(capture_contains(&capture, "Show application statistics"));
  CHECK(capture_contains(&capture, "Reports command and error counters"));
  CHECK(!capture_contains(&capture, app.runtime_secret));

  capture_clear(&capture);
  CHECK(bsc_execute_line_with_builtins(&console, &workspace, NULL, "unknown", 7u, &result) ==
        BSC_STATUS_UNKNOWN_COMMAND);
  CHECK(result.application_result.command == NULL);
  CHECK(capture.used == 0u);
  CHECK(app.stats.commands_processed == 0u && app.stats.errors == 0u);
  CHECK(!capture_contains(&capture, app.runtime_secret));
  puts("PASS: basic_status complete-line integration");
  return 0;
}
