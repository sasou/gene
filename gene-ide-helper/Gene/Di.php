<?php
namespace Gene;

/**
 * Di
 * 
 * @author  sasou<admin@php-gene.com>
 * @version  5.4.3
 */
 
class Di
{
    protected static $_instance;
    protected $_reg;

    /**
     * __construct
     * 
     * @return mixed
     */
    private function __construct() {

    }

    /**
     * __clone
     * 
     * @return mixed
     */
    private function __clone() {

    }

    /**
     * getInstance
     *
     * @return mixed
     */
    public static function getInstance() {
        return new static();
    }

    /**
     * instance
     * 显式实例化一个类（走工厂加载 + 构造参数转发），但不写入容器注册表——
     * 每次调用都产生新对象，适用于瞬态/值对象。
     *
     * @param string $class 类名
     * @param array $params 构造函数参数（按顺序转发）
     * @return object|null 类不存在时返回 null
     */
    public static function instance($class, $params = []) {

    }

    /**
     * get
     * 
     * @param mixed $name name
     * @return mixed
     */
    public static function get($name) {

    }

    /**
     * has
     * 
     * @param mixed $name name
     * @return mixed
     */
    public static function has($name) {

    }

    /**
     * set
     * 
     * @param mixed $name name
     * @param mixed $value value
     * @return mixed
     */
    public static function set($name, $value) {

    }

    /**
     * del
     * 
     * @param mixed $name name
     * @return mixed
     */
    public static function del($name) {

    }

    /**
     * __set
     * 
     * @param mixed $name name
     * @param mixed $value value
     * @return mixed
     */
    public function __set($name, $value) {

    }

    /**
     * __get
     * 
     * @param mixed $name name
     * @return mixed
     */
    public function __get($name) {

    }

}