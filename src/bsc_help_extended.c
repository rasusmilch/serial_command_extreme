#include "bsc_help.h"

#include "bsc_config.h"
#include "internal/bsc_help_internal.h"

#include "bsc_string_view.h"

/** @brief Compile-time length for internal string literals only. */
#define BSC_HELP_EXT_LITERAL_LEN(value) (sizeof(value) - 1u)

/** @brief Write one compile-time literal through the shared help writer. */
#define BSC_HELP_EXT_WRITE_LITERAL(output, literal)                                                \
  bsc_help_internal_write((output), (literal), BSC_HELP_EXT_LITERAL_LEN(literal))

/** @brief Clear borrowed topic lookup result storage without retaining pointers. */
void bsc_help_topic_lookup_result_clear(bsc_help_topic_lookup_result_t *result) {
  if (result != NULL) {
    result->topic = NULL;
    result->topic_index = 0u;
    result->parent_command_index = 0u;
  }
}

/**
 * @brief Return the bounded length of a catalog-validated example line.
 *
 * The caller must run catalog validation before rendering. This helper scans
 * only within #BSC_MAX_LINE_LEN, owns no storage, and retains no pointers.
 */
static size_t bsc_help_ext_example_line_length(const char *text) {
  size_t length = 0u;
  while (length <= (size_t)BSC_MAX_LINE_LEN && text[length] != '\0') {
    length += 1u;
  }
  return length;
}

/**
 * @brief Scan validated catalog topics for one parent and explicit topic id.
 *
 * @param catalog Required borrowed catalog already structurally validated by the caller.
 * @param parent Required borrowed parent descriptor already resolved from catalog->commands.
 * @param parent_command_index Zero-based parent descriptor index in catalog->commands.
 * @param topic_id Required nonempty explicit-length topic id with non-NULL data.
 * @param result Required caller-owned result storage, left cleared unless a match is found.
 * @retval BSC_STATUS_OK One topic matched and result was populated with borrowed catalog metadata.
 * @retval BSC_STATUS_UNKNOWN_TOPIC No matching topic exists under the exact parent descriptor.
 *
 * The helper preserves catalog order, compares parents by exact pointer identity,
 * compares topic ids with ASCII-only case folding, owns no storage, allocates no
 * heap, retains no pointers after return except through caller result storage,
 * emits no output, and never invokes handlers, access callbacks, matcher,
 * parser, dispatcher, or console code.
 */
static bsc_status_t bsc_help_find_topic_record(const bsc_help_catalog_t *catalog,
                                               const bsc_command_t *parent,
                                               size_t parent_command_index,
                                               bsc_string_view_t topic_id,
                                               bsc_help_topic_lookup_result_t *result) {
  size_t topic_index;
  for (topic_index = 0u; topic_index < catalog->topic_count; ++topic_index) {
    const bsc_help_topic_t *topic = &catalog->topics[topic_index];
    if (topic->parent == parent && bsc_string_view_equals_cstr_ignore_case(topic_id, topic->id)) {
      result->topic = topic;
      result->topic_index = topic_index;
      result->parent_command_index = parent_command_index;
      return BSC_STATUS_OK;
    }
  }
  return BSC_STATUS_UNKNOWN_TOPIC;
}

/** @brief Validate catalog/help metadata and resolve one visible parent-scoped flat topic. */
bsc_status_t bsc_help_find_topic(const bsc_help_catalog_t *catalog,
                                 const bsc_string_view_t *parent_path_tokens,
                                 size_t parent_path_token_count,
                                 bsc_string_view_t topic_id,
                                 const bsc_help_options_t *options,
                                 bsc_help_topic_lookup_result_t *result) {
  bsc_help_lookup_result_t parent_result;
  bsc_status_t status;

  bsc_help_topic_lookup_result_clear(result);
  if (parent_path_token_count == 0u) {
    return BSC_STATUS_NO_INPUT;
  }
  if (topic_id.length == 0u) {
    return BSC_STATUS_NO_INPUT;
  }
  if (catalog == NULL || parent_path_tokens == NULL || result == NULL || topic_id.data == NULL) {
    return BSC_STATUS_INTERNAL_ERROR;
  }
  status = bsc_help_catalog_validate(catalog, NULL);
  if (status != BSC_STATUS_OK) {
    return status;
  }
  status = bsc_help_validate(catalog->commands, catalog->command_count, options, NULL);
  if (status != BSC_STATUS_OK) {
    return status;
  }
  status = bsc_help_internal_find_path_validated(catalog->commands,
                                                 catalog->command_count,
                                                 parent_path_tokens,
                                                 parent_path_token_count,
                                                 options,
                                                 &parent_result);
  if (status != BSC_STATUS_OK) {
    return status;
  }
  return bsc_help_find_topic_record(catalog, parent_result.command, parent_result.command_index, topic_id, result);
}

