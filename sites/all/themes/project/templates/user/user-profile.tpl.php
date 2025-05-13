<?php ?>

<div id="profile" class="profile">
  <div class="profile-content">

    <div class="profile-header">
      <div class="profile-name">
        <?php print $user_info["name"]; ?>
      </div>
      <div class="profile-role">
        <?php print $user_info['label']; ?>
      </div>
    </div>

    <div class="profile-info">
      <h4>Профиль</h4>
      <div class="row">
        <div class="col-xs-12 col-md-3"><div class="label">E-Mail</div></div>
        <div class="col-xs-12 col-md-9">
          <div class="field">
            <a href="mailto:<?php print $user_info['email']; ?>" class="email" rel="nofollow"><?php print $user_info['email']; ?></a>
          </div>
        </div>
      </div>
      <?php if ($user_info['companies']['formatted']): ?>
      <div class="row">
        <div class="col-xs-12 col-md-3"><div class="label">Список компаний</div></div>
        <div class="col-xs-12 col-md-9"><div class="field"><?php print $user_info['companies']['formatted']; ?></div></div>
      </div>
      <?php endif; ?>
    </div>

    <?php if (!empty($client_settings_form)): ?>
    <div class="profile-settings">
      <h4>Настройки почтовой рассылки</h4>
      <?php print drupal_render($client_settings_form); ?>
    </div>
    <?php endif; ?>

  </div>
</div>
