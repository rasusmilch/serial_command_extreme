#include "bsc_args.h"

#include <stdint.h>

#if BSC_ENABLE_FLOAT
#include "internal/bsc_float_parse.h"
#endif

/** @brief Return whether a byte is an ASCII decimal digit. */
static int bsc_args_is_digit(char value) { return value >= '0' && value <= '9'; }

/** @brief Return one ASCII-folded byte without consulting locale state. */
static char bsc_args_lower(char value) {
  return value >= 'A' && value <= 'Z' ? (char)(value + ('a' - 'A')) : value;
}

/** @brief Compare a borrowed token view to a null-terminated string with ASCII case folding. */
static int bsc_args_view_equals_ignore_case(bsc_string_view_t view, const char *text) {
  size_t index;
  if (text == NULL || (view.data == NULL && view.length > 0u)) {
    return 0;
  }
  for (index = 0u; index < view.length; ++index) {
    if (text[index] == '\0' || bsc_args_lower(view.data[index]) != bsc_args_lower(text[index])) {
      return 0;
    }
  }
  return text[view.length] == '\0';
}

/** @brief Check whether an explicit-length token contains a NUL byte and report its offset. */
static int bsc_args_has_nul(bsc_string_view_t view, size_t *offset) {
  size_t index;
  if (view.data == NULL && view.length > 0u) {
    if (offset != NULL) {
      *offset = 0u;
    }
    return 1;
  }
  for (index = 0u; index < view.length; ++index) {
    if (view.data[index] == '\0') {
      if (offset != NULL) {
        *offset = index;
      }
      return 1;
    }
  }
  return 0;
}

void bsc_parsed_args_clear(bsc_parsed_args_t *args) {
  size_t index;
  if (args == NULL) {
    return;
  }
  args->count = 0u;
  for (index = 0u; index < (size_t)BSC_MAX_ARGS; ++index) {
    args->values[index].type = BSC_ARG_NONE;
    args->values[index].data.text_value = bsc_string_view_from_parts(NULL, 0u);
  }
}

void bsc_arg_parse_error_clear(bsc_arg_parse_error_t *error) {
  if (error == NULL) {
    return;
  }
  error->reason = BSC_ARG_PARSE_ERROR_NONE;
  error->arg_index = 0u;
  error->token_offset = 0u;
}

/** @brief Clear any partial result, populate scalar diagnostics, and return the selected status. */
static bsc_status_t bsc_args_fail(bsc_parsed_args_t *out_args,
                                  bsc_arg_parse_error_t *error,
                                  bsc_arg_parse_error_reason_t reason,
                                  size_t arg_index,
                                  size_t token_offset,
                                  bsc_status_t status) {
  bsc_parsed_args_clear(out_args);
  if (error != NULL) {
    error->reason = reason;
    error->arg_index = arg_index;
    error->token_offset = token_offset;
  }
  return status;
}

/** @brief Return whether an argument type belongs to the active runtime descriptor set. */
static int bsc_args_type_valid(bsc_arg_type_t type) {
  return type == BSC_ARG_INT || type == BSC_ARG_UINT || type == BSC_ARG_FLOAT ||
         type == BSC_ARG_BOOL || type == BSC_ARG_ENUM || type == BSC_ARG_STRING ||
         type == BSC_ARG_SECRET;
}

