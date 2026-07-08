#include "bsc_string_view.h"

#include <string.h>

static char bsc_ascii_lower(char value) {
  if (value >= 'A' && value <= 'Z') {
    return (char)(value + ('a' - 'A'));
  }
  return value;
}

bsc_string_view_t bsc_string_view_from_parts(const char *data, size_t length) {
  bsc_string_view_t view;
  view.data = data;
  view.length = length;
  return view;
}

bsc_string_view_t bsc_string_view_from_cstr(const char *text) {
  return bsc_string_view_from_parts(text, text == NULL ? 0u : strlen(text));
}

bool bsc_string_view_is_empty(bsc_string_view_t view) {
  return view.length == 0u;
}

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
