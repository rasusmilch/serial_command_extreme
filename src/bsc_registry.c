#include "bsc_registry.h"

#include "bsc_config.h"

/**
 * @brief Fold an ASCII uppercase byte to lowercase for descriptor comparisons.
 *
 * This registry helper intentionally performs ASCII-only folding; it does not
 * allocate, inspect locale, or normalize non-ASCII command descriptor text.
 */
static char bsc_registry_ascii_lower(char value) {
  if (value >= 'A' && value <= 'Z') {
    return (char)(value + ('a' - 'A'));
  }
  return value;
}

/**
 * @brief Populate an optional validation error and return descriptor failure.
 *
 * The error output may be NULL. When supplied, it receives the reason and the
 * relevant command/path/argument/enum/duplicate indexes for static descriptor
 * schema validation; command descriptors themselves are not mutated.
 */
static bsc_status_t bsc_registry_fail(bsc_registry_validation_error_t *error,
                                      bsc_registry_error_reason_t reason,
                                      size_t command_index,
                                      size_t path_token_index,
                                      size_t arg_index,
                                      size_t enum_choice_index,
                                      size_t duplicate_command_index) {
  if (error != NULL) {
    error->reason = reason;
    error->command_index = command_index;
    error->path_token_index = path_token_index;
    error->arg_index = arg_index;
    error->enum_choice_index = enum_choice_index;
    error->duplicate_command_index = duplicate_command_index;
  }
  return BSC_STATUS_INVALID_DESCRIPTOR;
}

/**
 * @brief Reset an optional registry validation error result to no-error state.
 *
 * Accepts NULL so callers can use the same validation path whether or not they
 * need detailed diagnostics. This only clears the caller-owned result object.
 */
void bsc_registry_validation_error_clear(bsc_registry_validation_error_t *error) {
  if (error == NULL) {
    return;
  }
  error->reason = BSC_REGISTRY_ERROR_NONE;
  error->command_index = 0u;
  error->path_token_index = 0u;
  error->arg_index = 0u;
  error->enum_choice_index = 0u;
  error->duplicate_command_index = 0u;
}

/**
 * @brief Measure a descriptor string without allowing an unbounded scan.
 *
 * Scans at most limit + 1 bytes so invalid static descriptor strings cannot
 * force strlen over arbitrary memory. The caller owns the string storage and
 * receives too_long when no null terminator appears within the accepted bound.
 */
static size_t bsc_registry_bounded_length(const char *text, size_t limit, int *too_long) {
  size_t length;

  *too_long = 0;
  for (length = 0u; length <= limit; ++length) {
    if (text[length] == '\0') {
      return length;
    }
  }
  *too_long = 1;
  return limit + 1u;
}

/**
 * @brief Compare two validated C strings with ASCII case-insensitive matching.
 *
 * Used only after descriptor validation has ensured non-NULL, bounded strings.
 * This is schema comparison for registry validation, not runtime token matching.
 */
static int bsc_registry_cstr_equal_ignore_case(const char *left, const char *right) {
  size_t index = 0u;

  while (left[index] != '\0' && right[index] != '\0') {
    if (bsc_registry_ascii_lower(left[index]) != bsc_registry_ascii_lower(right[index])) {
      return 0;
    }
    index += 1u;
  }
  return left[index] == '\0' && right[index] == '\0';
}

/**
 * @brief Compare two command descriptor paths for duplicate detection.
 *
 * Paths must have already passed shape validation so their token arrays and
 * strings are safe to read. Duplicate command or group paths are rejected using
 * ASCII case-insensitive token comparison to avoid ambiguous static descriptors.
 */
static int bsc_registry_paths_equal_ignore_case(const bsc_command_t *left, const bsc_command_t *right) {
  size_t index;

  if (left->path_len != right->path_len) {
    return 0;
  }
  for (index = 0u; index < left->path_len; ++index) {
    if (!bsc_registry_cstr_equal_ignore_case(left->path[index], right->path[index])) {
      return 0;
    }
  }
  return 1;
}

/**
 * @brief Return whether an argument type enum value is part of the descriptor schema.
 */
static int bsc_registry_arg_type_is_valid(bsc_arg_type_t type) {
  return type == BSC_ARG_NONE || type == BSC_ARG_INT || type == BSC_ARG_UINT ||
         type == BSC_ARG_FLOAT || type == BSC_ARG_BOOL || type == BSC_ARG_ENUM ||
         type == BSC_ARG_STRING || type == BSC_ARG_SECRET;
}