/** @brief Defensively validate descriptor fields that typed parsing must read safely. */
static bsc_status_t bsc_args_validate_descriptor(const bsc_command_t *command,
                                                 bsc_arg_parse_error_t *error) {
  size_t index;
  if (command == NULL) {
    if (error != NULL) {
      error->reason = BSC_ARG_PARSE_ERROR_INVALID_API;
    }
    return BSC_STATUS_INTERNAL_ERROR;
  }
  if (command->node_type != BSC_NODE_COMMAND || command->arg_count > (size_t)BSC_MAX_ARGS ||
      (command->arg_count > 0u && command->args == NULL)) {
    return bsc_args_fail(NULL, error, BSC_ARG_PARSE_ERROR_INVALID_DESCRIPTOR, 0u, 0u,
                         BSC_STATUS_INVALID_DESCRIPTOR);
  }
  for (index = 0u; index < command->arg_count; ++index) {
    const bsc_arg_def_t *arg = &command->args[index];
    if (arg->name == NULL || !bsc_args_type_valid(arg->type)) {
      return bsc_args_fail(NULL, error, BSC_ARG_PARSE_ERROR_INVALID_DESCRIPTOR, index, 0u,
                           BSC_STATUS_INVALID_DESCRIPTOR);
    }
    switch (arg->type) {
    case BSC_ARG_INT:
      if (arg->min_int > arg->max_int) {
        return bsc_args_fail(NULL, error, BSC_ARG_PARSE_ERROR_INVALID_DESCRIPTOR, index, 0u,
                             BSC_STATUS_INVALID_DESCRIPTOR);
      }
      break;
    case BSC_ARG_UINT:
      if (arg->min_uint > arg->max_uint) {
        return bsc_args_fail(NULL, error, BSC_ARG_PARSE_ERROR_INVALID_DESCRIPTOR, index, 0u,
                             BSC_STATUS_INVALID_DESCRIPTOR);
      }
      break;
    case BSC_ARG_FLOAT:
#if BSC_ENABLE_FLOAT
      if (arg->min_float != arg->min_float || arg->max_float != arg->max_float ||
          arg->min_float < -(float)BSC_COMPACT_FLOAT_MAX_MAGNITUDE ||
          arg->max_float > (float)BSC_COMPACT_FLOAT_MAX_MAGNITUDE ||
          arg->min_float > arg->max_float) {
        return bsc_args_fail(NULL, error, BSC_ARG_PARSE_ERROR_INVALID_DESCRIPTOR, index, 0u,
                             BSC_STATUS_INVALID_DESCRIPTOR);
      }
#else
      return bsc_args_fail(NULL, error, BSC_ARG_PARSE_ERROR_FLOAT_DISABLED, index, 0u,
                           BSC_STATUS_INVALID_DESCRIPTOR);
#endif
      break;
    case BSC_ARG_ENUM:
      if (arg->enum_choices == NULL || arg->enum_choice_count == 0u ||
          arg->enum_choice_count > (size_t)BSC_MAX_ENUM_CHOICES) {
        return bsc_args_fail(NULL, error, BSC_ARG_PARSE_ERROR_INVALID_DESCRIPTOR, index, 0u,
                             BSC_STATUS_INVALID_DESCRIPTOR);
      }
      break;
    case BSC_ARG_STRING:
    case BSC_ARG_SECRET:
      if (arg->min_length > arg->max_length || arg->max_length > (size_t)BSC_MAX_TOKEN_LEN) {
        return bsc_args_fail(NULL, error, BSC_ARG_PARSE_ERROR_INVALID_DESCRIPTOR, index, 0u,
                             BSC_STATUS_INVALID_DESCRIPTOR);
      }
      break;
    case BSC_ARG_BOOL:
      break;
    case BSC_ARG_NONE:
      return bsc_args_fail(NULL, error, BSC_ARG_PARSE_ERROR_INVALID_DESCRIPTOR, index, 0u,
                           BSC_STATUS_INVALID_DESCRIPTOR);
    }
  }
  return BSC_STATUS_OK;
}

