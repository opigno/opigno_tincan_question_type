<?php


class OpignoTincanQuestionTypeQuestion extends QuizQuestion
{
  const sessionKeyFile = 'opigno_tincan_question_type_fid';
  const sessionRegistration = 'opigno_tincan_question_type_registration';
  const publicPackagesFolder = 'public://opigno_tincan/';
  const publicExtractedPackagesFolder = 'public://opigno_tincan_extracted/';
  const maxScore = 50;





  /**
   * Save question type specific node properties
   */
  public function saveNodeProperties($is_new = FALSE)
  {
    if (!isset($_SESSION[$this::sessionKeyFile]))
    {
      form_set_error('file', t('Error while uploading the file.'));
      return;
    }

    $file = file_load($_SESSION[$this::sessionKeyFile]);
    if (!$file)
    {
      form_set_error('file', t('Error while opening the file.'));
      return;
    }

    $package_info = $this->getInfoFromExtractedPackage($file);
    if ($package_info === false)
    {
      form_set_error('file', t('The package does not contain an activity ID or a launch file'));
      return;
    }

    // TODO: If is not new, remove old package after the update (first, use file_usage_delete())

    $file->status = FILE_STATUS_PERMANENT;
    file_save($file);
    file_usage_add($file, 'opigno_tincan_question_type', 'node', $this->node->nid);

    $activity_id = $package_info['id'];
    $launch_filename = $package_info['launch'];

    db_merge(OpignoTincanQuestionTypePropertiesDatabase::dbName)
      ->key(array('nid' => $this->node->nid, 'vid' => $this->node->vid))
      ->fields(array(
        'fid' => $_SESSION[$this::sessionKeyFile],
        'activity_id' => $activity_id,
        'launch_filename' => $launch_filename
      ))
      ->execute();
  }
  

  /**
   * Getter function returning properties to be loaded when the node is loaded.
   *
   * @see load hook in quiz_question.module (quiz_question_load)
   *
   * @return array
   */
  public function getNodeProperties() {
    if (isset($this->nodeProperties['fid']))
      return $this->nodeProperties;

    parent::getNodeProperties();
    $result = db_select(OpignoTincanQuestionTypePropertiesDatabase::dbName, 't')
      ->condition('nid', $this->node->nid)
      ->condition('vid', $this->node->vid)
      ->fields('t')
      ->range(0, 1)
      ->execute()->fetchAssoc();

    if ($result)
      $this->nodeProperties += $result;

    return $this->nodeProperties;
  }


  /**
   * Provides validation for question before it is created.
   *
   * When a new question is created and initially submited, this is
   * called to validate that the settings are acceptable.
   *
   * @param $form
   *  The processed form.
   */
  public function validateNode(array &$form) {
    $file = file_load($form['file']['#value']['fid']);

    if ($file && $file->status != FILE_STATUS_PERMANENT)
    {
      if ($this->unzipPackage($file)) {
        $package_info = $this->getInfoFromExtractedPackage($file);
        if ($package_info === false)
          form_set_error('file', t('The package does not contain the activity ID or the launch file'));
        else
          $_SESSION[$this::sessionKeyFile] = $form['file']['#value']['fid'];
      }
      else
        form_set_error('file', t('Unable to open the archive'));
    }
  }


  /**
   * Get the form used to create a new question.
   *
   * @param
   *  FAPI form state
   * @return
   *  Must return a FAPI array.
   */
  public function getCreationForm(array &$form_state = NULL)
  {
    if (isset($this->node->nid) && !isset($this->nodeProperties['fid']))
      $this->getNodeProperties();

    return array(
      'file' => array(
        '#type' => 'managed_file',
        '#title' => t('TinCan package'),
        '#default_value' => $this->nodeProperties['fid'],
        '#upload_location' => $this::publicPackagesFolder,
        '#upload_validators' => array(
          'file_validate_extensions' => array('zip')
        ),
        '#required' => true
      )
    );
  }

  /**
   * Get the maximum possible score for this question.
   */
  public function getMaximumScore() {
    return self::maxScore;
  }


