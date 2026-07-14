#include "bsc_help.h"

#include "bsc_config.h"

#include <stdint.h>

/** @brief Compile-time length for internal string literals only. */
#define BSC_HELP_LITERAL_LEN(value) (sizeof(value) - 1u)

/** @brief Return options with documented default static visibility. */
static bsc_help_options_t bsc_help_default_options(void) {
  bsc_help_options_t options;
  options.include_advanced = true;
  options.include_factory = false;
  options.include_locked = false;
  options.include_hidden = false;
  return options;
}

void bsc_help_options_init(bsc_help_options_t *options) {
  if (options != NULL) {
    *options = bsc_help_default_options();
  }
}

void bsc_help_validation_error_clear(bsc_help_validation_error_t *error) {
  if (error == NULL) {
    return;
  }
  error->reason = BSC_HELP_ERROR_NONE;
  error->command_index = 0u;
  error->path_token_index = 0u;
  error->arg_index = 0u;
  error->enum_choice_index = 0u;
  error->required_parent_depth = 0u;
  bsc_registry_validation_error_clear(&error->registry_error);
}

void bsc_help_lookup_result_clear(bsc_help_lookup_result_t *result) {
  if (result != NULL) {
    result->command = NULL;
    result->command_index = 0u;
  }
}

/** @brief Populate a help validation error without retaining descriptors. */
static bsc_status_t bsc_help_fail(bsc_help_validation_error_t *error,
                                  bsc_help_error_reason_t reason,
                                  size_t command_index,
                                  size_t path_token_index,
                                  size_t arg_index,
                                  size_t enum_choice_index,
                                  size_t required_parent_depth) {
  if (error != NULL) {
    error->reason = reason;
    error->command_index = command_index;
    error->path_token_index = path_token_index;
    error->arg_index = arg_index;
    error->enum_choice_index = enum_choice_index;
    error->required_parent_depth = required_parent_depth;
  }
  return BSC_STATUS_INVALID_DESCRIPTOR;
}

/** @brief Return whether static help visibility includes a descriptor. */
static int bsc_help_is_visible(const bsc_command_t *command, const bsc_help_options_t *supplied) {
  bsc_help_options_t defaults;
  const bsc_help_options_t *options = supplied;

  if (options == NULL) {
    defaults = bsc_help_default_options();
    options = &defaults;
  }
  if ((command->flags & BSC_COMMAND_FLAG_HIDDEN) != 0u && !options->include_hidden) {
    return 0;
  }
  switch (command->access) {
  case BSC_ACCESS_NORMAL:
    return 1;
  case BSC_ACCESS_ADVANCED:
    return options->include_advanced ? 1 : 0;
  case BSC_ACCESS_FACTORY:
    return options->include_factory ? 1 : 0;
  case BSC_ACCESS_LOCKED:
    return options->include_locked ? 1 : 0;
  default:
    return 0;
  }
}

/** @brief Measure validated help prose within BSC_MAX_HELP_TEXT_LEN and reject control bytes. */
static bsc_status_t bsc_help_validate_text(const char *text,
                                           int required,
                                           bsc_help_error_reason_t missing_reason,
                                           bsc_help_error_reason_t empty_reason,
                                           bsc_help_error_reason_t too_long_reason,
                                           size_t command_index,
                                           size_t arg_index,
                                           size_t enum_choice_index,
                                           bsc_help_validation_error_t *error) {
  size_t index;

  if (text == NULL) {
    return required ? bsc_help_fail(error, missing_reason, command_index, 0u, arg_index, enum_choice_index, 0u)
                    : BSC_STATUS_OK;
  }
  for (index = 0u; index <= (size_t)BSC_MAX_HELP_TEXT_LEN; ++index) {
    unsigned char byte = (unsigned char)text[index];
    if (byte == '\0') {
      if (index == 0u) {
        return bsc_help_fail(error, empty_reason, command_index, 0u, arg_index, enum_choice_index, 0u);
      }
      return BSC_STATUS_OK;
    }
    if (byte < 0x20u || byte == 0x7fu) {
      return bsc_help_fail(error, BSC_HELP_ERROR_INVALID_HELP_TEXT_CONTROL_BYTE, command_index, 0u,
                           arg_index, enum_choice_index, 0u);
    }
  }
  return bsc_help_fail(error, too_long_reason, command_index, 0u, arg_index, enum_choice_index, 0u);
}


