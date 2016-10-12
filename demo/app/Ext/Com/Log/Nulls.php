<?php
namespace Ext\Com\Log;
/**
 * Null Log
 */
class Nulls extends baseAbstract
{

    protected function _handler($text)
    {
        return;
    }

}
