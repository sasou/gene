<?php
namespace Ext\Com;
/**
 * Web services factory
 *
 */
class WebServices
{

    public static function factory($config)
    {
        $class = '\\Ext\\Com\\WebServices\\' . ucfirst($config['adapter']);
        return new $class($config);
    }

}
