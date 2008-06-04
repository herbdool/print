<?php
/* $Id$ */

require_once(drupal_get_path('module', 'print') .'/print.pages.inc');

/**
 * Menu callback for the send by e-mail form.
 *
 * @ingroup forms
 */
function print_mail_form($form_state) {
  global $user;
  $print_mail_settings = variable_get('print_mail_settings', print_mail_settings_default());
  $form = array();

  // Remove the printmail/ prefix
  $path = substr($_GET['q'], strpos($_GET['q'], '/')+1);
  if (is_numeric($path)) {
    $path = "node/". $path;
  }
  $cid = isset($_GET['comment']) ? $_GET['comment'] : NULL;
  $form['path'] = array('#type' => 'value', '#value' => $path);
  $forn['cid'] =  array('#type' => 'value', '#value' => $cid);

  $form['fld_from_addr'] = array(
    '#type' => 'textfield',
    '#title' => t('Your e-mail'),
    '#size' => 62,
  );
  $form['fld_from_name'] = array(
    '#type' => 'textfield',
    '#title' => t('Your name'),
    '#size' => 62,
  );
  $form['txt_to_addrs'] = array(
    '#type' => 'textarea',
    '#title' => t('Send to'),
    '#rows' => 3,
    '#resizable' => FALSE,
    '#description' => t('Enter multiple addresses separated by commas and/or different lines.'),
  );
  $form['fld_subject'] = array(
    '#type' => 'textfield',
    '#title' => t('Subject'),
    '#size' => 62,
  );
  $form['txt_message'] = array(
    '#type' => 'textarea',
    '#title' => t('Your message'),
    '#rows' => 6,
  );

  if ($print_mail_settings['teaser_choice']) {
    $form['chk_teaser'] = array(
      '#type' => 'checkbox',
      '#title' => t('Send only the teaser'),
      '#default_value' => $print_mail_settings['teaser_default'],
    );
  }
  else {
    $form['chk_teaser'] = array('#type' => 'value', '#value' => $print_mail_settings['teaser_default']);
  }

  $form['btn_submit'] = array(
    '#type' => 'submit',
    '#value' => 'Send e-mail',
  );
  $form['btn_clear'] = array(
    '#type' => 'submit',
    '#value' => 'Clear form',
    '#validate' => array('print_mail_form_reset'),
  );
  $form['btn_cancel'] = array(
    '#type' => 'submit',
    '#value' => 'Cancel',
    '#validate' => array(),
    '#submit' => array('print_mail_form_cancel'),
  );

  if ($user->uid != 0) {
    $user_name = $user->name;
    $form['fld_from_addr']['#default_value'] = $user->mail;
    $form['fld_from_addr']['#disabled'] = TRUE;
    $form['fld_from_addr']['#value'] = $user->mail;
    $form['fld_from_name']['#default_value'] = $user->name;
  }
  else {
    $user_name = t('Someone');
  }
  $site_name = variable_get('site_name', t('an interesting site'));
  $form['fld_subject']['#default_value'] = t('!user has sent you a message from !site', 
                                             array('!user' => $user_name, '!site' => $site_name));

  return $form;
}

function theme_print_mail_form($form) {
  $content = '';
  foreach (element_children($form) as $key) {
    $tmp = drupal_render($form[$key]);
    switch ($key) {
      case 'fld_from_addr':
      case 'fld_from_name':
      case 'txt_to_addrs':
      case 'fld_subject':
        $tmp = str_replace('<div class="form-item"', '<div style="clear: left;" class="form-item"', $tmp);
        $tmp = str_replace('<label', '<label style="width: 100px; float: left;"', $tmp);
        break;
    }
    $content .= $tmp;
  }
  return $content;
}

/**
 * Validate the send by-email form submission.
 *
 * @ingroup forms
 */
