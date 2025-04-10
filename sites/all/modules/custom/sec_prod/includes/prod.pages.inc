<?php

function prod_nom_list_form($form, &$form_state)
{
  // дефолтные значения из сессии или текущие
  $month_default    = ext_user_get_session_setting('page_nom_list_month') ?? date('n');
  $year_default     = ext_user_get_session_setting('page_nom_list_year') ?? date('Y');
  $sort_by          = ext_user_get_session_setting('page_nom_list_sort') ?? 'date';

  $month          = empty($form_state["values"]["month"])   ? $month_default : $form_state["values"]['month'];
  $year           = empty($form_state["values"]['year'])    ? $year_default : $form_state["values"]['year'];
  $company        = empty($form_state["values"]['company']) ? null : $form_state["values"]['company'];
  $nomenklatura   = empty($form_state["values"]['nom'])     ? null : $form_state["values"]['nom'];

  // если значение задано пользователем, сохранить в сессии
  if (!empty($form_state["values"]["month"])) ext_user_store_session_setting('page_nom_list_month', $form_state["values"]["month"]);
  if (!empty($form_state["values"]["year"])) ext_user_store_session_setting('page_nom_list_year', $form_state["values"]["year"]);
  if (!empty($form_state["triggering_element"]) && in_array($form_state["triggering_element"]["#name"], ['abc', 'date'])) {
    $sort_by = $form_state["triggering_element"]["#name"];
    ext_user_store_session_setting('page_nom_list_sort', $sort_by);
  }

  $month_start = gmmktime(0, 0, 0, $month, 1, $year);

  if (user_has_role(CLIENT_RID))
    $button_add = '<a class="btn btn-primary autodialog" data-dialog-width="650" href="/production/message/add/0/' . $month_start . '">Новая заявка</a>';

  $form = [
    '#prefix' => '<div id="nom-list-form-wrapper" class="nom-list-page">' .
                    (isset($button_add) ? '<div class="nom-add">' . $button_add . '</div>' : ''),
    '#suffix' => '</div>',
    'filters' => [
      '#type' => 'container',
      '#attributes' => ['class' => ['filters']],
    ],
    'sortby' => ['#type' => 'hidden', '#value' => $sort_by],
  ];

  $month_opts = [];
  for($i = 1; $i <= 12; $i++) {
    $month_opts[$i] = helper_month_label_ru($i);
  }
  $form['filters']['month'] = [
    '#title' => 'Месяц',
    '#type' => 'select',
    '#options' => $month_opts,
    '#default_value' => [$month],
    '#ajax' => [
      'callback' => 'prod_nom_list_form_callback',
      'wrapper' => 'nom-list-form-wrapper',
    ],
    '#prefix' => '<div class="filter"><div class="month-select-wrapper">',
  ];
  $year_opts = [];
  for($i = -1; $i <= 1; $i++) {
    $yr = date('Y') + $i;
    $year_opts[$yr] = $yr;
  }
  $form['filters']['year'] = [
    '#title' => 'Год',
    '#type' => 'select',
    '#options' => $year_opts,
    '#default_value' => [$year],
    '#ajax' => [
      'callback' => 'prod_nom_list_form_callback',
      'wrapper' => 'nom-list-form-wrapper',
    ],
    '#suffix' => '</div></div>',
  ];


  // получить все Компании, но далее отфильтровать по наличию Сообщений
  $nom_arr = $sort_arr = $options_n = [];
  $companies_from_nom = [];
  if ($companies = ext_user_get_companies_by_user()) {
    // подготовить массив с инфо о Номенклатуре и изменениями плана
    $nom_list = prod_nomenklatura_get_list_by_messages(['company_ids' => array_keys($companies), 'month_ts' => $month_start]);
    foreach ($nom_list as $nomid => $nom) {
      $companies_from_nom[$nom->company_id] = $nom->company_id;

      // выводим только для заданной компании и номенклатуры
      if (!$company || $nom->company_id == $company) {
        // подготовить опции для селекта (ключ по label для сортировки)
        $nom_info = prod_nomenklatura_get_info($nomid);
        $options_n[$nom_info['label']] = $nomid;

        if (!$nomenklatura || $nomid == $nomenklatura) {
          $nom_arr[$nomid] = [
            'nom' => $nom_info,
            'message' => null,
            'plan_rec' => null,
          ];

          $nom_arr[$nomid]['warning'] = prod_nomenklatura_get_warnings($month_start, $nomid)['last_warning'];

          if ($message_last = prod_message_get_last($month_start, $nomid)) {
            $nom_arr[$nomid]['message'] = prod_message_get_info($message_last->id);
          }
          if ($plan_rec_last = prod_plan_record_get_last($month_start, $nomid)) {
            $nom_arr[$nomid]['plan_rec'] = prod_plan_record_get_info($plan_rec_last->id);
          }

          $sort_key = ($sort_by == 'abc') ? $nom_info['label'] : $nom_arr[$nomid]['message']['created'] . str_pad($nomid, '10', '0', STR_PAD_LEFT);
          $sort_arr[$sort_key] = $nomid;
        }
      }
    }
  }
  // отсортировать в зависимости от заданного пользователем
  if ($sort_by == 'abc') ksort($sort_arr); else krsort($sort_arr);

  // подготовить список Номенклатуры
  $output = '<div class="nom-list">';
  if ($nom_arr) {
    foreach ($sort_arr as $key) {
      $output .= theme('nomenklatura_teaser', ['nom_info' => $nom_arr[$key]['nom'], 'message_info' => $nom_arr[$key]['message'], 'plan_rec_info' => $nom_arr[$key]['plan_rec'], 'month_start' => $month_start, 'warning' => $nom_arr[$key]['warning']]);
    }
  } else {
    $output .= '<p>Заявок на производство не найдено, попробуйте изменить значения фильтров.</p>';
  }
  $output .= '</div>';

  // исключить из списка Компаний те, по которым нет Сообщений
  $options_c = [];
  foreach ($companies_from_nom as $cid) {
    $options_c[$cid] = $companies[$cid]['name'];
  }
  if (count($options_c) != 1) {
    $options_c[0] = '- Все -';
    ksort($options_c);
  }

  $company = $company ?? array_key_first($options_c);

  // готовим форму
  $form['filters']['company'] = [
    '#title' => 'Компания',
    '#type' => 'select',
    '#options' => $options_c,
    '#default_value' => $company,
    '#ajax' => [
      'callback' => 'prod_nom_list_form_callback',
      'wrapper' => 'nom-list-form-wrapper',
    ],
    '#prefix' => '<div class="filter">',
    '#suffix' => '</div>',
  ];


  if (count($options_n) != 1) {
    $options_n['- Все -'] = 0;
  }
  ksort($options_n);
  $options_n = array_flip($options_n);
  $nomenklatura = $nomenklatura ?? array_key_first($options_n);

  $form['filters']['nom'] = [
    '#title' => 'Номенклатура',
    '#type' => 'select',
    '#options' => $options_n,
    '#default_value' => $nomenklatura,
    '#ajax' => [
      'callback' => 'prod_nom_list_form_callback',
      'wrapper' => 'nom-list-form-wrapper',
    ],
    '#prefix' => '<div class="filter">',
    '#suffix' => '</div>',
  ];

  if ($sort_by == 'abc') {
    $form['filters']['abc'] = [
      '#markup' => '<div class="sort form-item"><span class="btn btn-link selected">по алфавиту</span>'
    ];

    $form['filters']['datetime'] = [
      '#type' => 'button',
      '#value' => 'по дате изменения',
      '#name' => 'date',
      '#ajax' => [
        'callback' => 'prod_nom_list_form_callback',
        'wrapper' => 'nom-list-form-wrapper',
      ],
      '#attributes' => ['class' => ['btn-link']],
      '#suffix' => '</div>',
    ];

  } else {
    $form['filters']['abc'] = [
      '#type' => 'button',
      '#value' => 'по алфавиту',
      '#name' => 'abc',
      '#ajax' => [
        'callback' => 'prod_nom_list_form_callback',
        'wrapper' => 'nom-list-form-wrapper',
      ],
      '#attributes' => ['class' => ['btn-link']],
      '#prefix' => '<div class="sort form-item">',
    ];
    $form['filters']['datetime'] = [
      '#markup' => '<span class="btn btn-link selected">по дате изменения</span></div>'
    ];
  }

  $form['list'] = [
    '#markup' => $output,
  ];

  return $form;
}
function prod_nom_list_form_callback($form, $form_values)
{
  return $form;
}

