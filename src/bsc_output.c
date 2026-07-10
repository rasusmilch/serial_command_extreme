#include "bsc_output.h"

#include <string.h>

/**
 * @brief Adapt bounded byte writes to the configured output callback.
 *
 * Validates that the output sink and callback are present, treats a zero-length
 * write as a successful no-op, and maps callback short writes to
 * BSC_STATUS_OUTPUT_TRUNCATED. The data buffer is caller-owned and is not
 * retained after the callback returns.
 */
static bsc_status_t bsc_out_write_bytes(bsc_output_t *out, const char *data, size_t length) {
  size_t written;

  if (out == NULL || out->write == NULL) {
    return BSC_STATUS_INTERNAL_ERROR;
  }
  if (length == 0u) {
    return BSC_STATUS_OK;
  }
  written = out->write(out->user, data, length);
  return written == length ? BSC_STATUS_OK : BSC_STATUS_OUTPUT_TRUNCATED;
}

/**
 * @brief Write a nullable C string through the shared byte-write adapter.
 *
 * NULL text is treated as an empty write. Non-NULL text is measured with
 * strlen before forwarding so the callback receives an explicit byte count.
 */
bsc_status_t bsc_out_write(bsc_output_t *out, const char *text) {
  if (text == NULL) {
    return bsc_out_write_bytes(out, "", 0u);
  }
  return bsc_out_write_bytes(out, text, strlen(text));
}

/**
 * @brief Write text followed by one newline byte.
 *
 * The text write is attempted first. If it fails, that first status is
 * returned and the newline is not written; otherwise a single "\n" byte is
 * sent through the same bounded output adapter.
 */
bsc_status_t bsc_out_writeln(bsc_output_t *out, const char *text) {
  bsc_status_t status;

  status = bsc_out_write(out, text);
  if (status != BSC_STATUS_OK) {
    return status;
  }
  return bsc_out_write_bytes(out, "\n", 1u);
}
