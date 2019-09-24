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
    private $session_data = [];
    /**
     * $session_id sessio_id
     * @var string
     */
    protected $session_id = null;
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
        }
    }
    
    
    /**
     * init
     */
    public function init(string $sess_id = "") {
        if($sess_id != "") {
            $this->session_id = $sess_id;
        } else {
            $cookie_session_id = isset($this->request->cookie[$this->cookie_key]) ? $this->request->cookie[$this->cookie_key] : null;
            if(empty($cookie_session_id)) {
                $sess_id = md5(uniqid(microtime(true),true));
                $this->response->cookie($this->cookie_key, $sess_id, time() + $this->cookie_lifetime, $this->cookie_path, $this->cookie_domain, false, false);
                $this->session_id = $sess_id;
            } else {
                $this->session_id = $cookie_session_id;
            }
        }
        $this->session_data = $this->load();
    }
    
    /**
     * load 加载获取session数据
     * @param    string  $sess_id
     * @return   array
     */
    protected function load() {
        $data = $this->driver->get($this->session_id);
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
        // 如果没有设置SESSION,则不保存,防止覆盖
        if(empty($this->session_data)) {
            return false;
        }
        return $this->driver->set($this->session_id, serialize($this->session_data), $this->session_lifetime);
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
            $this->session_data[$key] = $data;
            return $this->save();
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
            return $this->session_data;
        }
        return $this->session_data[$key] ?? null;
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
        return isset($this->session_data[$key]);
    }

    /**
     * delete 删除某个key
     * @param    string  $key
     * @return   boolean
     */
    public function del(string $key) {
        if($this->has($key)) {
            unset($this->session_data[$key]);
            return $this->save();
        }
        return false;      
    }
    /**
     * clear 清空某个session
     * @return  boolean
     */
    public function destroy() {
        if(!empty($this->session_data)) {
            $this->session_data = [];
            // 使cookie失效
            $this->response->cookie($this->cookie_key, $this->session_id, time() - 600, $this->cookie_path, $this->cookie_domain);
            // redis中完全删除session_key
            return $this->driver->del($this->session_id);
        }
        return false;
    }

}
