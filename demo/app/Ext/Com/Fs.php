<?php
namespace Ext\Com;
/**
 * Fs
 */
class Fs
{

    /**
     * create dir
     * @param type $dir
     * @param type $mode
     * @return boolean
     */
    public static function mkdir($dir, $mode = 0755)
    {
        if (is_dir($dir)) {
            return TRUE;
        }
        if (mkdir($dir, $mode, TRUE)) {
            return TRUE;
        }
        throw new Cola_\Exception(dirname($dir) . " can not be written");
    }

}
