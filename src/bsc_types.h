#ifndef BSC_TYPES_H
#define BSC_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "bsc_output.h"
#include "bsc_status.h"
#include "bsc_string_view.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file bsc_types.h
 * @brief Static command descriptor metadata for the Serial Command Extreme core.
 *
 * This header defines public metadata types shared by the current registry
 * validator, matcher, typed argument parser, and current selected-command
 * dispatcher plus future help-rendering and console-integration modules. It
 * performs no validation, matching, parsing, dispatch, help rendering, I/O,
 * allocation, runtime
 * registration, alias expansion, optional-argument handling, custom parsing, or
 * custom validation.
 *
 * Descriptor objects borrow all pointed-to storage. Command descriptor arrays,
 * path arrays, path strings, argument arrays, enum choice arrays, help strings,
 * callback functions, and opaque context pointers are caller-owned and must
 * remain valid while current registry validation, current matching, current
 * typed argument parsing, current selected-command dispatch, or future
 * console/help code references them.
 * The core does not copy or release descriptor metadata.
 *
 * Current parsed string and secret argument values returned by
 * bsc_parse_command_args() remain #bsc_string_view_t values borrowed from the
 * active command-line/token storage. An application that needs such values after
 * active command execution or token-buffer reuse must copy them into bounded
 * application-owned storage.
 */

struct bsc_command;
struct bsc_parsed_args;

/**
 * @brief Static positional argument type declared by a command descriptor.
 *
 * MVP descriptors do not model optional positional arguments. The current
 * registry validator and typed parser interpret these values together with
 * #bsc_arg_def_t metadata.
 */
typedef enum bsc_arg_type {
  /** No argument value is expected. */
  BSC_ARG_NONE = 0,
  /** Signed decimal integer argument. */
  BSC_ARG_INT,
  /** Unsigned decimal integer argument. */
  BSC_ARG_UINT,
  /** Floating-point argument. */
  BSC_ARG_FLOAT,
  /** Boolean argument. */
  BSC_ARG_BOOL,
  /** String choice argument matched against a borrowed enum choice table. */
  BSC_ARG_ENUM,
  /** Bounded string argument borrowed from active token storage by the typed parser. */
  BSC_ARG_STRING,
  /** Bounded secret argument borrowed from active token storage by the typed parser. */
  BSC_ARG_SECRET
} bsc_arg_type_t;

/**
 * @brief Command descriptor node kind.
 *
 * Groups are namespace nodes used by current registry validation and matcher
 * behavior. Commands are executable descriptor leaves selected by the current
 * matcher and then passed by callers to the current typed parser or selected-
 * command dispatcher.
 */
typedef enum bsc_node_type {
  /** Non-executable namespace/group node. */
  BSC_NODE_GROUP = 0,
  /** Executable command node. */
  BSC_NODE_COMMAND
} bsc_node_type_t;

/**
 * @brief Execution/access policy level for a command descriptor.
 *
 * This enum does not encode help visibility. Hidden/listing behavior is
 * represented by #bsc_command_flags_t so access policy and help visibility stay
 * separate.
 */
typedef enum bsc_access_level {
  /** Normal operator command. */
  BSC_ACCESS_NORMAL = 0,
  /** Advanced command that may require elevated policy later. */
  BSC_ACCESS_ADVANCED,
  /** Factory/service command denied by default unless current dispatch policy explicitly allows it. */
  BSC_ACCESS_FACTORY,
  /** Locked command denied by default unless current dispatch policy explicitly allows it. */
  BSC_ACCESS_LOCKED
} bsc_access_level_t;

/**
 * @brief Bitmask type for command descriptor metadata flags.
 *
 * Flags describe metadata such as help/list visibility. They do not implement
 * access checks or command matching.
 */
typedef uint32_t bsc_command_flags_t;

/** No command metadata flags are set. */
#define BSC_COMMAND_FLAG_NONE ((bsc_command_flags_t)0u)

/** Hide the command from broad future help/list output unless policy allows it. */
#define BSC_COMMAND_FLAG_HIDDEN ((bsc_command_flags_t)(1u << 0u))

/**
 * @brief Borrowed enum/string-choice descriptor.
 *
 * `name` and `help` point to caller/static-owned strings. `help` may be NULL
 * when no choice-specific help text is provided. `value` is the stable semantic
 * value current argument parsing exposes in #bsc_arg_value_t enum results.
 */
