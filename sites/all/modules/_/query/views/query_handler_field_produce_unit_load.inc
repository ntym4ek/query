<?php

/**
 * Description of what my handler does.
 */
class query_handler_field_produce_unit_load extends views_handler_field
{
  function query()
  {
    $this->ensure_my_table();
    $this->add_additional_fields();
  }

  /**
   * Renders the field handler.
   */
  function render($values)
  {
    // вывести список дат с загрузкой для каждой установки
    $produce_unit_tid = $values->tid;

    // определить месяц отчёта
    $date = empty($this->view->exposed_raw_input["field_produce_date_value"]["value"]) ? $this->view->query->where[2]["conditions"][0]["value"][":field_data_field_produce_date_field_produce_date_value"] : $this->view->exposed_raw_input["field_produce_date_value"]["value"];

    list($year, $month) = explode('-', $date);
    $month_start = gmmktime(0,0,0, $month, 1, $year);
    $load = query_get_produce_unit_load($produce_unit_tid, $month_start);
    $volume = 0;
    foreach($load as $load_item) {
      $volume += $load_item['amount'];
    }

    $output =
    '<div class="produce-unit">' .
      '<div class="produce-unit-name">' .
        '<label class="views-label" for="edit-name--2">Установка</label>' .
        '<h3>' . $values->taxonomy_term_data_name . '</h3>' .
      '</div>' .
      '<div class="produce-unit-dates">' .
        '<label class="views-label">Даты</label>' .
        '<div class="form-checkboxes">';
    for ($i = 1; $i <= date("t", $month_start); $i++) {
      $day_start = $month_start + ($i - 1) * 60 * 60 * 24;
      $class = '';
      if (!empty($load[$day_start])) {
        $class = $load[$day_start]['amount'] ? ' loaded' : ' service';
      };
      $tooltip = empty($load[$day_start]) ? '' : ' data-toggle="tooltip" data-placement="top" data-html="true" title="' . $load[$day_start]['nomenklatura']['name'] . ($load[$day_start]['amount'] ? '<br />' . $load[$day_start]['amount'] : '') . '"';
      $output .= '<span class="c-box' . $class . '"' . $tooltip . '>' . $i . '</span>';
    }
    $output .=
      '</div>' .
      '</div>' .
      '<div class="produce-unit-aux">' .
        '<label class="views-label">Общий объём</label>' .
        '<h3>' . number_format($volume, 0, ',', ' ') . '</h3>' .
      '</div>' .
    '</div>';

    return $output;
  }
}