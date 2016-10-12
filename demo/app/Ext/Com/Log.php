<?php
namespace Ext\Com;
/**
 * Log factory
 */
class Log
{

    public static function factory($config)
    {
        $adapter = $config['adapter'];
        $class = '\\Ext\\Com\\Log\\' . ucfirst($adapter);
        return new $class($config);
    }

}