/**
 * @brief Return whether an access-level enum value is accepted in descriptors.
 */
static int bsc_registry_access_is_valid(bsc_access_level_t access) {
  return access == BSC_ACCESS_NORMAL || access == BSC_ACCESS_ADVANCED ||
         access == BSC_ACCESS_FACTORY || access == BSC_ACCESS_LOCKED;
}

/**
 * @brief Validate one static descriptor string field against token limits.
 *
 * Checks NULL, empty, and BSC_MAX_TOKEN_LEN overflow cases and records the
 * caller-selected error reason and descriptor indexes. This validates schema
 * fields only; it does not parse runtime input or mutate descriptor storage.
 */
static bsc_status_t bsc_registry_validate_string_field(const char *text,
                                                       bsc_registry_error_reason_t null_reason,
                                                       bsc_registry_error_reason_t empty_reason,
                                                       bsc_registry_error_reason_t too_long_reason,
                                                       size_t command_index,
                                                       size_t path_token_index,
                                                       size_t arg_index,
                                                       size_t enum_choice_index,
                                                       bsc_registry_validation_error_t *error) {
  int too_long;
  size_t length;

  if (text == NULL) {
    return bsc_registry_fail(error, null_reason, command_index, path_token_index, arg_index,
                             enum_choice_index, 0u);
  }

  length = bsc_registry_bounded_length(text, (size_t)BSC_MAX_TOKEN_LEN, &too_long);
  if (length == 0u) {
    return bsc_registry_fail(error, empty_reason, command_index, path_token_index, arg_index,
                             enum_choice_index, 0u);
  }
  if (too_long) {
    return bsc_registry_fail(error, too_long_reason, command_index, path_token_index, arg_index,
                             enum_choice_index, 0u);
  }
  return BSC_STATUS_OK;
}

/**
 * @brief Validate enum argument choices in a static command descriptor.
 *
 * Ensures the caller-owned choice table is present, bounded by
 * BSC_MAX_ENUM_CHOICES, has valid names, and contains no duplicate names
 * (ASCII case-insensitive) or values. This does not match user input.
 */
static bsc_status_t bsc_registry_validate_enum_choices(const bsc_arg_def_t *arg,
                                                       size_t command_index,
                                                       size_t arg_index,
                                                       bsc_registry_validation_error_t *error) {
  size_t choice_index;
  size_t compare_index;
  bsc_status_t status;

  if (arg->enum_choices == NULL) {
    return bsc_registry_fail(error, BSC_REGISTRY_ERROR_ENUM_MISSING_CHOICES, command_index, 0u,
                             arg_index, 0u, 0u);
  }
  if (arg->enum_choice_count == 0u) {
    return bsc_registry_fail(error, BSC_REGISTRY_ERROR_ENUM_CHOICE_COUNT_ZERO, command_index, 0u,
                             arg_index, 0u, 0u);
  }
  if (arg->enum_choice_count > (size_t)BSC_MAX_ENUM_CHOICES) {
    return bsc_registry_fail(error, BSC_REGISTRY_ERROR_ENUM_CHOICE_COUNT_TOO_HIGH, command_index,
                             0u, arg_index, 0u, 0u);
  }

  for (choice_index = 0u; choice_index < arg->enum_choice_count; ++choice_index) {
    status = bsc_registry_validate_string_field(arg->enum_choices[choice_index].name,
                                                BSC_REGISTRY_ERROR_NULL_ENUM_CHOICE_NAME,
                                                BSC_REGISTRY_ERROR_EMPTY_ENUM_CHOICE_NAME,
                                                BSC_REGISTRY_ERROR_ENUM_CHOICE_NAME_TOO_LONG,
                                                command_index, 0u, arg_index, choice_index, error);
    if (status != BSC_STATUS_OK) {
      return status;
    }
  }

  for (choice_index = 1u; choice_index < arg->enum_choice_count; ++choice_index) {
    for (compare_index = 0u; compare_index < choice_index; ++compare_index) {
      if (bsc_registry_cstr_equal_ignore_case(arg->enum_choices[choice_index].name,
                                             arg->enum_choices[compare_index].name)) {
        return bsc_registry_fail(error, BSC_REGISTRY_ERROR_DUPLICATE_ENUM_CHOICE_NAME,
                                 command_index, 0u, arg_index, choice_index, 0u);
      }
      if (arg->enum_choices[choice_index].value == arg->enum_choices[compare_index].value) {
        return bsc_registry_fail(error, BSC_REGISTRY_ERROR_DUPLICATE_ENUM_CHOICE_VALUE,
                                 command_index, 0u, arg_index, choice_index, 0u);
      }
    }
  }

  return BSC_STATUS_OK;
}

