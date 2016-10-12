<?php
namespace Ext\Com\WebServices;

class Rest extends baseAbstract
{

    /**
     * Url string
     *
     * @var string
     */
    private $_url = '';

    /**
     * work confg
     * @var array
     */
    private $_workConfig = array();

    /**
     *  Http objcet
     * @var objcet
     */
    private $_http = NULL;

    public function __construct($params = array())
    {
        parent::__construct($params);
        $this->_http = new Cola_Com_Http();
        $this->_workConfig = $params['servers']['default'];
        $this->_url = $this->_workConfig['host'] . ':' . $this->_workConfig['port'] . $this->_workConfig['services'];

//        if (!$this->ping($this->_url)) {
//            $this->_workConfig = $params['servers']['slave'];
//            $this->_url = $this->_workConfig['host'] . ':' . $this->_workConfig['port'] . $this->_workConfig['services'];
//        }
    }

    public function __call($name, $arguments)
    {
        $data = array('method' => $name, 'data' => json_encode($arguments));
        $result = json_decode($this->get($this->_url, $data), TRUE);
        //处理正确结果
        if (isset($result[0]['status']) && $result[0]['status']) {
            $result = $result[1]['result'];
            if (isset($result['boolean'])) {

                return (boolean) $result['boolean'];
            } elseif (isset($result['int'])) {

                return (int) $result['int'];
            } elseif (isset($result['string'])) {

                return (string) $result['string'];
            } else {

                return $result;
            }
        } else {
            //写圈子调用失败记录写失败日志
            throw new Cola_\Exception("Error# Method: {$name} params: " . json_encode($arguments) . " error: {$result[1]['error']}");
        }
    }

    /**
     * Http put
     *
     * @param string $url
     * @param array $data
     * @return Mixed
     */
    public function put($url = NULL, array $data = array())
    {
        NULL === $url && $url = $this->_url;
        $opts = isset($this->_wordConfig['options']) ? $this->_wordConfig['options'] : array();
        return $this->_http->post($this->_url, $data, $opts);
    }

    /**
     * Http get
     *
     * @param string $url
     * @param array $data
     * @return Mixed
     */
    public function get($url = NULL, $data = array())
    {
        NULL === $url && $url = $this->_url;
        $opts = isset($this->_wordConfig['options']) ? $this->_wordConfig['options'] : array();
        return $this->_http->get($url, $data, $opts);
    }

    /**
     * ping services is work
     * @param string $url
     * @return int
     */
    public function ping($url)
    {
        try {
            $result = json_decode($this->_http->get($url, array('method' => 'ping', 'data' => '[]'), array('timeout' => 1)), TRUE);
            return isset($result[1]['result']);
        } catch (Cola_\Exception $exc) {
            return FALSE;
        }
    }

}