/** @brief Parse a signed decimal int32 token with checked unsigned magnitude accumulation. */
static bsc_status_t bsc_args_parse_int(bsc_string_view_t token,
                                       const bsc_arg_def_t *arg,
                                       bsc_arg_value_t *value,
                                       bsc_arg_parse_error_t *error,
                                       bsc_parsed_args_t *out_args,
                                       size_t arg_index) {
  size_t index = 0u;
  int negative = 0;
  uint32_t magnitude = 0u;
  uint32_t limit;
  if (token.length == 0u || token.data == NULL) {
    return bsc_args_fail(out_args, error, BSC_ARG_PARSE_ERROR_INVALID_VALUE, arg_index, 0u,
                         BSC_STATUS_INVALID_ARGUMENT_TYPE);
  }
  if (token.data[index] == '+') {
    return bsc_args_fail(out_args, error, BSC_ARG_PARSE_ERROR_INVALID_VALUE, arg_index, index,
                         BSC_STATUS_INVALID_ARGUMENT_TYPE);
  }
  if (token.data[index] == '-') {
    negative = 1;
    index += 1u;
    if (index == token.length) {
      return bsc_args_fail(out_args, error, BSC_ARG_PARSE_ERROR_INVALID_VALUE, arg_index, index - 1u,
                           BSC_STATUS_INVALID_ARGUMENT_TYPE);
    }
  }
  limit = negative ? 2147483648u : 2147483647u;
  for (; index < token.length; ++index) {
    uint32_t digit;
    if (token.data[index] == '\0') {
      return bsc_args_fail(out_args, error, BSC_ARG_PARSE_ERROR_EMBEDDED_NUL, arg_index, index,
                           BSC_STATUS_INVALID_ARGUMENT_TYPE);
    }
    if (!bsc_args_is_digit(token.data[index])) {
      return bsc_args_fail(out_args, error, BSC_ARG_PARSE_ERROR_INVALID_VALUE, arg_index, index,
                           BSC_STATUS_INVALID_ARGUMENT_TYPE);
    }
    digit = (uint32_t)(token.data[index] - '0');
    if (magnitude > (uint32_t)((limit - digit) / 10u)) {
      return bsc_args_fail(out_args, error,
                           negative ? BSC_ARG_PARSE_ERROR_BELOW_MINIMUM : BSC_ARG_PARSE_ERROR_ABOVE_MAXIMUM,
                           arg_index, index, BSC_STATUS_ARGUMENT_OUT_OF_RANGE);
    }
    magnitude = (uint32_t)(magnitude * 10u + digit);
  }
  value->type = BSC_ARG_INT;
  if (negative && magnitude == 2147483648u) {
    value->data.int_value = INT32_MIN;
  } else if (negative) {
    value->data.int_value = (int32_t)(-(int32_t)magnitude);
  } else {
    value->data.int_value = (int32_t)magnitude;
  }
  if (value->data.int_value < arg->min_int) {
    return bsc_args_fail(out_args, error, BSC_ARG_PARSE_ERROR_BELOW_MINIMUM, arg_index, 0u,
                         BSC_STATUS_ARGUMENT_OUT_OF_RANGE);
  }
  if (value->data.int_value > arg->max_int) {
    return bsc_args_fail(out_args, error, BSC_ARG_PARSE_ERROR_ABOVE_MAXIMUM, arg_index, 0u,
                         BSC_STATUS_ARGUMENT_OUT_OF_RANGE);
  }
  return BSC_STATUS_OK;
}