/** @brief Return target metadata for one descriptor, or NULL when no metadata applies. */
static const bsc_help_target_t *bsc_help_ext_find_target(const bsc_help_catalog_t *catalog,
                                                         const bsc_command_t *command) {
  size_t index;
  for (index = 0u; index < catalog->target_count; ++index) {
    if (catalog->targets[index].target == command) {
      return &catalog->targets[index];
    }
  }
  return NULL;
}

/** @brief Count topics whose parent exactly equals one descriptor. */
static size_t bsc_help_ext_count_topics(const bsc_help_catalog_t *catalog, const bsc_command_t *parent) {
  size_t count = 0u;
  size_t index;
  for (index = 0u; index < catalog->topic_count; ++index) {
    if (catalog->topics[index].parent == parent) {
      count += 1u;
    }
  }
  return count;
}

/** @brief Count related descriptors visible under static help visibility. */
static size_t bsc_help_ext_count_visible_related(const bsc_help_related_t *related,
                                                 size_t related_count,
                                                 const bsc_help_options_t *options) {
  size_t count = 0u;
  size_t index;
  for (index = 0u; index < related_count; ++index) {
    if (bsc_help_internal_is_visible(related[index].target, options)) {
      count += 1u;
    }
  }
  return count;
}

/** @brief Render ordered note or warning prose entries. */
static bsc_status_t bsc_help_ext_render_text_list(bsc_output_t *output,
                                                  const char *heading,
                                                  size_t heading_length,
                                                  const bsc_help_text_list_t *list,
                                                  int *sections) {
  size_t index;
  bsc_status_t status;
  if (list->count == 0u) {
    return BSC_STATUS_OK;
  }
  status = bsc_help_internal_section(output, heading, heading_length, sections);
  if (status != BSC_STATUS_OK) return status;
  for (index = 0u; index < list->count; ++index) {
    status = BSC_HELP_EXT_WRITE_LITERAL(output, "  - ");
    if (status != BSC_STATUS_OK) return status;
    status = bsc_help_internal_write(output, list->items[index], bsc_help_internal_prose_length(list->items[index]));
    if (status != BSC_STATUS_OK) return status;
    status = BSC_HELP_EXT_WRITE_LITERAL(output, "\n");
    if (status != BSC_STATUS_OK) return status;
  }
  return BSC_STATUS_OK;
}

/** @brief Render ordered static presentation examples and optional descriptions. */
static bsc_status_t bsc_help_ext_render_examples(bsc_output_t *output,
                                                 const bsc_help_example_t *examples,
                                                 size_t example_count,
                                                 int *sections) {
  size_t index;
  bsc_status_t status;
  if (example_count == 0u) {
    return BSC_STATUS_OK;
  }
  status = bsc_help_internal_section(output, "EXAMPLES", BSC_HELP_EXT_LITERAL_LEN("EXAMPLES"), sections);
  if (status != BSC_STATUS_OK) return status;
  for (index = 0u; index < example_count; ++index) {
    status = BSC_HELP_EXT_WRITE_LITERAL(output, "  ");
    if (status != BSC_STATUS_OK) return status;
    status = bsc_help_internal_write(output, examples[index].line, bsc_help_ext_example_line_length(examples[index].line));
    if (status != BSC_STATUS_OK) return status;
    status = BSC_HELP_EXT_WRITE_LITERAL(output, "\n");
    if (status != BSC_STATUS_OK) return status;
    if (examples[index].description != NULL) {
      status = BSC_HELP_EXT_WRITE_LITERAL(output, "    ");
      if (status != BSC_STATUS_OK) return status;
      status = bsc_help_internal_write(output,
                                       examples[index].description,
                                       bsc_help_internal_prose_length(examples[index].description));
      if (status != BSC_STATUS_OK) return status;
      status = BSC_HELP_EXT_WRITE_LITERAL(output, "\n");
      if (status != BSC_STATUS_OK) return status;
    }
  }
  return BSC_STATUS_OK;
}