/**
 * @brief Validate one argument descriptor for static registry schema correctness.
 *
 * Checks name, type, range, string length, and enum-choice metadata against the
 * bounded core limits. The descriptor is only read; runtime argument parsing and
 * handler dispatch are outside this module boundary.
 */
static bsc_status_t bsc_registry_validate_arg(const bsc_arg_def_t *arg,
                                              size_t command_index,
                                              size_t arg_index,
                                              bsc_registry_validation_error_t *error) {
  bsc_status_t status;

  status = bsc_registry_validate_string_field(arg->name, BSC_REGISTRY_ERROR_INVALID_ARG_NAME,
                                              BSC_REGISTRY_ERROR_INVALID_ARG_NAME,
                                              BSC_REGISTRY_ERROR_ARG_NAME_TOO_LONG, command_index,
                                              0u, arg_index, 0u, error);
  if (status != BSC_STATUS_OK) {
    return status;
  }

  if (!bsc_registry_arg_type_is_valid(arg->type)) {
    return bsc_registry_fail(error, BSC_REGISTRY_ERROR_INVALID_ARG_TYPE, command_index, 0u,
                             arg_index, 0u, 0u);
  }
  if (arg->type == BSC_ARG_NONE) {
    return bsc_registry_fail(error, BSC_REGISTRY_ERROR_ARG_NONE_NOT_ALLOWED, command_index, 0u,
                             arg_index, 0u, 0u);
  }

  switch (arg->type) {
  case BSC_ARG_INT:
    if (arg->min_int > arg->max_int) {
      return bsc_registry_fail(error, BSC_REGISTRY_ERROR_INVALID_ARG_RANGE, command_index, 0u,
                               arg_index, 0u, 0u);
    }
    break;
  case BSC_ARG_UINT:
    if (arg->min_uint > arg->max_uint) {
      return bsc_registry_fail(error, BSC_REGISTRY_ERROR_INVALID_ARG_RANGE, command_index, 0u,
                               arg_index, 0u, 0u);
    }
    break;
  case BSC_ARG_FLOAT:
    if (arg->min_float > arg->max_float) {
      return bsc_registry_fail(error, BSC_REGISTRY_ERROR_INVALID_ARG_RANGE, command_index, 0u,
                               arg_index, 0u, 0u);
    }
    break;
  case BSC_ARG_STRING:
  case BSC_ARG_SECRET:
    if (arg->min_length > arg->max_length || arg->max_length > (size_t)BSC_MAX_TOKEN_LEN) {
      return bsc_registry_fail(error, BSC_REGISTRY_ERROR_INVALID_STRING_LENGTH_RANGE,
                               command_index, 0u, arg_index, 0u, 0u);
    }
    break;
  case BSC_ARG_ENUM:
    return bsc_registry_validate_enum_choices(arg, command_index, arg_index, error);
  case BSC_ARG_BOOL:
  case BSC_ARG_NONE:
    break;
  }

  return BSC_STATUS_OK;
}

/**
 * @brief Validate one static command or group descriptor.
 *
 * Checks path shape, node type, handler presence rules, argument descriptors,
 * access level, and known flags without registering dynamically or dispatching
 * handlers. Descriptor arrays remain caller-owned and unmodified.
 */
