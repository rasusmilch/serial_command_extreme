#include "bsc_help.h"

#include "internal/bsc_help_internal.h"

#include "bsc_string_view.h"

/** @brief Clear borrowed topic lookup result storage without retaining pointers. */
void bsc_help_topic_lookup_result_clear(bsc_help_topic_lookup_result_t *result) {
  if (result != NULL) {
    result->topic = NULL;
    result->topic_index = 0u;
    result->parent_command_index = 0u;
  }
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