function prod_nom_page($nom_id, $month_start)
{
  // todo проверить доступ пользователя к номенклатуре
  $nom_info = prod_nomenklatura_get_info($nom_id);
  if (!in_array($nom_info['company_id'], array_keys(ext_user_get_companies_by_user()))) {
    return 'У вас нет доступа к данной записи. Если вы считаете, что это ошибка, свяжитесь с нами по e-mail <a href="mailto:support@kccc.ru">support@kccc.ru</a>';
  }

  $output = '';
  $volume = null;

  // собрать актуальную информацию о состоянии плана производства
  $output_pu = '';
  if ($plan_rec = prod_plan_record_get_last($month_start, $nom_id)) {
    $plan_rec_info = prod_plan_record_get_info($plan_rec->id);

    if ($plan_rec_info['load'] && $plan_rec_info["volume"]) {
      $volume = 0;
      $output_pu = '<div class="produce-units">' .
                      '<div class="produce-units-list">';
      $produce_units = prod_produce_unit_get_load($month_start, null, $plan_rec->id);
      foreach ($produce_units as $pu) {
        $volume += $pu['amount'];
        $output_pu .= theme('produce_unit', ['produce_unit' => $pu, 'month_start' => $month_start, 'show_amount' => true, 'show_nomenklatura' => false, 'show_month' => true]);
      }
      $output_pu .= '</div></div>';
      $output_pu .=  '<div class="produce-unit-legenda bottom"><div><span></span>- установка свободна</div><div><span class="loaded"></span>- установка занята</div><div><span class="company-loaded"></span>- другая продукция компании</div><div><span class="selected"></span>- даты производства</div></div>';
    }
  }
  $field_title    = theme('field_custom', ['title' => 'Номенклатура', 'content' => $nom_info['label']]);
  $field_volume   = $plan_rec ? theme('field_custom', ['title' => 'Объём выпуска', 'content' => helper_number_format($volume, 2, ' ')]) : '';
  $month_label    = drupal_strtolower(helper_month_label_ru(date('n', $month_start)) . ' ' . date('Y', $month_start));
  $field_month    = theme('field_custom', ['title' => 'Период', 'content' => $month_label]);


  // подготовить историю сообщений
  $output_m = '';
  if ($messages = prod_message_get_all($month_start, $nom_id)) {
    foreach ($messages as $message) {
      $message_info = prod_message_get_info($message->id);
      $output_m .= theme('message', ['message_info' => $message_info]);
    }
  }

  // определить наличие Предупреждений
  $class_warn = '';
  $warning_text = '';
  $warnings = prod_nomenklatura_get_warnings($month_start, $nom_id);
  if ($warnings['last_warning']) {
    if ($warnings['last_warning'] == PROD_WARNING_YELLOW) $class_warn = ' warn-yellow';
    if ($warnings['last_warning'] == PROD_WARNING_RED)    $class_warn = ' warn-red';

    // текст предупреждения
    $warning_message_info = prod_message_get_info(array_keys($warnings['list'])[0]);
    $warning_text = $warning_message_info["comment"];
  }

  // определить доступные Действия
  $date_vol_last = prod_message_get_last_date_volume($month_start, $nom_id);
  $url = '/production/message/add/' . $nom_id . '/' . $month_start;
  $button_class = prod_plan_has_approved($month_start) ? ' btn-danger' : ' btn-primary';

  // если до конца месяца осталось меньше чем PROD_DAYS_BEFORE_PRODUCE_START, запретить приём заявок на изменения, кроме документов
  $today_start = helper_get_day_start_from_timestamp();
  $next_month_start = gmmktime(0, 0, 0, date('n', $month_start)+1, date('j', $month_start), date('Y', $month_start));
  $queries_not_allowed = $today_start + PROD_DAYS_BEFORE_PRODUCE_START * 24*60*60 >= $next_month_start;
  $month_is_over = $today_start >= $next_month_start;

  $output_a = '';
  if (user_has_role(CLIENT_RID)) $output_a .= '<a class="btn' . $button_class . ' autodialog" data-dialog-width="650" href="' . $url . '?subject=date"' . ($date_vol_last['volume'] && !$queries_not_allowed && !$month_is_over ? '' : 'disabled') . '>Перенести дату выпуска</a>';
  if (user_has_role(CLIENT_RID)) $output_a .= '<a class="btn' . $button_class . ' autodialog" data-dialog-width="650" href="' . $url . '?subject=volume"' . (!$queries_not_allowed && !$month_is_over ? '' : 'disabled') . '>Изменить объём выпуска</a>';
  if (user_has_role(CLIENT_RID)) $output_a .= '<a class="btn btn-default autodialog" data-dialog-width="650" href="' . $url . '?subject=cancel"' . ($date_vol_last['volume'] && !$queries_not_allowed && !$month_is_over ? '' : 'disabled') . '>Отменить выпуск</a>';
  $output_a .= '<a class="btn btn-default autodialog" data-dialog-width="650" href="' . $url . '?subject=docs"' . (!$month_is_over ? '' : 'disabled') . '>Отправить документы</a>';

  // Настройки
  $output_s = '';
  if (user_has_role(CLIENT_RID)) {
    $settings_form = drupal_get_form('prod_nom_page_settings_form', $month_start, $nom_id);
    $output_s = drupal_render($settings_form);
  }

  // сформировать вывод
  $output .=  '<div class="nom-page">';
  $output .=    '<div class="n-header' . $class_warn . '">';
  $output .=        '<div class="n-title"' . (variable_get('ext_admin_debug', 0) ? ' title="' . $nom_info["id_1c"] . '"' : '') . '>';
  $output .=          $field_title;
  $output .=        '</div>';
  $output .=        '<div class="n-month">';
  $output .=          $field_month;
  $output .=        '</div>';
  $output .=        '<div class="n-volume">';
  $output .=          $field_volume;
  $output .=        '</div>';
  $output .=    '</div>';

  if ($warning_text) {
    $output .=      '<div class="n-notes' . $class_warn . '">';
    $output .=        'Причина предупреждения: ' . drupal_strtolower($warning_text);
    $output .=      '</div>';
  }

  $output .=    '<div class="n-plan">';
  $output .=      $output_pu;
  $output .=    '</div>';

  $output .=    '<div class="n-history">';
  $output .=      '<div class="n-left">';
  $output .=        '<div class="n-actions">';
  $output .=        '<label class="block-label">Действия</label>';
  $output .=          $output_a;
  $output .=        '</div>';
  if ($output_s) {
    $output .=      '<div class="n-settings">';
    $output .=        '<label class="block-label">Настройки</label>';
    $output .=        $output_s;
    $output .=        '</div>';
  }
  $output .=      '</div>';

  $output .=      '<div class="n-messages">';
  $output .=      '<label class="block-label">История изменений</label>';
  $output .=        $output_m;
  $output .=      '</div>';
  $output .=    '</div>';
  $output .=  '</div>';

  return $output;
}

