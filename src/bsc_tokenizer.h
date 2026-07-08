#ifndef BSC_TOKENIZER_H
#define BSC_TOKENIZER_H

#include <stddef.h>

#include "bsc_status.h"
#include "bsc_string_view.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file bsc_tokenizer.h
 * @brief In-place bounded tokenizer for caller/workspace-owned command lines.
 *
 * The tokenizer is a platform-independent C99 core helper. It owns no command
 * text storage, no token array, no heap allocation, and no hidden static
 * scratch. Callers provide the mutable line storage and token-view storage that
 * belong to the active command-execution workspace.
 */

/**
 * @brief Tokenize one bounded mutable command line in place.
 *
 * @param line Caller/workspace-owned mutable bytes to scan. Must not be NULL.
 *   The tokenizer may mutate this storage to remove quote delimiters and compact
 *   supported quoted escapes. The span is exactly @p line_length bytes and is
 *   not measured with strlen; a trailing C terminator, if present, is outside
 *   the scanned span unless included by @p line_length.
 * @param line_length Number of bytes to scan, excluding any C terminator if the
 *   caller stores one. Values greater than #BSC_MAX_LINE_LEN are rejected.
 * @param tokens Caller/workspace-owned output array of #bsc_string_view_t
 *   values. May be NULL only when @p token_capacity is zero.
 * @param token_capacity Number of token-view elements available in @p tokens.
 *   The accepted token count is bounded by the smaller of this capacity and
 *   #BSC_MAX_TOKENS.
 * @param token_count Caller-owned output count. Must not be NULL. The function
 *   sets `*token_count` to zero before validation failures when possible and to
 *   the number of produced tokens on success.
 *
 * @retval BSC_STATUS_OK Tokenization succeeded and @p token_count reports the
 *   number of views written to @p tokens.
 * @retval BSC_STATUS_NO_INPUT The supplied span was empty or contained only
 *   spaces and tabs.
 * @retval BSC_STATUS_LINE_TOO_LONG @p line_length exceeded #BSC_MAX_LINE_LEN.
 * @retval BSC_STATUS_TOKEN_TOO_LONG A semantic token exceeded
 *   #BSC_MAX_TOKEN_LEN bytes after any in-place quoted escape compaction.
 * @retval BSC_STATUS_TOO_MANY_TOKENS The input contained more tokens than
 *   `min(BSC_MAX_TOKENS, token_capacity)` permits.
 * @retval BSC_STATUS_UNTERMINATED_QUOTE The supplied span ended before a quoted
 *   token was closed.
 * @retval BSC_STATUS_INVALID_SYNTAX The line contained unsupported syntax, such
 *   as CR/LF bytes, unsupported quoted escapes, or adjacent quoted/bare text.
 * @retval BSC_STATUS_INTERNAL_ERROR A required pointer or capacity combination
 *   was invalid.
 *
 * Token views borrow bytes inside @p line and are not null-terminated unless the
 * original line storage happens to contain a terminator at that position. The
 * views remain valid only while @p line remains valid and unmodified. The
 * tokenizer copies no token text into tokenizer-owned storage; quoted tokens are
 * represented by compacting semantic content inside @p line itself.
 *
 * Only space and tab separate tokens. CR and LF are rejected instead of treated
 * as whitespace; future console/adapters are responsible for stripping line
 * endings before calling this function. Double quotes delimit quoted tokens from
 * idle state. Inside quotes, only `\"` and `\\` escapes are supported. Outside
 * quotes, backslash, single quote, punctuation, comment-looking bytes, and
 * non-ASCII bytes are ordinary token content unless they are one of the explicit
 * grammar bytes described above.
 *
 * The function uses caller-provided storage only and has no platform I/O. It is
 * reentrant for independent caller-owned buffers. Thread, task, or ISR safety is
 * determined by the caller's ownership and serialization of @p line, @p tokens,
 * and @p token_count.
 */
bsc_status_t bsc_tokenize_line(char *line,
                               size_t line_length,
                               bsc_string_view_t *tokens,
                               size_t token_capacity,
                               size_t *token_count);

#ifdef __cplusplus
}
#endif

#endif /* BSC_TOKENIZER_H */
