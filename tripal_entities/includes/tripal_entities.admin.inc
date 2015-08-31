<?php

/**
 * Launchpad for feature administration.
 *
 * @ingroup tripal_feature
 */
function tripal_entities_admin_view() {
  $output = '';

  // set the breadcrumb
  $breadcrumb = array();
  $breadcrumb[] = l('Home', '<front>');
  $breadcrumb[] = l('Administration', 'admin');
  $breadcrumb[] = l('Tripal', 'admin/tripal');
  $breadcrumb[] = l('Manage Data types', 'admin/tripal/data_types');
  drupal_set_breadcrumb($breadcrumb);

/*   // Add the view
  $view = views_embed_view('tripal_feature_admin_features','default');
  if (isset($view)) {
    $output .= $view;
  }
  else {
    $output .= '<p>The Feature module uses primarily views to provide an '
      . 'administrative interface. Currently one or more views needed for this '
        . 'administrative interface are disabled. <strong>Click each of the following links to '
          . 'enable the pertinent views</strong>:</p>';
    $output .= '<ul>';
    $output .= '<li>'.l('Features View', 'admin/tripal/chado/tripal_feature/views/features/enable').'</li>';
    $output .= '</ul>';
  }

  // Add a summary chart.
  //-----------------------------------
  $output .= theme('tripal_feature_bar_chart_type_organism_summary');
  drupal_add_js('
    Drupal.behaviors.tripalFeature_moveAdminSummaryChart = {
      attach: function (context, settings) {
        jQuery("#tripal-feature-admin-summary").insertBefore( jQuery(".view-filters") );
    }};
  ', 'inline'); */

  return $output;
}

/**
 *
 * @param unknown $form
 * @param unknown $form_state
 * @return multitype:
 */
function tripal_entities_admin_publish_form($form, &$form_state) {
  $form = array();

  // Set the defaults.
  $cv_id = NULL;
  $term_name = NULL;

  // Set defaults using the form state.
  if (array_key_exists('values', $form_state)) {
    $cv_id = array_key_exists('cv_id', $form_state['values']) ? $form_state['values']['cv_id'] : NULL;
    $term_name = array_key_exists('term_name', $form_state['values']) ? $form_state['values']['term_name'] : NULL;
  }

  // Let the user select the vocabulary and tripal_data but only if they haven't
  // already selected a tripal_data.
  $sql = "
    SELECT CV.cv_id, CV.name
    FROM {tripal_entity_type} TET
      INNER JOIN {cv} CV on CV.cv_id = TET.cv_id
    ORDER BY CV.name
  ";
  $vocabs = chado_query($sql);
  $cvs = array();
  while ($vocab = $vocabs->fetchObject()) {
    $cvs[$vocab->cv_id] = $vocab->name;
  }
  $form['cv_id'] = array(
    '#type' => 'select',
    '#title' => t('Vocabulary'),
    '#options' => $cvs,
    '#required' => TRUE,
    '#description' => t('Select a vocabulary that contains the term you would like to set as publishable. Only vocabularies that are linked to data are shown.'),
    '#default_value' => $cv_id,
    '#ajax' => array(
      'callback' => "tripal_entities_admin_publish_form_ajax_callback",
      'wrapper' => 'tripal_entities_admin_publish_form',
      'effect' => 'fade',
      'method' => 'replace'
    )
  );

  // If we have a CV ID then we want to provide an autocomplete field
  if ($cv_id) {
    $form['cvterm_select']['term_name'] = array(
      '#title'       => t('Data Type'),
      '#type'        => 'textfield',
      '#description' => t("Please enter the name of the data type to set as publishable.  The data type must be a valid term in the selected vocabulary. This field will autopopulate as you type to help find available data types."),
      '#required'    => TRUE,
      '#default_value' => $term_name,
      '#autocomplete_path' => "admin/tripal/chado/tripal_cv/cvterm/auto_name/$cv_id",
    );

    $form['cvterm_select']['select_button'] = array(
      '#type' => 'submit',
      '#value' => t('Publish Data'),
      '#name' => 'publish',
    );
  }


  $form['#prefix'] = '<div id="tripal_entities_admin_publish_form">';
  $form['#suffix'] = '</div>';
  return $form;
}

/**
 * An Ajax callback for the tripal_entities_admin_publish_form..
 */
