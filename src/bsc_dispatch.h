#ifndef BSC_DISPATCH_H
#define BSC_DISPATCH_H

#include <stddef.h>

#include "bsc_args.h"
#include "bsc_output.h"
#include "bsc_status.h"
#include "bsc_string_view.h"
#include "bsc_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file bsc_dispatch.h
 * @brief Selected-command access enforcement, typed parsing, and handler dispatch.
 *
 * This module is the bounded dispatch stage after command matching and before
 * future high-level console orchestration. It consumes one already-selected
 * executable descriptor plus the caller-supplied remaining positional token
 * slice, evaluates the command access policy, invokes the current typed
 * positional parser, and then calls the command handler exactly once on success.
 * It does not tokenize raw input, search registries, interpret matcher results,
 * own line/token/parsed storage, render help, perform authentication, format
 * status text, or write automatic output.
 */

/**
 * @brief Dispatch an already-selected executable command descriptor.
 *
 * @param app_context Optional caller-owned application context. It is passed
 *   unchanged to the access callback and handler and is never inspected,
 *   retained, allocated, or released by dispatch.
 * @param command Required borrowed executable command descriptor selected by a
 *   caller or by #bsc_match_command. Normal use supplies a descriptor table that
 *   passed #bsc_registry_validate; dispatch performs only narrow shallow checks
 *   needed before access and parsing.
 * @param arg_tokens Borrowed positional token slice for the selected command.
 *   May be NULL only when @p arg_token_count is zero. Token views are
 *   explicit-length and need not be NUL-terminated.
 * @param arg_token_count Number of entries in @p arg_tokens. This count is
 *   passed unchanged to #bsc_parse_command_args after access succeeds, including
 *   counts larger than #BSC_MAX_ARGS so the parser can report ordinary extra
 *   operator arguments.
 * @param parsed_args Required caller-owned parsed-argument storage. It is
 *   cleared at entry whenever non-NULL. On successful parsing it is passed to
 *   the handler; after handler failure it remains available for caller
 *   inspection. On access denial, dispatch-level failures, or parser failures it
 *   contains no parsed values or stale string/secret views.
 * @param parse_error Required caller-owned parse diagnostic storage. It is
 *   cleared at entry whenever non-NULL. Dispatch-level failures and access
 *   denial leave it at #BSC_ARG_PARSE_ERROR_NONE. Parser-origin failures preserve
 *   the structured parser diagnostic. Handler results leave it at NONE because
 *   parsing succeeded.
 * @param output Optional caller-owned output target passed unchanged to the
 *   handler, including NULL. Dispatch itself never validates, writes through, or
 *   retains the output target.
 * @retval BSC_STATUS_OK Access, parsing, and handler execution succeeded.
 * @retval BSC_STATUS_INTERNAL_ERROR Dispatch API inputs were invalid.
 * @retval BSC_STATUS_INVALID_DESCRIPTOR Shallow executable descriptor checks or
 *   parser descriptor checks failed.
 * @retval BSC_STATUS_ACCESS_DENIED The default access matrix or callback denied
 *   execution before parsing.
 * @retval BSC_STATUS_MISSING_ARGUMENT Parser reported a missing positional
 *   argument.
 * @retval BSC_STATUS_EXTRA_ARGUMENT Parser reported unexpected positional
 *   arguments.
 * @retval BSC_STATUS_INVALID_ARGUMENT_TYPE Parser reported invalid argument
 *   syntax or type.
 * @retval BSC_STATUS_ARGUMENT_OUT_OF_RANGE Parser reported a numeric range
 *   failure.
 * @retval BSC_STATUS_ARGUMENT_TOO_LONG Parser reported text above its maximum
 *   length.
 * @retval BSC_STATUS_ARGUMENT_TOO_SHORT Parser reported text below its minimum
 *   length.
 * @retval BSC_STATUS_INVALID_ENUM_VALUE Parser reported an unknown enum choice.
 * @retval BSC_STATUS_OUTPUT_TRUNCATED A handler propagated output truncation.
 * @retval BSC_STATUS_APP_ERROR A handler returned application failure or an
 *   unrecognized status value.
 *
 * Access policy: without an access callback, NORMAL and ADVANCED commands are
 * allowed while FACTORY and LOCKED commands are denied. When a callback is
 * present, its true/false result is authoritative for every valid access level;
 * it is invoked exactly once after dispatch API and shallow descriptor checks
 * and before argument parsing. The callback receives app_context, the exact
 * command pointer, and command->access. The hidden flag has no independent
 * exact-dispatch denial effect and remains future help/listing metadata.
 *
 * Handler policy: the handler is invoked synchronously and exactly once only
 * after valid API inputs, a dispatchable descriptor, allowed access, and
 * successful typed parsing. It receives app_context, the exact command pointer,
 * parsed_args, and output unchanged. command->command_context remains available
 * through the descriptor. Every currently defined bsc_status_t returned by the
 * handler is propagated unchanged; any unrecognized value is mapped to
 * #BSC_STATUS_APP_ERROR.
 *
 * Ownership and lifetime: command descriptors, nested metadata, token arrays,
 * token bytes, parsed_args, parse_error, app_context, command_context, and
 * output are caller-owned. Parsed string and secret values are borrowed views
 * into active token storage and are guaranteed useful only for the synchronous
 * handler call and active dispatch operation; handlers needing persistent text
 * or secrets must copy them into bounded application-owned storage before
 * returning. Dispatch retains no pointers after return, allocates no heap
 * memory, creates no token/descriptor copies, and uses no hidden mutable static
 * workspace.
 *
 * Reentrancy and context: dispatch is reentrant for independent parsed-result,
 * diagnostic, token, mutable application-state, and output storage; immutable
 * descriptor tables may be shared. Synchronization of shared callback state is
 * the application's responsibility. Dispatch invokes arbitrary application
 * callbacks, so ISR suitability is guaranteed only when all supplied callbacks,
 * state, and output use are ISR-safe and the caller accepts the bounded parser
 * work.
 */
bsc_status_t bsc_dispatch_command(void *app_context,
                                  const bsc_command_t *command,
                                  const bsc_string_view_t *arg_tokens,
                                  size_t arg_token_count,
                                  bsc_parsed_args_t *parsed_args,
                                  bsc_arg_parse_error_t *parse_error,
                                  bsc_output_t *output);

#ifdef __cplusplus
}
#endif

#endif /* BSC_DISPATCH_H */