/**
 * @brief Validate one emitted identifier string against presentation control bytes.
 *
 * Registry validation has already bounded identifiers by BSC_MAX_TOKEN_LEN and
 * guaranteed termination. Help adds a stricter presentation-layer control-byte
 * rule for every path token, argument name, and enum choice name that rendering
 * may emit. The scan never uses the prose limit and never truncates a valid
 * identifier.
 */
static bsc_status_t bsc_help_validate_identifier(const char *text,
                                                 bsc_help_error_reason_t reason,
                                                 size_t command_index,
                                                 size_t path_token_index,
                                                 size_t arg_index,
                                                 size_t enum_choice_index,
                                                 bsc_help_validation_error_t *error) {
  size_t index;
  for (index = 0u; index <= (size_t)BSC_MAX_TOKEN_LEN; ++index) {
    unsigned char byte = (unsigned char)text[index];
    if (byte == '\0') {
      return BSC_STATUS_OK;
    }
    if (byte < 0x20u || byte == 0x7fu) {
      return bsc_help_fail(error, reason, command_index, path_token_index, arg_index, enum_choice_index, 0u);
    }
  }
  return bsc_help_fail(error, reason, command_index, path_token_index, arg_index, enum_choice_index, 0u);
}

/** @brief Return whether candidate is an explicit visible group for a prefix depth. */
static int bsc_help_is_parent_group(const bsc_command_t *candidate,
                                    const bsc_command_t *child,
                                    size_t depth,
                                    const bsc_help_options_t *options) {
  size_t index;
  if (candidate->node_type != BSC_NODE_GROUP || candidate->path_len != depth ||
      !bsc_help_is_visible(candidate, options)) {
    return 0;
  }
  for (index = 0u; index < depth; ++index) {
    if (!bsc_string_view_equals_cstr_ignore_case(bsc_string_view_from_cstr(child->path[index]),
                                                candidate->path[index])) {
      return 0;
    }
  }
  return 1;
}

/** @brief Validate that every visible nested descriptor has visible explicit parent groups. */
static bsc_status_t bsc_help_validate_parent_groups(const bsc_command_t *commands,
                                                    size_t command_count,
                                                    const bsc_help_options_t *options,
                                                    bsc_help_validation_error_t *error) {
  size_t command_index;
  for (command_index = 0u; command_index < command_count; ++command_index) {
    size_t depth;
    if (!bsc_help_is_visible(&commands[command_index], options)) {
      continue;
    }
    for (depth = 1u; depth < commands[command_index].path_len; ++depth) {
      size_t parent_index;
      int found = 0;
      for (parent_index = 0u; parent_index < command_count; ++parent_index) {
        if (bsc_help_is_parent_group(&commands[parent_index], &commands[command_index], depth, options)) {
          found = 1;
          break;
        }
      }
      if (!found) {
        return bsc_help_fail(error, BSC_HELP_ERROR_MISSING_VISIBLE_PARENT_GROUP, command_index, 0u, 0u, 0u, depth);
      }
    }
  }
  return BSC_STATUS_OK;
}

