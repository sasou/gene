<?php
namespace Ext;
/**
 * Ext Model
 */
abstract class Model
{

    const ERROR_VALIDATE_CODE = -400;
    const ERROR_MODEL_CODE = -401;

    /**
     * Db name
     *
     * @var string
     */
    protected $_db = '_db';

    /**
     * Table name, with prefix and main name
     *
     * @var string
     */
    protected $_table;

    /**
     * Primary key
     *
     * @var string
     */
    protected $_pk = 'id';

    /**
     * Cache config
     *
     * @var mixed, string for config key and array for config
     */
    protected $_cacheKey = '_cache';

    /**
     * Cache object
     *
     * @var object
     */
    protected $_cache = NULL;

    /**
     * Cache expire time
     *
     * @var int
     */
    protected $_ttl = 60;

    /**
     * Validate rules
     *
     * @var array
     */
    protected $_validate = array();

    /**
     * Error infomation
     *
     * @var array
     */
    public $error = array();

    /**
     * Load data
     *
     * @param int $id colunm value
     * @param string $col column key
     * @return array
     */
    public function load($id, $col = null)
    {
        is_null($col) && $col = $this->_pk;
        try {
            return $this->db->load($id, $col, $this->_table);
        } catch (\Exception $e) {
            $this->error(array('code' => $e->getCode(), 'msg' => $e->getMessage()));
            return false;
        }
    }

    /**
     * Find result
     *
     * $opts = array(
     *       'fileds' => '*',
     *       'where' => 1,
     *       'order' => null,
     *       'start' => -1,
     *       'limit' => -1)
     *
     * @param array $opts
     * @return array
     * @deprecated 2.1
     */
    public function find($opts = array())
    {
        is_string($opts) && $opts = array('where' => $opts);

        $opts += array('table' => $this->_table);

        try {
            return $this->db->find($opts);
        } catch (\Exception $e) {
            $this->error(array('code' => $e->getCode(), 'msg' => $e->getMessage()));
            return FALSE;
        }
    }

    /**
     * Count result
     *
     * @param string $where condition don't Contain "where" key
     * @param string $table if not table name default initialization table name
     * @return int
     * @deprecated 2.1
     */
    public function count($where, $table = NULL)
    {
        NULL === $table && $table = $this->_table;

        try {
            return $this->db->count($where, $table);
        } catch (\Exception $e) {
            $this->error(array('code' => $e->getCode(), 'msg' => $e->getMessage()));
            return FALSE;
        }
    }

    /**
     * Query SQL
     *
     * @param string $sql DML
     * @return mixed
     */
    public function query($sql, $data = array())
    {
        try {
            return $this->db->query($sql, $data);
        } catch (\Exception $e) {
            $this->error(array('code' => $e->getCode(), 'msg' => $e->getMessage()));
            return FALSE;
        }
    }

    /**
     * Get SQL result
     *
     * @param string $sql DML
     * @return array
     */
    public function sql($sql, $data = array())
    {
        try {
            return $this->db->sql($sql, $data);
        } catch (\Exception $e) {
            $this->error(array('code' => $e->getCode(), 'msg' => $e->getMessage()));
            return FALSE;
        }
    }

    /**
     * Insert
     *
     * @param array $data insert data
     * @param string $table table name, if not table name default initialization table name
     * @return boolean
     */
    public function insert($data, $table = NULL)
    {
        NULL === $table && $table = $this->_table;

        try {
            return $this->db->insert($data, $table);
        } catch (\Exception $e) {
            $this->error(array('code' => $e->getCode(), 'msg' => $e->getMessage()));
            return FALSE;
        }
    }

    /**
     * Update
     *
     * @param int $id primary key value
     * @param array $data update data
     * @return mixed
     */
    public function update($id, $data)
    {
        try {
            return $this->db->_update($id, $data, $this->_pk, $this->_table);
        } catch (\Exception $e) {
            $this->error(array('code' => $e->getCode(), 'msg' => $e->getMessage()));
            return FALSE;
        }
    }

    /**
     * Delete
     *
     * @param int $id column value
     * @param string $col column name, not column key defalut primary key.
     * @return boolean
     */
    public function delete($id, $col = NULL)
    {
        NULL === $col && $col = $this->_pk;
        try {
            return $this->db()->_delete($id, $col, $this->_table);
        } catch (\Exception $e) {
            $this->error(array('code' => $e->getCode(), 'msg' => $e->getMessage()));
            return FALSE;
        }
    }
	
    /**
     * DeleteIn
     *
     * @param string $where
     * @param string $table
     * @return boolean
     */
    public function deleteIn($id, $col = null)
    {
        is_null($col) && $col = $this->_pk;
        $where = "{$col} in({$id})";
        try {
            $result = $this->db->delete($where, $this->_table);
            return $result;
        }
        catch (\Exception $e) {
            $this->error = array('code' => $e->getCode(), 'msg' => $e->getMessage());
            return false;
        }
    }