function prod_message_add_form($form, &$form_state, $nomenklatura_id = null, $month_start = null)
{
  // Выполняемые ниже проверки:
  //  - своя номенклатура,
  //  - корректность задания начала месяца,

  // тема Сообщения (запрос на производство, смену даты, объёма и т.д.)
  if (!$nomenklatura_id) {
    $subject = 'add';
    drupal_set_title('Новая заявка на выпуск');
  }
  else $subject = $_GET['subject'] ?? '';

  // дата, начиная с которой возможно начало производства
  // определяется PROD_DAYS_BEFORE_PRODUCE_START
  $permitted_date_start = helper_get_day_start_from_timestamp() + PROD_DAYS_BEFORE_PRODUCE_START * 24*60*60;

  // если сменился месяц или год, заменить дату начала производства
  if (isset($form_state['triggering_element']['#name']) && in_array($form_state['triggering_element']['#name'], ['month', 'year'])) {
    $month_start = gmmktime(0, 0,0, $form_state['values']['month'], 1, $form_state['values']['year']);
  }

  // для запроса на новое производство добавить можно не раньше месяца, в который попадает дата начала производства (выше)
  if ($subject == 'add') {
    if ($month_start < helper_get_month_start_from_timestamp($permitted_date_start)) $month_start = helper_get_month_start_from_timestamp($permitted_date_start);
  }
  // или просто проверить timestamp начала месяца на корректность
  else {
    $month_start = helper_get_month_start_from_timestamp($month_start);
  }


  $intro = '';

  $form = [
    '#prefix' => '<div id="message-add-wrapper" class="message-add-wrapper">',
    '#suffix' => '</div>',
    'subject' => ['#type' => 'hidden', '#value' => $subject],
  ];

  // если не задана Номенклатура, то это Сообщение о заявке на Новый продукт
  $companies = ext_user_get_companies_by_user();
  if (!$nomenklatura_id) {
    $intro = '<p>Чтобы добавить заявку на выпуск продукта, выберите номенклатуру, задайте дату начала и объём производства.</p>';

    // подготовить список Номенклатуры клиента (если несколько компаний, то последовательно)
    $options = [];
    if ($companies) {
      foreach ($companies as $company) {
        $nom_list = prod_nomenklatura_get_list(['company_ids' => [$company['id']]]);
        foreach ($nom_list as $nom) {
          $nom_info = prod_nomenklatura_get_info($nom->nom_id);
          $options[$nom->nom_id] = $nom_info['label'];
        }
      }

      // исключить уже добавленную на месяц номенклатуру
//      if ($month_start) {
//        $nom_list_month = prod_nomenklatura_get_list_by_messages(['month_ts' => $month_start]);
//        foreach ($nom_list_month as $nom) {
//          unset($options[$nom->nom_id]);
//        }
//      }
    }
  } else {
    $nom_info = production_nomenklatura_get_info($nomenklatura_id);
    // проверить соответствие заданной в url Номенклатуры Компаниям клиента (от взлома ссылки)
    if (!in_array($nom_info['client_id'], array_keys($companies))) {
      $form['intro'] = [
        '#markup' => '<div class="intro">Вы не можете добавлять чужую номенклатуру.</div>',
      ];
      return $form;
    }

    $options[$nomenklatura_id] = $nom_info['label'];
  }

  if ($subject == 'date') $intro = '<p>Для изменения даты и/или объёма выпуска заполните соответствующие поля.</p>';
  if ($subject == 'volume') $intro = '<p>Для изменения даты и/или объёма выпуска заполните соответствующие поля.</p>';
  if ($subject == 'cancel') $intro = '<p>Для отмены выпуска продукта заполните поле комментария и нажмите "Отправить сообщение".</p>';

  $form['intro'] = [
    '#markup' => '<div class="intro">' . $intro . '</div>',
  ];

  $form['nom'] = [
    '#title' => 'Номенклатура',
    '#type' => 'select',
    '#options' => $options,
    '#disabled' => $subject != 'add',
    '#default_value' => $nomenklatura_id ?? array_key_first($options),
  ];

  // если задан месяц, в котором создано сообщение, то установить его же
  $month = $form_values["values"]["month"] ?? ($month_start ? date('n', $month_start) : date('n'));
  $year = $form_values["values"]["year"] ?? ($month_start ? date('Y', $month_start) : date('Y'));

  if ($subject == 'add') {
    $form['month']['#markup'] = '<div class="row"><div class="col-xs-12 col-md-5"><div class="month-select-wrapper"></div></div>';
  } else {
    $form['month']['month'] = [
      '#title' => 'Месяц',
      '#type' => 'select',
      '#options' => [$month => helper_month_label_ru($month)],
      '#default_value' => $month,
      '#disabled' => $subject != 'add',
      '#prefix' => '<div class="row"><div class="col-xs-12 col-md-5"><div class="month-select-wrapper">',
      '#ajax' => [
        'callback' => 'prod_message_add_callback',
        'wrapper' => 'message-add-wrapper',
      ],
    ];
    $form['month']['year'] = [
      '#title' => 'Год',
      '#type' => 'select',
      '#options' => [$year =>$year],
      '#default_value' => $year,
      '#disabled' => $subject != 'add',
      '#suffix' => '</div></div>',
      '#ajax' => [
        'callback' => 'prod_message_add_callback',
        'wrapper' => 'message-add-wrapper',
      ],
    ];
  }

  // если добавление Номенклатуры или возобновление, после отмены
  $date_vol_last = prod_message_get_last_date_volume($month_start, $nomenklatura_id, DATE_VOL_FROM_PLAN);

  $volume_default = 0;
  if ($subject == 'add' || $date_vol_last['volume'] == 0) {
    $date_start = $month_start;
  } else {
    // прошлые значения
    // из последнего сообщения, клиентского или производственного с записью плана.
    $date_start = $date_vol_last['date'];
    $volume_default = $date_vol_last['volume'];
  }
  if ($date_start < $permitted_date_start) $date_start = $permitted_date_start;

  $form['date'] = [
    '#type' => 'date_popup',
    '#title' => 'Начало выпуска',
    '#required' => in_array($subject, ['add', 'date']),
    '#default_value' => date('Y-m-d', $date_start),
    '#disabled' => !in_array($subject, ['add', 'volume', 'date']),
    '#prefix' => '<div class="col-xs-12 col-md-4">',
    '#suffix' => '</div>',
    '#date_format' => 'd.m.Y',
    '#date_year_range' => '0:+1',
    '#date_label_position' => 'invisible',
  ];
  // если дата была изменена после смены месяца/года, то '#default_value' не сработает
  // и нужно менять $form_state
  if (isset($form_state['triggering_element']['#name']) && in_array($form_state['triggering_element']['#name'], ['month', 'year'])) {
    $form_state["input"]["date"]["date"] = date('d.m.Y', $date_start);
    $form_state["values"]["date"] = date('Y-m-d', $date_start);
  }


  $form['volume'] = [
    '#type' => 'textfield',
    '#size' => 20,
    '#title' => 'Объём, л(кг)',
    '#required' => in_array($subject, ['add', 'volume']),
    '#default_value' => $subject == 'cancel' ? 0 : $volume_default,
    '#disabled' => !in_array($subject, ['add', 'volume', 'date']),
    '#prefix' => '<div class="col-xs-12 col-md-3">',
    '#suffix' => '</div></div>',
  ];

  $form['comment'] = [
    '#type' => 'textarea',
    '#rows' => 3,
    '#resizable' => false,
    '#required' => true,
    '#title' => 'Комментарий к заявке',
  ];

  if ($subject == 'docs') {
    $form['docs'] = [
      '#type' => 'plupload',
      '#title' => 'Документы',
      '#autoupload' => TRUE,
      '#upload_validators' => [
        'file_validate_extensions' => array('txt doc docx xls xlsx pdf'),
      ],
      '#plupload_settings' => [
        'runtimes' => 'html5',
        'multiple_queues' => true, // возможность загружать в несколько приёмов
      ],
    ];
  } else {
    // предупреждение о рисках
    if (prod_plan_has_approved($month_start) && in_array($subject, ['date', 'volume'])) {
      $warning = '<p>План производства уже утверждён!</p><p>Внесение изменений может привести к срыву установленных сроков выпуска продукции и к увеличению стоимости предоставляемых услуг.</p>';
      $form['warning'] = [
        '#markup' => '<div class="warning">' . $warning . '</div>',
      ];
    }
  }

  $form['actions'] = ['#type' => 'actions'];
  $form['actions']['submit'] = [
    '#type' => 'submit',
    '#value' => 'Отправить сообщение',
    '#attributes' => ['class' => ['btn-primary']],
    '#ajax' => [
      'callback' => 'prod_message_add_callback',
      'wrapper' => 'message-add-wrapper',
    ],
  ];

  return $form;
}
function prod_message_add_callback($form, &$form_state)
{
  return $form;
}
function prod_message_add_form_validate($form, &$form_state)
{
  if (!$form["date"]["#disabled"]) {
    // если date required, то проверить, изменено ли значение
    if ($form_state['values']['subject'] == 'date' && $form["date"]["#required"] && date('Y-m-d', strtotime($form_state['values']['date'])) == $form["date"]["#default_value"]) {
      form_set_error('date', 'Вы выбрали действие "Перенести дату выпуска". Задайте новую дату в соответствующем поле.');
      return false;
    }
    // дата не может быть меньше минимального периода до начала производства
    $permitted_day_start = helper_get_day_start_from_timestamp() + PROD_DAYS_BEFORE_PRODUCE_START * 24 * 60 * 60;
    if (helper_get_day_start_from_timestamp(strtotime($form_state['values']['date'])) < $permitted_day_start) {
      form_set_error('date', 'Дата начала выпуска не может быть ранее, чем ' . date('d.m.Y', $permitted_day_start)) . ' г.';
      return false;
    }
    // дата для переноса даты не должна быть раньше текущего месяца
    if ($form_state['values']['subject'] != 'add') {
      $month_start = gmmktime(0, 0, 0, $form_state['values']['month'], 1, $form_state['values']['year']);
      if (strtotime($form_state['values']['date']) < $month_start) {
        form_set_error('date', 'Нельзя перенести выпуск в предыдущие месяцы. Создайте новую заявку на странице списка заявок.');
        return false;
      }
    }
  }

  if (!$form["volume"]["#disabled"]) {
    // если volume required, то проверить, изменено ли значение
    if ($form_state['values']['subject'] == 'volume' && $form["volume"]["#required"] && $form_state['values']['volume'] == $form["volume"]["#default_value"]) {
      form_set_error('volume', 'Вы выбрали действие "Изменить объём выпуска". Задайте новый объём в соответствующем поле.');
      return false;
    }
    // объём не может быть нулевым или отрицательным, кроме отмены
    if ($form_state['values']['subject'] != 'cancel' && $form_state['values']['volume'] <= 0) {
      form_set_error('volume', 'Вы выбрали действие "Изменить объём выпуска". Задайте объём выпуска отличный от нуля. Для отмены выпуска выберите соответствующее действие.');
      return false;
    }
  }

  if ($form_state['values']['subject'] == 'docs' && empty($form_state['values']['docs'])) {
    form_set_error('docs', 'Добавьте хотя бы один файл или отмените действие.');
    return false;
  }

  // для $subject == add определить месяц/год
  if ($form_state['values']['subject'] == 'add') {
    $form_state['values']['month'] = date('n', strtotime($form_state['values']['date']));
    $form_state['values']['year'] = date('Y', strtotime($form_state['values']['date']));
  }
}
function prod_message_add_form_submit($form, &$form_state)
{
  $month_start = gmmktime(0, 0, 0, $form_state['values']['month'], 1 , $form_state['values']['year']);
  $nom_id = $form_state['values']['nom'];

  // прошлые значения
  // из последнего сообщения, клиентского или производственного с записью плана.
  $date_vol_last = prod_message_get_last_date_volume($month_start, $nom_id);

  // текущие значения
  $changes = [];
  $params = [];
  $date = helper_get_day_start_from_timestamp(strtotime($form_state['values']['date']));
  $volume = $form_state['values']['volume'];
  $comment = $form_state['values']['comment'];

  // сформировать Изменение
  $subject = $form_state['values']['subject'];
  if ($subject != 'docs')  {
    if ($subject == 'add')  {
      $changes[] = PROD_CHANGE_ADD_TO_PLAN;
    } else {
      if ($subject == 'cancel') {
        $changes[] = PROD_CHANGE_CANCEL;
      } else {
        if (empty($date_vol_last['volume'])) {
          $changes[] = PROD_CHANGE_ADD_TO_PLAN;
        } else {
          if (($subject == 'volume' && !isset($date_vol_last['volume'])) || (isset($date_vol_last['volume']) && $date_vol_last['volume'] != $volume)) {
            $changes[] = PROD_CHANGE_VOLUME;
          }
          if (($subject == 'date' && !isset($date_vol_last['date']))
            || (isset($date_vol_last['date']) && $date_vol_last['date'] != $date)) {
            $changes[] = PROD_CHANGE_DATE;
          }
        }
      }
    }
  } else $changes[] = PROD_CHANGE_DOCS;

  // Дату и Объём хранить для каждого Сообщения
  $params['volume'] = ['from' => $date_vol_last['volume'], 'to' => $volume];
  $params['date'] = ['from' => $date_vol_last['date'], 'to' => $date];

  // загрузить файлы
  $files = [];
  if (!empty($form_state['values']['docs'])) {
    // на базе кода из plupload.module
    $scheme = 'temporary://';
    foreach ($form_state['values']['docs'] as $uploaded_file) {
      if ($uploaded_file['status'] == 'done') {
        $source = $uploaded_file['tmppath'];
        $destination = file_stream_wrapper_uri_normalize($scheme . $uploaded_file['name']);
        $destination = file_unmanaged_move($source, $destination, FILE_EXISTS_RENAME);
        $file = plupload_file_uri_to_object($destination);
        file_save($file);
        $file = file_load($file->fid);
        $file->display = 1; // без этого поля выдаст ошибку
        $files[] = $file;
      }
    }
  }

  // добавить Сообщение
  $message = prod_message_create([
    'created' => REQUEST_TIME,
    'month_ts' => $month_start,
    'nom_id' => $nom_id,
    'author' => user_has_role(MANAGER_RID) ? PROD_MESSAGE_AUTHOR_PROD : PROD_MESSAGE_AUTHOR_CLIENT,
    'comment' => check_plain($comment),
    'changes' => $changes ?? null,
    'params' => $params,
    'files' => $files,
  ]);

  if (!user_has_role(MANAGER_RID)) {
    // экспортировать Сообщение
    prod_message_export($message->id);
  }

  // редирект на страницу Номенклатуры из ajax формы
  $url_redirect = '/production/nomenklatura/' . $form_state['values']['nom'] . '/' . $month_start;
  ctools_include('ajax');
  ctools_add_js('ajax-responder');
  $commands[] = ctools_ajax_command_redirect($url_redirect);
  print ajax_render($commands);
  drupal_exit();
}

