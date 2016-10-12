<?php
namespace Ext\Com\Log;
/**
 * Log File
 */
class File extends baseAbstract
{

    protected function _handler($text)
    {
        $dir = dirname($this->_options['file']);
        is_dir($dir) || \Ext\Com\Fs::mkdir($dir, $this->_options['mode']);
        return file_put_contents($this->_options['file'], $text . "\n", FILE_APPEND | LOCK_EX);
    }

}
