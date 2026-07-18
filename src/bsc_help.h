#ifndef BSC_HELP_H
#define BSC_HELP_H

#include <stdbool.h>
#include <stddef.h>

#include "bsc_registry.h"
#include "bsc_string_view.h"
#include "bsc_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file bsc_help.h
 * @brief Pure bounded generated help validation, lookup, and rendering.
 *
 * The help module consumes caller-owned immutable command descriptors and emits
 * deterministic LF-only text through caller-owned #bsc_output_t sinks. It owns
 * no descriptor, option, result, output, or string storage; retains no pointers
 * after returning; allocates no heap memory; and does not invoke command
 * handlers or execution access callbacks. Help metadata remains borrowed
 * NUL-terminated C-string data and is validated with #BSC_MAX_HELP_TEXT_LEN
 * bounds before rendering so rendering performs no unbounded help-prose scans.
 */

typedef struct bsc_help_options {
  /** Include advanced descriptors; default true. */
  bool include_advanced;
  /** Include factory descriptors; default false. */
  bool include_factory;
  /** Include locked descriptors; default false. */
  bool include_locked;
  /** Include descriptors flagged hidden; default false. */
  bool include_hidden;
} bsc_help_options_t;

typedef enum bsc_help_error_reason {
  /** No help validation error is present. */
  BSC_HELP_ERROR_NONE = 0,
  /** Ordinary registry validation failed; inspect the nested registry diagnostic. */
  BSC_HELP_ERROR_REGISTRY_INVALID,
  /** A visible descriptor had a NULL required summary. */
  BSC_HELP_ERROR_MISSING_SUMMARY,
  /** A visible descriptor had an empty required summary. */
  BSC_HELP_ERROR_EMPTY_SUMMARY,
  /** A visible descriptor summary was unterminated within #BSC_MAX_HELP_TEXT_LEN. */
  BSC_HELP_ERROR_SUMMARY_TOO_LONG,
  /** A visible executable command had a NULL required description. */
  BSC_HELP_ERROR_MISSING_EXECUTABLE_DESCRIPTION,
  /** A visible executable command had an empty required description. */
  BSC_HELP_ERROR_EMPTY_DESCRIPTION,
  /** A visible descriptor description was unterminated within #BSC_MAX_HELP_TEXT_LEN. */
  BSC_HELP_ERROR_DESCRIPTION_TOO_LONG,
  /** A non-NULL optional argument help string was empty. */
  BSC_HELP_ERROR_EMPTY_ARGUMENT_HELP,
  /** An argument help string was unterminated within #BSC_MAX_HELP_TEXT_LEN. */
  BSC_HELP_ERROR_ARGUMENT_HELP_TOO_LONG,
  /** A non-NULL optional enum-choice help string was empty. */
  BSC_HELP_ERROR_EMPTY_ENUM_CHOICE_HELP,
  /** An enum-choice help string was unterminated within #BSC_MAX_HELP_TEXT_LEN. */
  BSC_HELP_ERROR_ENUM_CHOICE_HELP_TOO_LONG,
  /** Help prose contained CR, LF, another ASCII control byte, or DEL. */
  BSC_HELP_ERROR_INVALID_HELP_TEXT_CONTROL_BYTE,
  /** A visible command path token emitted by help contained a presentation control byte. */
  BSC_HELP_ERROR_INVALID_PATH_TOKEN_CONTROL_BYTE,
  /** A visible argument name emitted by help contained a presentation control byte. */
  BSC_HELP_ERROR_INVALID_ARGUMENT_NAME_CONTROL_BYTE,
  /** A visible enum-choice name emitted by help contained a presentation control byte. */
  BSC_HELP_ERROR_INVALID_ENUM_CHOICE_NAME_CONTROL_BYTE,
  /** A visible nested descriptor lacked a visible explicit group at the recorded prefix depth. */
  BSC_HELP_ERROR_MISSING_VISIBLE_PARENT_GROUP
} bsc_help_error_reason_t;

