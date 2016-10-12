<?php
namespace Ext\Com;
/**
 * Db factory
 */
class Db
{

    public static function factory($config)
    {
        $config += array('masterslave' => false, 'adapter' => 'Pdo\\Mysql');
        if ($config['masterslave']) {
            return new \Ext\Com\Db\Masterslave($config);
        }

        $adapter = $config['adapter'];
        $class = '\\Ext\\Com\\Db\\' . ucfirst($adapter);
        return new $class($config);
    }

}