function tripal_entities_admin_publish_form_ajax_callback($form, $form_state) {
  // return the form so Drupal can update the content on the page
  return $form;
}
/**
 * Implements hook_validate() for the tripal_entities_admin_publish_form.
 *
 */
function tripal_entities_admin_publish_form_validate($form, &$form_state) {
  $cv_id = $form_state['values']['cv_id'];
  $term_name = $form_state['values']['term_name'];

  // Make sure the term_name is a real term in the vocabulary.
  $type = tripal_get_cvterm(array(
    'name' => $term_name,
    'cv_id' => $cv_id
  ));
  if (!$type) {
    form_set_error('term_name', t("The data type is not a valid name for the selected vocabulary."));
  }

  // Make sure the term is used in the site:
  $values = array(
    'cvterm_id' => $type->cvterm_id,
  );
  $bundles = chado_select_record('tripal_entity_bundle', array('bundle_id'), $values);
  if (count($bundles) == 0) {
    form_set_error('term_name', t("The data type, %type, is not associated with data on this site and thus cannot be set as publishable.", array('%type' => $term_name)));
  }

  // Make sure the term is not already published.
  $values = array(
    'cvterm_id' => $type->cvterm_id,
    'publish' => 1,
  );
  $bundles = chado_select_record('tripal_entity_bundle', array('bundle_id'), $values);
  if (count($bundles) > 0) {
    form_set_error('term_name', t("This data type is already set as publishable."));
  }
}
/**
 * Implements hook_submit() for the tripal_entities_admin_publish_form.
 *
 */
function tripal_entities_admin_publish_form_submit($form, &$form_state) {
  $cv_id = $form_state['values']['cv_id'];
  $term_name = $form_state['values']['term_name'];

  // Get the data type using the $term_name and $cv_id.
  $type = chado_generate_var('cvterm', array('cv_id' => $cv_id, 'name' => $term_name));


  // Start the transaction.
  $transaction = db_transaction();
  try {

    // We don't need to check if the vocabulary is used because the
    // form only shows those vocabs that are used.

    // Mark this entity as published.
    $match = array('cv_id' => $cv_id);
    $values = array('publish' => 1);
    $success = chado_update_record('tripal_entity_type', $match, $values);
    if (!$success) {
      throw new Exception('Cannot set the vocabulary as publishable');
    }

    // We have already checked in the validate if the term already exists
    // as a bundle. So, if we're here we can enable it.
    $match = array('cvterm_id' => $type->cvterm_id);
    $values = array('publish' => 1);
    $success = chado_update_record('tripal_entity_bundle', $match, $values);
    if (!$success) {
      throw new Exception('Cannot set the data type as publishable');
    }

    // Get the bundle
    $records = chado_select_record('tripal_entity_bundle', array('*'), $match);
    $bundle = $records[0];
    $bundle_name = $type->dbxref_id->db_id->name . '_' . $type->dbxref_id->accession;
    $entity_type_name = $type->dbxref_id->db_id->name;

    // Get the list of tables where this cvterm is used.
    $values = array('bundle_id' => $bundle->bundle_id);
    $tables = chado_select_record('tripal_entity_bundle_source', array('*'), $values);

    // Iterate through the tables.
    foreach ($tables as $table) {
      $tablename = $table->data_table;

      // We only want to look at base tables.
      if ($tablename == 'cvterm_dbxref' || $tablename == 'cvterm_relationship' ||
          $tablename == 'cvtermpath' || $tablename == 'cvtermprop' || $tablename == 'chadoprop' ||
          $tablename == 'cvtermsynonym' || preg_match('/_relationship$/', $tablename) ||
          preg_match('/_cvterm$/', $tablename)) {
        continue;
      }

      // Add in the bundle's fields.
      tripal_entities_add_bundle_fields($tablename, $entity_type_name, $bundle_name);

    }
    drupal_set_message(t('Data type, %type, is now set as publishable.', array('%type' => $term_name)));
  }
  catch (Exception $e) {
    $transaction->rollback();
    drupal_set_message('Failure publishing this data type: ' . $e->getMessage(), 'error');
    watchdog_exception('trp_entities', $e);
  }

}

/**
 *
 * @param $table
 * @param $entity_type
 * @param $bundle_name
 */
