#ifndef BSC_HELP_INTERNAL_H
#define BSC_HELP_INTERNAL_H

#include <stddef.h>

#include "bsc_help.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file bsc_help_internal.h
 * @brief Narrow shared internals for pure generated-help lookup policy.
 *
 * These declarations are private to the core library. They centralize help
 * option defaults, static descriptor visibility, and already-validated exact
 * path lookup so ordinary help and extended topic lookup cannot drift. The
 * helpers own no storage, allocate no heap, retain no pointers after return,
 * invoke no handlers or access callbacks, and do not use matcher, parser,
 * dispatcher, or console behavior.
 */

/**
 * @brief Return the default generated-help visibility options by value.
 *
 * @return Default options: advanced included, factory/locked/hidden excluded.
 * The returned value contains no borrowed pointers and is safe for synchronous
 * task/thread or host-test use. The helper performs no I/O and retains no
 * state.
 */
bsc_help_options_t bsc_help_internal_default_options(void);

/**
 * @brief Evaluate static generated-help visibility for one descriptor.
 *
 * @param command Required borrowed descriptor to inspect.
 * @param options Optional borrowed visibility options; NULL uses defaults.
 * @return Nonzero when the descriptor is visible under the supplied static
 * policy, zero when it is filtered.
 *
 * The helper reads only descriptor access metadata and flags. It does not call
 * execution access callbacks, handlers, matcher, parser, dispatcher, or console
 * code, and retains no pointers after return.
 */
int bsc_help_internal_is_visible(const bsc_command_t *command, const bsc_help_options_t *options);

/**
 * @brief Find an exact visible descriptor path after validation has completed.
 *
 * @param commands Required borrowed authoritative descriptor table.
 * @param command_count Number of descriptors in commands.
 * @param path_tokens Required explicit-length path token array.
 * @param path_token_count Number of path tokens; zero returns #BSC_STATUS_NO_INPUT.
 * @param options Optional borrowed visibility options; NULL uses defaults.
 * @param result Required caller-owned result storage, cleared on entry and on failure.
 * @retval BSC_STATUS_OK One exact visible descriptor matched; result borrows it.
 * @retval BSC_STATUS_NO_INPUT No path tokens were supplied.
 * @retval BSC_STATUS_UNKNOWN_COMMAND No exact visible descriptor matched.
 * @retval BSC_STATUS_AMBIGUOUS_COMMAND More than one visible descriptor matched defensively.
 * @retval BSC_STATUS_INTERNAL_ERROR Required API pointers were invalid.
 *
 * This helper deliberately does not validate the registry or help metadata; its
 * callers must complete validation first. It performs exact path-length matching
 * with ASCII case-insensitive token comparison, filters through
 * #bsc_help_internal_is_visible, owns no storage, allocates no heap, retains no
 * pointers internally after return, and never invokes handlers or access
 * callbacks. It is intended for synchronous task/thread or host-test use, not
 * ISR use, because callers typically run bounded validation before calling it.
 */
bsc_status_t bsc_help_internal_find_path_validated(const bsc_command_t *commands,
                                                   size_t command_count,
                                                   const bsc_string_view_t *path_tokens,
                                                   size_t path_token_count,
                                                   const bsc_help_options_t *options,
                                                   bsc_help_lookup_result_t *result);

#ifdef __cplusplus
}
#endif

#endif /* BSC_HELP_INTERNAL_H */