typedef struct bsc_help_validation_error {
  /** Help failure reason, or #BSC_HELP_ERROR_NONE after clear/success. */
  bsc_help_error_reason_t reason;
  /** Failing descriptor index when applicable; zero when cleared or not applicable. */
  size_t command_index;
  /** Failing command path token index for path-token presentation failures; zero otherwise. */
  size_t path_token_index;
  /** Failing argument index for argument or enum-choice failures; zero otherwise. */
  size_t arg_index;
  /** Failing enum-choice index for enum-choice failures; zero otherwise. */
  size_t enum_choice_index;
  /** Missing parent prefix depth for parent-group failures; zero otherwise. */
  size_t required_parent_depth;
  /** Nested ordinary registry diagnostic for #BSC_HELP_ERROR_REGISTRY_INVALID; cleared otherwise. */
  bsc_registry_validation_error_t registry_error;
} bsc_help_validation_error_t;

typedef struct bsc_help_lookup_result {
  /** Borrowed matched descriptor pointer, or NULL after clear/failure; never retained by the core. */
  const bsc_command_t *command;
  /** Matched descriptor index, or zero after clear/failure. */
  size_t command_index;
} bsc_help_lookup_result_t;


/**
 * @brief Borrowed ordered list of extended-help prose items.
 *
 * The list owns no storage. When count is zero, items may be NULL or non-NULL and is ignored. When count is nonzero,
 * items must point at count borrowed NUL-terminated strings, each bounded by BSC_MAX_HELP_TEXT_LEN and free of CR, LF,
 * ASCII control bytes, and DEL. Used for distinct NOTES and WARNINGS sections by future renderers.
 */
typedef struct bsc_help_text_list {
  /** Borrowed array of borrowed help text items. Ignored when count is zero. */
  const char *const *items;
  /** Number of active entries in items. */
  size_t count;
} bsc_help_text_list_t;

/**
 * @brief Borrowed presentation-only example metadata for future help rendering.
 *
 * The required line and optional description are static/caller-owned text. Catalog validation bounds and checks them for
 * presentation safety but never tokenizes, parses, matches, executes, or scans them for credential-like content. Secret
 * examples must use application-authored placeholders such as <new-password>, ********, or <secret>.
 */
typedef struct bsc_help_example {
  /** Required borrowed presentation command line. It is never parsed or executed by help validation. */
  const char *line;
  /** Optional borrowed explanatory prose; NULL omits the explanation. */
  const char *description;
} bsc_help_example_t;

/**
 * @brief Borrowed related-command reference by exact descriptor pointer.
 *
 * The target pointer must equal one element in the owning catalog's authoritative command array. Related metadata never
 * becomes an alias, matcher input, parser input, dispatch edge, or execution edge.
 */
typedef struct bsc_help_related {
  /** Required borrowed descriptor pointer that must equal one element in the owning catalog command table. */
  const bsc_command_t *target;
} bsc_help_related_t;

/**
 * @brief Borrowed extended metadata attached to one command or group descriptor.
 *
 * The target pointer must equal one element in the owning catalog command table. Notes, warnings, examples, and related
 * references are optional pointer/count arrays; zero counts ignore their pointers. Target metadata is structural and does
 * not affect tokenizer, matcher, argument parsing, access, dispatch, or existing help rendering in Task 11C-1.
 */
typedef struct bsc_help_target {
  /** Required borrowed descriptor pointer for the command or group receiving extended metadata. */
  const bsc_command_t *target;
  /** Optional borrowed note prose entries rendered in metadata order by future renderers. */
  bsc_help_text_list_t notes;
  /** Optional borrowed warning prose entries rendered in metadata order by future renderers. */
  bsc_help_text_list_t warnings;
  /** Optional borrowed presentation examples. Ignored when example_count is zero. */
  const bsc_help_example_t *examples;
  /** Number of active entries in examples. */
  size_t example_count;
  /** Optional borrowed related-command references. Ignored when related_count is zero. */
  const bsc_help_related_t *related;
  /** Number of active entries in related. */
  size_t related_count;
} bsc_help_target_t;

