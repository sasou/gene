<?php
namespace Gene\Orm;

/**
 * ORM 查询构建器（薄代理 Gene\Db\*）
 *
 * 条件存于 Query 对象，终端方法再应用到 Db；执行后与析构时 reset，
 * 避免 FPM/Swoole 下链式状态污染。
 *
 * @author  sasou<admin@php-gene.com>
 * @version 6.0.0
 */
final class Query
{
    /**
     * @param mixed $where  array 条件或 SQL 片段
     * @param mixed $bind   绑定参数（字符串 where 时）
     * @return $this
     */
    public function where($where, $bind = null)
    {
        return $this;
    }

    /**
     * @param string $in
     * @param mixed $bind
     * @return $this
     */
    public function in($in, $bind = null)
    {
        return $this;
    }

    /**
     * @param string $order
     * @return $this
     */
    public function order($order)
    {
        return $this;
    }

    /**
     * 单参：取 $num 行；双参：LIMIT offset,count（与 paginate / MySQL 语义一致，驱动自适应）
     *
     * @param int $num   单参时为行数；双参时为 offset
     * @param int|null $limit  行数（双参时）
     * @return $this
     */
    public function limit($num, $limit = null)
    {
        return $this;
    }

    /**
     * @return array|null
     */
    public function all()
    {
        return null;
    }

    /**
     * @return array|null
     */
    public function row()
    {
        return null;
    }

    /**
     * @return mixed
     */
    public function cell()
    {
        return null;
    }

    /**
     * @return int
     */
    public function count()
    {
        return 0;
    }
}
