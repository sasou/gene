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
 * @version 6.1.0
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

    /**
     * @var string|null 创建时间列名（null/'' = 不写该列）
     * @since 6.1.0
     */
    protected static $createdAt = 'created_at';

    /**
     * @var string|null 更新时间列名（null/'' = 不写该列）
     * @since 6.1.0
     */
    protected static $updatedAt = 'updated_at';

    /**
     * @var string 时间戳格式：'datetime'（Y-m-d H:i:s）| 'unix'（int）
     * @since 6.1.0
     */
    protected static $timestampFormat = 'datetime';

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

    public function __isset($name)
    {
        return false;
    }

    public function __unset($name)
    {
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
     * @param bool $asModel return a hydrated model instance when true
     * @return array|static|null
     */
    public static function find($id, $asModel = false)
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
     * @param string|null $order 列表阶段排序（如 'id desc'）；count 阶段不带 order（6.1.0+）
     * @return array{count:int,list:array}
     */
    public static function paginate($where, $offset, $limit, $order = null)
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
     * updateBy — 按条件更新。
     *
     * $where 语义（注意：不是 raw SQL 片段）：
     *  - 关联数组：条件集合（如 ['status' => 1]，值可以是 ['%kw%', 'like'] 等）
     *    · 键必须是字符串列名；数字键（如 [0 => 1]）不会产出谓词，会响亮失败
     *      （PDOException: incomplete input）或匹配 0 行，**不会**静默全表写
     *    · 值为空 op 数组（如 ['id' => []]）同上：响亮失败或 0 行
     *  - 标量：视为**主键值**（生成 `pk=?` 并绑定原值；'status=1' 这类字符串
     *    会静默匹配 0 行，需要 raw 片段请用 query()->where('…')->update($data)）
     *  - 空数组 / null：**抛异常**（6.1.0+，拒绝无 WHERE 全表更新）
     *
     * @param array|int|string $where
     * @param array $data
     * @return int 影响行数
     * @throws \Exception 空条件（[] / null）时抛出；非空但语义为空的数组
     *                 （数字键、空 op 数组）由 makeWhere 响亮失败，不会静默全表写
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
     * findMany — 主键 IN 批量取（一次查询替代 N 次 find / 全表扫描）。
     * 空数组返回 [] 且不发 SQL；>1000 个 id 发 E_NOTICE（建议分批）。
     *
     * @param array $ids
     * @param bool $preserveOrder true 时结果顺序与 $ids 一致（PHP 侧重排，不用 FIELD()）
     * @return array
     * @since 6.1.0
     */
    public static function findMany(array $ids, $preserveOrder = false)
    {
        return [];
    }

    /**
     * createMany — 批量插入（Db::batchInsert，一次 round-trip），返回影响行数。
     * 每行必须拥有相同的键且顺序一致（VALUES 按位置对齐），否则抛异常；
     * timestamps 逐行填充。大批量请调用方分片（建议 500/批），>5000 行发 E_NOTICE。
     *
     * @param array $rows
     * @return int
     * @since 6.1.0
     */
    public static function createMany(array $rows)
    {
        return 0;
    }

    /**
     * insertIgnore — 幂等写入：MySQL INSERT IGNORE / SQLite INSERT OR IGNORE；
     * Pgsql/Mssql 抛异常（用 sql() + ON CONFLICT/MERGE）。返回影响行数（被忽略时 0）。
     * 驱动语义不可移植，跨驱动项目勿当通用 API。
     *
     * @param array $data
     * @return int
     * @since 6.1.0
     */
    public static function insertIgnore(array $data)
    {
        return 0;
    }

    /**
     * updateOrCreate — 按 $where 查到则更新（返回影响行数），否则插入
     * （返回新 id；关联数组 $where 的键值会并入新行）。非原子操作；
     * 有并发竞争时请用唯一键 + insertIgnore/upsert。
     * $where 语义同 updateBy：数组=条件集合、标量=主键值；
     * 空数组 / null 在更新分支**抛异常**（6.1.0+，拒绝全表更新）。
     *
     * @param array|int|string $where
     * @param array $data
     * @return int|string
     * @since 6.1.0
     */
    public static function updateOrCreate($where, array $data)
    {
        return 0;
    }

    /**
     * toggle — 状态翻转（CAS：UPDATE ... SET field=? WHERE pk=? AND field=?，
     * 并发下败者返回 0 而不是二次翻转）。$timestamps 开启时自动同步
     * $updatedAt 列。返回影响行数（行不存在/竞争失败为 0）。
     *
     * @param mixed $id
     * @param string $field
     * @param array $values 两个候选值，默认 [0, 1]
     * @return int
     * @since 6.1.0
     */
    public static function toggle($id, $field, $values = [0, 1])
    {
        return 0;
    }

    /**
     * transaction — 在本模型 $connection 对应的 Db 上执行回调事务。
     * 语义同 Gene\Db\*::transaction()；跨模型只要共用同一连接即可嵌套。
     * transact() 为别名。
     *
     * @param callable $fn
     * @return mixed
     * @since 6.1.0
     */
    public static function transaction($fn)
    {
        return null;
    }

    /**
     * @see transaction()
     * @param callable $fn
     * @return mixed
     * @since 6.1.0
     */
    public static function transact($fn)
    {
        return null;
    }

    /**
     * @param array $data
     * @return static
     */
    public function fill(array $data, $hydrate = true)
    {
        return $this;
    }

    /**
     * Explicitly set whether this model represents a persisted row.
     *
     * @param bool $exists
     * @return static
     */
    public function setExists($exists = false)
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