bsc_status_t bsc_help_validate(const bsc_command_t *commands,
                               size_t command_count,
                               const bsc_help_options_t *options,
                               bsc_help_validation_error_t *error) {
  bsc_registry_validation_error_t registry_error;
  bsc_status_t status;
  size_t command_index;

  bsc_help_validation_error_clear(error);
  bsc_registry_validation_error_clear(&registry_error);
  status = bsc_registry_validate(commands, command_count, &registry_error);
  if (status != BSC_STATUS_OK) {
    if (error != NULL) {
      error->reason = BSC_HELP_ERROR_REGISTRY_INVALID;
      error->registry_error = registry_error;
    }
    return BSC_STATUS_INVALID_DESCRIPTOR;
  }

  for (command_index = 0u; command_index < command_count; ++command_index) {
    const bsc_command_t *command = &commands[command_index];
    size_t arg_index;
    if (!bsc_help_is_visible(command, options)) {
      continue;
    }
    for (arg_index = 0u; arg_index < command->path_len; ++arg_index) {
      status = bsc_help_validate_identifier(command->path[arg_index], BSC_HELP_ERROR_INVALID_PATH_TOKEN_CONTROL_BYTE,
                                            command_index, arg_index, 0u, 0u, error);
      if (status != BSC_STATUS_OK) {
        return status;
      }
    }
    for (arg_index = 0u; arg_index < command->arg_count; ++arg_index) {
      const bsc_arg_def_t *arg = &command->args[arg_index];
      size_t choice_index;
      status = bsc_help_validate_identifier(arg->name, BSC_HELP_ERROR_INVALID_ARGUMENT_NAME_CONTROL_BYTE,
                                            command_index, 0u, arg_index, 0u, error);
      if (status != BSC_STATUS_OK) {
        return status;
      }
      if (arg->type == BSC_ARG_ENUM) {
        for (choice_index = 0u; choice_index < arg->enum_choice_count; ++choice_index) {
          status = bsc_help_validate_identifier(arg->enum_choices[choice_index].name,
                                                BSC_HELP_ERROR_INVALID_ENUM_CHOICE_NAME_CONTROL_BYTE,
                                                command_index, 0u, arg_index, choice_index, error);
          if (status != BSC_STATUS_OK) {
            return status;
          }
        }
      }
    }
    status = bsc_help_validate_text(command->summary, 1, BSC_HELP_ERROR_MISSING_SUMMARY,
                                    BSC_HELP_ERROR_EMPTY_SUMMARY, BSC_HELP_ERROR_SUMMARY_TOO_LONG,
                                    command_index, 0u, 0u, error);
    if (status != BSC_STATUS_OK) {
      return status;
    }
    status = bsc_help_validate_text(command->description, command->node_type == BSC_NODE_COMMAND,
                                    BSC_HELP_ERROR_MISSING_EXECUTABLE_DESCRIPTION,
                                    BSC_HELP_ERROR_EMPTY_DESCRIPTION, BSC_HELP_ERROR_DESCRIPTION_TOO_LONG,
                                    command_index, 0u, 0u, error);
    if (status != BSC_STATUS_OK) {
      return status;
    }
    for (arg_index = 0u; arg_index < command->arg_count; ++arg_index) {
      const bsc_arg_def_t *arg = &command->args[arg_index];
      size_t choice_index;
      status = bsc_help_validate_text(arg->help, 0, BSC_HELP_ERROR_NONE,
                                      BSC_HELP_ERROR_EMPTY_ARGUMENT_HELP,
                                      BSC_HELP_ERROR_ARGUMENT_HELP_TOO_LONG, command_index,
                                      arg_index, 0u, error);
      if (status != BSC_STATUS_OK) {
        return status;
      }
      if (arg->type == BSC_ARG_ENUM) {
        for (choice_index = 0u; choice_index < arg->enum_choice_count; ++choice_index) {
          status = bsc_help_validate_text(arg->enum_choices[choice_index].help, 0, BSC_HELP_ERROR_NONE,
                                          BSC_HELP_ERROR_EMPTY_ENUM_CHOICE_HELP,
                                          BSC_HELP_ERROR_ENUM_CHOICE_HELP_TOO_LONG, command_index,
                                          arg_index, choice_index, error);
          if (status != BSC_STATUS_OK) {
            return status;
          }
        }
      }
    }
  }
  return bsc_help_validate_parent_groups(commands, command_count, options, error);
}

/** @brief Compare an exact descriptor path to explicit-length lookup tokens. */
static int bsc_help_path_matches(const bsc_command_t *command,
                                 const bsc_string_view_t *path_tokens,
                                 size_t path_token_count) {
  size_t index;
  if (command->path_len != path_token_count) {
    return 0;
  }
  for (index = 0u; index < path_token_count; ++index) {
    if (!bsc_string_view_equals_cstr_ignore_case(path_tokens[index], command->path[index])) {
      return 0;
    }
  }
  return 1;
}

bsc_status_t bsc_help_find_path(const bsc_command_t *commands,
                                size_t command_count,
                                const bsc_string_view_t *path_tokens,
                                size_t path_token_count,
                                const bsc_help_options_t *options,
                                bsc_help_lookup_result_t *result) {
  size_t command_index;
  const bsc_command_t *found = NULL;
  size_t found_index = 0u;
  bsc_status_t status;

  bsc_help_lookup_result_clear(result);
  if (path_token_count == 0u) {
    return BSC_STATUS_NO_INPUT;
  }
  if (path_tokens == NULL || result == NULL) {
    return BSC_STATUS_INTERNAL_ERROR;
  }
  status = bsc_help_validate(commands, command_count, options, NULL);
  if (status != BSC_STATUS_OK) {
    return status;
  }
  for (command_index = 0u; command_index < command_count; ++command_index) {
    if (!bsc_help_is_visible(&commands[command_index], options) ||
        !bsc_help_path_matches(&commands[command_index], path_tokens, path_token_count)) {
      continue;
    }
    if (found != NULL) {
      return BSC_STATUS_AMBIGUOUS_COMMAND;
    }
    found = &commands[command_index];
    found_index = command_index;
  }
  if (found == NULL) {
    return BSC_STATUS_UNKNOWN_COMMAND;
  }
  result->command = found;
  result->command_index = found_index;
  return BSC_STATUS_OK;
}

