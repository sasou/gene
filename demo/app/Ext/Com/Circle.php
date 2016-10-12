<?php
namespace Ext\Com;
/**
 * Circle porxy
 */
class Circle
{

    /**
     * Factory Circle porxy
     * @param array $config
     * @return object
     */
    public static function porxy($config = array())
    {
        (empty($config)) && $config = Cola::$_config->get('_webServicesCircle');
        return \Ext\Com\WebServices::factory($config);
    }

}