/**
 * @brief Borrowed flat non-executable topic metadata under one command or group descriptor.
 *
 * Topics are single-token records in Task 11C-1. They inherit future rendering visibility from their parent descriptor,
 * have no access levels or visibility flags, cannot contain child topics, and cannot reference other topic records. The
 * optional description is validated when present and must be non-empty. Topic metadata never affects execution.
 */
typedef struct bsc_help_topic {
  /** Required borrowed parent descriptor pointer; topics inherit this descriptor's future rendering visibility. */
  const bsc_command_t *parent;
  /** Required borrowed single-token topic identifier; topics are flat and non-executable in Task 11C. */
  const char *id;
  /** Required borrowed one-line topic summary. */
  const char *summary;
  /** Optional borrowed topic description; non-NULL descriptions must be non-empty and bounded. */
  const char *description;
  /** Optional borrowed note prose entries. */
  bsc_help_text_list_t notes;
  /** Optional borrowed warning prose entries. */
  bsc_help_text_list_t warnings;
  /** Optional borrowed presentation examples. Ignored when example_count is zero. */
  const bsc_help_example_t *examples;
  /** Number of active entries in examples. */
  size_t example_count;
  /** Optional borrowed related-command references. Ignored when related_count is zero. */
  const bsc_help_related_t *related;
  /** Number of active entries in related. */
  size_t related_count;
} bsc_help_topic_t;

/**
 * @brief Borrowed authoritative extended-help catalog.
 *
 * The commands pointer and command_count are the authoritative registry identity for every target, topic parent, and
 * related-command pointer. The catalog owns no storage, retains no pointers after validation, and can wrap a valid
 * command table with zero target/topic records. Target count is bounded by command_count; topic count is bounded by
 * BSC_MAX_HELP_TOPICS.
 */
typedef struct bsc_help_catalog {
  /** Authoritative borrowed command table used by all catalog structural validation. */
  const bsc_command_t *commands;
  /** Number of descriptors in commands. */
  size_t command_count;
  /** Optional borrowed extended metadata records keyed by descriptor pointer. Ignored when target_count is zero. */
  const bsc_help_target_t *targets;
  /** Number of active entries in targets; must not exceed command_count. */
  size_t target_count;
  /** Optional borrowed flat topic records. Ignored when topic_count is zero. */
  const bsc_help_topic_t *topics;
  /** Number of active entries in topics; must not exceed BSC_MAX_HELP_TOPICS. */
  size_t topic_count;
} bsc_help_catalog_t;
/**
 * @brief Caller-owned result for pure flat-topic lookup.
 *
 * The result owns no storage. On success, topic borrows the exact element in
 * catalog->topics at topic_index, and parent_command_index identifies
 * topic->parent within catalog->commands. The parent descriptor pointer is
 * available through topic->parent and is intentionally not duplicated here. All
 * borrowed pointers remain valid only while the catalog, command table, metadata
 * arrays, and pointed-to strings remain alive and unchanged. The core never
 * retains this result or its pointers after return.
 */
typedef struct bsc_help_topic_lookup_result {
  /** Borrowed matching topic, or NULL after clear/failure. */
  const bsc_help_topic_t *topic;
  /** Zero-based index of topic in catalog->topics, or zero after clear/failure. */
  size_t topic_index;
  /** Zero-based index of topic->parent in catalog->commands, or zero after clear/failure. */
  size_t parent_command_index;
} bsc_help_topic_lookup_result_t;


/**
 * @brief Catalog structural validation failure reasons.
 *
 * Reasons describe borrowed catalog metadata only and are independent of help visibility. Diagnostics identify failing
 * indexes without copying metadata text, runtime line text, token views, parsed values, or secret values.
 */
