#ifndef BSC_CONFIG_H
#define BSC_CONFIG_H

/**
 * @file bsc_config.h
 * @brief Compile-time defaults for the Serial Command Extreme C core.
 *
 * These defaults are intentionally conservative bounds used across the current
 * and planned C core. Projects may override them with compiler definitions
 * before this header is included. The values document storage and validation
 * limits for tokenizer, registry, matcher, typed-parser behavior, and future
 * output/console helpers.
 */

/** Maximum static command descriptors accepted by registry validation and matcher inputs. */
#ifndef BSC_MAX_COMMANDS
#define BSC_MAX_COMMANDS 64u
#endif

/** Maximum enum choices accepted for one argument descriptor by registry validation. */
#ifndef BSC_MAX_ENUM_CHOICES
#define BSC_MAX_ENUM_CHOICES 16u
#endif

/** Maximum bytes accepted by tokenizer input validation and complete-line console input. */
#ifndef BSC_MAX_LINE_LEN
#define BSC_MAX_LINE_LEN 160u
#endif

/** Maximum token count for tokenizer output and matcher input expectations. */
#ifndef BSC_MAX_TOKENS
#define BSC_MAX_TOKENS 24u
#endif

/** Maximum bytes in one tokenizer token or registry descriptor string. */
#ifndef BSC_MAX_TOKEN_LEN
#define BSC_MAX_TOKEN_LEN 64u
#endif

/** Maximum path depth for registry descriptor paths and matcher path handling. */
#ifndef BSC_MAX_PATH_TOKENS
#define BSC_MAX_PATH_TOKENS 6u
#endif

/** Maximum static positional argument descriptors for one executable command. */
#ifndef BSC_MAX_ARGS
#define BSC_MAX_ARGS 8u
#endif

/** Enable compact runtime float argument parsing when nonzero. */
#ifndef BSC_ENABLE_FLOAT
#define BSC_ENABLE_FLOAT 1
#endif

#if BSC_ENABLE_FLOAT != 0 && BSC_ENABLE_FLOAT != 1
#error "BSC_ENABLE_FLOAT must be 0 or 1"
#endif

/** Inclusive compact-float input magnitude limit shared by registry and parser. */
#define BSC_COMPACT_FLOAT_MAX_MAGNITUDE 1000000000u

/** Maximum accepted digits after the decimal point in compact float arguments. */
#ifndef BSC_MAX_FLOAT_FRACTION_DIGITS
#define BSC_MAX_FLOAT_FRACTION_DIGITS 6u
#endif

#if BSC_MAX_FLOAT_FRACTION_DIGITS < 1 || BSC_MAX_FLOAT_FRACTION_DIGITS > 6
#error "BSC_MAX_FLOAT_FRACTION_DIGITS must be between 1 and 6"
#endif

/** Maximum bytes accepted in one generated-help prose string, excluding the terminator. */
#ifndef BSC_MAX_HELP_TEXT_LEN
#define BSC_MAX_HELP_TEXT_LEN 512u
#endif

/** Maximum bytes that bounded formatting helpers may emit per chunk if added later. */
#ifndef BSC_OUTPUT_CHUNK_LEN
#define BSC_OUTPUT_CHUNK_LEN 96u
#endif

#endif /* BSC_CONFIG_H */