/** @brief Parse an unsigned decimal uint32 token with checked multiply/add. */
static bsc_status_t bsc_args_parse_uint(bsc_string_view_t token,
                                        const bsc_arg_def_t *arg,
                                        bsc_arg_value_t *value,
                                        bsc_arg_parse_error_t *error,
                                        bsc_parsed_args_t *out_args,
                                        size_t arg_index) {
  size_t index;
  uint32_t parsed = 0u;
  if (token.length == 0u || token.data == NULL) {
    return bsc_args_fail(out_args, error, BSC_ARG_PARSE_ERROR_INVALID_VALUE, arg_index, 0u,
                         BSC_STATUS_INVALID_ARGUMENT_TYPE);
  }
  for (index = 0u; index < token.length; ++index) {
    uint32_t digit;
    if (token.data[index] == '\0') {
      return bsc_args_fail(out_args, error, BSC_ARG_PARSE_ERROR_EMBEDDED_NUL, arg_index, index,
                           BSC_STATUS_INVALID_ARGUMENT_TYPE);
    }
    if (!bsc_args_is_digit(token.data[index])) {
      return bsc_args_fail(out_args, error, BSC_ARG_PARSE_ERROR_INVALID_VALUE, arg_index, index,
                           BSC_STATUS_INVALID_ARGUMENT_TYPE);
    }
    digit = (uint32_t)(token.data[index] - '0');
    if (parsed > (uint32_t)((UINT32_MAX - digit) / 10u)) {
      return bsc_args_fail(out_args, error, BSC_ARG_PARSE_ERROR_ABOVE_MAXIMUM, arg_index, index,
                           BSC_STATUS_ARGUMENT_OUT_OF_RANGE);
    }
    parsed = (uint32_t)(parsed * 10u + digit);
  }
  value->type = BSC_ARG_UINT;
  value->data.uint_value = parsed;
  if (parsed < arg->min_uint) {
    return bsc_args_fail(out_args, error, BSC_ARG_PARSE_ERROR_BELOW_MINIMUM, arg_index, 0u,
                         BSC_STATUS_ARGUMENT_OUT_OF_RANGE);
  }
  if (parsed > arg->max_uint) {
    return bsc_args_fail(out_args, error, BSC_ARG_PARSE_ERROR_ABOVE_MAXIMUM, arg_index, 0u,
                         BSC_STATUS_ARGUMENT_OUT_OF_RANGE);
  }
  return BSC_STATUS_OK;
}

/** @brief Parse approved boolean spellings with ASCII case folding. */
static bsc_status_t bsc_args_parse_bool(bsc_string_view_t token,
                                        bsc_arg_value_t *value,
                                        bsc_arg_parse_error_t *error,
                                        bsc_parsed_args_t *out_args,
                                        size_t arg_index) {
  size_t nul_offset;
  if (bsc_args_has_nul(token, &nul_offset)) {
    return bsc_args_fail(out_args, error, BSC_ARG_PARSE_ERROR_EMBEDDED_NUL, arg_index, nul_offset,
                         BSC_STATUS_INVALID_ARGUMENT_TYPE);
  }
  value->type = BSC_ARG_BOOL;
  if (bsc_args_view_equals_ignore_case(token, "on") || bsc_args_view_equals_ignore_case(token, "true") ||
      bsc_args_view_equals_ignore_case(token, "1")) {
    value->data.bool_value = true;
    return BSC_STATUS_OK;
  }
  if (bsc_args_view_equals_ignore_case(token, "off") || bsc_args_view_equals_ignore_case(token, "false") ||
      bsc_args_view_equals_ignore_case(token, "0")) {
    value->data.bool_value = false;
    return BSC_STATUS_OK;
  }
  return bsc_args_fail(out_args, error, BSC_ARG_PARSE_ERROR_INVALID_VALUE, arg_index, 0u,
                       BSC_STATUS_INVALID_ARGUMENT_TYPE);
}

/** @brief Parse an enum by ASCII case-insensitive full-token choice matching. */
static bsc_status_t bsc_args_parse_enum(bsc_string_view_t token,
                                        const bsc_arg_def_t *arg,
                                        bsc_arg_value_t *value,
                                        bsc_arg_parse_error_t *error,
                                        bsc_parsed_args_t *out_args,
                                        size_t arg_index) {
  size_t index;
  size_t nul_offset;
  if (bsc_args_has_nul(token, &nul_offset)) {
    return bsc_args_fail(out_args, error, BSC_ARG_PARSE_ERROR_EMBEDDED_NUL, arg_index, nul_offset,
                         BSC_STATUS_INVALID_ARGUMENT_TYPE);
  }
  for (index = 0u; index < arg->enum_choice_count; ++index) {
    if (bsc_args_view_equals_ignore_case(token, arg->enum_choices[index].name)) {
      value->type = BSC_ARG_ENUM;
      value->data.enum_value = arg->enum_choices[index].value;
      return BSC_STATUS_OK;
    }
  }
  return bsc_args_fail(out_args, error, BSC_ARG_PARSE_ERROR_INVALID_ENUM_CHOICE, arg_index, 0u,
                       BSC_STATUS_INVALID_ENUM_VALUE);
}