/** @brief Render ordered topics for one descriptor. */
static bsc_status_t bsc_help_ext_render_topics(bsc_output_t *output,
                                               const bsc_help_catalog_t *catalog,
                                               const bsc_command_t *parent,
                                               size_t topic_count,
                                               int *sections) {
  size_t index;
  bsc_status_t status;
  if (topic_count == 0u) {
    return BSC_STATUS_OK;
  }
  status = bsc_help_internal_section(output, "TOPICS", BSC_HELP_EXT_LITERAL_LEN("TOPICS"), sections);
  if (status != BSC_STATUS_OK) return status;
  for (index = 0u; index < catalog->topic_count; ++index) {
    const bsc_help_topic_t *topic = &catalog->topics[index];
    if (topic->parent != parent) {
      continue;
    }
    status = BSC_HELP_EXT_WRITE_LITERAL(output, "  ");
    if (status != BSC_STATUS_OK) return status;
    status = bsc_help_internal_write(output, topic->id, bsc_help_internal_identifier_length(topic->id));
    if (status != BSC_STATUS_OK) return status;
    status = BSC_HELP_EXT_WRITE_LITERAL(output, " - ");
    if (status != BSC_STATUS_OK) return status;
    status = bsc_help_internal_write(output, topic->summary, bsc_help_internal_prose_length(topic->summary));
    if (status != BSC_STATUS_OK) return status;
    status = BSC_HELP_EXT_WRITE_LITERAL(output, "\n");
    if (status != BSC_STATUS_OK) return status;
  }
  return BSC_STATUS_OK;
}

/** @brief Render ordered visible related descriptor entries. */
static bsc_status_t bsc_help_ext_render_related(bsc_output_t *output,
                                                const bsc_help_related_t *related,
                                                size_t related_count,
                                                size_t visible_count,
                                                const bsc_help_options_t *options,
                                                int *sections) {
  size_t index;
  bsc_status_t status;
  if (visible_count == 0u) {
    return BSC_STATUS_OK;
  }
  status = bsc_help_internal_section(output, "RELATED", BSC_HELP_EXT_LITERAL_LEN("RELATED"), sections);
  if (status != BSC_STATUS_OK) return status;
  for (index = 0u; index < related_count; ++index) {
    if (!bsc_help_internal_is_visible(related[index].target, options)) {
      continue;
    }
    status = bsc_help_internal_entry(output, related[index].target);
    if (status != BSC_STATUS_OK) return status;
  }
  return BSC_STATUS_OK;
}

/** @brief Render target metadata sections after the already-rendered base page. */
static bsc_status_t bsc_help_ext_render_target_sections(bsc_output_t *output,
                                                        const bsc_help_catalog_t *catalog,
                                                        const bsc_command_t *command,
                                                        const bsc_help_target_t *target,
                                                        size_t topic_count,
                                                        size_t visible_related_count,
                                                        const bsc_help_options_t *options,
                                                        int *sections) {
  bsc_status_t status;
  if (target != NULL) {
    status = bsc_help_ext_render_text_list(output, "NOTES", BSC_HELP_EXT_LITERAL_LEN("NOTES"), &target->notes, sections);
    if (status != BSC_STATUS_OK) return status;
    status = bsc_help_ext_render_text_list(output, "WARNINGS", BSC_HELP_EXT_LITERAL_LEN("WARNINGS"), &target->warnings,
                                           sections);
    if (status != BSC_STATUS_OK) return status;
    status = bsc_help_ext_render_examples(output, target->examples, target->example_count, sections);
    if (status != BSC_STATUS_OK) return status;
  }
  status = bsc_help_ext_render_topics(output, catalog, command, topic_count, sections);
  if (status != BSC_STATUS_OK) return status;
  if (target != NULL) {
    return bsc_help_ext_render_related(output, target->related, target->related_count, visible_related_count, options,
                                       sections);
  }
  return BSC_STATUS_OK;
}

/** @brief Render the topic NAME section with a colon separator between parent path and topic id. */
static bsc_status_t bsc_help_ext_render_topic_name(bsc_output_t *output,
                                                   const bsc_help_topic_t *topic,
                                                   int *sections) {
  bsc_status_t status = bsc_help_internal_section(output, "NAME", BSC_HELP_EXT_LITERAL_LEN("NAME"), sections);
  if (status != BSC_STATUS_OK) return status;
  status = BSC_HELP_EXT_WRITE_LITERAL(output, "  ");
  if (status != BSC_STATUS_OK) return status;
  status = bsc_help_internal_write_path(output, topic->parent);
  if (status != BSC_STATUS_OK) return status;
  status = BSC_HELP_EXT_WRITE_LITERAL(output, ": ");
  if (status != BSC_STATUS_OK) return status;
  status = bsc_help_internal_write(output, topic->id, bsc_help_internal_identifier_length(topic->id));
  if (status != BSC_STATUS_OK) return status;
  status = BSC_HELP_EXT_WRITE_LITERAL(output, " - ");
  if (status != BSC_STATUS_OK) return status;
  status = bsc_help_internal_write(output, topic->summary, bsc_help_internal_prose_length(topic->summary));
  if (status != BSC_STATUS_OK) return status;
  return BSC_HELP_EXT_WRITE_LITERAL(output, "\n");
}