/** @brief Write one fragment and immediately propagate output failure. */
static bsc_status_t bsc_help_write(bsc_output_t *output, const char *data, size_t length) {
  return bsc_out_write_bytes(output, data, length);
}

/** @brief Write one compile-time string literal without runtime length scanning. */
#define BSC_HELP_WRITE_LITERAL(output, literal) bsc_help_write((output), (literal), BSC_HELP_LITERAL_LEN(literal))

/**
 * @brief Return the length of previously validated help prose.
 *
 * Precondition: bsc_help_validate_text() has already accepted the prose under
 * BSC_MAX_HELP_TEXT_LEN. The helper therefore scans only within that prose
 * bound and is never used for identifiers or internal literals.
 */
static size_t bsc_help_prose_length(const char *text) {
  size_t length = 0u;
  while (length <= (size_t)BSC_MAX_HELP_TEXT_LEN && text[length] != '\0') {
    length += 1u;
  }
  return length;
}

/**
 * @brief Return the length of a registry-validated emitted identifier.
 *
 * Precondition: ordinary registry validation has accepted the identifier under
 * BSC_MAX_TOKEN_LEN and help validation has rejected presentation control bytes.
 * The helper never uses BSC_MAX_HELP_TEXT_LEN, so small prose limits cannot
 * truncate path tokens, argument names, or enum choice names.
 */
static size_t bsc_help_identifier_length(const char *text) {
  size_t length = 0u;
  while (length <= (size_t)BSC_MAX_TOKEN_LEN && text[length] != '\0') {
    length += 1u;
  }
  return length;
}

/** @brief Emit a descriptor path token-by-token without a path buffer. */
static bsc_status_t bsc_help_write_path(bsc_output_t *output, const bsc_command_t *command) {
  size_t index;
  bsc_status_t status;
  for (index = 0u; index < command->path_len; ++index) {
    if (index != 0u) {
      status = BSC_HELP_WRITE_LITERAL(output, " ");
      if (status != BSC_STATUS_OK) {
        return status;
      }
    }
    status = bsc_help_write(output, command->path[index], bsc_help_identifier_length(command->path[index]));
    if (status != BSC_STATUS_OK) {
      return status;
    }
  }
  return BSC_STATUS_OK;
}

/** @brief Emit a section separator before all but the first rendered section. */
static bsc_status_t bsc_help_section(bsc_output_t *output,
                                     const char *heading,
                                     size_t heading_length,
                                     int *section_started) {
  bsc_status_t status;
  if (*section_started) {
    status = BSC_HELP_WRITE_LITERAL(output, "\n");
    if (status != BSC_STATUS_OK) {
      return status;
    }
  }
  *section_started = 1;
  status = bsc_help_write(output, heading, heading_length);
  if (status != BSC_STATUS_OK) {
    return status;
  }
  return BSC_HELP_WRITE_LITERAL(output, "\n");
}

/** @brief Emit one COMMANDS entry in the approved descriptor-order grammar. */
static bsc_status_t bsc_help_entry(bsc_output_t *output, const bsc_command_t *command) {
  bsc_status_t status;
  status = BSC_HELP_WRITE_LITERAL(output, "  ");
  if (status != BSC_STATUS_OK) {
    return status;
  }
  status = bsc_help_write_path(output, command);
  if (status != BSC_STATUS_OK) {
    return status;
  }
  status = BSC_HELP_WRITE_LITERAL(output, " - ");
  if (status != BSC_STATUS_OK) {
    return status;
  }
  status = bsc_help_write(output, command->summary, bsc_help_prose_length(command->summary));
  if (status != BSC_STATUS_OK) {
    return status;
  }
  return BSC_HELP_WRITE_LITERAL(output, "\n");
}

/** @brief Return whether child is an immediate visible child of group. */
static int bsc_help_is_immediate_child(const bsc_command_t *group,
                                       const bsc_command_t *child,
                                       const bsc_help_options_t *options) {
  size_t index;
  if (!bsc_help_is_visible(child, options) || child->path_len != group->path_len + 1u) {
    return 0;
  }
  for (index = 0u; index < group->path_len; ++index) {
    if (!bsc_string_view_equals_cstr_ignore_case(bsc_string_view_from_cstr(group->path[index]), child->path[index])) {
      return 0;
    }
  }
  return 1;
}