    /**
     * Escape string
     *
     * @param string $str
     * @return string
     * @deprecated 2.1
     */
    public function escape($str)
    {
        return $this->db->escape($str);
    }

    /**
     * Connect db from config
     *
     * @param array $config db config
     * @param string db config key name
     * @return \Ext\Com\Db
     */
    public function db($name = NULL)
    {
        NULL === $name && $name = $this->_db;

        if (is_array($name)) {
            return \Ext\Com\Db::factory($name);
        }

        $regName = "_db_{$name}";
        if (!$db = \gene\reg::get($regName)) {
            $config = (array) \gene\application::config($name);
            $db = \Ext\Com\Db::factory($config);
            \gene\reg::set($regName, $db);
        }

        return $db;
    }

    /**
     * Init \Ext\Com\Cache
     *
     * @param mixed $name cache config key name
     * @return \Ext\Com\Cache cache object
     */
    public function cache($name = NULL)
    {
        NULL === $name && $name = $this->_cacheKey;
        if (is_array($name)) {
            return \Ext\Com\Cache::factory($name);
        }
        $regName = "_cache_$name";
        if (!$cache = \gene\reg::get($regName)) {
            $config = (array) \gene\application::config($name);
            $cache = \Ext\Com\Cache::factory($config);
            \gene\reg::set($regName, $cache);
        }

        return $cache;
    }

    /**
     * Get function cache
     *
     * @param string $func function name
     * @param array $args function args
     * @param int $ttl cache ttl
     * @return mixed
     */
    public function cached($func, array $args = array(), $ttl = NULL)
    {
        NULL === $ttl && $ttl = $this->_ttl;
        NULL === $this->_cache && $this->_cache = $this->cache();
        $key = md5(get_class($this) . $func . serialize($args));

        if (!$data = $this->_cache->get($key)) {
            $data = call_user_func_array(array($this, $func), $args);
            $this->_cache->set($key, $data, $ttl);
        }
        return $data;
    }

    /**
     * unset function cache
     *
     * @param string $func function name
     * @param array $args function args
     * @return boolean
     */
    public function unsetCached($func, array $args = array())
    {
        NULL === $this->_cache && $this->_cache = $this->cache();
        $key = md5(get_class($this) . $func . serialize($args));
        return $this->_cache->delete($key);
    }

    /**
     * Set table Name
     *
     * @param string $table table name
     * @return string table name
     */
    public function table($table = NULL)
    {
        NULL === $table && $this->_table = $table;

        return $this->_table;
    }

    /**
     * Get or set error
     *
     * @param mixed $error string|array
     * @return mixed $error string|array
     */
    public function error($error = NULL)
    {
        if (NULL !== $error) {
            $this->error = $error;
            throw new \gene\Exception($error['code'], $error['msg']);
        }

        return $this->error;
    }

    /**
     * Validate
     *
     * @param array $data validate data
     * @param boolean $ignoreNotExists
     * @param array $rules validate rule
     * @return boolean
     */
    public function validate($data, $ignoreNotExists = FALSE, $rules = NULL)
    {

        NULL === $rules && $rules = $this->_validate;
        if (empty($rules)) {
            return TRUE;
        }
        $validate = new \Ext\Com\Validate();
        $result = $validate->check($data, $rules, $ignoreNotExists);

        if (!$result) {
            $this->error = array('code' => self::ERROR_VALIDATE_CODE, 'msg' => $validate->errors);
            return FALSE;
        }

        return TRUE;
    }

    /**
     * get static class
     * @return self
     * @deprecated 2.1
     */
    public static function init()
    {
        $regName = '_class' . get_called_class();
        if (!$class = \gene\reg::get($regName)) {
            $class = new static();
            \gene\reg::set($regName, $class);
        }
        return $class;
    }

    /**
     * get pdo
     * @return PDO
     */
    public function getPdo()
    {
        return $this->db()->getPdo();
    }
	
    /**
     * Get One result
     *
     * @param string $sql
     * @return array
     */
    public function getOne($colName, $id, $col = null)
    {
        is_null($col) && $col = $this->_pk;
        $where = "{$col} = '{$id}'";
        $sql = "select {$colName} from {$this->_table} where {$where} limit 1";
        try {
            $result = $this->db->col($sql);
            return $result;
        }
        catch (\Exception $e) {
            $this->error = array('code' => $e->getCode(), 'msg' => $e->getMessage());
            return false;
        }
    }