typedef enum bsc_help_catalog_error_reason {
  /** No catalog validation error is present. */
  BSC_HELP_CATALOG_ERROR_NONE = 0,
  /** The required catalog API pointer was NULL. */
  BSC_HELP_CATALOG_ERROR_NULL_CATALOG,
  /** Ordinary registry validation failed for catalog->commands; inspect registry_error. */
  BSC_HELP_CATALOG_ERROR_REGISTRY_INVALID,
  /** target_count was nonzero while targets was NULL. */
  BSC_HELP_CATALOG_ERROR_TARGETS_POINTER_COUNT,
  /** topic_count was nonzero while topics was NULL. */
  BSC_HELP_CATALOG_ERROR_TOPICS_POINTER_COUNT,
  /** target_count exceeded catalog->command_count. */
  BSC_HELP_CATALOG_ERROR_TOO_MANY_TARGETS,
  /** topic_count exceeded BSC_MAX_HELP_TOPICS. */
  BSC_HELP_CATALOG_ERROR_TOO_MANY_TOPICS,
  /** A target or topic parent descriptor pointer was not an exact catalog command element. */
  BSC_HELP_CATALOG_ERROR_INVALID_DESCRIPTOR_REFERENCE,
  /** Two target metadata records referenced the same descriptor. */
  BSC_HELP_CATALOG_ERROR_DUPLICATE_TARGET,
  /** A notes or warnings list had count nonzero with a NULL items pointer. */
  BSC_HELP_CATALOG_ERROR_TEXT_LIST_POINTER_COUNT,
  /** A notes or warnings list count exceeded BSC_MAX_HELP_TEXT_ITEMS. */
  BSC_HELP_CATALOG_ERROR_TOO_MANY_TEXT_ITEMS,
  /** An active notes or warnings text item pointer was NULL. */
  BSC_HELP_CATALOG_ERROR_MISSING_TEXT,
  /** An active notes or warnings text item was empty. */
  BSC_HELP_CATALOG_ERROR_EMPTY_TEXT,
  /** An active notes or warnings text item exceeded BSC_MAX_HELP_TEXT_LEN. */
  BSC_HELP_CATALOG_ERROR_TEXT_TOO_LONG,
  /** An active notes or warnings text item contained CR, LF, another ASCII control byte, or DEL. */
  BSC_HELP_CATALOG_ERROR_INVALID_TEXT_CONTROL_BYTE,
  /** example_count was nonzero while examples was NULL. */
  BSC_HELP_CATALOG_ERROR_EXAMPLES_POINTER_COUNT,
  /** example_count exceeded BSC_MAX_HELP_EXAMPLES. */
  BSC_HELP_CATALOG_ERROR_TOO_MANY_EXAMPLES,
  /** An active example line pointer was NULL. */
  BSC_HELP_CATALOG_ERROR_MISSING_EXAMPLE_LINE,
  /** An active example line was empty. */
  BSC_HELP_CATALOG_ERROR_EMPTY_EXAMPLE_LINE,
  /** An active example line exceeded BSC_MAX_LINE_LEN. */
  BSC_HELP_CATALOG_ERROR_EXAMPLE_LINE_TOO_LONG,
  /** An active example line contained CR, LF, another ASCII control byte, or DEL. */
  BSC_HELP_CATALOG_ERROR_INVALID_EXAMPLE_LINE_CONTROL_BYTE,
  /** A non-NULL optional example description was empty, overlength, or contained a control byte. */
  BSC_HELP_CATALOG_ERROR_INVALID_EXAMPLE_DESCRIPTION,
  /** related_count was nonzero while related was NULL. */
  BSC_HELP_CATALOG_ERROR_RELATED_POINTER_COUNT,
  /** related_count exceeded BSC_MAX_HELP_RELATED. */
  BSC_HELP_CATALOG_ERROR_TOO_MANY_RELATED,
  /** A related descriptor pointer was not an exact catalog command element. */
  BSC_HELP_CATALOG_ERROR_INVALID_RELATED_DESCRIPTOR,
  /** Two related entries in one list referenced the same descriptor. */
  BSC_HELP_CATALOG_ERROR_DUPLICATE_RELATED_DESCRIPTOR,
  /** Target metadata related to its own target descriptor. */
  BSC_HELP_CATALOG_ERROR_TARGET_RELATED_SELF_REFERENCE,
  /** A topic id pointer was NULL. */
  BSC_HELP_CATALOG_ERROR_MISSING_TOPIC_ID,
  /** A topic id was empty. */
  BSC_HELP_CATALOG_ERROR_EMPTY_TOPIC_ID,
  /** A topic id exceeded BSC_MAX_TOKEN_LEN. */
  BSC_HELP_CATALOG_ERROR_TOPIC_ID_TOO_LONG,
  /** A topic id contained CR, LF, another ASCII control byte, or DEL. */
  BSC_HELP_CATALOG_ERROR_INVALID_TOPIC_ID_CONTROL_BYTE,
  /** A topic summary pointer was NULL. */
  BSC_HELP_CATALOG_ERROR_MISSING_TOPIC_SUMMARY,
  /** A topic summary was empty. */
  BSC_HELP_CATALOG_ERROR_EMPTY_TOPIC_SUMMARY,
  /** A topic summary exceeded BSC_MAX_HELP_TEXT_LEN. */
  BSC_HELP_CATALOG_ERROR_TOPIC_SUMMARY_TOO_LONG,
  /** A topic summary contained CR, LF, another ASCII control byte, or DEL. */
  BSC_HELP_CATALOG_ERROR_INVALID_TOPIC_SUMMARY_CONTROL_BYTE,
  /** A non-NULL optional topic description was empty, overlength, or contained a control byte. */
  BSC_HELP_CATALOG_ERROR_INVALID_TOPIC_DESCRIPTION,
  /** Two topics under the same parent had the same id under ASCII case folding. */
  BSC_HELP_CATALOG_ERROR_DUPLICATE_TOPIC
} bsc_help_catalog_error_reason_t;

