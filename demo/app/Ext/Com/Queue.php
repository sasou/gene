<?php
namespace Ext\Com;
/**
 * Queue factory
 */
class Queue
{

    public static function factory($config)
    {
        $adapter = $config['adapter'];
        $class = '\\Ext\\Com\\Queue\\' . ucfirst($adapter);
        return new $class($config);
    }

}