/** @brief Emit the NAME section shared by groups and commands. */
static bsc_status_t bsc_help_render_name(bsc_output_t *output, const bsc_command_t *command, int *sections) {
  bsc_status_t status = bsc_help_section(output, "NAME", BSC_HELP_LITERAL_LEN("NAME"), sections);
  if (status != BSC_STATUS_OK) {
    return status;
  }
  status = BSC_HELP_WRITE_LITERAL(output, "  ");
  if (status != BSC_STATUS_OK) {
    return status;
  }
  status = bsc_help_write_path(output, command);
  if (status != BSC_STATUS_OK) {
    return status;
  }
  status = BSC_HELP_WRITE_LITERAL(output, " - ");
  if (status != BSC_STATUS_OK) {
    return status;
  }
  status = bsc_help_write(output, command->summary, bsc_help_prose_length(command->summary));
  if (status != BSC_STATUS_OK) {
    return status;
  }
  return BSC_HELP_WRITE_LITERAL(output, "\n");
}

/** @brief Emit a signed integer using a local non-escaping 16-byte buffer. */
static bsc_status_t bsc_help_write_int32(bsc_output_t *output, int32_t value) {
  char buffer[16];
  char digits[10];
  size_t used = 0u;
  size_t digit_count = 0u;
  uint32_t magnitude;
  if (value < 0) {
    buffer[used++] = '-';
    magnitude = value == INT32_MIN ? 2147483648u : (uint32_t)(-value);
  } else {
    magnitude = (uint32_t)value;
  }
  do {
    digits[digit_count++] = (char)('0' + (magnitude % 10u));
    magnitude /= 10u;
  } while (magnitude != 0u);
  while (digit_count != 0u) {
    buffer[used++] = digits[--digit_count];
  }
  return bsc_help_write(output, buffer, used);
}

/** @brief Emit an unsigned integer using a local non-escaping 16-byte buffer. */
static bsc_status_t bsc_help_write_uint32(bsc_output_t *output, uint32_t value) {
  char buffer[16];
  char digits[10];
  size_t used = 0u;
  size_t digit_count = 0u;
  do {
    digits[digit_count++] = (char)('0' + (value % 10u));
    value /= 10u;
  } while (value != 0u);
  while (digit_count != 0u) {
    buffer[used++] = digits[--digit_count];
  }
  return bsc_help_write(output, buffer, used);
}

/** @brief Return the configured decimal scale for deterministic compact float output. */
static uint64_t bsc_help_float_scale(void) {
  uint64_t scale = 1u;
  size_t index;
  for (index = 0u; index < (size_t)BSC_MAX_FLOAT_FRACTION_DIGITS; ++index) {
    scale *= 10u;
  }
  return scale;
}

/** @brief Emit a float with locale-independent rounded decimal text in a 32-byte buffer. */
static bsc_status_t bsc_help_write_float(bsc_output_t *output, float value) {
  char buffer[32];
  char int_digits[20];
  char frac_digits[6];
  size_t used = 0u;
  size_t int_count = 0u;
  uint64_t scale = bsc_help_float_scale();
  double number = (double)value;
  int negative = number < 0.0 ? 1 : 0;
  double magnitude = negative ? -number : number;
  uint64_t scaled = (uint64_t)(magnitude * (double)scale + 0.5);
  uint64_t integer = scaled / scale;
  uint64_t fraction = scaled % scale;
  size_t frac_count = (size_t)BSC_MAX_FLOAT_FRACTION_DIGITS;

  if (negative && scaled != 0u) {
    buffer[used++] = '-';
  }
  do {
    int_digits[int_count++] = (char)('0' + (integer % 10u));
    integer /= 10u;
  } while (integer != 0u);
  while (int_count != 0u) {
    buffer[used++] = int_digits[--int_count];
  }
  if (fraction != 0u) {
    size_t index;
    for (index = 0u; index < (size_t)BSC_MAX_FLOAT_FRACTION_DIGITS; ++index) {
      frac_digits[(size_t)BSC_MAX_FLOAT_FRACTION_DIGITS - 1u - index] = (char)('0' + (fraction % 10u));
      fraction /= 10u;
    }
    while (frac_count > 0u && frac_digits[frac_count - 1u] == '0') {
      frac_count -= 1u;
    }
    buffer[used++] = '.';
    for (index = 0u; index < frac_count; ++index) {
      buffer[used++] = frac_digits[index];
    }
  }
  return bsc_help_write(output, buffer, used);
}