function prod_nom_page_settings_form($form, $form_state, $month_start, $nom_id)
{
  $user_settings = variable_get('user_settings_' . $GLOBALS['user']->uid);

  if (!empty($form_state["triggering_element"]) && $form_state["triggering_element"]["#name"] == 'mail') {
    $user_settings['mail']['nom'][$form_state['values']['nom_id']] = $form_state['values']['mail'];
    variable_set('user_settings_' . $GLOBALS['user']->uid, $user_settings);
  }

  $form = [
    '#prefix' => '<div id="nom-page-settings-wrapper" class="nom-page-settings-wrapper">',
    '#suffix' => '</div>',
    'nom_id' => ['#type' => 'hidden', '#value' => $nom_id],
    'month_start' => ['#type' => 'hidden', '#value' => $month_start],
  ];


  $form['mail'] = [
    '#type' => 'checkbox',
    '#name' => 'mail',
    '#title' => 'уведомлять по e-mail об изменениях по продукту',
    '#default_value' => !isset($user_settings['mail']['nom'][$nom_id]) || $user_settings['mail']['nom'][$nom_id], //по умолчанию уведомлять
    '#ajax' => [
      'callback' => 'prod_nom_page_settings_form_callback',
      'wrapper' => 'nom-page-settings-wrapper',
    ],
  ];

  return $form;
}
function prod_nom_page_settings_form_callback($form, $form_state)
{
  return $form;
}

