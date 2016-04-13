<?php


class OpignoTincanQuestionTypePropertiesDatabase
{
  const dbName = 'opigno_tincan_question_type_question_properties';

  public static $fields = array(
    'nid' => array(
      'type' => 'int',
      'unsigned' => TRUE,
      'not null' => TRUE
    ),
    'vid' => array(
      'type' => 'int',
      'unsigned' => TRUE,
      'not null' => TRUE,
    ),
    'fid' => array(
      'type' => 'int',
      'unsigned' => TRUE,
      'not null' => TRUE,
    ),
    'activity_id' => array(
      'type' => 'text'
    ),
    'launch_filename' => array(
      'type' => 'text'
    )
  );

  public static $indexes = array(
    'question_nid' => array('nid'),
    'question_vid' => array('vid')
  );
}

class OpignoTincanQuestionTypeAnswersDatabase
{
  const dbName = 'opigno_tincan_question_type_answers';

  public static $fields = array(
    'rid' => array(
      'type' => 'int',
      'unsigned' => true,
      'not null' => true
    ),
    'registration' => array(
      'type' => 'text',
      'not null' => true
    )
  );

  public static $indexes = array(
    'result_id' => array('rid')
  );
}
