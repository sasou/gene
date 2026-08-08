<?php
namespace Gene\Orm;

/**
 * ActiveRecord 基类（精简 v1）
 *
 * 继承 Gene\Model：保留 DI（$this->db）、getInstance、success/error/data。
 * 子类声明 static $table / $primaryKey / $fields 即可获得 CRUD。
 *
 * @property \Gene\Db\Mysql $db
 * @property \Gene\Cache\Memcached $memcache
 * @property \Gene\Cache\Redis $redis
 * @property \Gene\Cache\Cache $cache
 * @property \Gene\Validate $validate
 *
 * @author  sasou<admin@php-gene.com>
 * @version 6.0.0
 */
class Model extends \Gene\Model
{
    /** @var string */
    protected static $table = '';

    /** @var string */
    protected static $primaryKey = 'id';

    /** @var array|string|null */
    protected static $fields = null;

    /** @var bool */
    protected static $timestamps = false;

    /** @var string DI 服务名 */
    protected static $connection = 'db';

    /** @var array|null */
    protected $attributes;

    /** @var bool 是否已持久化（由 fill/find/save 维护；勿外部篡改） */
    protected $exists = false;

    /**
     * 属性优先；未命中时回退 DI（$this->db 等）
     * @param string $name
     * @return mixed
     */
    public function __get($name)
    {
        return null;
    }

    /**
     * 写入 attributes（非 DI）
     * @param string $name
     * @param mixed $value
     * @return bool
     */
    public function __set($name, $value)
    {
        return false;
    }

    /**
     * @return Query
     */
    public static function query()
    {
        return null;
    }

    /**
     * @param mixed $where
     * @param mixed $bind
     * @return Query
     */
    public static function where($where, $bind = null)
    {
        return null;
    }

    /**
     * @param mixed $id
     * @return array|null
     */
    public static function find($id)
    {
        return null;
    }

    /**
     * @param array|mixed $where
     * @return array
     */
    public static function findAll($where = [])
    {
        return [];
    }

    /**
     * @param array|mixed $where
     * @param int $offset
     * @param int $limit
     * @return array{count:int,list:array}
     */
    public static function paginate($where, $offset, $limit)
    {
        return ['count' => 0, 'list' => []];
    }

    /**
     * @param array $data
     * @return int
     */
    public static function create(array $data)
    {
        return 0;
    }

    /**
     * @param array|mixed $where
     * @param array $data
     * @return int
     */
    public static function updateBy($where, array $data)
    {
        return 0;
    }

    /**
     * @param mixed $id
     * @return int
     */
    public static function destroy($id)
    {
        return 0;
    }

    /**
     * @param array $ids
     * @return int
     */
    public static function destroyAll(array $ids)
    {
        return 0;
    }

    /**
     * @param array $data
     * @return static
     */
    public function fill(array $data)
    {
        return $this;
    }

    /**
     * @return int
     */
    public function save()
    {
        return 0;
    }

    /**
     * @return int
     */
    public function delete()
    {
        return 0;
    }

    /**
     * @return array
     */
    public function toArray()
    {
        return [];
    }
}