    /**
     * Get Row result
     *
     * @param string $sql
     * @return array
     */
    public function getRow($sql, $params = array())
    {
        try {
            $result = $this->db->row($sql, $params);
            return $result;
        }
        catch (\Exception $e) {
            $this->error = array('code' => $e->getCode(), 'msg' => $e->getMessage());
            return false;
        }
    }
	
    /**
     * 通过sql 获取数据
     *
     * @param string $sql
     * @param int $page 1
     * @param int $rows 20
     * @return boolean multitype
     */
    public function sqlPagerData($sql, $page = 1, $rows = 20)
    {
        $page = intval($page);
        $rows = intval($rows);
		$limit = "";

        if ($page > 0) {
            $start = ($page - 1) * $rows;
            $limit = " limit {$start},{$rows} ";
        }
        $data = $this->sql($sql . $limit);
        $sql = "select count(*) from (" . $sql . ") as sy";
        $count = $this->db()->col($sql);
        return array(
            'data' => $data,
            'count' => $count);
    }

    /**
     * 通过sql 获取数据与分页信息
     *
     * @param string $sql
     * @param int $page 1
     * @param int $rows 20
     * @param string $url "/Admin/index/run/page/%page%";
     * @return boolean multitype
     */
    public function sqlPager($sql, $page, $rows, $url, $ajax = 0)
    {
        $data = $this->sqlPagerData($sql, $page, $rows);

        if (!$data)
            return false;
		$pager = new \Ext\Com\Pager($page, $rows, $data['count'], $url, $ajax);
		$data['page'] = $pager->html();
        return $data;
    }
	
    /**
     * isok() 修改状态
     *
     * @param Int id
     * @return Int
     */
    public function isok($id, $col = null, $isok = null)
    {
        if ($this->_status == '')
            return 0;
        is_null($col) && $col = $this->_pk;
        is_null($isok) && $isok = $this->_status;
        $sql = "update {$this->_table} set {$isok}=ABS({$isok}-1) where {$col} = '{$id}'";
        return $this->db()->sql($sql);
    }
	
    /**
     * 常用拼接搜索条件
     * @param array $search 搜索参数
     * @return string
     */
    public function buildSearch($search, $type = true)
    {
        $str = '';
        if (is_array($search)) {
            foreach ($search as $k => $v) {
                if ($str == '') {
                    $str = ((gettype($v) == "string") && $type) ? $k . " like '%{$v}%'" : $k . "='{$v}'";
                } else {
                    $str .= ((gettype($v) == "string") && $type) ? ' and ' . $k . " like '%{$v}%'" : ' and ' .
                        $k . "='{$v}'";
                }
            }
        }
        return $str;
    }
	
    /**
     * getList() 分页数据列表
     *
     * @param Int $page
     * @param Int $rows
     * @param String $select
     * @param String $where
     * @param String $order
     * @param String $url
     * @return Array
     */
    public function getList($page = 1, $rows = 20, $select = '', $where = '', $order =
        '', $url = '', $join = '')
    {
        $select == '' && $select = "*";
        $where != '' && $where = " where {$where}";
        $order != '' && $order = " order by {$order}";
        $sql = "select {$select} from {$this->_table} {$join} {$where} {$order}";
        return $this->sqlPager($sql, $page, $rows, $url);
    }

    /**
     * getTree() 分页数据列表
     *
     * @param Int $page
     * @param Int $rows
     * @param String $select
     * @param String $where
     * @param String $order
     * @param String $url
     * @return Array
     */
    public function getTree($page = 1, $rows = 20, $select = '', $where = '', $order =
        '', $url = '', $join = '')
    {
        $select == '' && $select = "a.*";
        $where != '' && $where = " where {$where}";
        $order != '' && $order = " order by {$order}";
        $sql = "SELECT {$select},(select count({$this->_pk}) from {$this->_table} where {$this->_pid}= a.{$this->_pk}) as num from {$this->_table} a {$join} {$where} {$order}";
        return $this->sqlPager($sql, $page, $rows, $url);
    }

    /**
     * Dynamic set vars
     *
     * @param string $key
     * @param mixed $value
     */
    public function __set($key, $value = NULL)
    {
        $this->$key = $value;
    }

    /**
     * Dynamic get vars
     *
     * @param string $key
     */
    public function __get($key)
    {
        switch ($key) {
            case 'db':
                $this->db = $this->db();
                return $this->db;
            case 'cache' :
                $this->cache = $this->cache();
                return $this->cache;
            case 'com':
                $this->com = new Cola_Com();
                return $this->com;
            case 'view':
                $this->view = new Cola_View();
                return $this->view;
            case 'helper':
                $this->helper = new Cola_Helper();
                return $this->helper;

            default:
                throw new \gene\Exception(0, 'Undefined property: ' . get_class($this) . '::' . $key);
        }
    }

}