/** @brief Render an optional topic description section. */
static bsc_status_t bsc_help_ext_render_topic_description(bsc_output_t *output,
                                                          const char *description,
                                                          int *sections) {
  bsc_status_t status;
  if (description == NULL) {
    return BSC_STATUS_OK;
  }
  status = bsc_help_internal_section(output, "DESCRIPTION", BSC_HELP_EXT_LITERAL_LEN("DESCRIPTION"), sections);
  if (status != BSC_STATUS_OK) return status;
  status = BSC_HELP_EXT_WRITE_LITERAL(output, "  ");
  if (status != BSC_STATUS_OK) return status;
  status = bsc_help_internal_write(output, description, bsc_help_internal_prose_length(description));
  if (status != BSC_STATUS_OK) return status;
  return BSC_HELP_EXT_WRITE_LITERAL(output, "\n");
}

bsc_status_t bsc_help_render_catalog_path(const bsc_help_catalog_t *catalog,
                                          const bsc_string_view_t *path_tokens,
                                          size_t path_token_count,
                                          const bsc_help_options_t *options,
                                          bsc_output_t *output) {
  bsc_help_lookup_result_t result;
  const bsc_help_target_t *target;
  size_t topic_count;
  size_t visible_related_count = 0u;
  bsc_status_t status;
  int sections = 0;

  if (path_token_count == 0u) {
    return BSC_STATUS_NO_INPUT;
  }
  if (catalog == NULL || path_tokens == NULL) {
    return BSC_STATUS_INTERNAL_ERROR;
  }
  status = bsc_help_catalog_validate(catalog, NULL);
  if (status != BSC_STATUS_OK) return status;
  status = bsc_help_validate(catalog->commands, catalog->command_count, options, NULL);
  if (status != BSC_STATUS_OK) return status;
  status = bsc_help_internal_find_path_validated(catalog->commands, catalog->command_count, path_tokens, path_token_count,
                                                 options, &result);
  if (status != BSC_STATUS_OK) return status;
  target = bsc_help_ext_find_target(catalog, result.command);
  topic_count = bsc_help_ext_count_topics(catalog, result.command);
  if (target != NULL) {
    visible_related_count = bsc_help_ext_count_visible_related(target->related, target->related_count, options);
  }
  if (output == NULL || output->write == NULL) {
    return BSC_STATUS_INTERNAL_ERROR;
  }
  status = bsc_help_internal_render_resolved_path(catalog->commands,
                                                  catalog->command_count,
                                                  result.command,
                                                  options,
                                                  output,
                                                  &sections);
  if (status != BSC_STATUS_OK) return status;
  return bsc_help_ext_render_target_sections(output, catalog, result.command, target, topic_count, visible_related_count,
                                             options, &sections);
}

bsc_status_t bsc_help_render_topic(const bsc_help_catalog_t *catalog,
                                   const bsc_string_view_t *parent_path_tokens,
                                   size_t parent_path_token_count,
                                   bsc_string_view_t topic_id,
                                   const bsc_help_options_t *options,
                                   bsc_output_t *output) {
  bsc_help_topic_lookup_result_t result;
  size_t visible_related_count;
  bsc_status_t status;
  int sections = 0;

  if (parent_path_token_count == 0u) {
    return BSC_STATUS_NO_INPUT;
  }
  if (topic_id.length == 0u) {
    return BSC_STATUS_NO_INPUT;
  }
  if (catalog == NULL || parent_path_tokens == NULL || topic_id.data == NULL) {
    return BSC_STATUS_INTERNAL_ERROR;
  }
  status = bsc_help_find_topic(catalog, parent_path_tokens, parent_path_token_count, topic_id, options, &result);
  if (status != BSC_STATUS_OK) return status;
  visible_related_count = bsc_help_ext_count_visible_related(result.topic->related, result.topic->related_count, options);
  if (output == NULL || output->write == NULL) {
    return BSC_STATUS_INTERNAL_ERROR;
  }
  status = bsc_help_ext_render_topic_name(output, result.topic, &sections);
  if (status != BSC_STATUS_OK) return status;
  status = bsc_help_ext_render_topic_description(output, result.topic->description, &sections);
  if (status != BSC_STATUS_OK) return status;
  status = bsc_help_ext_render_text_list(output, "NOTES", BSC_HELP_EXT_LITERAL_LEN("NOTES"), &result.topic->notes,
                                         &sections);
  if (status != BSC_STATUS_OK) return status;
  status = bsc_help_ext_render_text_list(output, "WARNINGS", BSC_HELP_EXT_LITERAL_LEN("WARNINGS"), &result.topic->warnings,
                                         &sections);
  if (status != BSC_STATUS_OK) return status;
  status = bsc_help_ext_render_examples(output, result.topic->examples, result.topic->example_count, &sections);
  if (status != BSC_STATUS_OK) return status;
  return bsc_help_ext_render_related(output, result.topic->related, result.topic->related_count, visible_related_count,
                                     options, &sections);
}
