<?php


function query_buzy_import()
{
  query_produce_buzy_import();
  drupal_goto('<front>');
}

function query_plan_import()
{
  query_produce_plan_import();

  drupal_goto('<front>');
}

function query_other_import()
{
  query_davalec_import();
  query_produce_units_import();
  query_nomenklatura_import();
  query_nomenklatura_produce_units_import();

  drupal_goto('<front>');
}

function query_plan_delete()
{
  $efquery = new EntityFieldQuery();
  $result = $efquery
    ->entityCondition('entity_type', 'query')
    ->execute();
  if ($result) {
    entity_delete_multiple('query', array_keys($result['query']));
  }

  $efquery = new EntityFieldQuery();
  $result = $efquery
    ->entityCondition('entity_type', 'load')
    ->execute();
  if ($result) {
    entity_delete_multiple('load', array_keys($result['load']));
  }

  drupal_goto('<front>');
}

function query_produce_buzy_import()
{
  if ($buzy_array = file("c:\ЗанятостьУстановок.csv")) {
    foreach($buzy_array as $key => $buzy) {
      if (!$key) continue;

      if ($buzy = iconv('Windows-1251', 'UTF-8', trim($buzy))) {
        list($produce_unit_1cID, $date_str, $status) = explode('|', $buzy);

        if ($status == 2) {
          $produce_unit = query_get_produce_unit_by_1cID($produce_unit_1cID);
          $date = strtotime($date_str);
          $day_start = gmmktime(0, 0, 0, date('n', $date), date('j', $date), date('Y', $date));

          $load = entity_create('load', ['type' => 'load']);
          $load_wr = entity_metadata_wrapper('load', $load);
          $load_wr->field_produce_unit = $produce_unit->tid;
          $load_wr->field_produce_date = $day_start;
          $load_wr->field_produce_output = 0;
          $load_wr->save();
        }
      }
    }
    drupal_set_message('Занятость установок импортирована');
  } else {
    drupal_set_message('Файл ЗанятостьУстановок.csv не найден.', 'warning');
  }
}

function query_produce_plan_import()
{
  if ($plan_array_1c = file("c:\ПланПроизводства.csv")) {

    // подготовить массив с Планом для внесения в базу
    // определить Заявки и посчитать общее производимое количество
    $plan = [];
    foreach ($plan_array_1c as $key => $plan_item_1c) {
      if (!$key) continue;

      if ($plan_item_1c = iconv('Windows-1251', 'UTF-8', trim($plan_item_1c))) {
        list($articul, $produce_unit_1cID, $date_str, $output) = explode('|', $plan_item_1c);

        if ($articul && $produce_unit_1cID && $date_str && $output) {
          if (($nom = query_get_nomenklatura_by_artikul($articul))
            && ($produce_unit = query_get_produce_unit_by_1cID($produce_unit_1cID))) {
            $date = strtotime($date_str);
            $month_start = gmmktime(0, 0, 0, date('n', $date), 1, date('Y', $date));
            $day_start = gmmktime(0, 0, 0, date('n', $date), date('j', $date), date('Y', $date));

            if (isset($plan[$nom->tid])) {
              $plan[$nom->tid]['field_qty'] += $output;
              $plan[$nom->tid]['field_load'][] = [
                'field_produce_unit' => $produce_unit->tid,
                'field_produce_date' => $day_start,
                'field_produce_output' => $output,
              ];
            } else {
              $plan[$nom->tid] = [
                'field_month' => $month_start,
                'field_nomenklatura' => $nom->tid,
                'uid' => $nom->field_user['und'][0]['target_id'],
                'field_qty' => $output,
                'field_load' => [
                  [
                    'field_produce_unit' => $produce_unit->tid,
                    'field_produce_date' => $day_start,
                    'field_produce_output' => $output,
                  ]
                ],
              ];
            }
          } else {
            // todo ошибку об отсутствии Номенклатуры или Установки в лог
          }
        }
      }
    }

    // создание или обновление Запросов на месяц
    foreach ($plan as $plan_item) {
      // проверить наличие Запроса на месяц даты
      $efquery = new EntityFieldQuery();
      $result = $efquery
        ->entityCondition('entity_type', 'query')
        ->fieldCondition('field_month', 'value', $plan_item['field_month'])
        ->fieldCondition('field_nomenklatura', 'target_id', $plan_item['field_nomenklatura'])
        ->fieldCondition('field_query_type', 'value', 'main')
        ->execute();

      if (!$result) {
        $query = entity_create('query', [
          'type' => 'query',
          'uid' => $plan_item['uid'],
        ]);
        entity_save('query', $query);
      } else {
        $query = current($result['query']);
      }

      $query_wr = entity_metadata_wrapper('query', $query);
      $query_wr->field_month = $plan_item['field_month'];
      $query_wr->field_nomenklatura = $plan_item['field_nomenklatura'];
      $query_wr->field_qty = $plan_item['field_qty'];
      $query_wr->field_query_type = 'main';
      $query_wr->field_query_status_client = 'processing';
      $query_wr->field_query_status_manager = 'approved';
      $query_wr->save();

      query_query_load_update($query->id, $plan_item['field_load']);
    }

    drupal_set_message('План производства импортирован');
  } else {
    drupal_set_message('Файл ПланПроизводства.csv не найден.', 'warning');
  }
}