/**
 * @brief Caller-owned catalog structural validation diagnostic.
 *
 * The diagnostic is cleared on entry/success and filled with scalar indexes for the first structural failure. Index fields
 * are zero when not applicable. registry_error is populated only for BSC_HELP_CATALOG_ERROR_REGISTRY_INVALID. The
 * diagnostic never stores borrowed text, runtime input, token views, parsed values, or secret values.
 */
typedef struct bsc_help_catalog_validation_error {
  /** Catalog structural failure reason, or NONE after clear/success. */
  bsc_help_catalog_error_reason_t reason;
  /** Failing target metadata index when applicable. */
  size_t target_index;
  /** Failing topic index when applicable. */
  size_t topic_index;
  /** Failing note/warning text item index when applicable. */
  size_t item_index;
  /** Failing example index when applicable. */
  size_t example_index;
  /** Failing related-command index when applicable. */
  size_t related_index;
  /** Resolved or failing command descriptor index when applicable. */
  size_t command_index;
  /** Duplicate target/topic/related index when applicable. */
  size_t duplicate_index;
  /** Nested registry diagnostic for REGISTRY_INVALID; cleared otherwise. */
  bsc_registry_validation_error_t registry_error;
} bsc_help_catalog_validation_error_t;

/**
 * @brief Initialize help visibility options to defaults.
 * @param options Caller-owned options storage, or NULL for no effect.
 * Defaults include advanced descriptors and exclude factory, locked, and hidden descriptors.
 */
void bsc_help_options_init(bsc_help_options_t *options);

/**
 * @brief Clear a caller-owned help validation diagnostic.
 * @param error Optional diagnostic storage. NULL is accepted. All scalar indexes and nested registry data are reset.
 */
void bsc_help_validation_error_clear(bsc_help_validation_error_t *error);

/**
 * @brief Clear a caller-owned lookup result.
 * @param result Optional result storage. NULL is accepted. Any borrowed descriptor pointer is discarded.
 */
