#include "bsc_help.h"

#include "bsc_config.h"

/** @brief Clear all catalog diagnostic fields without retaining caller metadata. */
void bsc_help_catalog_validation_error_clear(bsc_help_catalog_validation_error_t *error) {
  if (error == NULL) {
    return;
  }
  error->reason = BSC_HELP_CATALOG_ERROR_NONE;
  error->target_index = 0u;
  error->topic_index = 0u;
  error->item_index = 0u;
  error->example_index = 0u;
  error->related_index = 0u;
  error->command_index = 0u;
  error->duplicate_index = 0u;
  bsc_registry_validation_error_clear(&error->registry_error);
}

/** @brief Populate a catalog structural diagnostic and return INVALID_DESCRIPTOR. */
static bsc_status_t bsc_help_catalog_fail(bsc_help_catalog_validation_error_t *error,
                                          bsc_help_catalog_error_reason_t reason,
                                          size_t target_index,
                                          size_t topic_index,
                                          size_t item_index,
                                          size_t example_index,
                                          size_t related_index,
                                          size_t command_index,
                                          size_t duplicate_index) {
  if (error != NULL) {
    error->reason = reason;
    error->target_index = target_index;
    error->topic_index = topic_index;
    error->item_index = item_index;
    error->example_index = example_index;
    error->related_index = related_index;
    error->command_index = command_index;
    error->duplicate_index = duplicate_index;
  }
  return BSC_STATUS_INVALID_DESCRIPTOR;
}

/** @brief Return nonzero when a byte is disallowed in rendered/help metadata. */
static int bsc_help_catalog_is_control(unsigned char byte) { return byte < 0x20u || byte == 0x7fu; }

/** @brief Validate a required or optional text string with a caller-supplied byte bound. */
static bsc_status_t bsc_help_catalog_validate_string(const char *text,
                                                     int required,
                                                     size_t limit,
                                                     bsc_help_catalog_error_reason_t missing_reason,
                                                     bsc_help_catalog_error_reason_t empty_reason,
                                                     bsc_help_catalog_error_reason_t too_long_reason,
                                                     bsc_help_catalog_error_reason_t control_reason,
                                                     size_t target_index,
                                                     size_t topic_index,
                                                     size_t item_index,
                                                     size_t example_index,
                                                     size_t related_index,
                                                     bsc_help_catalog_validation_error_t *error) {
  size_t index;
  if (text == NULL) {
    return required ? bsc_help_catalog_fail(error, missing_reason, target_index, topic_index, item_index, example_index,
                                            related_index, 0u, 0u)
                    : BSC_STATUS_OK;
  }
  for (index = 0u; index <= limit; ++index) {
    unsigned char byte = (unsigned char)text[index];
    if (byte == '\0') {
      if (index == 0u) {
        return bsc_help_catalog_fail(error, empty_reason, target_index, topic_index, item_index, example_index,
                                     related_index, 0u, 0u);
      }
      return BSC_STATUS_OK;
    }
    if (bsc_help_catalog_is_control(byte)) {
      return bsc_help_catalog_fail(error, control_reason, target_index, topic_index, item_index, example_index,
                                   related_index, 0u, 0u);
    }
  }
  return bsc_help_catalog_fail(error, too_long_reason, target_index, topic_index, item_index, example_index,
                               related_index, 0u, 0u);
}

/** @brief Resolve a descriptor pointer by exact equality against authoritative command elements. */
static bsc_status_t bsc_help_catalog_resolve_command(const bsc_help_catalog_t *catalog,
                                                     const bsc_command_t *reference,
                                                     bsc_help_catalog_error_reason_t reason,
                                                     size_t target_index,
                                                     size_t topic_index,
                                                     size_t related_index,
                                                     size_t *resolved_index,
                                                     bsc_help_catalog_validation_error_t *error) {
  size_t index;
  if (resolved_index != NULL) {
    *resolved_index = 0u;
  }
  if (reference == NULL) {
    return bsc_help_catalog_fail(error, reason, target_index, topic_index, 0u, 0u, related_index, 0u, 0u);
  }
  for (index = 0u; index < catalog->command_count; ++index) {
    if (reference == &catalog->commands[index]) {
      if (resolved_index != NULL) {
        *resolved_index = index;
      }
      return BSC_STATUS_OK;
    }
  }
  return bsc_help_catalog_fail(error, reason, target_index, topic_index, 0u, 0u, related_index, 0u, 0u);
}