/** @brief Emit the generated synopsis directly from descriptor path and required args. */
static bsc_status_t bsc_help_render_synopsis(bsc_output_t *output, const bsc_command_t *command, int *sections) {
  size_t index;
  bsc_status_t status = bsc_help_section(output, "SYNOPSIS", BSC_HELP_LITERAL_LEN("SYNOPSIS"), sections);
  if (status != BSC_STATUS_OK) {
    return status;
  }
  status = BSC_HELP_WRITE_LITERAL(output, "  ");
  if (status != BSC_STATUS_OK) {
    return status;
  }
  status = bsc_help_write_path(output, command);
  if (status != BSC_STATUS_OK) {
    return status;
  }
  for (index = 0u; index < command->arg_count; ++index) {
    status = BSC_HELP_WRITE_LITERAL(output, " <");
    if (status != BSC_STATUS_OK) {
      return status;
    }
    status = bsc_help_write(output, command->args[index].name, bsc_help_identifier_length(command->args[index].name));
    if (status != BSC_STATUS_OK) {
      return status;
    }
    status = BSC_HELP_WRITE_LITERAL(output, ">");
    if (status != BSC_STATUS_OK) {
      return status;
    }
  }
  return BSC_HELP_WRITE_LITERAL(output, "\n");
}

/** @brief Emit optional DESCRIPTION after validation has made mandatory prose safe. */
static bsc_status_t bsc_help_render_description(bsc_output_t *output, const char *description, int *sections) {
  bsc_status_t status;
  if (description == NULL) {
    return BSC_STATUS_OK;
  }
  status = bsc_help_section(output, "DESCRIPTION", BSC_HELP_LITERAL_LEN("DESCRIPTION"), sections);
  if (status != BSC_STATUS_OK) {
    return status;
  }
  status = BSC_HELP_WRITE_LITERAL(output, "  ");
  if (status != BSC_STATUS_OK) {
    return status;
  }
  status = bsc_help_write(output, description, bsc_help_prose_length(description));
  if (status != BSC_STATUS_OK) {
    return status;
  }
  return BSC_HELP_WRITE_LITERAL(output, "\n");
}

/** @brief Emit ARGUMENTS lines while preserving descriptor argument order. */
static bsc_status_t bsc_help_render_arguments(bsc_output_t *output, const bsc_command_t *command, int *sections) {
  size_t index;
  bsc_status_t status;
  if (command->arg_count == 0u) {
    return BSC_STATUS_OK;
  }
  status = bsc_help_section(output, "ARGUMENTS", BSC_HELP_LITERAL_LEN("ARGUMENTS"), sections);
  if (status != BSC_STATUS_OK) {
    return status;
  }
  for (index = 0u; index < command->arg_count; ++index) {
    const bsc_arg_def_t *arg = &command->args[index];
    status = BSC_HELP_WRITE_LITERAL(output, "  ");
    if (status != BSC_STATUS_OK) {
      return status;
    }
    status = bsc_help_write(output, arg->name, bsc_help_identifier_length(arg->name));
    if (status != BSC_STATUS_OK) {
      return status;
    }
    if (arg->help != NULL) {
      status = BSC_HELP_WRITE_LITERAL(output, " - ");
      if (status != BSC_STATUS_OK) {
        return status;
      }
      status = bsc_help_write(output, arg->help, bsc_help_prose_length(arg->help));
      if (status != BSC_STATUS_OK) {
        return status;
      }
    }
    status = BSC_HELP_WRITE_LITERAL(output, "\n");
    if (status != BSC_STATUS_OK) {
      return status;
    }
  }
  return BSC_STATUS_OK;
}

/** @brief Emit one argument name prefix used by VALID VALUES. */
static bsc_status_t bsc_help_arg_prefix(bsc_output_t *output, const bsc_arg_def_t *arg) {
  bsc_status_t status = BSC_HELP_WRITE_LITERAL(output, "  ");
  if (status != BSC_STATUS_OK) {
    return status;
  }
  status = bsc_help_write(output, arg->name, bsc_help_identifier_length(arg->name));
  if (status != BSC_STATUS_OK) {
    return status;
  }
  return BSC_HELP_WRITE_LITERAL(output, ": ");
}

