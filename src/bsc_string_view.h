#ifndef BSC_STRING_VIEW_H
#define BSC_STRING_VIEW_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file bsc_string_view.h
 * @brief Borrowed immutable string slices for bounded parser code.
 *
 * A string view stores a pointer and byte length without copying. View data is
 * not required to be null-terminated. The caller owns the pointed-to storage and
 * must keep it valid for the duration documented by each API that returns or
 * consumes a view.
 */

/**
 * @brief Immutable borrowed byte string.
 *
 * `data` may be NULL only when `length` is zero. The view never owns storage and
 * does not imply any allocation, deallocation, or platform I/O behavior.
 */
typedef struct bsc_string_view {
  /** Pointer to the first byte of the borrowed slice. */
  const char *data;
  /** Number of bytes in the slice; no terminator is required or counted. */
  size_t length;
} bsc_string_view_t;

/**
 * @brief Construct a view from an explicit pointer and length.
 *
 * @param data Pointer to caller-owned bytes. May be NULL only when length is 0.
 * @param length Number of bytes to expose through the view.
 * @return A borrowed view. The caller retains ownership of the bytes.
 */
bsc_string_view_t bsc_string_view_from_parts(const char *data, size_t length);

/**
 * @brief Construct a view over a null-terminated C string.
 *
 * @param text Null-terminated string, or NULL to create an empty view.
 * @return A borrowed view whose length is measured with strlen when text is not
 *   NULL. The returned view does not own `text`.
 */
bsc_string_view_t bsc_string_view_from_cstr(const char *text);

/**
 * @brief Return whether a view has zero length.
 *
 * @param view View to inspect.
 * @return true when view.length is zero, false otherwise.
 */
bool bsc_string_view_is_empty(bsc_string_view_t view);

/**
 * @brief Compare a view exactly with a null-terminated C string.
 *
 * @param view Borrowed view; it does not need a terminator.
 * @param text Null-terminated string to compare with. NULL matches only an
 *   empty view.
 * @return true when byte length and byte contents match exactly.
 */
bool bsc_string_view_equals_cstr(bsc_string_view_t view, const char *text);

/**
 * @brief Compare a view with a C string using ASCII case folding.
 *
 * @param view Borrowed view; it does not need a terminator.
 * @param text Null-terminated string to compare with. NULL matches only an
 *   empty view.
 * @return true when lengths match and all ASCII letters compare equal ignoring
 *   case. Non-ASCII bytes are compared unchanged.
 * @note This function avoids locale-dependent behavior so host and embedded
 *   builds remain deterministic.
 */
bool bsc_string_view_equals_cstr_ignore_case(bsc_string_view_t view, const char *text);

#ifdef __cplusplus
}
#endif

#endif /* BSC_STRING_VIEW_H */