void bsc_help_lookup_result_clear(bsc_help_lookup_result_t *result);

/**
 * @brief Clear a caller-owned catalog validation diagnostic.
 * @param error Optional diagnostic storage; NULL is accepted.
 * Clears all scalar indexes and the nested registry diagnostic. The function retains no pointers.
 */
void bsc_help_catalog_validation_error_clear(bsc_help_catalog_validation_error_t *error);

/**
 * @brief Clear a caller-owned pure topic lookup result.
 * @param result Optional result storage; NULL is accepted.
 * Sets the borrowed topic pointer to NULL and scalar indexes to zero, retains no pointers, and performs no allocation or
 * I/O.
 */
void bsc_help_topic_lookup_result_clear(bsc_help_topic_lookup_result_t *result);

/**
 * @brief Validate borrowed extended-help catalog structure independently of rendering visibility.
 * @param catalog Required borrowed catalog whose commands/command_count are the authoritative registry.
 * @param error Optional caller-owned diagnostic cleared on entry and filled on failure.
 * @retval BSC_STATUS_OK The catalog is structurally valid.
 * @retval BSC_STATUS_INTERNAL_ERROR Required API input was invalid.
 * @retval BSC_STATUS_INVALID_DESCRIPTOR Registry or catalog metadata validation failed.
 *
 * The validator calls #bsc_registry_validate on the authoritative command table and never calls visibility-dependent
 * generated-help validation. All catalog storage, nested arrays, relationship pointers, strings, examples, and related
 * metadata remain caller-owned or static and need only outlive this synchronous call. Relationship pointers must equal
 * exact elements of catalog->commands; metadata never affects tokenizer, matcher, parser, dispatch, aliases, handlers,
 * execution access callbacks, or runtime argument values. Topics are flat single-token non-executable records that
 * inherit their parent descriptor visibility in future renderers; Task 11C adds no nested topics, topic visibility flags,
 * topic access levels, or topic-to-topic relationships. Static examples are presentation text only and should use
 * application-authored placeholders such as <new-password>, ********, or <secret> for secret arguments.
 *
 * The function emits no output, allocates no heap, uses no hidden workspace, retains no pointers after return, and is
 * reentrant for immutable independent catalogs and diagnostics. It performs bounded but nontrivial scans and is intended
 * for normal task/thread context rather than ISR-oriented use.
 */
bsc_status_t bsc_help_catalog_validate(const bsc_help_catalog_t *catalog,
                                        bsc_help_catalog_validation_error_t *error);

/**
 * @brief Find a flat non-executable help topic under an exact visible parent path.
 * @param catalog Required borrowed catalog; catalog->commands and catalog->command_count are the authoritative registry.
 * @param parent_path_tokens Required explicit-length parent path token array when parent_path_token_count is nonzero.
 * @param parent_path_token_count Number of parent path tokens; zero returns #BSC_STATUS_NO_INPUT.
 * @param topic_id Explicit-length topic identifier; zero length returns #BSC_STATUS_NO_INPUT.
 * @param options Optional borrowed visibility options; NULL means defaults. Topics inherit parent descriptor visibility.
 * @param result Required caller-owned result storage; cleared on entry and on every failure.
 * @retval BSC_STATUS_OK One topic matched under the visible parent; result borrows catalog metadata.
 * @retval BSC_STATUS_NO_INPUT No parent path tokens or an empty topic id was supplied.
 * @retval BSC_STATUS_UNKNOWN_COMMAND Parent path was absent or filtered by help visibility.
 * @retval BSC_STATUS_UNKNOWN_TOPIC Parent path was visible, but no topic under that parent matched.
 * @retval BSC_STATUS_INVALID_DESCRIPTOR Catalog, registry, duplicate topic, or visible help metadata validation failed.
 * @retval BSC_STATUS_INTERNAL_ERROR Required API pointers were invalid, including nonempty topic_id with NULL data.
 *
 * The function validates before lookup, derives the registry only from the catalog, matches topic ids with ASCII
 * case-insensitive comparison, owns no storage, retains no pointers internally after return, allocates no heap, uses no
 * hidden workspace, emits no output, and never invokes handlers, access callbacks, matcher, parser, dispatcher, console
 * routing, runtime parsed arguments, or runtime secret values. It is reentrant for immutable metadata and independent
 * result objects, and is intended for synchronous task/thread or host-test use rather than ISR use.
 */
