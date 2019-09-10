<?php
namespace Ext\Queue;

/**
 * Redis
 *
 * use https://github.com/nicolasff/phpredis
 */
class Redis extends Base
{

    protected $conn;
    
    public $options = array(
        'persistent' => true,
        'host' => '127.0.0.1',
        'port' => 6379,
        'timeout' => 3,
        'ttl' => 0,
    );
    
    public $optionKeys = [\Redis::OPT_SERIALIZER, \Redis::OPT_PREFIX];

    public function __construct($params)
    {
        parent::__construct($params);
        $this->conn = new \Redis();
        if ($this->options['persistent']) {
            $this->conn->pconnect($this->options['host'], $this->options['port'], $this->options['timeout']);
        } else {
            $this->conn->connect($this->options['host'], $this->options['port'], $this->options['timeout']);
        }
        foreach ($this->optionKeys as $key) {
            if (isset($this->options[$key])) {
                $this->conn->setOption($key, $this->options[$key]);
            }
        }
    }

    /**
     * Get from queue
     *
     * @param int $timeout >=0 for block, negative for non-blocking
     * @return bool
     */
    public function get($timeout = 0)
    {
        if (0 > $timeout) {
            return $this->conn->rPop($this->_name);
        } else {
            $data = $this->conn->brPop((array) $this->_name, $timeout);
            return isset($data[1]) ? $data[1] : false;
        }
    }

    /**
     * Put data
     * @param mixed $value
     * @return bool
     */
    public function put($value)
    {
        return $this->conn->lPush($this->_name, $value);
    }

    /**
     * Get multi record
     * @param integer $limit
     * @param integer $timeout
     * @return array
     */
    public function getMulti($limit, $timeout = 0)
    {
        $ret = array();
        for ($i = 0; $i < $limit; $i++) {
            $item = $this->get($timeout);
            if (false === $item) {
                break;
            }
            $ret[] = $item;
        }
        return $ret;
    }

}