/** @brief Validate string/secret byte length and borrow the original token view. */
static bsc_status_t bsc_args_parse_text(bsc_string_view_t token,
                                        const bsc_arg_def_t *arg,
                                        bsc_arg_value_t *value,
                                        bsc_arg_parse_error_t *error,
                                        bsc_parsed_args_t *out_args,
                                        size_t arg_index) {
  size_t nul_offset;
  if (bsc_args_has_nul(token, &nul_offset)) {
    return bsc_args_fail(out_args, error, BSC_ARG_PARSE_ERROR_EMBEDDED_NUL, arg_index, nul_offset,
                         BSC_STATUS_INVALID_ARGUMENT_TYPE);
  }
  if (token.length < arg->min_length) {
    return bsc_args_fail(out_args, error, BSC_ARG_PARSE_ERROR_TEXT_TOO_SHORT, arg_index, 0u,
                         BSC_STATUS_ARGUMENT_TOO_SHORT);
  }
  if (token.length > arg->max_length) {
    return bsc_args_fail(out_args, error, BSC_ARG_PARSE_ERROR_TEXT_TOO_LONG, arg_index, 0u,
                         BSC_STATUS_ARGUMENT_TOO_LONG);
  }
  value->type = arg->type;
  value->data.text_value = token;
  return BSC_STATUS_OK;
}

#if BSC_ENABLE_FLOAT
/** @brief Parse the compact decimal float grammar and enforce descriptor bounds. */
static bsc_status_t bsc_args_parse_float(bsc_string_view_t token,
                                         const bsc_arg_def_t *arg,
                                         bsc_arg_value_t *value,
                                         bsc_arg_parse_error_t *error,
                                         bsc_parsed_args_t *out_args,
                                         size_t arg_index) {
  float parsed = 0.0f;
  size_t offset = 0u;
  bsc_float_parse_error_t parse_status = bsc_float_parse_compact(token, &parsed, &offset);
  if (parse_status == BSC_FLOAT_PARSE_EMBEDDED_NUL) {
    return bsc_args_fail(out_args, error, BSC_ARG_PARSE_ERROR_EMBEDDED_NUL, arg_index, offset,
                         BSC_STATUS_INVALID_ARGUMENT_TYPE);
  }
  if (parse_status == BSC_FLOAT_PARSE_TOO_MANY_FRACTION_DIGITS) {
    return bsc_args_fail(out_args, error, BSC_ARG_PARSE_ERROR_TOO_MANY_FLOAT_FRACTION_DIGITS,
                         arg_index, offset, BSC_STATUS_INVALID_ARGUMENT_TYPE);
  }
  if (parse_status == BSC_FLOAT_PARSE_ABOVE_SUPPORTED_RANGE) {
    return bsc_args_fail(out_args, error, BSC_ARG_PARSE_ERROR_ABOVE_MAXIMUM, arg_index, offset,
                         BSC_STATUS_ARGUMENT_OUT_OF_RANGE);
  }
  if (parse_status == BSC_FLOAT_PARSE_BELOW_SUPPORTED_RANGE) {
    return bsc_args_fail(out_args, error, BSC_ARG_PARSE_ERROR_BELOW_MINIMUM, arg_index, offset,
                         BSC_STATUS_ARGUMENT_OUT_OF_RANGE);
  }
  if (parse_status != BSC_FLOAT_PARSE_OK) {
    return bsc_args_fail(out_args, error, BSC_ARG_PARSE_ERROR_INVALID_VALUE, arg_index, offset,
                         BSC_STATUS_INVALID_ARGUMENT_TYPE);
  }
  value->type = BSC_ARG_FLOAT;
  value->data.float_value = parsed;
  if (parsed < arg->min_float) {
    return bsc_args_fail(out_args, error, BSC_ARG_PARSE_ERROR_BELOW_MINIMUM, arg_index, 0u,
                         BSC_STATUS_ARGUMENT_OUT_OF_RANGE);
  }
  if (parsed > arg->max_float) {
    return bsc_args_fail(out_args, error, BSC_ARG_PARSE_ERROR_ABOVE_MAXIMUM, arg_index, 0u,
                         BSC_STATUS_ARGUMENT_OUT_OF_RANGE);
  }
  return BSC_STATUS_OK;
}
#endif

