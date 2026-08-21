#ifndef SCE_EXAMPLE_BASIC_STATUS_H
#define SCE_EXAMPLE_BASIC_STATUS_H

#include <stddef.h>
#include <stdint.h>

#include "bsc_types.h"

/**
 * @file basic_status.h
 * @brief Application-owned state and static registry for the basic host example.
 */

/** @brief Small bounded set of statistics owned by the example application. */
typedef struct basic_status_stats {
  /** Number of commands observed by the example application. */
  uint32_t commands_processed;
  /** Number of application errors observed by the example application. */
  uint32_t errors;
} basic_status_stats_t;

/**
 * @brief Return the immutable descriptor registry used by the example.
 *
 * @param command_count Required destination for the descriptor count.
 * @return Static registry pointer, or NULL when @p command_count is NULL.
 *
 * The returned storage has program lifetime and must not be modified or freed.
 */
const bsc_command_t *basic_status_commands(size_t *command_count);

#endif
