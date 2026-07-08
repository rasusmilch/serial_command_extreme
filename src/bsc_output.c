#include "bsc_output.h"

#include <string.h>

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

bsc_status_t bsc_out_write(bsc_output_t *out, const char *text) {
  if (text == NULL) {
    return bsc_out_write_bytes(out, "", 0u);
  }
  return bsc_out_write_bytes(out, text, strlen(text));
}

bsc_status_t bsc_out_writeln(bsc_output_t *out, const char *text) {
  bsc_status_t status;

  status = bsc_out_write(out, text);
  if (status != BSC_STATUS_OK) {
    return status;
  }
  return bsc_out_write_bytes(out, "\n", 1u);
}