bsc_status_t bsc_parse_command_args(const bsc_command_t *command,
                                    const bsc_string_view_t *arg_tokens,
                                    size_t arg_token_count,
                                    bsc_parsed_args_t *out_args,
                                    bsc_arg_parse_error_t *error) {
  size_t index;
  bsc_status_t status;

  if (out_args != NULL) {
    bsc_parsed_args_clear(out_args);
  }
  if (error != NULL) {
    bsc_arg_parse_error_clear(error);
  }
  if (out_args == NULL || error == NULL || (arg_tokens == NULL && arg_token_count > 0u)) {
    return bsc_args_fail(out_args, error, BSC_ARG_PARSE_ERROR_INVALID_API, 0u, 0u,
                         BSC_STATUS_INTERNAL_ERROR);
  }

  status = bsc_args_validate_descriptor(command, error);
  if (status != BSC_STATUS_OK) {
    bsc_parsed_args_clear(out_args);
    return status;
  }
  if (arg_token_count < command->arg_count) {
    return bsc_args_fail(out_args, error, BSC_ARG_PARSE_ERROR_MISSING_ARGUMENT, arg_token_count, 0u,
                         BSC_STATUS_MISSING_ARGUMENT);
  }
  if (arg_token_count > command->arg_count) {
    return bsc_args_fail(out_args, error, BSC_ARG_PARSE_ERROR_EXTRA_ARGUMENT, command->arg_count, 0u,
                         BSC_STATUS_EXTRA_ARGUMENT);
  }

  for (index = 0u; index < command->arg_count; ++index) {
    const bsc_arg_def_t *arg = &command->args[index];
    bsc_arg_value_t *value = &out_args->values[index];
    switch (arg->type) {
    case BSC_ARG_INT:
      status = bsc_args_parse_int(arg_tokens[index], arg, value, error, out_args, index);
      break;
    case BSC_ARG_UINT:
      status = bsc_args_parse_uint(arg_tokens[index], arg, value, error, out_args, index);
      break;
    case BSC_ARG_FLOAT:
#if BSC_ENABLE_FLOAT
      status = bsc_args_parse_float(arg_tokens[index], arg, value, error, out_args, index);
#else
      status = bsc_args_fail(out_args, error, BSC_ARG_PARSE_ERROR_FLOAT_DISABLED, index, 0u,
                             BSC_STATUS_INVALID_DESCRIPTOR);
#endif
      break;
    case BSC_ARG_BOOL:
      status = bsc_args_parse_bool(arg_tokens[index], value, error, out_args, index);
      break;
    case BSC_ARG_ENUM:
      status = bsc_args_parse_enum(arg_tokens[index], arg, value, error, out_args, index);
      break;
    case BSC_ARG_STRING:
    case BSC_ARG_SECRET:
      status = bsc_args_parse_text(arg_tokens[index], arg, value, error, out_args, index);
      break;
    case BSC_ARG_NONE:
      status = bsc_args_fail(out_args, error, BSC_ARG_PARSE_ERROR_INVALID_DESCRIPTOR, index, 0u,
                             BSC_STATUS_INVALID_DESCRIPTOR);
      break;
    }
    if (status != BSC_STATUS_OK) {
      return status;
    }
  }
  out_args->count = command->arg_count;
  return BSC_STATUS_OK;
}

/** @brief Write one constant diagnostic fragment and propagate output truncation. */
static bsc_status_t bsc_args_write(bsc_output_t *output, const char *text) {
  return bsc_out_write(output, text);
}

