<?php

/**
 * @file
 * Drush commands file.
 */

/**
 * Implements hook_drush_command().
 */
function bg_process_drush_command() {
  $items = array();

  $items['background-process'] = array(
    'aliases' => array('bgp'),
    'description' => 'Processes the entities with the given field and performs an expensive task on them.',
    'arguments' => array(
      'field-name' => array(
        'description' => 'The name of the field in the entities to process.',
        'example-value' => 'field_third_party_uri',
        'required' => TRUE,
      ),
    ),
    'options' => array(
      'size' => array(
        'description' => 'The number of entities to process in a single batch request.',
        'example-value' => '50',
      ),
    ),
    'examples' => array(
      'drush bgp field_third_party_uri' => 'Will process all entities with the field field_third_party_uri.',
    ),
  );

  return $items;
}

/**
 * Implements hook_drush_help().
 */
function bg_process_drush_help($section) {
  if ($section != 'drush:background-process') {
    return NULL;
  }
  return dt('Processes the entities with the given field and performs an expensive task on them.');
}

/**
 * Main command callback.
 *
 * @param string $field_name
 *   The name of the field in the entities to process.
 */
function drush_bg_process_background_process($field_name = NULL) {
  if (!$field_name) {
    return;
  }
  $batch_size = drush_get_option('size', 10);
  $entity_list = _drush_bg_callback_get_entity_list($field_name);
  $chunks = array_chunk($entity_list, $batch_size);
  // At this point we have the $sandbox['entity_list'] array populated. Each
  // position in this array is an array containing a chunk array, containing the
  // entity type and entity identifier to be processed. Something like:
  // $chunks = array(
  //   array(
  //     array('entity_type' => 'node', 'entity_id' => 123),
  //     array('entity_type' => 'node', 'entity_id' => 45),
  //   ),
  //   array(
  //     array('entity_type' => 'file', 'entity_id' => 98),
  //     array('entity_type' => 'file', 'entity_id' => 640),
  //     array('entity_type' => 'taxonomy_term', 'entity_id' => 74),
  //   ),
  // );

  // Prepare the batch operations configuration array. To create a drush command
  // that performs batch operations the only thing that we need to do is create
  // the array of configurations for each operation.
  $total = count($entity_list);
  $progress = 0;
  $operations = array();
  foreach ($chunks as $chunk) {
    $progress += count($chunk);
    // For each chunk drush will call _drush_bg_callback_process_entity_batch
    // and pass in the $chunk and a message to display some feedback to the
    // user.
    $operations[] = array('_drush_bg_callback_process_entity_batch', array(
      $chunk,
      dt('@percent% (Processing @progress of @total)', array(
        '@percent' => round(100 * $progress / $total),
        '@progress' => $progress,
        '@total' => $total,
      ))),
    );
  }
  $batch = array(
    'operations' => $operations,
    'title' => dt('Entity process callback batch'),
    'finished' => '_drush_bg_callback_process_entity_batch_finished',
    'progress_message' => dt('@current entities of @total were processed.'),
  );

  // Get the batch process all ready!
  batch_set($batch);

  // Start processing the batch operations.
  drush_backend_batch_process();
}

/**
 * Helper function to get the list of entities to process.
 *
 * @param string $field_name
 *   The name of the field contained in the bundles to process.
 *
 * @return array[]
 *   An array of entity_type and entity_id pairs.
 */
function _drush_bg_callback_get_entity_list($field_name) {
  if (!$field_name) {
    return array();
  }
  // All of the entities that need to be updated contain the field.
  $field_info = field_info_field($field_name);
  // $field_info['bundles'] contains information about the entities and bundles
  // that have this particular field attached to them.
  $entity_list = array();
  foreach ($field_info['bundles'] as $entity_type => $bundles) {
    $query = new \EntityFieldQuery();
    $results = $query
      ->entityCondition('entity_type', $entity_type)
      ->entityCondition('bundle', $bundles, 'IN')
      ->execute();
    if (empty($results[$entity_type])) {
      continue;
    }
    // Add the ids with the entity type to the $entity_list array, that will be
    // processed later.
    $ids = array_keys($results[$entity_type]);
    $entity_list += array_map(function ($id) use ($entity_type) {
      return array(
        'entity_type' => $entity_type,
        'entity_id' => $id,
      );
    }, $ids);
  }
  return $entity_list;
}

/**
 * Helper function to process a batch.
 *
 * @param array[] $chunk
 *   The array of entity_type and entity_id pairs.
 * @param string $details
 *   A feedback message to be sent to the user.
 * @param array $context
 *   This param is added automatically by drush. It is used to interact with the
 *   process executing the batches.
 */
function _drush_bg_callback_process_entity_batch($chunk, $details, &$context) {
  $context['message'] = $details;
  // Make sure to only initialize the results the first time.
  if (!isset($context['results']['success'])) {
    $context['results']['success'] = $context['results']['error'] = 0;
  }
  foreach ($chunk as $item) {
    $success = _bg_process_perform_expensive_task($item['entity_type'], $item['entity_id']);
    $success ? $context['results']['success']++ : $context['results']['error']++;
  }
}

/**
 * This callback is called when the batch process finishes.
 */
function _drush_bg_callback_process_entity_batch_finished($success, $results, $operations) {
  if ($success) {
    // Let the user know we have finished.
    drush_log(dt('@succeeded entities were processed correctly. @errored entities failed.', array(
      '@succeeded' => empty($results['success']) ? 0 : $results['success'],
      '@errored' => empty($results['error']) ? 0 : $results['error'],
    )), 'ok');
  }
}
