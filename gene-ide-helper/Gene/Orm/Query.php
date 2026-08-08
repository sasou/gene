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
     * @param mixed $where
     * @param mixed $fields
     * @return $this
     */
    public function where($where, $fields = null)
    {
        return $this;
    }

    /**
     * @param string $in
     * @param mixed $fields
     * @return $this
     */
    public function in($in, $fields = null)
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
     * @param int $num
     * @param int|null $offset
     * @return $this
     */
    public function limit($num, $offset = null)
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
     * @return int|mixed
     */
    public function count()
    {
        return 0;
    }
}
