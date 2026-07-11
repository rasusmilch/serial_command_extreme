#include "bsc_dispatch.h"

/** @brief Return whether a status is one of the stable public enumerators. */
static int bsc_dispatch_status_is_valid(bsc_status_t status) {
  switch (status) {
  case BSC_STATUS_OK:
  case BSC_STATUS_NO_INPUT:
  case BSC_STATUS_LINE_TOO_LONG:
  case BSC_STATUS_TOKEN_TOO_LONG:
  case BSC_STATUS_TOO_MANY_TOKENS:
  case BSC_STATUS_UNTERMINATED_QUOTE:
  case BSC_STATUS_INVALID_SYNTAX:
  case BSC_STATUS_UNKNOWN_COMMAND:
  case BSC_STATUS_AMBIGUOUS_COMMAND:
  case BSC_STATUS_GROUP_REQUIRES_SUBCOMMAND:
  case BSC_STATUS_MISSING_ARGUMENT:
  case BSC_STATUS_EXTRA_ARGUMENT:
  case BSC_STATUS_INVALID_ARGUMENT_TYPE:
  case BSC_STATUS_ARGUMENT_OUT_OF_RANGE:
  case BSC_STATUS_ARGUMENT_TOO_LONG:
  case BSC_STATUS_INVALID_ENUM_VALUE:
  case BSC_STATUS_INVALID_DESCRIPTOR:
  case BSC_STATUS_ACCESS_DENIED:
  case BSC_STATUS_OUTPUT_TRUNCATED:
  case BSC_STATUS_APP_ERROR:
  case BSC_STATUS_INTERNAL_ERROR:
  case BSC_STATUS_ARGUMENT_TOO_SHORT:
    return 1;
  default:
    return 0;
  }
}

/** @brief Return whether an access level is supported by the dispatch policy. */
static int bsc_dispatch_access_is_valid(bsc_access_level_t access) {
  return access == BSC_ACCESS_NORMAL || access == BSC_ACCESS_ADVANCED || access == BSC_ACCESS_FACTORY ||
         access == BSC_ACCESS_LOCKED;
}

/** @brief Return whether descriptor flags contain only currently supported bits. */
static int bsc_dispatch_flags_are_valid(bsc_command_flags_t flags) {
  return (flags & ~BSC_COMMAND_FLAG_HIDDEN) == BSC_COMMAND_FLAG_NONE;
}

/** @brief Apply the default no-callback access matrix for one valid level. */
static int bsc_dispatch_default_access_allows(bsc_access_level_t access) {
  return access == BSC_ACCESS_NORMAL || access == BSC_ACCESS_ADVANCED;
}

/** @brief Validate shallow executable-command fields read before access parsing. */
static bsc_status_t bsc_dispatch_validate_command(const bsc_command_t *command) {
  if (command->node_type != BSC_NODE_COMMAND || command->handler == NULL ||
      !bsc_dispatch_access_is_valid(command->access) || !bsc_dispatch_flags_are_valid(command->flags) ||
      command->arg_count > (size_t)BSC_MAX_ARGS || (command->arg_count > 0u && command->args == NULL)) {
    return BSC_STATUS_INVALID_DESCRIPTOR;
  }
  return BSC_STATUS_OK;
}

/** @brief Normalize handler status values to the public dispatch contract. */
static bsc_status_t bsc_dispatch_normalize_handler_status(bsc_status_t status) {
  return bsc_dispatch_status_is_valid(status) ? status : BSC_STATUS_APP_ERROR;
}

bsc_status_t bsc_dispatch_command(void *app_context,
                                  const bsc_command_t *command,
                                  const bsc_string_view_t *arg_tokens,
                                  size_t arg_token_count,
                                  bsc_parsed_args_t *parsed_args,
                                  bsc_arg_parse_error_t *parse_error,
                                  bsc_output_t *output) {
  bsc_status_t status;
  int access_allowed;

  if (parsed_args != NULL) {
    bsc_parsed_args_clear(parsed_args);
  }
  if (parse_error != NULL) {
    bsc_arg_parse_error_clear(parse_error);
  }

  if (command == NULL || parsed_args == NULL || parse_error == NULL || (arg_token_count > 0u && arg_tokens == NULL)) {
    return BSC_STATUS_INTERNAL_ERROR;
  }

  status = bsc_dispatch_validate_command(command);
  if (status != BSC_STATUS_OK) {
    return status;
  }

  if (command->access_fn != NULL) {
    access_allowed = command->access_fn(app_context, command, command->access) ? 1 : 0;
  } else {
    access_allowed = bsc_dispatch_default_access_allows(command->access);
  }
  if (!access_allowed) {
    return BSC_STATUS_ACCESS_DENIED;
  }

  status = bsc_parse_command_args(command, arg_tokens, arg_token_count, parsed_args, parse_error);
  if (status != BSC_STATUS_OK) {
    return status;
  }

  return bsc_dispatch_normalize_handler_status(command->handler(app_context, command, parsed_args, output));
}