static bsc_status_t bsc_registry_validate_command(const bsc_command_t *command,
                                                  size_t command_index,
                                                  bsc_registry_validation_error_t *error) {
  size_t index;
  bsc_status_t status;

  if (command->path == NULL) {
    return bsc_registry_fail(error, BSC_REGISTRY_ERROR_NULL_PATH, command_index, 0u, 0u, 0u, 0u);
  }
  if (command->path_len == 0u) {
    return bsc_registry_fail(error, BSC_REGISTRY_ERROR_EMPTY_PATH, command_index, 0u, 0u, 0u, 0u);
  }
  if (command->path_len > (size_t)BSC_MAX_PATH_TOKENS) {
    return bsc_registry_fail(error, BSC_REGISTRY_ERROR_PATH_TOO_DEEP, command_index, 0u, 0u, 0u, 0u);
  }

  for (index = 0u; index < command->path_len; ++index) {
    status = bsc_registry_validate_string_field(command->path[index],
                                                BSC_REGISTRY_ERROR_NULL_PATH_TOKEN,
                                                BSC_REGISTRY_ERROR_EMPTY_PATH_TOKEN,
                                                BSC_REGISTRY_ERROR_PATH_TOKEN_TOO_LONG,
                                                command_index, index, 0u, 0u, error);
    if (status != BSC_STATUS_OK) {
      return status;
    }
  }

  if (command->node_type != BSC_NODE_GROUP && command->node_type != BSC_NODE_COMMAND) {
    return bsc_registry_fail(error, BSC_REGISTRY_ERROR_INVALID_NODE_TYPE, command_index, 0u, 0u,
                             0u, 0u);
  }
  if (command->node_type == BSC_NODE_COMMAND && command->handler == NULL) {
    return bsc_registry_fail(error, BSC_REGISTRY_ERROR_COMMAND_MISSING_HANDLER, command_index, 0u,
                             0u, 0u, 0u);
  }
  if (command->node_type == BSC_NODE_GROUP && command->handler != NULL) {
    return bsc_registry_fail(error, BSC_REGISTRY_ERROR_GROUP_HAS_HANDLER, command_index, 0u, 0u,
                             0u, 0u);
  }

  if (command->arg_count > (size_t)BSC_MAX_ARGS) {
    return bsc_registry_fail(error, BSC_REGISTRY_ERROR_ARG_COUNT_TOO_HIGH, command_index, 0u, 0u,
                             0u, 0u);
  }
  if (command->arg_count > 0u && command->args == NULL) {
    return bsc_registry_fail(error, BSC_REGISTRY_ERROR_NULL_ARGS_WITH_COUNT, command_index, 0u,
                             0u, 0u, 0u);
  }
  for (index = 0u; index < command->arg_count; ++index) {
    status = bsc_registry_validate_arg(&command->args[index], command_index, index, error);
    if (status != BSC_STATUS_OK) {
      return status;
    }
  }

  if (!bsc_registry_access_is_valid(command->access)) {
    return bsc_registry_fail(error, BSC_REGISTRY_ERROR_INVALID_COMMAND_ACCESS, command_index, 0u,
                             0u, 0u, 0u);
  }
  if ((command->flags & ~BSC_COMMAND_FLAG_HIDDEN) != 0u) {
    return bsc_registry_fail(error, BSC_REGISTRY_ERROR_INVALID_COMMAND_FLAGS, command_index, 0u,
                             0u, 0u, 0u);
  }

  return BSC_STATUS_OK;
}

/**
 * @brief Validate a complete caller-owned static command descriptor table.
 *
 * This module validates descriptor schema only: it does not register commands
 * dynamically, parse runtime input, dispatch handlers, or mutate descriptors.
 * After per-command validation, it rejects duplicate command/group paths using
 * ASCII case-insensitive comparison so later matching is unambiguous.
 */
bsc_status_t bsc_registry_validate(const bsc_command_t *commands,
                                   size_t command_count,
                                   bsc_registry_validation_error_t *error) {
  size_t command_index;
  size_t compare_index;
  bsc_status_t status;

  bsc_registry_validation_error_clear(error);

  if (commands == NULL) {
    return bsc_registry_fail(error, BSC_REGISTRY_ERROR_NULL_COMMANDS, 0u, 0u, 0u, 0u, 0u);
  }
  if (command_count == 0u) {
    return bsc_registry_fail(error, BSC_REGISTRY_ERROR_ZERO_COMMANDS, 0u, 0u, 0u, 0u, 0u);
  }
  if (command_count > (size_t)BSC_MAX_COMMANDS) {
    return bsc_registry_fail(error, BSC_REGISTRY_ERROR_TOO_MANY_COMMANDS, 0u, 0u, 0u, 0u, 0u);
  }

  for (command_index = 0u; command_index < command_count; ++command_index) {
    status = bsc_registry_validate_command(&commands[command_index], command_index, error);
    if (status != BSC_STATUS_OK) {
      return status;
    }
  }

  /* Duplicate paths are compared after shape validation so all path pointers are safe to read. */
  for (command_index = 1u; command_index < command_count; ++command_index) {
    for (compare_index = 0u; compare_index < command_index; ++compare_index) {
      if (bsc_registry_paths_equal_ignore_case(&commands[command_index], &commands[compare_index])) {
        return bsc_registry_fail(error, BSC_REGISTRY_ERROR_DUPLICATE_COMMAND_PATH, command_index,
                                 0u, 0u, 0u, compare_index);
      }
    }
  }

  return BSC_STATUS_OK;
}
