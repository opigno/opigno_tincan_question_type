<?php

/**
 * @file
 * This file contains the class OpignoTincanQuestionTypeResponse.
 */



/**
 * Class OpignoTincanQuestionTypeResponse
 * This class goal is to manage the user's answer(s) for the TinCan Question Type.
 */
class OpignoTincanQuestionTypeResponse extends QuizQuestionResponse
{
  protected $registration = null;



  public function __construct($result_id, \stdClass $question_node, $answer) {
    parent::__construct($result_id, $question_node, $answer);

    $this->registration = self::getRegistration($result_id);
  }


  /**
   * Save the current response.
   */
  public function save() {
    // Nothing to save
  }

  /**
   * Delete the response.
   */
  public function delete()
  {
    if (!empty($this->rid))
      $this->deleteRegistration($this->rid);
  }

  /**
   * Calculate the score for the response.
   */
  public function score() {
    return $this->getScoreFromLRS();
  }

  /**
   * Get the user's response.
   */
  public function getResponse() {
    return '';
  }





  public static function getRegistration($result_id)
  {
    if (!empty($result_id))
    {
      $result = db_select(OpignoTincanQuestionTypeAnswersDatabase::dbName, 't')
        ->condition('rid', $result_id)
        ->fields('t')
        ->execute()->fetchAssoc();

      if ($result)
        return $result['registration'];


      return self::saveRegistrationFromSession($result_id);
    }


    if (isset($_SESSION[ OpignoTincanQuestion::sessionRegistration ]))
      return $_SESSION[ OpignoTincanQuestion::sessionRegistration ];


    return false;
  }


  protected static function saveRegistrationFromSession($result_id)
  {
    if (isset($_SESSION[ OpignoTincanQuestion::sessionRegistration ]))
    {
      $registration = $_SESSION[ OpignoTincanQuestion::sessionRegistration ];

      self::saveRegistration($registration, $result_id);

      unset($_SESSION[ OpignoTincanQuestion::sessionRegistration ]);

      return $registration;
    }

    return false;
  }


  protected static function saveRegistration($registration, $result_id)
  {
    db_insert(OpignoTincanQuestionTypeAnswersDatabase::dbName)
      ->fields(array(
        'rid' => $result_id,
        'registration' => $registration
      ))
      ->execute();
  }


  protected function deleteRegistration($result_id)
  {
    db_delete(OpignoTincanQuestionTypeAnswersDatabase::dbName)
      ->condition(array(
        'rid' => $result_id
      ))
      ->execute();
  }


  protected function getScoreFromLRS()
  {
    $lrs = $this->getLRSConnection();
    if (!$lrs) {
      drupal_set_message(t('Connection to the LRS failed'));
      return 0;
    }


    if (empty($this->registration)) {
      drupal_set_message(t('There was an error while answering the question, please go back and try again.'));
      return 0;
    }

    if (!isset($this->question->activity_id)) {
      drupal_set_message(t('Error while obtaining the activity ID. Maybe a malformed TinCan package.'));
      return 0;
    }

    $scoreStatement = $this->getStatementFinalScore($lrs, $this->registration, $this->question->activity_id);
    if (!$scoreStatement)
      return 0;


    return $this->getScoreFromStatement($scoreStatement);
  }


  /**
   * @return bool|\TinCan\RemoteLRS
   */
  protected function getLRSConnection()
  {
    $endpoint = variable_get('opigno_tincan_api_endpoint', '');
    $username = variable_get('opigno_tincan_api_username', '');
    $password = variable_get('opigno_tincan_api_password', '');

    if (empty($endpoint) || empty($username) || empty($password)) {
      drupal_set_message(t('Please configure first the Opigno TinCan API module.'));
      if (drupal_valid_path('admin/opigno/system/tincan'))
        drupal_set_message(t('Go to !url', array('!url' => l('the settings page', 'admin/opigno/system/tincan'))));

      return false;
    }

    return new TinCan\RemoteLRS(
      $endpoint,
      '1.0.1',
      $username,
      $password
    );
  }


  /**
   * @param \TinCan\RemoteLRS $lrs
   * @param $registration_uuid
   * @param $activity_id
   * @return bool|\TinCan\Statement
   */
  protected function getStatementFinalScore(\TinCan\RemoteLRS $lrs, $registration_uuid, $activity_id)
  {
    $activity = new TinCan\Activity();
    $activity->setId($activity_id);

    $verbPassed = new TinCan\Verb();
    $verbPassed->setId(OpignoTincanApiTinCanVerbs::$passed['id']);

    // Test with passed verb
    $result = $lrs->queryStatements(array(
      'activity' => $activity,
      'registration' => $registration_uuid,
      'verb' => $verbPassed,
      'limit' => 1
    ));
    $statements = $result->content->getStatements();

    // If nothing with passed, test with failed verb
    if (count($statements) === 0)
    {
      $verbFailed = new TinCan\Verb();
      $verbFailed->setId(OpignoTincanApiTinCanVerbs::$failed['id']);

      $result = $lrs->queryStatements(array(
        'activity' => $activity,
        'registration' => $registration_uuid,
        'verb' => $verbFailed,
        'limit' => 1
      ));

      $statements = $result->content->getStatements();
    }

    if (count($statements) > 0)
      return $statements[0];
    else
      return false;
  }


  protected function getScoreFromStatement($statement)
  {
    $result = $statement->getResult();
    if (!isset($result))
      return 0;

    $score = $result->getScore();
    if (isset($score))
    {
      $scaled = $score->getScaled();
      if (isset($scaled) && $scaled >= 0)
        return $scaled * OpignoTincanQuestion::maxScore;

      $raw = $score->getRaw();
      $max = $score->getMax();
      $min = $score->getMin();
      if (!isset($min))
        $min = 0;

      if (isset($raw) && isset($max))
        return ((float) ($raw-$min) / ($max-$min)) * OpignoTincanQuestion::maxScore;
    }

    $success = $result->getSuccess();
    if (isset($success))
      return OpignoTincanQuestion::maxScore;

    return 0;
  }
}