typedef struct bsc_enum_choice {
  /** Borrowed choice name used by current parser diagnostics and future help modules. */
  const char *name;
  /** Stable semantic value associated with the choice name. */
  int32_t value;
  /** Optional borrowed help text for this choice. */
  const char *help;
} bsc_enum_choice_t;

/**
 * @brief Borrowed positional argument descriptor.
 *
 * The fields are metadata only. This type performs no parsing or validation.
 * Numeric range fields are checked for descriptor consistency by the current
 * registry validator and applied by current runtime argument parsing. String
 * and secret length fields are byte counts and use `size_t` to match
 * #bsc_string_view_t.length. `enum_choices` points to a caller/static-owned
 * array with `enum_choice_count` entries and is meaningful for #BSC_ARG_ENUM.
 */
typedef struct bsc_arg_def {
  /** Borrowed argument name for help and diagnostics. */
  const char *name;
  /** Declared argument type. */
  bsc_arg_type_t type;
  /** Minimum signed integer value for #BSC_ARG_INT. */
  int32_t min_int;
  /** Maximum signed integer value for #BSC_ARG_INT. */
  int32_t max_int;
  /** Minimum unsigned integer value for #BSC_ARG_UINT. */
  uint32_t min_uint;
  /** Maximum unsigned integer value for #BSC_ARG_UINT. */
  uint32_t max_uint;
  /** Minimum floating-point value for #BSC_ARG_FLOAT. */
  float min_float;
  /** Maximum floating-point value for #BSC_ARG_FLOAT. */
  float max_float;
  /** Minimum byte length for #BSC_ARG_STRING and #BSC_ARG_SECRET. */
  size_t min_length;
  /** Maximum byte length for #BSC_ARG_STRING and #BSC_ARG_SECRET. */
  size_t max_length;
  /** Borrowed enum choice array for #BSC_ARG_ENUM. */
  const bsc_enum_choice_t *enum_choices;
  /** Number of borrowed entries in `enum_choices`. */
  size_t enum_choice_count;
  /** Optional borrowed help text for this argument. */
  const char *help;
} bsc_arg_def_t;

/**
 * @brief Command handler callback signature used by selected-command dispatch.
 *
 * `struct bsc_parsed_args` is defined by `bsc_args.h` for the current typed
 * parser. The parser does not invoke handlers; current selected-command
 * dispatch invokes handlers exactly after access approval and successful typed
 * argument parsing.
 */
typedef bsc_status_t (*bsc_command_handler_t)(void *app_context,
                                              const struct bsc_command *command,
                                              const struct bsc_parsed_args *args,
                                              bsc_output_t *output);

/**
 * @brief Access-policy callback signature used by selected-command dispatch.
 *
 * Current dispatch invokes this optional policy hook before argument parsing.
 * The callback receives application context, the exact command descriptor, and
 * the required access level; command_context remains available through the
 * descriptor.
 */
typedef bool (*bsc_command_access_fn_t)(void *app_context,
                                        const struct bsc_command *command,
                                        bsc_access_level_t required_access);

/**
 * @brief Static borrowed command descriptor.
 *
 * Paths are represented by a borrowed array of borrowed string pointers plus a
 * length. The descriptor does not embed path, argument, enum, parsed-argument,
 * or workspace storage. All pointers remain owned by the application/static
 * descriptor table provider and must outlive current registry validation,
 * current matcher use, current typed parsing, current selected-command dispatch,
 * and future console/help use.
 */
typedef struct bsc_command {
  /** Borrowed static array of `path_len` literal command path tokens. */
  const char *const *path;
  /** Number of literal tokens in `path`. */
  size_t path_len;
  /** Descriptor node kind. */
  bsc_node_type_t node_type;
  /** Borrowed static argument descriptor array for executable commands. */
  const bsc_arg_def_t *args;
  /** Number of entries in `args`. */
  size_t arg_count;
  /** Optional handler for executable commands invoked by current dispatch. */
  bsc_command_handler_t handler;
  /** Opaque caller-owned per-command context pointer. */
  void *command_context;
  /** Execution/access metadata enforced by current selected-command dispatch. */
  bsc_access_level_t access;
  /** Visibility/metadata flags such as #BSC_COMMAND_FLAG_HIDDEN. */
  bsc_command_flags_t flags;
  /** Optional access-policy hook evaluated by current selected-command dispatch. */
  bsc_command_access_fn_t access_fn;
  /** Optional borrowed one-line help summary. */
  const char *summary;
  /** Optional borrowed longer help description. */
  const char *description;
} bsc_command_t;

#ifdef __cplusplus
}
#endif

#endif /* BSC_TYPES_H */