function tripal_entities_add_bundle_fields($tablename, $entity_type_name, $bundle_name) {

  // Iterate through the columns of the table and see if fields have been
  // created for each one. If not, then create them.
  $schema = chado_get_schema($tablename);
  $columns = $schema['fields'];
  foreach ($columns as $column_name => $details) {
    $field_name = $tablename . '__' . $column_name;
    $field = field_info_field($field_name);

    // Skip the primary key field.
    if ($column_name == $schema['primary key'][0]) {
      continue;
    }

    // Determine if the field is required.
    $is_required = 0;
    if (array_key_exists('not null', $details)) {
      $is_required = $details['not null'] ? 1 : 0;
    }

    // Determine what type of field this should be.
    // Drupal data types are: https://www.drupal.org/node/159605.
    // Field types are here:  https://www.drupal.org/node/1879542
    $field_type = '';
    $widget_type = '';
    $settings = array();
    $label = '';
    switch($details['type']) {
      case 'char':
        // unsupported by postgres
        break;
      case 'varchar':
        $field_type = 'text';
        $widget_type = 'text_textfield';
        $settings['max_length'] = $details['length'];
        break;
      case 'text':
        $field_type = 'text';
        $widget_type = 'text_textarea';
        $settings['max_length'] = '';
        break;
      case 'blob':
        // not sure how to support a blob field.
        continue;
        break;
      case 'int':
        $field_type = 'number_integer';
        $widget_type = 'number';
        break;
      case 'float':
        $field_type = 'number_float';
        $widget_type = 'number';
        $settings['precision'] = 10;
        $settings['scale'] = 2;
        $settings['decimal_separator'] = '.';
        break;
      case 'numeric':
        $field_type = 'number_decimal';
        $widget_type = 'number';
        break;
      case 'serial':
        // Serial fields are most likely not needed as a field.
        break;
      case 'boolean':
        // TODO: what is the proper field for booleans?????
        break;
      case 'datetime':
        // TODO: What is the proper datetime fields ??????
        break;
      default:
        drupal_set_message(t("Unhandled field type: %type", array('%type' => $details['type'])), 'warning');
        $field_type = 'text';
        $widget_type = 'text_textarea';
        if (array_key_exists('length', $details)) {
          $settings['max_length'] = $details['length'];
        }
    }

    // If we don't have a field type then we don't need to create a field.
    if (!$field_type) {
      // If we don't have a field type but it is required and doesn't have
      // a default value then we are in trouble.
      if ($is_required and !array_key_exists('default', $details)) {
        throw new Exception(t('The %table.%field type, %type, is not yet supported for Entity fields, but it is required,',
          array('%table' => $tablename, '%field' => $column_name, '%type' => $details['type'])));
      }
      continue;
    }

    // If this field is a foreign key field then we will have a special custom
    // field provided by Tripal.
    $is_fk = FALSE;
    if (array_key_exists('foreign keys', $details)) {
      foreach ($details['foreign keys'] as $remote_table => $fk_details) {
        if (array_key_exists($field_name, $fk_details['columns'])) {
          $is_fk = TRUE;
        }
      }
    }

    // If this column is a FK relationship then use a custom Tripal
    // defined field type for it.
    if ($is_fk) {
      // TODO: We need a better way to get the fields for FK relationships.
      // It's not a good idea to enumerate them all here. We need some sort
      // of hook or something that will let us lookup the correct field.
      switch ($column_name) {
        case 'organism_id':
          $field_type = 'organism_id';
          $label = 'Organism';
          break;
      }
      continue;
    }

    // If the field doesn't exist then create it.
    if (!$field) {
      $field = array(
        'field_name' => $field_name,
        'type' => $field_type,
        'cardinality' => 1,
        'locked' => TRUE,
        'storage' => array(
          'type' => 'tripal_entities_storage'
        ),
      );
      field_create_field($field);
    }

    // Attach the field to the bundle.
    $field_instance = array(
      'field_name' => $field_name,
      'label' => $label ? $label : ucwords(preg_replace('/_/', ' ', $column_name)),
      'widget' => array(
        'type' => $widget_type
      ),
      'entity_type' => $entity_type_name,
      'required' => $is_required,
      'settings' => $settings,
      'bundle' => $bundle_name,
    );
    field_create_instance($field_instance);
  }
}
/**
 *
 * @param unknown $form
 * @param unknown $form_state
 * @return multitype:
 */
