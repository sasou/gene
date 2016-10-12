<?php
namespace Ext\Com\Cache;
/**
 * Memcached
 */
class Memcached extends baseAbstract
{

    /**
     * Constructor
     *
     * @param array $options
     */
    public function __construct($options = array())
    {
        parent::__construct($options);

        if (isset($this->options['persistent'])) {
            $this->conn = new Memcached($this->options['persistent']);
        } else {
            $this->conn = new Memcached();
        }

        $this->conn->addServers($this->options['servers']);
    }

    /**
     * Set cache
     *
     * @param string $id
     * @param mixed $data
     * @param int $ttl
     * @return boolean
     */
    public function set($id, $data, $ttl = NULL)
    {
        NULL === $ttl && $ttl = $this->_options['ttl'];

        return $this->conn->set($id, $data, $ttl);
    }

    /**
     * Get Cache Data
     *
     * @param mixed $id
     * @return array
     */
    public function get($id)
    {
        return is_array($id) ? $this->conn->getMulti($id) : $this->conn->get($id);
    }

}