bsc_status_t bsc_help_find_topic(const bsc_help_catalog_t *catalog,
                                 const bsc_string_view_t *parent_path_tokens,
                                 size_t parent_path_token_count,
                                 bsc_string_view_t topic_id,
                                 const bsc_help_options_t *options,
                                 bsc_help_topic_lookup_result_t *result);


/**
 * @brief Validate registry shape, help-visible metadata, emitted identifiers, and visible parent groups.
 * @param commands Borrowed descriptor table; ordinary registry rules define pointer/count validity.
 * @param command_count Number of descriptors.
 * @param options Optional borrowed visibility options; NULL means defaults.
 * @param error Optional caller-owned diagnostic cleared on entry and filled on failure.
 * @retval BSC_STATUS_OK The table is valid for generated help under the supplied visibility options.
 * @retval BSC_STATUS_INVALID_DESCRIPTOR Registry, help prose, identifier, or parent-group validation failed.
 * The function emits no output, invokes no handlers/access callbacks, retains no pointers, and is reentrant for
 * immutable tables with independent diagnostics. It is task-context oriented; ISR suitability depends on bounded scan tolerance.
 */
bsc_status_t bsc_help_validate(const bsc_command_t *commands,
                               size_t command_count,
                               const bsc_help_options_t *options,
                               bsc_help_validation_error_t *error);

/**
 * @brief Find one visible descriptor by exact ASCII case-insensitive path tokens.
 * @param commands Borrowed descriptor table.
 * @param command_count Number of descriptors.
 * @param path_tokens Borrowed explicit-length path tokens; required when @p path_token_count is nonzero.
 * @param path_token_count Number of path tokens; zero returns #BSC_STATUS_NO_INPUT.
 * @param options Optional borrowed visibility options; NULL means defaults.
 * @param result Required caller-owned result, cleared on entry and failure.
 * @retval BSC_STATUS_OK Exact visible command or group found; result borrows the descriptor until caller metadata changes.
 * @retval BSC_STATUS_NO_INPUT No path tokens were supplied.
 * @retval BSC_STATUS_UNKNOWN_COMMAND No exact visible target matched, including filtered targets.
 * @retval BSC_STATUS_AMBIGUOUS_COMMAND Defensive duplicate visible match was detected.
 * @retval BSC_STATUS_INVALID_DESCRIPTOR Validation failed before lookup.
 * @retval BSC_STATUS_INTERNAL_ERROR Required pointers were invalid.
 * The function emits no output, invokes no handlers/access callbacks, treats no token as an argument or subtopic,
 * retains no pointers, and is reentrant for independent result storage.
 */
bsc_status_t bsc_help_find_path(const bsc_command_t *commands,
                                size_t command_count,
                                const bsc_string_view_t *path_tokens,
                                size_t path_token_count,
                                const bsc_help_options_t *options,
                                bsc_help_lookup_result_t *result);

/**
 * @brief Render the top-level visible help index under a COMMANDS heading.
 * @param commands Borrowed descriptor table.
 * @param command_count Number of descriptors.
 * @param options Optional borrowed visibility options; NULL means defaults.
 * @param output Required caller-owned output sink; callback blocking/ISR behavior is sink-defined.
 * @retval BSC_STATUS_OK Output completed.
 * @retval BSC_STATUS_INVALID_DESCRIPTOR Validation failed before any output was emitted.
 * @retval BSC_STATUS_OUTPUT_TRUNCATED The first short write occurred and rendering stopped immediately.
 * @retval BSC_STATUS_INTERNAL_ERROR Output target or required API input was invalid.
 * Emits LF-only deterministic descriptor-order text, retains no pointers, invokes no handlers/access callbacks,
 * and never reads runtime parsed arguments or secret values.
 */
