<?php
namespace Ext\Com\WebServices;

class Yar extends baseAbstract
{

    /**
     * Url string
     *
     * @var string
     */
    private $url = '';

    /**
     * work confg
     * @var array
     */
    public $yarClient = null;

    public function __construct(array $params = array())
    {
        parent::__construct($params);
        $this->url = $params['host'] . ':' . $params['port'] . $params['services'];
        $this->yarClient = new yar_client($this->url);
        if (!empty($params['options'])) {
            foreach ($params['options'] as $k => $v) {
                $this->yarClient->SetOpt($k, $v);
            }
        }
    }

    public function __call($name, $arguments)
    {
        return $this->yarClient->call($name, $arguments);
    }

    /**
     * yar
     *
     * @param string $url
     * @param array $data
     * @return Mixed
     */
    public function put($methon, array $params = array(), $url = NULL)
    {
        NULL === $url && $url = $this->url;
        $client = new yar_client($url);
        return $client->call($methon, $params);
    }

    /**
     * void
     */
    public function get()
    {

    }

}