/** @brief Compare two flat topic identifiers with ASCII case folding. */
static int bsc_help_catalog_text_equal_ignore_case(const char *left, const char *right) {
  size_t index = 0u;
  for (;;) {
    unsigned char a = (unsigned char)left[index];
    unsigned char b = (unsigned char)right[index];
    if (a >= (unsigned char)'A' && a <= (unsigned char)'Z') {
      a = (unsigned char)(a + ((unsigned char)'a' - (unsigned char)'A'));
    }
    if (b >= (unsigned char)'A' && b <= (unsigned char)'Z') {
      b = (unsigned char)(b + ((unsigned char)'a' - (unsigned char)'A'));
    }
    if (a != b) {
      return 0;
    }
    if (a == '\0') {
      return 1;
    }
    ++index;
  }
}

/** @brief Validate one notes or warnings list. */
static bsc_status_t bsc_help_catalog_validate_text_list(const bsc_help_text_list_t *list,
                                                        size_t target_index,
                                                        size_t topic_index,
                                                        bsc_help_catalog_validation_error_t *error) {
  size_t index;
  if (list->count > 0u && list->items == NULL) {
    return bsc_help_catalog_fail(error, BSC_HELP_CATALOG_ERROR_TEXT_LIST_POINTER_COUNT, target_index, topic_index, 0u,
                                 0u, 0u, 0u, 0u);
  }
  if (list->count > (size_t)BSC_MAX_HELP_TEXT_ITEMS) {
    return bsc_help_catalog_fail(error, BSC_HELP_CATALOG_ERROR_TOO_MANY_TEXT_ITEMS, target_index, topic_index, 0u,
                                 0u, 0u, 0u, 0u);
  }
  for (index = 0u; index < list->count; ++index) {
    bsc_status_t status = bsc_help_catalog_validate_string(list->items[index], 1, (size_t)BSC_MAX_HELP_TEXT_LEN,
                                                           BSC_HELP_CATALOG_ERROR_MISSING_TEXT,
                                                           BSC_HELP_CATALOG_ERROR_EMPTY_TEXT,
                                                           BSC_HELP_CATALOG_ERROR_TEXT_TOO_LONG,
                                                           BSC_HELP_CATALOG_ERROR_INVALID_TEXT_CONTROL_BYTE,
                                                           target_index, topic_index, index, 0u, 0u, error);
    if (status != BSC_STATUS_OK) {
      return status;
    }
  }
  return BSC_STATUS_OK;
}