/** @brief Emit generated valid-value text that mirrors current parser descriptor schema. */
static bsc_status_t bsc_help_render_valid_values(bsc_output_t *output, const bsc_command_t *command, int *sections) {
  size_t index;
  bsc_status_t status;
  if (command->arg_count == 0u) {
    return BSC_STATUS_OK;
  }
  status = bsc_help_section(output, "VALID VALUES", BSC_HELP_LITERAL_LEN("VALID VALUES"), sections);
  if (status != BSC_STATUS_OK) {
    return status;
  }
  for (index = 0u; index < command->arg_count; ++index) {
    const bsc_arg_def_t *arg = &command->args[index];
    size_t choice_index;
    status = bsc_help_arg_prefix(output, arg);
    if (status != BSC_STATUS_OK) {
      return status;
    }
    switch (arg->type) {
    case BSC_ARG_INT:
      status = BSC_HELP_WRITE_LITERAL(output, "integer, ");
      if (status == BSC_STATUS_OK) status = bsc_help_write_int32(output, arg->min_int);
      if (status == BSC_STATUS_OK) status = BSC_HELP_WRITE_LITERAL(output, "..");
      if (status == BSC_STATUS_OK) status = bsc_help_write_int32(output, arg->max_int);
      break;
    case BSC_ARG_UINT:
      status = BSC_HELP_WRITE_LITERAL(output, "unsigned integer, ");
      if (status == BSC_STATUS_OK) status = bsc_help_write_uint32(output, arg->min_uint);
      if (status == BSC_STATUS_OK) status = BSC_HELP_WRITE_LITERAL(output, "..");
      if (status == BSC_STATUS_OK) status = bsc_help_write_uint32(output, arg->max_uint);
      break;
    case BSC_ARG_FLOAT:
      status = BSC_HELP_WRITE_LITERAL(output, "decimal, ");
      if (status == BSC_STATUS_OK) status = bsc_help_write_float(output, arg->min_float);
      if (status == BSC_STATUS_OK) status = BSC_HELP_WRITE_LITERAL(output, "..");
      if (status == BSC_STATUS_OK) status = bsc_help_write_float(output, arg->max_float);
      break;
    case BSC_ARG_BOOL:
      status = BSC_HELP_WRITE_LITERAL(output, "on | off | true | false | 1 | 0");
      break;
    case BSC_ARG_ENUM:
      for (choice_index = 0u; choice_index < arg->enum_choice_count; ++choice_index) {
        if (choice_index != 0u) {
          status = BSC_HELP_WRITE_LITERAL(output, " | ");
          if (status != BSC_STATUS_OK) return status;
        }
        status = bsc_help_write(output, arg->enum_choices[choice_index].name,
                                bsc_help_identifier_length(arg->enum_choices[choice_index].name));
        if (status != BSC_STATUS_OK) return status;
      }
      break;
    case BSC_ARG_STRING:
      status = BSC_HELP_WRITE_LITERAL(output, "string, ");
      if (status == BSC_STATUS_OK) status = bsc_help_write_uint32(output, (uint32_t)arg->min_length);
      if (status == BSC_STATUS_OK) status = BSC_HELP_WRITE_LITERAL(output, "..");
      if (status == BSC_STATUS_OK) status = bsc_help_write_uint32(output, (uint32_t)arg->max_length);
      if (status == BSC_STATUS_OK) status = BSC_HELP_WRITE_LITERAL(output, " bytes");
      break;
    case BSC_ARG_SECRET:
      status = BSC_HELP_WRITE_LITERAL(output, "secret, ");
      if (status == BSC_STATUS_OK) status = bsc_help_write_uint32(output, (uint32_t)arg->min_length);
      if (status == BSC_STATUS_OK) status = BSC_HELP_WRITE_LITERAL(output, "..");
      if (status == BSC_STATUS_OK) status = bsc_help_write_uint32(output, (uint32_t)arg->max_length);
      if (status == BSC_STATUS_OK) status = BSC_HELP_WRITE_LITERAL(output, " bytes");
      break;
    case BSC_ARG_NONE:
      status = BSC_STATUS_INVALID_DESCRIPTOR;
      break;
    }
    if (status != BSC_STATUS_OK) {
      return status;
    }
    status = BSC_HELP_WRITE_LITERAL(output, "\n");
    if (status != BSC_STATUS_OK) {
      return status;
    }
    if (arg->type == BSC_ARG_ENUM) {
      for (choice_index = 0u; choice_index < arg->enum_choice_count; ++choice_index) {
        if (arg->enum_choices[choice_index].help != NULL) {
          status = BSC_HELP_WRITE_LITERAL(output, "    ");
          if (status != BSC_STATUS_OK) return status;
          status = bsc_help_write(output, arg->enum_choices[choice_index].name,
                                  bsc_help_identifier_length(arg->enum_choices[choice_index].name));
          if (status != BSC_STATUS_OK) return status;
          status = BSC_HELP_WRITE_LITERAL(output, " - ");
          if (status != BSC_STATUS_OK) return status;
          status = bsc_help_write(output, arg->enum_choices[choice_index].help,
                                  bsc_help_prose_length(arg->enum_choices[choice_index].help));
          if (status != BSC_STATUS_OK) return status;
          status = BSC_HELP_WRITE_LITERAL(output, "\n");
          if (status != BSC_STATUS_OK) return status;
        }
      }
    }
  }
  return BSC_STATUS_OK;
}

