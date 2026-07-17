#ifndef BSC_CONSOLE_H
#define BSC_CONSOLE_H

#include <stdbool.h>
#include <stddef.h>

#include "bsc_args.h"
#include "bsc_config.h"
#include "bsc_help.h"
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

/** @brief Coarse phase that produced the primary console execution status. */
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
  BSC_CONSOLE_PHASE_DISPATCH,
  /** The built-in-aware execution API routed an explicit library built-in. */
  BSC_CONSOLE_PHASE_BUILTIN
} bsc_console_phase_t;

/**
 * @brief Built-in route recognized by #bsc_execute_line_with_builtins.
 *
 * These values describe only explicit library built-ins recognized by the
 * built-in-aware complete-line API. Ordinary application routing leaves the
 * value as #BSC_CONSOLE_BUILTIN_NONE, including application commands named
 * `help` or `commands` executed through #bsc_execute_line.
 */
typedef enum bsc_console_builtin {
  /** No library built-in was recognized; application routing or input validation applies. */
  BSC_CONSOLE_BUILTIN_NONE = 0,
  /** The input was `help` and requested the generated top-level help index. */
  BSC_CONSOLE_BUILTIN_HELP_INDEX,
  /** The input was `help <path...>` and requested exact descriptor-path help. */
  BSC_CONSOLE_BUILTIN_HELP_PATH,
  /** The input was `commands` and requested the generated executable-command list. */
  BSC_CONSOLE_BUILTIN_COMMANDS
} bsc_console_builtin_t;

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
 * @brief Non-secret result metadata for built-in-aware complete-line execution.
 *
 * The return value from #bsc_execute_line_with_builtins is the authoritative
 * status; this result intentionally stores no duplicate status field. `phase`
 * is the coarse stage that produced that status. `builtin` identifies an
 * explicit library built-in only when one was recognized. `application_result`
 * is meaningful only when `builtin` is #BSC_CONSOLE_BUILTIN_NONE and contains
 * the same application metadata that #bsc_execute_line would have produced.
 *
 * Collision pointers borrow immutable descriptor storage and remain valid only
 * while the registry remains alive and unchanged. Collision metadata is
 * populated only when built-in routing detects that the invoked built-in name is
 * reserved by an application descriptor. No raw line text, token views, parsed
 * values, parsed string views, parsed secret views, runtime secrets, or help
 * target pointers are exposed.
 */
typedef struct bsc_console_builtins_result {
  /** Coarse phase that produced the returned status. */
  bsc_console_phase_t phase;
  /** Recognized built-in route, or #BSC_CONSOLE_BUILTIN_NONE. */
  bsc_console_builtin_t builtin;
  /** Ordinary application result metadata; meaningful only when `builtin` is NONE. */
  bsc_console_result_t application_result;
  /** First conflicting borrowed application descriptor for a built-in collision, or NULL. */
  const bsc_command_t *collision;
  /** Index of `collision` when collision is non-NULL. */
  size_t collision_index;
} bsc_console_builtins_result_t;

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
 * @brief Clear optional built-in-aware console result metadata.
 *
 * @param result Optional result to clear. NULL is accepted.
 *
 * The embedded application result is cleared with #bsc_console_result_clear.
 * Collision metadata is discarded; no descriptor storage is mutated.
 */
void bsc_console_builtins_result_clear(bsc_console_builtins_result_t *result);

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

/**
 * @brief Execute one complete line with optional `help` and `commands` built-ins.
 *
 * @param console Required initialized console; it is read-only during execution.
 * @param workspace Required caller-owned workspace initialized before first use.
 * @param help_options Optional borrowed help visibility options; NULL uses the
 *   #bsc_help_options_init defaults. Non-NULL options are copied by value before
 *   routing and are not retained.
 * @param line Borrowed input byte span. May be NULL only when line_length is 0.
 * @param line_length Number of bytes to submit, excluding any C-string terminator.
 * @param result Optional caller-owned non-secret result metadata, cleared on entry.
 * @return Primary execution status from input validation, tokenization, built-in
 *   routing/rendering, matching, or selected-command dispatch.
 *
 * This API shares the same bounded input pipeline as #bsc_execute_line: it
 * validates the explicit byte span, rejects embedded NUL bytes, copies the input
 * exactly once into #bsc_console_workspace_t line storage, tokenizes that storage
 * exactly once, and cleans the active workspace on every active exit path.
 * Built-in recognition occurs after tokenization and before application matching
 * so quoted path tokens use the normal tokenizer while help paths never use
 * longest-prefix command dispatch. Application matcher, access callbacks, typed
 * parsing, dispatch, and handlers are not called for recognized built-ins.
 *
 * `help` renders the existing generated top-level index. `help <path...>` passes
 * every token after `help` as the complete exact help descriptor path.
 * `commands` renders the existing complete visible executable-command list.
 * `commands <anything>` returns #BSC_STATUS_EXTRA_ARGUMENT and emits no output.
 *
 * Built-in names are reserved only for the invoked built-in in this API. If a
 * descriptor path begins with the invoked built-in name, compared
 * ASCII-case-insensitively, the call returns #BSC_STATUS_INVALID_DESCRIPTOR,
 * populates collision metadata, emits no output, and does not call the
 * application matcher or handlers. #bsc_execute_line remains application-only
 * and may still execute ordinary application commands named `help` or
 * `commands`.
 *
 * Built-ins use the output wrapper copied into the console by #bsc_console_init.
 * When no output was configured, NULL is passed to the pure generated-help
 * renderer so its existing validation, lookup, and output-status precedence is
 * preserved. No fallback diagnostics, prompts, echo, final OK/ERR text, heap
 * allocation, hidden static workspace, or help-page buffer are introduced. The
 * function is synchronous, not generally ISR-suitable, and provides no mutex or
 * task-safety guarantee; callers must serialize shared workspace, descriptor,
 * application, and output state.
 */
bsc_status_t bsc_execute_line_with_builtins(const bsc_console_t *console,
                                            bsc_console_workspace_t *workspace,
                                            const bsc_help_options_t *help_options,
                                            const char *line,
                                            size_t line_length,
                                            bsc_console_builtins_result_t *result);

#ifdef __cplusplus
}
#endif

#endif /* BSC_CONSOLE_H */