/** @brief Validate presentation examples without parsing, matching, or secret inspection. */
static bsc_status_t bsc_help_catalog_validate_examples(const bsc_help_example_t *examples,
                                                       size_t example_count,
                                                       size_t target_index,
                                                       size_t topic_index,
                                                       bsc_help_catalog_validation_error_t *error) {
  size_t index;
  if (example_count > 0u && examples == NULL) {
    return bsc_help_catalog_fail(error, BSC_HELP_CATALOG_ERROR_EXAMPLES_POINTER_COUNT, target_index, topic_index, 0u,
                                 0u, 0u, 0u, 0u);
  }
  if (example_count > (size_t)BSC_MAX_HELP_EXAMPLES) {
    return bsc_help_catalog_fail(error, BSC_HELP_CATALOG_ERROR_TOO_MANY_EXAMPLES, target_index, topic_index, 0u,
                                 0u, 0u, 0u, 0u);
  }
  for (index = 0u; index < example_count; ++index) {
    bsc_status_t status = bsc_help_catalog_validate_string(examples[index].line, 1, (size_t)BSC_MAX_LINE_LEN,
                                                           BSC_HELP_CATALOG_ERROR_MISSING_EXAMPLE_LINE,
                                                           BSC_HELP_CATALOG_ERROR_EMPTY_EXAMPLE_LINE,
                                                           BSC_HELP_CATALOG_ERROR_EXAMPLE_LINE_TOO_LONG,
                                                           BSC_HELP_CATALOG_ERROR_INVALID_EXAMPLE_LINE_CONTROL_BYTE,
                                                           target_index, topic_index, 0u, index, 0u, error);
    if (status != BSC_STATUS_OK) {
      return status;
    }
    status = bsc_help_catalog_validate_string(examples[index].description, 0, (size_t)BSC_MAX_HELP_TEXT_LEN,
                                              BSC_HELP_CATALOG_ERROR_INVALID_EXAMPLE_DESCRIPTION,
                                              BSC_HELP_CATALOG_ERROR_INVALID_EXAMPLE_DESCRIPTION,
                                              BSC_HELP_CATALOG_ERROR_INVALID_EXAMPLE_DESCRIPTION,
                                              BSC_HELP_CATALOG_ERROR_INVALID_EXAMPLE_DESCRIPTION,
                                              target_index, topic_index, 0u, index, 0u, error);
    if (status != BSC_STATUS_OK) {
      return status;
    }
  }
  return BSC_STATUS_OK;
}

/** @brief Validate one related-command list, allowing cycles but rejecting duplicates and prohibited self-reference. */
static bsc_status_t bsc_help_catalog_validate_related(const bsc_help_catalog_t *catalog,
                                                      const bsc_help_related_t *related,
                                                      size_t related_count,
                                                      const bsc_command_t *self,
                                                      int reject_self,
                                                      size_t target_index,
                                                      size_t topic_index,
                                                      bsc_help_catalog_validation_error_t *error) {
  size_t index;
  if (related_count > 0u && related == NULL) {
    return bsc_help_catalog_fail(error, BSC_HELP_CATALOG_ERROR_RELATED_POINTER_COUNT, target_index, topic_index, 0u,
                                 0u, 0u, 0u, 0u);
  }
  if (related_count > (size_t)BSC_MAX_HELP_RELATED) {
    return bsc_help_catalog_fail(error, BSC_HELP_CATALOG_ERROR_TOO_MANY_RELATED, target_index, topic_index, 0u,
                                 0u, 0u, 0u, 0u);
  }
  for (index = 0u; index < related_count; ++index) {
    size_t command_index;
    bsc_status_t status = bsc_help_catalog_resolve_command(catalog, related[index].target,
                                                           BSC_HELP_CATALOG_ERROR_INVALID_RELATED_DESCRIPTOR,
                                                           target_index, topic_index, index, &command_index, error);
    size_t compare_index;
    if (status != BSC_STATUS_OK) {
      return status;
    }
    if (reject_self && related[index].target == self) {
      return bsc_help_catalog_fail(error, BSC_HELP_CATALOG_ERROR_TARGET_RELATED_SELF_REFERENCE, target_index,
                                   topic_index, 0u, 0u, index, command_index, 0u);
    }
    for (compare_index = 0u; compare_index < index; ++compare_index) {
      if (related[compare_index].target == related[index].target) {
        return bsc_help_catalog_fail(error, BSC_HELP_CATALOG_ERROR_DUPLICATE_RELATED_DESCRIPTOR, target_index,
                                     topic_index, 0u, 0u, index, command_index, compare_index);
      }
    }
  }
  return BSC_STATUS_OK;
}

