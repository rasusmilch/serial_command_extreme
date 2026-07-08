#ifndef BSC_OUTPUT_H
#define BSC_OUTPUT_H

#include <stddef.h>

#include "bsc_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file bsc_output.h
 * @brief Platform-independent output callback helpers.
 *
 * The core writes through caller-provided callbacks. It does not own UART,
 * Serial, stdout, file descriptors, RTOS objects, or sink buffers. These helpers
 * are synchronous and perform no heap allocation. Callback blocking behavior and
 * ISR safety are properties of the caller-provided sink.
 */

/**
 * @brief Write bytes to an output sink.
 *
 * @param user Caller-owned sink context.
 * @param data Pointer to bytes to write. The pointer is borrowed only for the
 *   duration of the call and is not required to remain valid afterwards.
 * @param length Number of bytes requested.
 * @return Number of bytes accepted by the sink. Returning less than length lets
 *   bsc_out_write report #BSC_STATUS_OUTPUT_TRUNCATED.
 */
typedef size_t (*bsc_write_fn_t)(void *user, const char *data, size_t length);

/**
 * @brief Callback output target.
 *
 * `user` is opaque caller-owned state. `write` must remain valid while the
 * output object is used. The structure does not own or manage the sink lifetime.
 */
typedef struct bsc_output {
  /** Caller-provided write callback, or NULL for an invalid output target. */
  bsc_write_fn_t write;
  /** Caller-owned context passed to `write`. */
  void *user;
} bsc_output_t;

/**
 * @brief Write a null-terminated string to an output target.
 *
 * @param out Output target. It borrows the callback and user context.
 * @param text Null-terminated text to write. NULL is treated as an empty write.
 * @retval BSC_STATUS_OK All requested bytes were accepted.
 * @retval BSC_STATUS_OUTPUT_TRUNCATED The sink accepted only part of the text.
 * @retval BSC_STATUS_INTERNAL_ERROR The output target was invalid.
 * @note This helper does not redact data; callers must avoid passing secrets.
 */
bsc_status_t bsc_out_write(bsc_output_t *out, const char *text);

/**
 * @brief Write a null-terminated string followed by a line feed.
 *
 * @param out Output target. It borrows the callback and user context.
 * @param text Null-terminated text to write before `\n`. NULL is treated as an
 *   empty line.
 * @retval BSC_STATUS_OK Text and newline were accepted.
 * @retval BSC_STATUS_OUTPUT_TRUNCATED The sink accepted only part of the output.
 * @retval BSC_STATUS_INTERNAL_ERROR The output target was invalid.
 * @note The helper emits `\n` only; platform adapters may translate line endings.
 */
bsc_status_t bsc_out_writeln(bsc_output_t *out, const char *text);

#ifdef __cplusplus
}
#endif

#endif /* BSC_OUTPUT_H */