function query_davalec_import()
{
  if ($davalec_array = file("c:\Давальцы.csv")) {
    foreach ($davalec_array as $key => $davalec) {
      if (!$key) continue;

      if ($davalec = iconv('Windows-1251', 'UTF-8', trim($davalec))) {
        list($davalec_id, $davalec_name) = explode('|', $davalec);

        if ($davalec_id && $davalec_name) {
          $query = new EntityFieldQuery();
          $users = $query
            ->entityCondition('entity_type', 'user')
            ->fieldCondition('field_oc_davalec_id', 'value', $davalec_id)
            ->execute();

          if (!$users) {
            $user = user_save(NULL, [
              'name' => $davalec_name,
              'pass' => user_password(8),
              'status' => 1,
              'roles' => [DRUPAL_AUTHENTICATED_RID => TRUE],
            ]);
          } else {
            $user = current($users['user']);
          }

          $user_wr = entity_metadata_wrapper('user', $user->uid);
          $user_wr->name = $davalec_name;
          $user_wr->field_oc_davalec_id = $davalec_id;
          $user_wr->save();
        }
      }
    }
    drupal_set_message('Давальцы импортированы');
  } else {
    drupal_set_message('Файл Давальцы.csv не найден.', 'warning');
  }
}

function query_produce_units_import()
{
  if ($pu_array = file("c:\Установки.csv")) {
    foreach($pu_array as $key => $punit) {
      if (!$key) continue;

      if ($punit = iconv('Windows-1251', 'UTF-8', trim($punit))) {
        list($punit_id, $punit_name) = explode('|', $punit);

        if ($punit_id && $punit_name) {
          $produce_unit = query_get_produce_unit_by_1cID($punit_id);

          if (!$produce_unit) {
            $produce_unit = (object)array(
              'vid' => taxonomy_vocabulary_machine_name_load('produce_units')->vid,
              'name' => $punit_name,
            );
            taxonomy_term_save($produce_unit);
          }

          // обновить значения полей
          $produce_unit_wr = entity_metadata_wrapper('taxonomy_term', $produce_unit);
          $produce_unit_wr->name = $punit_name;
          $produce_unit_wr->field_oc_produce_unit_id = $punit_id;
          $produce_unit_wr->save();
        }
      }
    }
    drupal_set_message('Установки импортированы');
  } else {
    drupal_set_message('Файл Установки.csv не найден.', 'warning');
  }
}

