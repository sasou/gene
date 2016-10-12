<?php
namespace Ext\Com\WebServices;
/**
 * Web services abstract
 */
abstract class baseAbstract
{

    /**
     * config param
     * @var array
     */
    protected $_params = array();

    /**
     * Web service name
     * @var string
     */
    protected $_name = '_defaultWebServices';

    /**
     * Constructor
     *
     * @param array $params
     */
    public function __construct($params = array())
    {
        foreach ($params as $key => $value) {
            $this->_params[$key] = $value;
        }
        if (isset($this->_params['name'])) {
            $this->name($this->_params['name']);
        }
    }

    /**
     * Set queue name
     *
     * @param string $name
     */
    public function name($name)
    {
        $this->_name = $name;
        return $this;
    }

    abstract public function put($data);

    abstract public function get();
}
