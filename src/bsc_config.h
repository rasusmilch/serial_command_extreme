#ifndef BSC_CONFIG_H
#define BSC_CONFIG_H

/**
 * @file bsc_config.h
 * @brief Compile-time defaults for the Serial Command Extreme C core.
 *
 * These defaults are intentionally conservative placeholders for the Phase 2A
 * skeleton. Projects may override them with compiler definitions before this
 * header is included. The values document bounded storage expectations for
 * future parser, registry, and output code; this task does not implement those
 * future parser features.
 */

/** Maximum static command descriptors accepted by registry validation. */
#ifndef BSC_MAX_COMMANDS
#define BSC_MAX_COMMANDS 64u
#endif

/** Maximum enum choices accepted for one argument descriptor. */
#ifndef BSC_MAX_ENUM_CHOICES
#define BSC_MAX_ENUM_CHOICES 16u
#endif

/** Maximum bytes accepted for one input line in future console code. */
#ifndef BSC_MAX_LINE_LEN
#define BSC_MAX_LINE_LEN 160u
#endif

/** Maximum token count reserved for future tokenizer output. */
#ifndef BSC_MAX_TOKENS
#define BSC_MAX_TOKENS 24u
#endif

/** Maximum bytes in one future token, excluding any terminator owned by callers. */
#ifndef BSC_MAX_TOKEN_LEN
#define BSC_MAX_TOKEN_LEN 64u
#endif

/** Maximum path depth for future static command descriptors. */
#ifndef BSC_MAX_PATH_TOKENS
#define BSC_MAX_PATH_TOKENS 6u
#endif

/** Maximum positional arguments for a future executable command descriptor. */
#ifndef BSC_MAX_ARGS
#define BSC_MAX_ARGS 8u
#endif

/** Maximum bytes that bounded formatting helpers may emit per chunk if added later. */
#ifndef BSC_OUTPUT_CHUNK_LEN
#define BSC_OUTPUT_CHUNK_LEN 96u
#endif

#endif /* BSC_CONFIG_H */