bsc_status_t bsc_help_render_index(const bsc_command_t *commands,
                                   size_t command_count,
                                   const bsc_help_options_t *options,
                                   bsc_output_t *output) {
  size_t index;
  bsc_status_t status;
  status = bsc_help_validate(commands, command_count, options, NULL);
  if (status != BSC_STATUS_OK) return status;
  status = BSC_HELP_WRITE_LITERAL(output, "COMMANDS\n");
  if (status != BSC_STATUS_OK) return status;
  for (index = 0u; index < command_count; ++index) {
    if (bsc_help_is_visible(&commands[index], options) && commands[index].path_len == 1u) {
      status = bsc_help_entry(output, &commands[index]);
      if (status != BSC_STATUS_OK) return status;
    }
  }
  return BSC_STATUS_OK;
}

bsc_status_t bsc_help_render_commands(const bsc_command_t *commands,
                                      size_t command_count,
                                      const bsc_help_options_t *options,
                                      bsc_output_t *output) {
  size_t index;
  bsc_status_t status;
  status = bsc_help_validate(commands, command_count, options, NULL);
  if (status != BSC_STATUS_OK) return status;
  status = BSC_HELP_WRITE_LITERAL(output, "COMMANDS\n");
  if (status != BSC_STATUS_OK) return status;
  for (index = 0u; index < command_count; ++index) {
    if (bsc_help_is_visible(&commands[index], options) && commands[index].node_type == BSC_NODE_COMMAND) {
      status = bsc_help_entry(output, &commands[index]);
      if (status != BSC_STATUS_OK) return status;
    }
  }
  return BSC_STATUS_OK;
}

/** @brief Render immediate visible children for a group page in descriptor order. */
static bsc_status_t bsc_help_render_group_children(bsc_output_t *output,
                                                   const bsc_command_t *commands,
                                                   size_t command_count,
                                                   const bsc_command_t *group,
                                                   const bsc_help_options_t *options,
                                                   int *sections) {
  size_t index;
  int any = 0;
  bsc_status_t status;
  for (index = 0u; index < command_count; ++index) {
    if (bsc_help_is_immediate_child(group, &commands[index], options)) {
      any = 1;
      break;
    }
  }
  if (!any) {
    return BSC_STATUS_OK;
  }
  status = bsc_help_section(output, "COMMANDS", BSC_HELP_LITERAL_LEN("COMMANDS"), sections);
  if (status != BSC_STATUS_OK) return status;
  for (index = 0u; index < command_count; ++index) {
    if (bsc_help_is_immediate_child(group, &commands[index], options)) {
      status = bsc_help_entry(output, &commands[index]);
      if (status != BSC_STATUS_OK) return status;
    }
  }
  return BSC_STATUS_OK;
}

bsc_status_t bsc_help_render_path(const bsc_command_t *commands,
                                  size_t command_count,
                                  const bsc_string_view_t *path_tokens,
                                  size_t path_token_count,
                                  const bsc_help_options_t *options,
                                  bsc_output_t *output) {
  bsc_help_lookup_result_t result;
  bsc_status_t status;
  int sections = 0;

  status = bsc_help_find_path(commands, command_count, path_tokens, path_token_count, options, &result);
  if (status != BSC_STATUS_OK) {
    return status;
  }
  if (output == NULL || output->write == NULL) {
    return BSC_STATUS_INTERNAL_ERROR;
  }
  status = bsc_help_render_name(output, result.command, &sections);
  if (status != BSC_STATUS_OK) return status;
  if (result.command->node_type == BSC_NODE_GROUP) {
    status = bsc_help_render_description(output, result.command->description, &sections);
    if (status != BSC_STATUS_OK) return status;
    return bsc_help_render_group_children(output, commands, command_count, result.command, options, &sections);
  }
  status = bsc_help_render_synopsis(output, result.command, &sections);
  if (status != BSC_STATUS_OK) return status;
  status = bsc_help_render_description(output, result.command->description, &sections);
  if (status != BSC_STATUS_OK) return status;
  status = bsc_help_render_arguments(output, result.command, &sections);
  if (status != BSC_STATUS_OK) return status;
  return bsc_help_render_valid_values(output, result.command, &sections);
}
