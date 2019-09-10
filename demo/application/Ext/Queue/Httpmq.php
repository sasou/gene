<?php
namespace Ext\Queue;

/**
 * Httpmq
 *
 */
class Httpmq extends Base
{

    private $uri = null;

    public function __construct($params)
    {
        parent::__construct($params);
        $this->uri = 'http://' . $this->options['host'] . ':' . $this->options['port'];
    }

    /**
     * Config
     *
     * Set or get configration
     * @param string $name
     * @param mixed $value
     * @return mixed
     */
    public function config($name = null, $value = null)
    {
        if (null === $name) {
            return $this->options;
        }
        if (null === $value) {
            return isset($this->options[$name]) ? $this->name[$name] : null;
        }
        $this->options[$name] = $value;

        return $this;
    }

    /**
     * Put data to queue.
     * @param string $content json data
     * @param string $name queue name
     * @param string $uri queue uri
     * @return Boolean
     */
    public function put($content, $name = null, $uri = null)
    {
        try {
            if (null === $name) {
                $name = $this->queueName;
            }
            if (null === $uri) {
                $uri = $this->uri;
            }
            $url = $uri . "?opt=put&name={$name}";
            $data = \Ext\Http::raw($url, json_encode($content, JSON_UNESCAPED_UNICODE));
            if ('HTTPMQ_PUT_OK' == $data) {
                return true;
            }
            return false;
        } catch (\Gene\Exception $e) {
            throw $e;
        }
    }

    /**
     * Get a queue data
     * @param string $name queue name
     * @param string $uri queue uri
     * @return mixed
     */
    public function get($name = null, $uri = null)
    {
        if (null === $name) {
            $name = $this->queueName;
        }
        if (null === $uri) {
            $uri = $this->uri;
        }
        $context = array(
            'opt' => 'get',
            'name' => $name
        );
        $data = \Ext\Http::get($uri, $context);
        if ('HTTPMQ_GET_END' == $data) {
                return false;
        }
        return $data;
    }

}