/** @brief Validate one target metadata record. */
static bsc_status_t bsc_help_catalog_validate_target(const bsc_help_catalog_t *catalog,
                                                     size_t target_index,
                                                     bsc_help_catalog_validation_error_t *error) {
  const bsc_help_target_t *target = &catalog->targets[target_index];
  size_t command_index;
  bsc_status_t status = bsc_help_catalog_resolve_command(catalog, target->target,
                                                         BSC_HELP_CATALOG_ERROR_INVALID_DESCRIPTOR_REFERENCE,
                                                         target_index, 0u, 0u, &command_index, error);
  if (status != BSC_STATUS_OK) return status;
  status = bsc_help_catalog_validate_text_list(&target->notes, target_index, 0u, error);
  if (status != BSC_STATUS_OK) return status;
  status = bsc_help_catalog_validate_text_list(&target->warnings, target_index, 0u, error);
  if (status != BSC_STATUS_OK) return status;
  status = bsc_help_catalog_validate_examples(target->examples, target->example_count, target_index, 0u, error);
  if (status != BSC_STATUS_OK) return status;
  return bsc_help_catalog_validate_related(catalog, target->related, target->related_count, target->target, 1,
                                           target_index, 0u, error);
}

/** @brief Validate one flat non-executable topic record. */
static bsc_status_t bsc_help_catalog_validate_topic(const bsc_help_catalog_t *catalog,
                                                    size_t topic_index,
                                                    bsc_help_catalog_validation_error_t *error) {
  const bsc_help_topic_t *topic = &catalog->topics[topic_index];
  size_t parent_index;
  bsc_status_t status = bsc_help_catalog_resolve_command(catalog, topic->parent,
                                                         BSC_HELP_CATALOG_ERROR_INVALID_DESCRIPTOR_REFERENCE,
                                                         0u, topic_index, 0u, &parent_index, error);
  if (status != BSC_STATUS_OK) return status;
  status = bsc_help_catalog_validate_string(topic->id, 1, (size_t)BSC_MAX_TOKEN_LEN,
                                            BSC_HELP_CATALOG_ERROR_MISSING_TOPIC_ID,
                                            BSC_HELP_CATALOG_ERROR_EMPTY_TOPIC_ID,
                                            BSC_HELP_CATALOG_ERROR_TOPIC_ID_TOO_LONG,
                                            BSC_HELP_CATALOG_ERROR_INVALID_TOPIC_ID_CONTROL_BYTE,
                                            0u, topic_index, 0u, 0u, 0u, error);
  if (status != BSC_STATUS_OK) return status;
  status = bsc_help_catalog_validate_string(topic->summary, 1, (size_t)BSC_MAX_HELP_TEXT_LEN,
                                            BSC_HELP_CATALOG_ERROR_MISSING_TOPIC_SUMMARY,
                                            BSC_HELP_CATALOG_ERROR_EMPTY_TOPIC_SUMMARY,
                                            BSC_HELP_CATALOG_ERROR_TOPIC_SUMMARY_TOO_LONG,
                                            BSC_HELP_CATALOG_ERROR_INVALID_TOPIC_SUMMARY_CONTROL_BYTE,
                                            0u, topic_index, 0u, 0u, 0u, error);
  if (status != BSC_STATUS_OK) return status;
  status = bsc_help_catalog_validate_string(topic->description, 0, (size_t)BSC_MAX_HELP_TEXT_LEN,
                                            BSC_HELP_CATALOG_ERROR_INVALID_TOPIC_DESCRIPTION,
                                            BSC_HELP_CATALOG_ERROR_INVALID_TOPIC_DESCRIPTION,
                                            BSC_HELP_CATALOG_ERROR_INVALID_TOPIC_DESCRIPTION,
                                            BSC_HELP_CATALOG_ERROR_INVALID_TOPIC_DESCRIPTION,
                                            0u, topic_index, 0u, 0u, 0u, error);
  if (status != BSC_STATUS_OK) return status;
  status = bsc_help_catalog_validate_text_list(&topic->notes, 0u, topic_index, error);
  if (status != BSC_STATUS_OK) return status;
  status = bsc_help_catalog_validate_text_list(&topic->warnings, 0u, topic_index, error);
  if (status != BSC_STATUS_OK) return status;
  status = bsc_help_catalog_validate_examples(topic->examples, topic->example_count, 0u, topic_index, error);
  if (status != BSC_STATUS_OK) return status;
  return bsc_help_catalog_validate_related(catalog, topic->related, topic->related_count, topic->parent, 0,
                                           0u, topic_index, error);
}

