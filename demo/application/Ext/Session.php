<?php
namespace Ext;

/**
 * Session
 */
class Session extends \Gene\Service
{
    /**
     * $driver 缓存驱动的实例
     * @var null
     */
    private $driver = null;
    /**
     * $session 寄存session的数据
     * @var array
     */
    private $_SESSION = [];
    /**
     * $session_id sessio_id
     * @var string
     */
    protected $session_id;
    /**
     * $cookie_key cookie的session的key
     * @var string
     */
    public $cookie_key = 'SESSID';
    
    /**
     * cookie的设置
     * @var integer
     */
    public  $cookie_lifetime = 7776000;
    public  $session_lifetime = 0;
    public  $cookie_domain = '';
    public  $cookie_path = '/';
    
    public function __construct($config)
    {
        if (isset($config['driver'])) {
            $this->driver = \Gene\Di::get($config['driver']);
            $cookie_session_id = isset($this->request->cookie[$this->cookie_key]) ? $this->request->cookie[$this->cookie_key] : null;
            $this->session_id = $cookie_session_id;
            if(empty($cookie_session_id)) {
                $sess_id = md5(uniqid(microtime(true),true));
                $this->response->cookie($this->cookie_key, $sess_id, time() + $this->cookie_lifetime, $this->cookie_path, $this->cookie_domain, false, false);
                $this->session_id = $sess_id;
            }
            $this->_SESSION = $this->load($this->session_id);
        }
    }
    
    /**
     * load 加载获取session数据
     * @param    string  $sess_id
     * @return   array
     */
    protected function load(string $sess_id) {
        if(!$this->session_id) {
            $this->session_id = $sess_id;
        }
        $data = $this->driver->get($sess_id);
        //先读数据，如果没有，就初始化一个
        if (!empty($data)) {
            return unserialize($data);
        }else {
            return [];
        }
    }
    
    /**
     * 保存Session
     * @return bool
     */
    public function save() {
        if(!$this->isStart || $this->readonly) {
            return true;
        }
        //设置为Session关闭状态
        $this->isStart = false;
        // 如果没有设置SESSION,则不保存,防止覆盖
        if(empty($this->_SESSION)) {
            return false;
        }
        return $this->driver->set($this->session_id, $this->session_lifetime, serialize($this->_SESSION));
    }
    
    /**
     * getSessionId 获取session_id
     * @return string
     */
    public function getSessionId() {
        return $this->session_id;
    }
    /**
     * set 设置session保存数据
     * @param   string   $key
     * @param   mixed  $data
     * @return  boolean
     */
    public function set(string $key, $data) {
        if(is_string($key) && isset($data)) {
            $this->_SESSION[$key] = $data;
            $this->save();
            return true;
        }
        return false;
    }
    /**
     * get 获取session的数据
     * @param   string  $key
     * @return  mixed
     */
    public function get(string $key = null) {
        if(is_null($key)) {
            return $this->_SESSION;
        }
        return $this->_SESSION[$key] ?? null;
    }
    /**
     * has 是否存在某个key
     * @param    string  $key 
     * @return   boolean
     */
    public function has(string $key) {
        if(!$key) {
            return false;
        }
        return isset($this->_SESSION[$key]);
    }

    /**
     * delete 删除某个key
     * @param    string  $key
     * @return   boolean
     */
    public function delete(string $key) {
        if($this->has($key)) {
            unset($this->_SESSION[$key]);
            return true;
        }
        return false;      
    }
    /**
     * clear 清空某个session
     * @return  boolean
     */
    public function destroy() {
        if(!empty($this->_SESSION)) {
            $this->_SESSION = [];
            // 使cookie失效
            $this->response->cookie($this->cookie_key, $this->session_id, time() - 600, $this->cookie_path, $this->cookie_domain);
            // redis中完全删除session_key
            return $this->driver->del($this->session_id);
        }
        return false;
    }

}