bsc_status_t bsc_help_render_index(const bsc_command_t *commands,
                                   size_t command_count,
                                   const bsc_help_options_t *options,
                                   bsc_output_t *output);

/**
 * @brief Render every visible executable command under a COMMANDS heading.
 * @param commands Borrowed descriptor table whose path tokens, argument metadata, summaries, and descriptions
 *        must remain valid only for the duration of the call; the renderer retains no descriptor pointers.
 * @param command_count Number of descriptors in @p commands. The same command/table validity rules used by
 *        #bsc_help_validate apply before any bytes are emitted.
 * @param options Optional borrowed static visibility options. NULL selects #bsc_help_options_init defaults:
 *        normal and advanced descriptors are visible, while factory, locked, and hidden descriptors are filtered.
 * @param output Required caller-owned output sink. The sink callback may block according to application policy;
 *        this API performs no serialization around a shared sink.
 * @retval BSC_STATUS_OK Output completed.
 * @retval BSC_STATUS_INVALID_DESCRIPTOR Registry or help metadata validation failed before any output was emitted.
 * @retval BSC_STATUS_OUTPUT_TRUNCATED The first partial/short sink write occurred; rendering stopped immediately
 *         and no fallback text or trailing newline was added.
 * @retval BSC_STATUS_INTERNAL_ERROR Required API inputs or output callback storage were invalid.
 *
 * The emitted document is LF-only, starts with a COMMANDS heading, lists only visible executable command descriptors
 * (groups are omitted), and preserves descriptor-table order without sorting or heap allocation. The function validates
 * descriptors before output, never invokes command handlers or command access callbacks, never reads parsed runtime
 * arguments or runtime secret values, and does not retain @p commands, @p options, or @p output after return. Calls are
 * reentrant for independent descriptor tables/options/output sinks; callers that share a sink across tasks must provide
 * external serialization. Because sink behavior may block and registry scans are bounded but nontrivial, this function is
 * intended for normal task/thread context rather than ISR context.
 */
bsc_status_t bsc_help_render_commands(const bsc_command_t *commands,
                                      size_t command_count,
                                      const bsc_help_options_t *options,
                                      bsc_output_t *output);

/**
 * @brief Render a visible exact group or executable-command help page.
 * @param commands Borrowed descriptor table.
 * @param command_count Number of descriptors.
 * @param path_tokens Borrowed explicit-length target path tokens.
 * @param path_token_count Number of target path tokens.
 * @param options Optional borrowed visibility options; NULL means defaults.
 * @param output Required caller-owned output sink.
 * @retval BSC_STATUS_OK Output completed.
 * @retval BSC_STATUS_NO_INPUT No path tokens were supplied.
 * @retval BSC_STATUS_UNKNOWN_COMMAND Target was absent or filtered.
 * @retval BSC_STATUS_INVALID_DESCRIPTOR Validation failed before output.
 * @retval BSC_STATUS_OUTPUT_TRUNCATED The first short write occurred and rendering stopped immediately.
 * @retval BSC_STATUS_INTERNAL_ERROR Required pointers or output target were invalid.
 * The renderer emits generated NAME/SYNOPSIS/DESCRIPTION/ARGUMENTS/VALID VALUES/COMMANDS sections as applicable,
 * invokes no handlers/access callbacks, retains no pointers, and never reads parsed arguments or runtime secrets.
 */
bsc_status_t bsc_help_render_path(const bsc_command_t *commands,
                                  size_t command_count,
                                  const bsc_string_view_t *path_tokens,
                                  size_t path_token_count,
                                  const bsc_help_options_t *options,
                                  bsc_output_t *output);

#ifdef __cplusplus
}
#endif

#endif /* BSC_HELP_H */
