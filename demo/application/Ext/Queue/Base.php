<?php
namespace Ext\Queue;

/**
 * Queue Abstract
 */
abstract class Base
{

    /**
     * config param
     * @var array
     */
    protected $options = ['host' => '127.0.0.1', 'port' => 1212];

    /**
     * Queue name
     * @var string
     */
    protected $queueName = '_defaultQueue';

    /**
     * Constructor
     *
     * @param array $options
     */
    public function __construct($params = array())
    {
        $this->options = $params + $this->options;
        if (isset($this->options['name'])) {
            $this->name($this->options['name']);
        }
    }

    /**
     * Set queue name
     *
     * @param string $name
     */
    public function name($name)
    {
        $this->queueName = $name;
        return $this;
    }

    abstract public function put($data);

    abstract public function get();
}