/** @brief Write an argument-name diagnostic prefix when descriptor metadata is safe. */
static bsc_status_t bsc_args_write_arg_prefix(const bsc_command_t *command,
                                              const bsc_arg_parse_error_t *error,
                                              bsc_output_t *output) {
  bsc_status_t status;
  if (command == NULL || error == NULL || command->args == NULL || error->arg_index >= command->arg_count ||
      command->args[error->arg_index].name == NULL) {
    return bsc_args_write(output, "argument: ");
  }
  status = bsc_args_write(output, "argument '");
  if (status != BSC_STATUS_OK) {
    return status;
  }
  status = bsc_args_write(output, command->args[error->arg_index].name);
  if (status != BSC_STATUS_OK) {
    return status;
  }
  return bsc_args_write(output, "': ");
}

/** @brief Write the type-specific invalid-value diagnostic body. */
static bsc_status_t bsc_args_write_invalid_value(const bsc_command_t *command,
                                                 const bsc_arg_parse_error_t *error,
                                                 bsc_output_t *output) {
  bsc_arg_type_t type = BSC_ARG_NONE;
  bsc_status_t status = bsc_args_write_arg_prefix(command, error, output);
  if (status != BSC_STATUS_OK) {
    return status;
  }
  if (command != NULL && error != NULL && command->args != NULL && error->arg_index < command->arg_count) {
    type = command->args[error->arg_index].type;
  }
  switch (type) {
  case BSC_ARG_INT:
    return bsc_args_write(output, "expected a signed decimal integer");
  case BSC_ARG_UINT:
    return bsc_args_write(output, "expected an unsigned decimal integer");
  case BSC_ARG_FLOAT:
    return bsc_args_write(output, "expected a decimal number such as -12.5");
  case BSC_ARG_BOOL:
    return bsc_args_write(output, "expected on, off, true, false, 1, or 0");
  default:
    return bsc_args_write(output, "expected a valid value");
  }
}

/** @brief Return the configured compact-float fractional digit limit as a one-byte string.
 *
 * BSC_MAX_FLOAT_FRACTION_DIGITS is compile-time validated to 1..6. Returning
 * string literals avoids printf-style formatting, heap allocation, and mutable
 * scratch while keeping operator diagnostics consistent with configuration.
 */
static const char *bsc_args_fraction_digit_limit_text(void) {
  switch ((size_t)BSC_MAX_FLOAT_FRACTION_DIGITS) {
  case 1u:
    return "1";
  case 2u:
    return "2";
  case 3u:
    return "3";
  case 4u:
    return "4";
  case 5u:
    return "5";
  default:
    return "6";
  }
}

/** @brief Write all enum choice names in descriptor order without copying token text. */
static bsc_status_t bsc_args_write_enum_choices(const bsc_command_t *command,
                                                const bsc_arg_parse_error_t *error,
                                                bsc_output_t *output) {
  size_t index;
  bsc_status_t status = bsc_args_write_arg_prefix(command, error, output);
  const bsc_arg_def_t *arg;
  if (status != BSC_STATUS_OK) {
    return status;
  }
  status = bsc_args_write(output, "expected one of: ");
  if (status != BSC_STATUS_OK) {
    return status;
  }
  if (command == NULL || error == NULL || command->args == NULL || error->arg_index >= command->arg_count) {
    return bsc_args_write(output, "<unknown>");
  }
  arg = &command->args[error->arg_index];
  for (index = 0u; index < arg->enum_choice_count; ++index) {
    if (index > 0u) {
      status = bsc_args_write(output, ", ");
      if (status != BSC_STATUS_OK) {
        return status;
      }
    }
    status = bsc_args_write(output, arg->enum_choices[index].name == NULL ? "<invalid>" : arg->enum_choices[index].name);
    if (status != BSC_STATUS_OK) {
      return status;
    }
  }
  return BSC_STATUS_OK;
}

