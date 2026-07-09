#include "bsc_console.h"

#include <stddef.h>

/**
 * @brief Initialize the current minimal console context storage.
 *
 * Stores the caller-provided application context pointer without taking
 * ownership of it. This early console initializer does not execute commands or
 * configure registries; it only validates the console pointer and returns
 * BSC_STATUS_INTERNAL_ERROR when that pointer is NULL.
 */
bsc_status_t bsc_console_init(bsc_console_t *console, void *app_context) {
  if (console == NULL) {
    return BSC_STATUS_INTERNAL_ERROR;
  }
  console->app_context = app_context;
  return BSC_STATUS_OK;
}
