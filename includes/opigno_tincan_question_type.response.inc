<?php

/**
 * @file
 * This file contains the class OpignoTincanQuestionTypeResponse.
 */



/**
 * Class OpignoTincanQuestionTypeResponse
 * This class goal is to manage the user's answer(s) for the TinCan Question Type.
 */
class OpignoTincanQuestionTypeResponse extends QuizQuestionResponse {


  /***************
   *
   * PROPERTIES
   *
   **************************/

  protected $registration = null;


  /***************
   *
   * OVERRIDDEN METHODS
   *
   **************************/

  /**
   * This constructor will init the $registration property.
   *
   * OpignoTincanQuestionTypeResponse constructor.
   * @param int $result_id The result ID
   * @param \stdClass $question_node The question node
   * @param array $answer The answer
   */
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

  
  /***************
   *
   * STATIC METHODS
   *
   **************************/

  /**
   * This method get the registration UUID from the database or from the
   *   PHPSESSION variable.
   * If it is from the PHPSESSION, the method will save this registration to the
   *   database.
   * 
   * @param $result_id int The result ID
   * @return bool|string The registration UUID if success, FALSE if not found.
   */
  public static function getRegistration($result_id) {
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


    if (isset($_SESSION[ OpignoTincanQuestionTypeQuestion::SESSIONKEY_REGISTRATION ]))
      return $_SESSION[ OpignoTincanQuestionTypeQuestion::SESSIONKEY_REGISTRATION];


    return false;
  }

  /**
   * This method will save the registration UUID that is in the PHPSESSION
   *   variable to the database.
   *
   * @param $result_id int The result ID to associate with the registration UUID
   * @return bool|string The registration UUID saved. FALSE if not found.
   */
  protected static function saveRegistrationFromSession($result_id) {
    if (isset($_SESSION[ OpignoTincanQuestionTypeQuestion::SESSIONKEY_REGISTRATION ]))
    {
      $registration = $_SESSION[ OpignoTincanQuestionTypeQuestion::SESSIONKEY_REGISTRATION ];

      self::saveRegistration($registration, $result_id);

      unset($_SESSION[ OpignoTincanQuestionTypeQuestion::SESSIONKEY_REGISTRATION ]);

      return $registration;
    }

    return false;
  }

  /**
   * This method will save the given registration UUID to the database.
   *
   * @param $registration string The UUID to save
   * @param $result_id int The result ID given by Quiz
   * @throws \Exception If an error occurred while inserting to the database.
   */
  protected static function saveRegistration($registration, $result_id) {
    db_insert(OpignoTincanQuestionTypeAnswersDatabase::dbName)
      ->fields(array(
        'rid' => $result_id,
        'registration' => $registration
      ))
      ->execute();
  }


  /***************
   *
   * PROTECTED METHODS
   *
   **************************/

  /**
   * This method deletes the registration from the database.
   * 
   * @param $result_id int The result ID linked to the registration to delete.
   */
  protected function deleteRegistration($result_id) {
    db_delete(OpignoTincanQuestionTypeAnswersDatabase::dbName)
      ->condition(array(
        'rid' => $result_id
      ))
      ->execute();
  }

  /**
   * This method will return the score from the LRS system for this response.
   * 
   * @return int The score not weighted
   */
  protected function getScoreFromLRS() {
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
   * This method returns the connection to the LRS. If there is a problem, this
   *   method will show an error message and return FALSE.
   *
   * @return bool|\TinCan\RemoteLRS FALSE in case of error. The LRS connection
   *   if connection was success.
   */
  protected function getLRSConnection() {
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
   * Get the statement containing the final score.
   *
   * @param \TinCan\RemoteLRS $lrs The LRS connection
   * @param $registration_uuid string The registration UUID
   * @param $activity_id string The activity ID of the statement
   * @return bool|\TinCan\Statement The statement. If not found, returns FALSE.
   */
  protected function getStatementFinalScore(\TinCan\RemoteLRS $lrs, $registration_uuid, $activity_id) {
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

  /**
   * This method calculate the score using a statement.
   *
   * @param $statement TinCan\Statement The statement that contains the score
   * @return float|int The final score ready to be returned for the Quiz module.
   *   Can returns 0 if the score is not found in the statement.
   */
  protected function getScoreFromStatement($statement) {
    $result = $statement->getResult();
    if (!isset($result))
      return 0;

    $score = $result->getScore();
    if (isset($score))
    {
      $scaled = $score->getScaled();
      if (isset($scaled) && $scaled >= 0)
        return $scaled * OpignoTincanQuestionTypeQuestion::SCORE_MAX;

      $raw = $score->getRaw();
      $max = $score->getMax();
      $min = $score->getMin();
      if (!isset($min))
        $min = 0;

      if (isset($raw) && isset($max))
        return ((float) ($raw-$min) / ($max-$min)) * OpignoTincanQuestionTypeQuestion::SCORE_MAX;
    }

    $success = $result->getSuccess();
    if (isset($success))
      return OpignoTincanQuestionTypeQuestion::SCORE_MAX;

    return 0;
  }
}
