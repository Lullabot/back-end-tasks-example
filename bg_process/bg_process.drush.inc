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
  // At this point we have the $entity_list array populated. Each position in
  // this array is an array containing the entity type and entity identifier to
  // be processed.
  // Something like:
  // $entity_list = array(
  //   array('entity_type' => 'node', 'entity_id' => 123),
  //   array('entity_type' => 'node', 'entity_id' => 45),
  //   array('entity_type' => 'file', 'entity_id' => 98),
  //   array('entity_type' => 'file', 'entity_id' => 640),
  //   array('entity_type' => 'taxonomy_term', 'entity_id' => 74),
  // );

  // Here is where we process all of the items:
  $succeeded = $errored = 0;
  foreach ($entity_list as $entity_item) {
    $success = _bg_process_perform_expensive_task($entity_item['entity_type'], $entity_item['entity_id']);
    $success ? $succeeded++ : $errored++;
  }

  // If we did not succeed a single time we consider that an error.
  if (!$succeeded) {
    return drush_set_error(dt('All of the entities failed processing.'));
  }

  drush_log(dt('@succeeded entities were processed correctly. @errored entities failed.', array(
    '@succeeded' => $succeeded,
    '@errored' => $errored,
  )), 'ok');
}