function print_mail_form_validate($form, &$form_state) {
  $from_addr = trim($form_state['values']['fld_from_addr']);
  $test = user_validate_mail($from_addr);
  if ($test) {
    form_set_error('fld_from_addr', $test);
  }

  // All new-lines are replaced by commas
  $to_addrs = str_replace(array("\r\n", "\n", "\r"), ',', $form_state['values']['txt_to_addrs']);
  // Create an array from the string
  $to_array = explode(',', $to_addrs);
  // Verify each element of the array
  foreach ($to_array as $key => $address) {
    $address = trim($address);
    if (preg_match('/(.*?) <(.*)>/s', $address, $matches)) {
      // Address is of the type User Name <user@domain.tld>
      $test = user_validate_mail($matches[2]);
      $to_array[$key] = trim($matches[1]) ." <". $matches[2] .">";
    }
    else {
      // Address must be user@domain.tld
      $test = user_validate_mail($address);
    }
    if ($test) {
      form_set_error('txt_to_addrs', $test);
    }
  }

  // In all fields, prevent insertion of custom headers
  foreach ($form_state['values'] as $key => $string) {
    if ( (substr($key, 0, 4) == "fld_") && ((strpos($string, "\n") !== FALSE) || (strpos($string, "\r") !== FALSE)) ) {
      form_set_error($key, 'Found invalid character');
    }
  }

  $subject = trim($form_state['values']['fld_subject']);
  if (empty($subject)) {
    form_set_error('fld_subject', t("You must enter a subject."));
  }
  $message = trim($form_state['values']['txt_message']);
  if (empty($message)) {
    form_set_error('txt_message', t("You must enter a message."));
  }

  $form_state['values']['fld_from_addr'] = $from_addr;
  $form_state['values']['fld_from_name'] = trim($form_state['values']['fld_from_name']);
  // Re-create the string from the re-organized array
  $form_state['values']['txt_to_addrs'] = implode(', ', $to_array);
  $form_state['values']['fld_subject'] = $subject;
  $form_state['values']['txt_message'] = $message;
}

/**
 * Process the send by-email form submission.
 *
 * @ingroup forms
 */
function print_mail_form_submit($form, &$form_state) {
  global $language;

  $from = $form_state['values']['fld_from_name'] ." <". $form_state['values']['fld_from_addr'] .">";
  $to = $form_state['values']['txt_to_addrs'];
  $params = array();
  $params['subject'] = $form_state['values']['fld_subject'];
  $params['message'] = $form_state['values']['txt_message'];
  $params['path'] = $form_state['values']['path'];
  $params['cid'] = $form_state['values']['cid'];
  $params['teaser'] = $form_state['values']['chk_teaser'];

  $site_name = variable_get('site_name', t('us'));
  $ret = drupal_mail('print_mail', 'sendpage', $to, $language, $params, $from, TRUE);
  if ($ret['result']) {
    drupal_set_message(t('Thank you for spreading the word about !site.', array('!site' => $site_name)));
  }
  $form_state['redirect'] = $form_state['values']['path'];
}

/**
 * Implementation of hook_mail().
 */
function print_mail_mail($key, &$message, $params) {
  switch ($key) {
    case 'sendpage':
      $sender_message = t('Message from sender:') .'<br /><br /><em>'. nl2br($params['message']) .'</em>';

      $print = print_controller($params['path'], $params['cid'], $params['teaser'], $sender_message);

      // Img elements must be set to absolute
//      $pattern = "@<(img\s[^>]*?)>@is";
//      $print["content"] = preg_replace_callback($pattern, "_print_rewrite_urls", $print["content"]);
//      $print["logo"] = preg_replace_callback($pattern, "_print_rewrite_urls", $print["logo"]);
//      $print["footer_message"] = preg_replace_callback($pattern, "_print_rewrite_urls", $print["footer_message"]);

      ob_start();
      include_once(_print_get_template("mail", $print["type"]));
      $html = ob_get_contents();
      ob_end_clean();

      $message['subject'] = $params['subject'];
      $message['body'] = $html;
      $message['headers']['Content-Type'] = 'text/html; charset=utf-8';
    break;
  }
}

/**
 * Process the send by-email form reset submission.
 *
 * @ingroup forms
 */
function print_mail_form_reset($form, &$form_state) {
  $form_state['rebuild'] = TRUE;
  drupal_set_message(t('The form has been reset to the default values.'));
}


/**
 * Process the send by-email form cancel submission.
 *
 * @ingroup forms
 */
function print_mail_form_cancel($form, &$form_state) {
  $form_state['redirect'] = $form_state['values']['path'];
}
