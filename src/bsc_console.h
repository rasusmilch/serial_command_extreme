#ifndef BSC_CONSOLE_H
#define BSC_CONSOLE_H

#include "bsc_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file bsc_console.h
 * @brief Minimal console context placeholder for the Phase 2A skeleton.
 *
 * This header intentionally does not provide command execution. Tokenization,
 * matching, argument parsing, dispatch, and help rendering are deferred to later
 * phases. The current type only establishes a documented place for caller-owned
 * application context without implying parser behavior exists.
 */

/**
 * @brief Minimal console context.
 *
 * The context borrows `app_context`; the core does not inspect, allocate, or
 * release it. No thread-safety or ISR-safety guarantee is made beyond ordinary
 * struct access in this skeleton.
 */
typedef struct bsc_console {
  /** Opaque caller-owned application context for future callbacks. */
  void *app_context;
} bsc_console_t;

/**
 * @brief Initialize a minimal console context.
 *
 * @param console Caller-owned console object to initialize.
 * @param app_context Opaque caller-owned pointer stored without copying.
 * @retval BSC_STATUS_OK The context was initialized.
 * @retval BSC_STATUS_INTERNAL_ERROR `console` was NULL.
 * @note This function does not configure commands or perform parser setup.
 */
bsc_status_t bsc_console_init(bsc_console_t *console, void *app_context);

#ifdef __cplusplus
}
#endif

#endif /* BSC_CONSOLE_H */
