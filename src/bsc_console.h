#ifndef BSC_CONSOLE_H
#define BSC_CONSOLE_H

#include <stdbool.h>
#include <stddef.h>

#include "bsc_args.h"
#include "bsc_config.h"
#include "bsc_matcher.h"
#include "bsc_output.h"
#include "bsc_registry.h"
#include "bsc_status.h"
#include "bsc_string_view.h"
#include "bsc_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file bsc_console.h
 * @brief Output-neutral complete-line orchestration for the bounded C core.
 *
 * The console layer binds an immutable command registry, caller application
 * context, and an optional copied output descriptor, then executes one explicit
 * byte span through the existing tokenizer, matcher, and selected-command
 * dispatcher. Execution storage is supplied by the caller as
 * #bsc_console_workspace_t so large bounded arrays are never hidden inside the
 * console object, allocated on the heap, or created as core stack scratch.
 *
 * The complete-line API performs exactly one intentional command-text copy from
 * the submitted const byte span into the mutable workspace line buffer. Token
 * views and parsed string/secret values borrow that workspace buffer and are
 * valid only during the active synchronous execution. The console writes no
 * automatic echo, errors, help, OK, or ERR text; handlers receive the configured
 * output target through dispatch when one was configured.
 */

/**
 * @brief Borrowed configuration used only while initializing a console.
 *
 * `commands` and every descriptor subobject remain caller/static-owned and must
 * stay immutable and alive for the initialized console lifetime. `app_context`
 * is borrowed unchanged and may be NULL. `output` is borrowed only during
 * #bsc_console_init; when non-NULL, the complete #bsc_output_t wrapper is copied
 * into the console while its callback and user pointer remain caller-owned.
 */
typedef struct bsc_console_config {
  /** Borrowed command descriptor table validated during initialization. */
  const bsc_command_t *commands;
  /** Number of command descriptors in `commands`; must pass registry validation. */
  size_t command_count;
  /** Opaque caller-owned application context forwarded to access callbacks and handlers. */
  void *app_context;
  /** Optional borrowed output wrapper copied by value during initialization. */
  const bsc_output_t *output;
} bsc_console_config_t;

/**
 * @brief Lightweight immutable console configuration after successful initialization.
 *
 * The core mutates this object only in #bsc_console_init. #bsc_execute_line
 * accepts a const pointer and does not modify the console during execution. The
 * console owns no descriptor, application, output-sink, or workspace storage;
 * applications must serialize shared mutable app/output state themselves.
 */
typedef struct bsc_console {
  /** Borrowed immutable command descriptor table. */
  const bsc_command_t *commands;
  /** Number of descriptors in `commands`. */
  size_t command_count;
  /** Borrowed application context forwarded unchanged, including NULL. */
  void *app_context;
  /** By-value copy of the optional output wrapper; callback/user remain borrowed. */
  bsc_output_t output;
  /** True when `output` contains a configured wrapper to pass to handlers. */
  bool has_output;
  /** True only after registry validation succeeds. */
  bool initialized;
} bsc_console_t;

/**
 * @brief Caller-owned bounded storage for one active complete-line execution.
 *
 * Applications may place this complete type statically, globally, inside a
 * persistent adapter/application object, or in explicitly sized task-owned
 * storage. It must be initialized before first use; static zero initialization
 * is a valid initial state, but #bsc_console_workspace_init is the recommended
 * normal practice. The active flag rejects recursive reuse of the same
 * workspace, but it is not atomic, not a mutex, and not task synchronization.
 */
typedef struct bsc_console_workspace {
  /** Mutable line storage; byte BSC_MAX_LINE_LEN is reserved for a debug terminator. */
  char line_buffer[BSC_MAX_LINE_LEN + 1u];
  /** Token views borrowed from `line_buffer` during active execution only. */
  bsc_string_view_t tokens[BSC_MAX_TOKENS];
  /** Number of active token views during execution; zero after cleanup. */
  size_t token_count;
  /** Transient matcher result cleared after every execution. */
  bsc_match_result_t match_result;
  /** Transient parsed arguments cleared after every execution. */
  bsc_parsed_args_t parsed_args;
  /** Transient parse diagnostic cleared after every execution. */
  bsc_arg_parse_error_t parse_error;
  /** Non-atomic guard for ordinary same-workspace recursive misuse. */
  bool execution_active;
} bsc_console_workspace_t;

