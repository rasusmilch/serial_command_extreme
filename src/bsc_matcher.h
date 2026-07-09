#ifndef BSC_MATCHER_H
#define BSC_MATCHER_H

#include <stddef.h>

#include "bsc_status.h"
#include "bsc_string_view.h"
#include "bsc_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file bsc_matcher.h
 * @brief Static command path matcher for tokenizer-produced token views.
 *
 * The matcher is the bounded path-selection stage between tokenization and
 * future argument parsing/dispatch. It consumes a caller-owned descriptor table
 * and a caller/workspace-owned array of borrowed token views, then reports the
 * longest matching executable command path or an exact group path that requires
 * a subcommand.
 *
 * Command descriptor tables, descriptor path arrays, and descriptor path strings
 * are owned by the application or static descriptor provider. They must remain
 * valid while #bsc_match_command runs and while the caller uses any command or
 * group pointer stored in #bsc_match_result_t. Token arrays are owned by the
 * caller or command workspace, and token view bytes are usually borrowed from
 * the active mutable line buffer produced by #bsc_tokenize_line. Token views are
 * not required to be null-terminated.
 *
 * This module performs no allocation, owns no storage, copies no token or
 * command text, mutates no token data, mutates no descriptors, performs no
 * platform I/O, parses no arguments, dispatches no handlers, invokes no access
 * callbacks, renders no help, performs no runtime registration, and retains no
 * hidden pointers after return. The only retained pointers are caller-visible
 * borrowed pointers written to #bsc_match_result_t.
 *
 * Normal callers should run #bsc_registry_validate before matching. For direct
 * or unvalidated use, the matcher still rejects obvious malformed path/node
 * descriptor fields needed for safe matching: NULL path arrays, zero path
 * lengths, path lengths above #BSC_MAX_PATH_TOKENS, NULL active path tokens, and
 * unsupported node types. It intentionally does not validate argument metadata,
 * handlers, access levels, flags, help strings, or command contexts.
 *
 * The functions are synchronous, perform no I/O, and allocate no memory. They
 * are reentrant for independent immutable descriptor/token inputs. ISR
 * suitability depends on the caller's tolerance for bounded linear scans over
 * the supplied descriptor table.
 */

/**
 * @brief Result of matching token views against a static command table.
 *
 * On #BSC_STATUS_OK, `command` and `command_index` identify the executable
 * command, `consumed_token_count` is the matched path length,
 * `remaining_token_index` equals `consumed_token_count`, and
 * `remaining_token_count` is the number of tokens left for future argument
 * parsing. `group` remains NULL for the MVP.
 *
 * On #BSC_STATUS_GROUP_REQUIRES_SUBCOMMAND, `group` and `group_index` identify
 * the exact matched group descriptor, `command` is NULL,
 * `consumed_token_count` is the group path length, `remaining_token_index`
 * equals `consumed_token_count`, and `remaining_token_count` is zero.
 *
 * On #BSC_STATUS_NO_INPUT, #BSC_STATUS_UNKNOWN_COMMAND,
 * #BSC_STATUS_AMBIGUOUS_COMMAND, #BSC_STATUS_INVALID_DESCRIPTOR, or
 * #BSC_STATUS_INTERNAL_ERROR, the result remains cleared.
 */
typedef struct bsc_match_result {
  /** Borrowed matched executable command descriptor, or NULL. */
  const bsc_command_t *command;
  /** Index of `command` in the caller-owned descriptor table. */
  size_t command_index;
  /** Number of leading input tokens consumed as command path tokens. */
  size_t consumed_token_count;
  /** Index of the first remaining token for future argument parsing. */
  size_t remaining_token_index;
  /** Number of remaining tokens for future argument parsing. */
  size_t remaining_token_count;
  /** Borrowed exact matched group descriptor for group-required status, or NULL. */
  const bsc_command_t *group;
  /** Index of `group` in the caller-owned descriptor table. */
  size_t group_index;
} bsc_match_result_t;

/**
 * @brief Clear a matcher result structure.
 *
 * @param result Caller-owned result to clear. NULL is accepted and ignored.
 *
 * All pointers are set to NULL and all indexes/counts are set to zero. The
 * function owns no storage and releases nothing.
 */
void bsc_match_result_clear(bsc_match_result_t *result);

/**
 * @brief Match tokenizer-produced tokens against a static command table.
 *
 * @param commands Caller-owned descriptor table. May be NULL only when
 *   `command_count` is zero. Descriptors are expected to have been validated by
 *   #bsc_registry_validate before normal use.
 * @param command_count Number of entries in `commands`; must not exceed
 *   #BSC_MAX_COMMANDS.
 * @param tokens Caller/workspace-owned token view array. May be NULL only when
 *   `token_count` is zero. Token bytes are borrowed and need not be
 *   null-terminated.
 * @param token_count Number of entries in `tokens`; must not exceed
 *   #BSC_MAX_TOKENS.
 * @param result Optional caller-owned output result. When non-NULL, it is
 *   cleared before matching and populated only for documented success/group
 *   statuses.
 *
 * @retval BSC_STATUS_OK A longest executable command path matched. Extra input
 *   tokens are reported as remaining tokens for future argument parsing.
 * @retval BSC_STATUS_NO_INPUT `token_count` was zero.
 * @retval BSC_STATUS_UNKNOWN_COMMAND No executable path matched and the input
 *   did not exactly match a group descriptor path.
 * @retval BSC_STATUS_GROUP_REQUIRES_SUBCOMMAND The input exactly matched a
 *   group descriptor path and no executable command path matched.
 * @retval BSC_STATUS_AMBIGUOUS_COMMAND Unvalidated/inconsistent descriptors
 *   produced same-length conflicting matches, including duplicate executable
 *   paths or a group/command same-path conflict under ASCII case folding.
 * @retval BSC_STATUS_INVALID_DESCRIPTOR A path/node descriptor field required
 *   for safe matching was malformed.
 * @retval BSC_STATUS_INTERNAL_ERROR The API was called with invalid pointers or
 *   counts outside configured bounds.
 */
bsc_status_t bsc_match_command(const bsc_command_t *commands,
                               size_t command_count,
                               const bsc_string_view_t *tokens,
                               size_t token_count,
                               bsc_match_result_t *result);

#ifdef __cplusplus
}
#endif

#endif /* BSC_MATCHER_H */