bsc_status_t bsc_help_catalog_validate(const bsc_help_catalog_t *catalog,
                                        bsc_help_catalog_validation_error_t *error) {
  bsc_registry_validation_error_t registry_error;
  bsc_status_t status;
  size_t index;

  bsc_help_catalog_validation_error_clear(error);
  bsc_registry_validation_error_clear(&registry_error);
  if (catalog == NULL) {
    return bsc_help_catalog_fail(error, BSC_HELP_CATALOG_ERROR_NULL_CATALOG, 0u, 0u, 0u, 0u, 0u, 0u, 0u);
  }
  status = bsc_registry_validate(catalog->commands, catalog->command_count, &registry_error);
  if (status != BSC_STATUS_OK) {
    if (error != NULL) {
      error->reason = BSC_HELP_CATALOG_ERROR_REGISTRY_INVALID;
      error->registry_error = registry_error;
    }
    return BSC_STATUS_INVALID_DESCRIPTOR;
  }
  if (catalog->target_count > 0u && catalog->targets == NULL) {
    return bsc_help_catalog_fail(error, BSC_HELP_CATALOG_ERROR_TARGETS_POINTER_COUNT, 0u, 0u, 0u, 0u, 0u, 0u, 0u);
  }
  if (catalog->topic_count > 0u && catalog->topics == NULL) {
    return bsc_help_catalog_fail(error, BSC_HELP_CATALOG_ERROR_TOPICS_POINTER_COUNT, 0u, 0u, 0u, 0u, 0u, 0u, 0u);
  }
  if (catalog->target_count > catalog->command_count) {
    return bsc_help_catalog_fail(error, BSC_HELP_CATALOG_ERROR_TOO_MANY_TARGETS, 0u, 0u, 0u, 0u, 0u, 0u, 0u);
  }
  if (catalog->topic_count > (size_t)BSC_MAX_HELP_TOPICS) {
    return bsc_help_catalog_fail(error, BSC_HELP_CATALOG_ERROR_TOO_MANY_TOPICS, 0u, 0u, 0u, 0u, 0u, 0u, 0u);
  }
  for (index = 0u; index < catalog->target_count; ++index) {
    size_t compare_index;
    status = bsc_help_catalog_validate_target(catalog, index, error);
    if (status != BSC_STATUS_OK) return status;
    for (compare_index = 0u; compare_index < index; ++compare_index) {
      if (catalog->targets[compare_index].target == catalog->targets[index].target) {
        size_t command_index = 0u;
        (void)bsc_help_catalog_resolve_command(catalog, catalog->targets[index].target,
                                               BSC_HELP_CATALOG_ERROR_INVALID_DESCRIPTOR_REFERENCE,
                                               index, 0u, 0u, &command_index, NULL);
        return bsc_help_catalog_fail(error, BSC_HELP_CATALOG_ERROR_DUPLICATE_TARGET, index, 0u, 0u, 0u, 0u,
                                     command_index, compare_index);
      }
    }
  }
  for (index = 0u; index < catalog->topic_count; ++index) {
    size_t compare_index;
    status = bsc_help_catalog_validate_topic(catalog, index, error);
    if (status != BSC_STATUS_OK) return status;
    for (compare_index = 0u; compare_index < index; ++compare_index) {
      if (catalog->topics[compare_index].parent == catalog->topics[index].parent &&
          bsc_help_catalog_text_equal_ignore_case(catalog->topics[compare_index].id, catalog->topics[index].id)) {
        return bsc_help_catalog_fail(error, BSC_HELP_CATALOG_ERROR_DUPLICATE_TOPIC, 0u, index, 0u, 0u, 0u,
                                     0u, compare_index);
      }
    }
  }
  return BSC_STATUS_OK;
}
