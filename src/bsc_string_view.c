#include "bsc_string_view.h"

#include <string.h>

/**
 * @brief Fold one ASCII uppercase byte to lowercase for view comparisons.
 *
 * Non-ASCII bytes and locale-specific casing are intentionally left unchanged.
 */
static char bsc_ascii_lower(char value) {
  if (value >= 'A' && value <= 'Z') {
    return (char)(value + ('a' - 'A'));
  }
  return value;
}

/**
 * @brief Build a borrowed string view from caller-owned data and length.
 *
 * The data pointer is not copied or owned, and it need not reference a
 * null-terminated buffer because later comparisons use the explicit length.
 */
bsc_string_view_t bsc_string_view_from_parts(const char *data, size_t length) {
  bsc_string_view_t view;
  view.data = data;
  view.length = length;
  return view;
}

/**
 * @brief Build a borrowed string view over a nullable C string.
 *
 * NULL becomes an empty view. Non-NULL text is measured with strlen, but the
 * view still borrows the caller-owned storage and does not retain ownership.
 */
bsc_string_view_t bsc_string_view_from_cstr(const char *text) {
  return bsc_string_view_from_parts(text, text == NULL ? 0u : strlen(text));
}

/**
 * @brief Return whether a borrowed view has zero length.
 */
bool bsc_string_view_is_empty(bsc_string_view_t view) {
  return view.length == 0u;
}

/**
 * @brief Compare a borrowed view to a nullable C string using explicit length.
 *
 * The view data is not treated as null-terminated. A NULL comparison string is
 * considered equal only to an empty view; a non-empty NULL view is not equal.
 */
bool bsc_string_view_equals_cstr(bsc_string_view_t view, const char *text) {
  if (text == NULL) {
    return view.length == 0u;
  }
  if (strlen(text) != view.length) {
    return false;
  }
  if (view.length == 0u) {
    return true;
  }
  if (view.data == NULL) {
    return false;
  }
  return memcmp(view.data, text, view.length) == 0;
}

/**
 * @brief Compare a borrowed view to a C string with ASCII-only case folding.
 *
 * The view data is compared by length and is not assumed to be null-terminated.
 * Case folding is limited to ASCII letters and leaves other bytes unchanged.
 */
bool bsc_string_view_equals_cstr_ignore_case(bsc_string_view_t view, const char *text) {
  size_t index;

  if (text == NULL) {
    return view.length == 0u;
  }
  if (strlen(text) != view.length) {
    return false;
  }
  if (view.length == 0u) {
    return true;
  }
  if (view.data == NULL) {
    return false;
  }

  for (index = 0u; index < view.length; ++index) {
    if (bsc_ascii_lower(view.data[index]) != bsc_ascii_lower(text[index])) {
      return false;
    }
  }
  return true;
}