/** @brief Coarse phase that produced the primary #bsc_execute_line status. */
typedef enum bsc_console_phase {
  /** No execution result has been recorded. */
  BSC_CONSOLE_PHASE_NONE = 0,
  /** Console, workspace, or input validation failed before tokenization. */
  BSC_CONSOLE_PHASE_INPUT,
  /** The tokenizer returned the primary status. */
  BSC_CONSOLE_PHASE_TOKENIZE,
  /** The matcher returned the primary status. */
  BSC_CONSOLE_PHASE_MATCH,
  /** The selected-command dispatcher was called and returned the primary status. */
  BSC_CONSOLE_PHASE_DISPATCH
} bsc_console_phase_t;

/**
 * @brief Optional non-secret result metadata for one complete-line execution.
 *
 * The return value from #bsc_execute_line is the authoritative primary status;
 * this result deliberately does not duplicate it. Command and group pointers
 * borrow immutable descriptor storage and remain valid only while the registry
 * remains alive and unchanged. Index fields are meaningful only when their
 * associated pointer is non-NULL. No raw line text, token views, parsed values,
 * or parsed string/secret views are exposed.
 */
typedef struct bsc_console_result {
  /** Coarse execution phase that produced the returned status. */
  bsc_console_phase_t phase;
  /** Borrowed matched executable command descriptor, or NULL. */
  const bsc_command_t *command;
  /** Index of `command` when command is non-NULL. */
  size_t command_index;
  /** Borrowed exact matched group descriptor, or NULL. */
  const bsc_command_t *group;
  /** Index of `group` when group is non-NULL. */
  size_t group_index;
  /** Structured parser diagnostic copied only when dispatch populated it. */
  bsc_arg_parse_error_t parse_error;
} bsc_console_result_t;

/**
 * @brief Initialize a caller-owned execution workspace to an inactive state.
 *
 * @param workspace Optional workspace to initialize. NULL is accepted.
 *
 * The full line buffer, token array, matcher result, parsed arguments, parse
 * diagnostic, token count, and active flag are cleared using ordinary byte and
 * field clearing. This is deterministic initialization and best-effort privacy
 * hardening for prior contents, not a cryptographic secure erasure primitive.
 * Calling this function on an actively executing workspace is caller misuse.
 */
void bsc_console_workspace_init(bsc_console_workspace_t *workspace);

/**
 * @brief Clear optional caller-owned console result metadata.
 *
 * @param result Optional result to clear. NULL is accepted.
 */
void bsc_console_result_clear(bsc_console_result_t *result);

/**
 * @brief Initialize a complete-line console from borrowed immutable metadata.
 *
 * @param console Required caller-owned console object to initialize.
 * @param config Required borrowed configuration used only for this call.
 * @param validation_error Optional registry diagnostic, cleared on entry.
 * @retval BSC_STATUS_OK Registry validation succeeded and the console is usable.
 * @retval BSC_STATUS_INTERNAL_ERROR Required API pointers were NULL.
 * @retval BSC_STATUS_INVALID_DESCRIPTOR The registry failed validation.
 *
 * Failed initialization leaves `console` in a deterministic inert state with
 * initialized false and no partially active configuration. No deinitialization
 * function is required because the core owns no resources.
 */
bsc_status_t bsc_console_init(bsc_console_t *console,
                              const bsc_console_config_t *config,
                              bsc_registry_validation_error_t *validation_error);

/**
 * @brief Execute one complete bounded command line through existing core stages.
 *
 * @param console Required initialized console; it is read-only during execution.
 * @param workspace Required caller-owned workspace initialized before first use.
 * @param line Borrowed input byte span. May be NULL only when line_length is 0.
 * @param line_length Number of bytes to submit, excluding any C-string terminator.
 * @param result Optional caller-owned non-secret result metadata, cleared on entry.
 * @return Primary execution status from input validation, tokenization, matching,
 *   or selected-command dispatch.
 *
 * The function rejects embedded NUL bytes with #BSC_STATUS_INVALID_SYNTAX before
 * tokenization. It copies exactly `line_length` bytes into the workspace, adds a
 * trailing NUL outside the tokenizer span, tokenizes in place, matches the
 * longest executable path, and dispatches the selected command. Token and parsed
 * string/secret views borrow workspace storage and are valid only during the
 * synchronous call; handlers must copy persistent values into their own bounded
 * storage. The workspace is cleared on every active exit path. The function is
 * not generally ISR-suitable and provides no mutex or thread-safety guarantee.
 */
bsc_status_t bsc_execute_line(const bsc_console_t *console,
                              bsc_console_workspace_t *workspace,
                              const char *line,
                              size_t line_length,
                              bsc_console_result_t *result);

#ifdef __cplusplus
}
#endif

#endif /* BSC_CONSOLE_H */