function query_nomenklatura_import()
{
  if ($nom_array = file("c:\Номенклатура.csv")) {
    $vid = taxonomy_vocabulary_machine_name_load('nomenklatura')->vid;

    foreach($nom_array as $key => $nom) {
      if (!$key) continue;

      if ($nom = iconv('Windows-1251', 'UTF-8', trim($nom))) {
        list($artikul, $name, $artikul_davalec, $davalec_id, $davalec_name) = explode('|', $nom);

        if ($name && $artikul && $davalec_id) {
          // проверить наличие записи Номенклатуры
          $query = new EntityFieldQuery();
          $terms = $query
            ->entityCondition('entity_type', 'taxonomy_term')
            ->propertyCondition('vid', $vid)
            ->fieldCondition('field_oc_artikul', 'value', $artikul)
            ->execute();

          if (!$terms) {
            $term = (object)array(
              'vid' => $vid,
              'name' => $name,
            );
            taxonomy_term_save($term);
          } else {
            $term = current($terms['taxonomy_term']);
          }

          // найти пользователя
          $query = new EntityFieldQuery();
          $users = $query
            ->entityCondition('entity_type', 'user')
            ->fieldCondition('field_oc_davalec_id', 'value', $davalec_id)
            ->execute();
          if (!$users) {
            // todo ошибку в лог
          } else {
            $user = current($users['user']);
          }

          // обновить значения полей
          if ($user) {
            $term_wr = entity_metadata_wrapper('taxonomy_term', $term);
            $term_wr->name = $name;
            $term_wr->field_oc_artikul = $artikul;
            $term_wr->field_oc_artikul_davalec = $artikul_davalec;
            $term_wr->field_user = $user->uid;
            $term_wr->save();
          }
        }
      }
    }
    drupal_set_message('Номенклатура импортирована');
  } else {
    drupal_set_message('Файл Номенклатура.csv не найден.', 'warning');
  }
}

function query_nomenklatura_produce_units_import()
{
  if ($nom_units_array = file("c:\Производительность.csv")) {

    foreach($nom_units_array as $key => $nom_unit) {
      if (!$key) continue;

      if ($nom_unit = iconv('Windows-1251', 'UTF-8', trim($nom_unit))) {
        list($artikul, $punit_id, $output) = explode('|', $nom_unit);

        if ($artikul && $punit_id && $output) {
          $nom = query_get_nomenklatura_by_artikul($artikul);
          $produce_unit = query_get_produce_unit_by_1cID($punit_id);

          if ($nom && $produce_unit) {
            $nom_wr = entity_metadata_wrapper('taxonomy_term', $nom);
            $produce_unit_wr = entity_metadata_wrapper('taxonomy_term', $produce_unit);
            $unit_exists = false;
            foreach ($nom_wr->field_produce_units->getIterator() as $performance_wr) {
              // если уже есть запись об установке - обновить производительность
              if ($performance_wr->field_produce_unit->getIdentifier() == $produce_unit_wr->getIdentifier()) {
                $performance_wr->field_produce_unit_output = $output;
                $performance_wr->save();
                $unit_exists = true;
              }
            }
            // запись о производительности Установки в Номенклатуре не найдена - добавить
            if (!$unit_exists) {
              $performance = entity_create('performance', [
                'type' => 'performance',
              ]);
              $performance->field_produce_unit = array(LANGUAGE_NONE => array(0 => array('tid' => $produce_unit_wr->getIdentifier())));
              $performance->field_produce_unit_output = array(LANGUAGE_NONE => array(0 => array('value' => $output)));
              entity_save('performance', $performance);

              $nom_wr->field_produce_units[] = $performance->id;
              $nom_wr->save();
            }
          }
        }
      }
    }
    drupal_set_message('Производительность импортирована');
  } else {
    drupal_set_message('Файл Производительность.csv не найден.', 'warning');
  }
}