  /**
   * Implementation of delete()
   *
   * @see QuizQuestion#delete($only_this_version)
   */
  public function delete($only_this_version = FALSE) {
    parent::delete($only_this_version);

    if (!isset($this->nodeProperties['fid']))
      $this->getNodeProperties();

    $file = file_load($this->nodeProperties['fid']);

    $isDeleted = false;
    if ($file)
    {
      file_usage_delete($file, 'opigno_tincan_question_type', 'node', $this->node->nid);
      $isDeleted = file_delete($file);
    }

    if ($isDeleted)
    {
      file_unmanaged_delete_recursive($this->getExtractPath($file));
    }

    // TODO: check if it works even when we create multiple version of this question
  }


  /**
   * Allow question types to override the body field title
   *
   * @return
   *  The title for the body field
   */
  public function getBodyFieldTitle() {
    return t('Tincan package description');
  }


  /**
   * Get the form through which the user will answer the question.
   *
   * @param $form_state
   *  The FAPI form_state array
   * @param $rid
   *  The result id.
   * @return
   *  Must return a FAPI array. At the moment all form elements that takes
   *  user response must have a key named "tries". (This is a Quiz 3.x legacy AFAIK. I'm
   *  not thrilled about it...)
   */
  public function getAnsweringForm(array $form_state = NULL, $rid)
  {
    global $user, $base_path;

    $this->getNodeProperties();

    $markup = '';

    $launch_file = $this->nodeProperties['launch_filename'];



    $lrs_endpoint = variable_get('opigno_tincan_api_endpoint', '');
    $lrs_username = variable_get('opigno_tincan_api_username', '');
    $lrs_password = variable_get('opigno_tincan_api_password', '');

    if ((empty($lrs_endpoint) || empty($lrs_username) || empty($lrs_password))
      && drupal_valid_path('admin/opigno/system/tincan'))
    {
      drupal_set_message(t('The module Opigno TinCan API is not configured. Go to !url', array('!url' => l('the settings page.', 'admin/opigno/system/tincan'))));
      unset($_SESSION[self::sessionRegistration]);
    }
    else
    {
      $auth = 'Basic '. base64_encode($lrs_username .':'. $lrs_password);

      $actor = array(
        'mbox_sha1sum' => sha1('mailto:' . $user->mail),
        'name' => $user->name
      );


      $registration = OpignoTincanResponse::getRegistration($rid);
      if ($registration === false)
        $registration = TinCan\Util::getUUID();

      $launch_file .=
        '?endpoint='. rawurlencode($lrs_endpoint) .
        '&auth='. rawurlencode($auth) .
        '&actor='. rawurlencode(json_encode($actor)) .
        '&registration='. rawurlencode($registration);

      $_SESSION[self::sessionRegistration] = $registration;
    }



    $file = file_load($this->nodeProperties['fid']);

    $filename = $this->getPackageName($file);

    $uri = $this::publicExtractedPackagesFolder . $filename .'/'. $launch_file;
    $uri = str_replace('public://', $base_path .'sites/default/files/', $uri);

    $markup .= '
      <iframe style="width: 100%; min-height: 800px; border: 0;" src="'. $uri .'"></iframe>
    ';



    $form = parent::getAnsweringForm($form_state, $rid);

    $form['iframe'] = array(
      '#type' => 'markup',
      '#markup' => $markup
    );

    return $form;
  }





  private function unzipPackage($file)
  {
    $archiver = archiver_get_archiver($file->uri);

    if (!$archiver) {
      return false;
    }
    else {
      $archiver->extract( $this->getExtractPath($file) );
      return true;
    }
  }


  private function getInfoFromExtractedPackage($file)
  {
    $tincan_file = $this->getExtractPath($file) .'tincan.xml';

    if (!file_exists( $this->getExtractPath($file) .'tincan.xml' ))
      return false;

    $xml = simplexml_load_file($tincan_file);
    if (!$xml)
      return false;

    // Check if the launch exists
    if (!isset($xml->activities->activity->launch))
      return false;

    // Check if the activity ID exists
    if (!isset($xml->activities->activity['id']))
      return false;

    return array(
      'launch' => (string) $xml->activities->activity->launch,
      'id' => (string) $xml->activities->activity['id']
    );
  }


  private function getExtractPath($file)
  {
    $filename = $this->getPackageName($file);
    return $this::publicExtractedPackagesFolder . $filename . '/';
  }


  private function getPackageName($file)
  {
    return pathinfo($file->uri, PATHINFO_FILENAME);
  }
}