function tripal_entities_admin_access_form($form, &$form_state) {
  $form = array();

  return $form;
}

/**
 * This function populates the Tripal entity tables using existing
 * data in the database.
 */
function tripal_entities_populate_entity_tables() {
  // Get the cvterm table and look for all of the tables that link to it.
  $schema = chado_get_schema('cvterm');
  $referring = $schema['referring_tables'];

  // Perform this action in a transaction
  $transaction = db_transaction();
  print "\nNOTE: Populating of tripal entity tables is performed using a database transaction. \n" .
    "If the load fails or is terminated prematurely then the entire set of \n" .
    "insertions/updates is rolled back and will not be found in the database\n\n";
  try {

    // Iterate through the referring tables to see what records are there.
    foreach ($referring as $tablename) {

      // We only want to look at base tables.
      if ($tablename == 'cvterm_dbxref' || $tablename == 'cvterm_relationship' ||
          $tablename == 'cvtermpath' || $tablename == 'cvtermprop' || $tablename == 'chadoprop' ||
          $tablename == 'cvtermsynonym' || preg_match('/_relationship$/', $tablename) ||
          preg_match('/_cvterm$/', $tablename)) {
        continue;
      }

      print "Examining $tablename...\n";
      $ref_schema = chado_get_schema($tablename);
      $fkeys = $ref_schema['foreign keys'];
      foreach ($fkeys['cvterm']['columns'] as $local_id => $remote_id) {

        // Get the list of cvterm_ids from existing records in the table.
        $sql = "
          SELECT $local_id
          FROM { " . $tablename . "}
          GROUP BY $local_id
        ";
        $results = chado_query($sql);
        while ($cvterm_id = $results->fetchField()) {

          // Get the CV term details and add it to the tripal_entity_type table if
          // it doesn't already exist.
          $cvterm = chado_generate_var('cvterm', array('cvterm_id' => $cvterm_id));

          // First add a record to the tripal_entity_type table.
          $values = array(
            'cv_id' => $cvterm->cv_id->cv_id,
            'db_id' => $cvterm->dbxref_id->db_id->db_id,
            'publish' => 0,
          );
          $entity_type_id = 0;
          $entity_type = chado_select_record('tripal_entity_type', array('entity_type_id'), $values);
          if (count($entity_type) == 0) {
            $entity_type = chado_insert_record('tripal_entity_type', $values);
            $entity_type_id = $entity_type['entity_type_id'];
          }
          else {
            $entity_type_id = $entity_type[0]->entity_type_id;
          }

          // Next add a record to the tripal_entity_bundle table.
          $values = array(
            'entity_type_id' => $entity_type_id,
            'cvterm_id' => $cvterm_id,
            'publish' => 0
          );
          $bundle_id = 0;
          $bundle = chado_select_record('tripal_entity_bundle', array('bundle_id'), $values);
          if (count($bundle) == 0) {
            $bundle = chado_insert_record('tripal_entity_bundle', $values);
            $bundle_id = $bundle['bundle_id'];
          }
          else {
            $bundle_id = $bundle[0]->bundle_id;
          }

          // Add the table where the records are found.
          $values = array(
            'bundle_id' => $bundle_id,
            'data_table' => $tablename,
            'type_table' => $tablename,
            'field' => $local_id
          );
          if (!chado_select_record('tripal_entity_bundle_source', array('bundle_source_id'), $values, array('has_record' => TRUE))) {
            chado_insert_record('tripal_entity_bundle_source', $values);
          }

          // Add the table where the records are found.
          $values = array(
            'entity_type_id' => $entity_type_id,
            'data_table' => $tablename,
            'type_table' => $tablename,
            'field' => $local_id
          );
          if (!chado_select_record('tripal_entity_type_source', array('entity_type_id'), $values, array('has_record' => TRUE))) {
            chado_insert_record('tripal_entity_type_source', $values);
          }
        }
      }
    }
  }
  catch (Exception $e) {
    print "\n"; // make sure we start errors on new line
    $transaction->rollback();
    watchdog_exception('tripal_ws', $e);
    print "FAILED: Rolling back database changes...\n";
  }
  print "\nDone.\n";
}