bsc_status_t bsc_arg_parse_error_write(const bsc_command_t *command,
                                       const bsc_arg_parse_error_t *error,
                                       bsc_output_t *output) {
  bsc_status_t status;
  if (error == NULL) {
    return bsc_args_write(output, "command argument parser was called incorrectly");
  }
  switch (error->reason) {
  case BSC_ARG_PARSE_ERROR_NONE:
    return BSC_STATUS_OK;
  case BSC_ARG_PARSE_ERROR_MISSING_ARGUMENT:
    if (command != NULL && command->args != NULL && error->arg_index < command->arg_count &&
        command->args[error->arg_index].name != NULL) {
      status = bsc_args_write(output, "missing required argument '");
      if (status != BSC_STATUS_OK) {
        return status;
      }
      status = bsc_args_write(output, command->args[error->arg_index].name);
      if (status != BSC_STATUS_OK) {
        return status;
      }
      return bsc_args_write(output, "'");
    }
    return bsc_args_write(output, "missing required argument");
  case BSC_ARG_PARSE_ERROR_EXTRA_ARGUMENT:
    return bsc_args_write(output, "unexpected extra argument");
  case BSC_ARG_PARSE_ERROR_INVALID_VALUE:
    return bsc_args_write_invalid_value(command, error, output);
  case BSC_ARG_PARSE_ERROR_TOO_MANY_FLOAT_FRACTION_DIGITS:
    status = bsc_args_write_arg_prefix(command, error, output);
    if (status != BSC_STATUS_OK) {
      return status;
    }
    status = bsc_args_write(output, "expected at most ");
    if (status != BSC_STATUS_OK) {
      return status;
    }
    status = bsc_args_write(output, bsc_args_fraction_digit_limit_text());
    if (status != BSC_STATUS_OK) {
      return status;
    }
    return bsc_args_write(output, " digits after the decimal point");
  case BSC_ARG_PARSE_ERROR_INVALID_ENUM_CHOICE:
    return bsc_args_write_enum_choices(command, error, output);
  case BSC_ARG_PARSE_ERROR_BELOW_MINIMUM:
    status = bsc_args_write_arg_prefix(command, error, output);
    return status == BSC_STATUS_OK ? bsc_args_write(output, "value is below the configured minimum") : status;
  case BSC_ARG_PARSE_ERROR_ABOVE_MAXIMUM:
    status = bsc_args_write_arg_prefix(command, error, output);
    return status == BSC_STATUS_OK ? bsc_args_write(output, "value is above the configured maximum") : status;
  case BSC_ARG_PARSE_ERROR_TEXT_TOO_SHORT:
    status = bsc_args_write_arg_prefix(command, error, output);
    return status == BSC_STATUS_OK ? bsc_args_write(output, "text is shorter than the configured minimum length") : status;
  case BSC_ARG_PARSE_ERROR_TEXT_TOO_LONG:
    status = bsc_args_write_arg_prefix(command, error, output);
    return status == BSC_STATUS_OK ? bsc_args_write(output, "text is longer than the configured maximum length") : status;
  case BSC_ARG_PARSE_ERROR_EMBEDDED_NUL:
    status = bsc_args_write_arg_prefix(command, error, output);
    return status == BSC_STATUS_OK ? bsc_args_write(output, "embedded NUL byte is not allowed") : status;
  case BSC_ARG_PARSE_ERROR_FLOAT_DISABLED:
    status = bsc_args_write_arg_prefix(command, error, output);
    return status == BSC_STATUS_OK ? bsc_args_write(output, "floating-point arguments are disabled in this build") : status;
  case BSC_ARG_PARSE_ERROR_INVALID_DESCRIPTOR:
    return bsc_args_write(output, "command argument configuration is invalid");
  case BSC_ARG_PARSE_ERROR_INVALID_API:
    return bsc_args_write(output, "command argument parser was called incorrectly");
  default:
    return bsc_args_write(output, "command argument parser was called incorrectly");
  }
}
