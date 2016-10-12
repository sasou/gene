<?php
namespace Ext\Com;
/**
 * Cache factory
 */
class Cache
{

    public static function factory($config)
    {
        $class = '\\Ext\\Com\\Cache\\' . ucfirst($config['adapter']);
        return new $class($config);
    }

}