function prod_year_plan_form($form, $form_state)
{
  // дефолтные значения из сессии или текущие
  $company_default  = ext_user_get_session_setting('page_year_plan_company') ?? '';
  $year_default     = ext_user_get_session_setting('page_year_plan_year') ?? (date('n') < 9 ? date('Y')-1 : date('Y'));

  $company        = empty($form_state["values"]['company']) ? $company_default : $form_state["values"]['company'];
  $year           = empty($form_state["values"]['year'])    ? $year_default : $form_state["values"]['year'];

  // если значение задано пользователем, сохранить в сессии
  if (!empty($form_state["values"]["company"])) ext_user_store_session_setting('page_year_plan_company', $form_state["values"]["company"]);
  if (!empty($form_state["values"]["year"]))    ext_user_store_session_setting('page_year_plan_year', $form_state["values"]["year"]);


  $form = [
    '#prefix' => '<div id="year-plan-form-wrapper" class="year-plan-page">',
    '#suffix' => '</div>',
    'filters' => [
      '#type' => 'container',
      '#attributes' => ['class' => ['filters']],
    ],
  ];

  $options_c = [];
  $query = db_select('taxonomy_term_data', 't');
  $query ->fields('t', ['tid', 'name']);
  $query ->orderBy('name');
  $query ->innerJoin('field_data_field_client', 'fc', 'fc.field_client_tid = t.tid');
  // клиенту ограничиваем выбор его компаниями
  if (user_has_role(ROLE_USER_PRODUCTION)) {
    $query ->condition('fc.field_client_tid', array_keys(ext_user_get_companies_by_user()), 'IN');
  }
  $query ->condition('fc.bundle', 'nomenklatura');
  $query ->innerJoin('field_data_field_pp_nomenklatura', 'fn', 'fn.field_pp_nomenklatura_target_id = fc.entity_id');
  if ($companies = $query->execute()->fetchAllAssoc('tid')) {
    foreach ($companies as $tid => $term) {
      $options_c[$tid] = $term->name;
    }
  }

  $company = $company && isset($options_c[$company]) ? $company : array_key_first($options_c);

  $form['filters']['company'] = array(
    '#title' => 'Компания',
    '#type' => 'select',
    '#options' => $options_c,
    '#default_value' => $company,
    '#ajax' => [
      'callback' => 'prod_year_plan_form_callback',
      'wrapper' => 'year-plan-form-wrapper',
    ],
  );

  $year_opts = [];
  for($i = -2; $i < 1; $i++) {
    $yr = date('Y') + $i;
    $year_opts[$yr] = $yr;
  }
  $form['filters']['year'] = [
    '#title' => 'Сезон',
    '#type' => 'select',
    '#options' => $year_opts,
    '#default_value' => [$year],
    '#ajax' => [
      'callback' => 'prod_year_plan_form_callback',
      'wrapper' => 'year-plan-form-wrapper',
    ],
  ];

  $form['table'] = [
    '#markup' => prod_year_table($company, $year),
  ];


  return $form;
}
function prod_year_plan_form_callback($form, $form_state)
{
  return $form;
}
function prod_year_table($company_id, $year)
{
  // собрать последние Записи плана по Номенклатуре за 12 месяцев начиная с сентября
  $sort_arr = [];
  $nom_arr = ['total' => ['year_volume' => 0]];
  for ($i = 1; $i <= 12; $i++) {
    $month_start = gmmktime(0, 0, 0, 9 + $i - 1, 1, $year);
    $month_num = date('n', $month_start);
    // получить список номенклатуры
    $nom_list = prod_nomenklatura_get_list_by_plan(['company_ids' => [$company_id], 'month_ts' => $month_start]);
    $nom_arr['total']['months'][$month_num] = 0;

    foreach($nom_list as $nomid => $nom) {
      if ($plan_rec_last = prod_plan_record_get_last($month_start, $nomid)) {
        $nom_info = prod_nomenklatura_get_info($nomid);
        if (!isset($nom_arr[$nomid])) {
          $nom_arr[$nomid] = [
            'label' => $nom_info['label'],
            'months' => [],
            'year_volume' => 0,
          ];
          $sort_arr[$nom_info['label']] = $nomid;
        }
        $plan_rec_info = prod_plan_record_get_info($plan_rec_last->id);
        $nom_arr[$nomid]['months'][$month_num] = ['month_start' => $month_start, 'volume' => $plan_rec_info['volume']];
        $nom_arr[$nomid]['year_volume'] += $plan_rec_info['volume'];

        $nom_arr['total']['months'][$month_num] += $plan_rec_info['volume'];
        $nom_arr['total']['year_volume'] += $plan_rec_info['volume'];
      }
    }
  }

  // шапка
  $header = ['Наименование', ['data' => 'На сезон', 'align' => 'center']];
  foreach ([9,10,11,12,1,2,3,4,5,6,7,8] as $m_num) {
    $header[] = ['data' => helper_month_label_ru($m_num), 'align' => 'center'];
  }

  // табличная часть
  ksort($sort_arr);
  $rows = [];
  foreach ($sort_arr as $n_tid) {
    if ($nom_arr[$n_tid]['year_volume']) {
      $row = [$nom_arr[$n_tid]['label'], ['data' => helper_number_format($nom_arr[$n_tid]['year_volume'], 1, ' '), 'align' => 'right']];
      foreach ([9,10,11,12,1,2,3,4,5,6,7,8] as $m_num) {
        if (!empty($nom_arr[$n_tid]['months'][$m_num]['volume'])) {
          $month_start = $nom_arr[$n_tid]['months'][$m_num]['month_start'];
          $volume = $nom_arr[$n_tid]['months'][$m_num]['volume'];
          $output = '<a href="/production/nomenklatura/' . $n_tid . '/' . $month_start . '" target="_blank">' . helper_number_format($volume, 1, ' ') . '</a>';
        } else $output = '';
        $row[] = ['data' => $output, 'align' => 'right'];
      }
      $rows[] = $row;
    }
  }

  // total
  $row = ['Суммарно по продуктам', ['data' => helper_number_format($nom_arr['total']['year_volume'], 1, ' '), 'align' => 'right']];
  foreach ([9,10,11,12,1,2,3,4,5,6,7,8] as $m_num) {
    if (!empty($nom_arr['total']['months'][$m_num])) {
      $volume = $nom_arr['total']['months'][$m_num];
      $output = helper_number_format($volume, 1, ' ');
    } else $output = '';
    $row[] = ['data' => $output, 'align' => 'right'];
  }
  $rows[] = ['data' => $row, 'class' => ['t-footer']];


  return  theme('table', ['header' => $header, 'rows' => $rows, 'attributes' => ['class' => ['table-year-plan']], 'empty' => 'Нет данных за выбранный сезон']);
}
