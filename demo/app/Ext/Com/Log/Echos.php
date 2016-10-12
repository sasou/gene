<?php
namespace Ext\Com\Log;
/**
 * Echo Log
 */
class Echos extends baseAbstract
{

    protected function _handler($text)
    {
        echo $text;
    }

}
