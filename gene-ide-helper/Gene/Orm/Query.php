<?php
namespace Gene\Orm;

/**
 * ORM 查询构建器（薄代理 Gene\Db\*）
 *
 * v2（6.1.0）：条件记录在 Query 对象的有序 ops 列表中，终端方法按调用
 * 顺序重放到 Db。重复的 where()/join()/in() 是**累加**而非覆盖；字符串
 * 条件之间由 Query 生成 " AND " 连接符。执行后与析构时 reset Db，避免
 * FPM/Swoole 下链式状态污染。
 *
 * 一次性语义：Query 是「构建 → 执行 → 丢弃」的一次性构建器，不可缓存
 * 复用，也不可交错构建两个 Query（它们共享同一 DI Db 句柄，执行时会先
 * reset）。paginate 在同一 Query 上串行重放两次（count/list）是安全的。
 *
 * @author  sasou<admin@php-gene.com>
 * @version 6.1.0
 */
final class Query
{
    /**
     * where — 四种形式：
     *  - where(['col' => $v, ...])        关联数组（走 Db makeWhere，标识符加引号）
     *  - where('name != ?', $bind)        原始片段 + 绑定（$bind 可为标量/数组）
     *  - where('id', '>=', 1)             比较运算简写：$op 白名单 > >= < <= != =，
     *                                     $col 必须是纯标识符（[A-Za-z0-9_.]），否则抛异常
     *  - where(34)                        标量主键简写：自动转为 primaryKey=? 并绑定
     *  - where("34")                      数字字符串同上（URL 参数等场景）
     *                                     （ primaryKey 取自 Model 的 $primaryKey 属性）
     * 多次调用以 AND 累加；数组 where 同名键后写覆盖先写。
     *
     * @param mixed $where
     * @param mixed $op_or_bind
     * @param mixed $val
     * @return $this
     */
    public function where($where, $op_or_bind = null, $val = null)
    {
        return $this;
    }

    /**
     * in — 两种形式：
     *  - in('id', [1, 2, 3])        列形式，展开为 id IN (?,?,?)
     *  - in('id in(?)', [1, 2])     原始占位符形式（透传 Db::in）
     * 空数组：不发 SQL，终端方法直接返回空结果（all()=[]、count()=0、
     * row()/cell()=null、paginate()={count:0,list:[]}、update()/delete()=0）
     * —— 绝不退化为无条件全表。>1000 个值发 E_NOTICE（建议分批）。
     *
     * @param string $in
     * @param mixed $bind
     * @return $this
     */
    public function in($in, $bind = null)
    {
        return $this;
    }

    /**
     * join — 对齐 Db::join；可多次调用。LEFT/RIGHT 等走 $type。
     * update()/delete() 不支持 join（调用即抛异常）。
     *
     * @param string $table 关联表（可带别名）
     * @param array $on ON 条件：leftColumn => rightColumn（两侧按标识符加引号）
     * @param string $type INNER/LEFT/RIGHT/CROSS/FULL/LEFT OUTER/...（默认 INNER）
     * @return $this
     */
    public function join($table, $on, $type = 'INNER')
    {
        return $this;
    }

    /**
     * group — 多次调用以 ", " 累加。
     * 注意：与 count()/paginate() 组合会抛异常（见 count() 注释）。
     * @param string $group
     * @return $this
     */
    public function group($group)
    {
        return $this;
    }

    /**
     * having — 多次调用以 " AND " 累加
     * @param string $having
     * @return $this
     */
    public function having($having)
    {
        return $this;
    }

    /**
     * order — 多次调用以 ", " 累加（如 'id desc'）
     * @param string $order
     * @return $this
     */
    public function order($order)
    {
        return $this;
    }

    /**
     * 单参：取 $num 行；双参：LIMIT offset,count（与 paginate / MySQL 语义一致，驱动自适应）
     * 多次调用后者覆盖前者。
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
     * fields — 本次查询的字段投影，覆盖 Model::$fields（array|string）
     *
     * @param array|string $fields
     * @return $this
     */
    public function fields($fields)
    {
        return $this;
    }

    /**
     * selectSub — 追加一个子查询列：", ($sql) AS `$alias`"。
     * $sql 为开发者书写（与 Db::sql() 同信任级别，不做转义）；
     * $alias 必须是纯标识符。仅 select 类终端生效（count 忽略）。
     *
     * @param string $sql
     * @param string $alias
     * @return $this
     */
    public function selectSub($sql, $alias)
    {
        return $this;
    }

    /**
     * whereLike — LIKE 模糊匹配：自动转义 \ % _ 并包成 %kw%，追加
     * ESCAPE 子句（驱动自适应）。业务若已自行转义（如 escapeLikePattern）
     * 请勿再调本方法，避免双转义。
     *
     * @param string $col 纯标识符列名
     * @param string $keyword 原始关键字（其中的 % _ 按字面量匹配）
     * @return $this
     */
    public function whereLike($col, $keyword)
    {
        return $this;
    }

    /**
     * lockForUpdate — 行锁（仅 select 终端生效）。
     * MySQL: FOR UPDATE；Pgsql: FOR UPDATE；Sqlite: no-op + E_NOTICE；
     * Mssql: 抛异常（需 WITH (UPDLOCK) 表提示，请用 sql()）。
     * 必须在事务内使用，否则 E_NOTICE。不可移植 API，见驱动差异说明。
     *
     * @return $this
     */
    public function lockForUpdate()
    {
        return $this;
    }

    /**
     * sharedLock — 共享锁。MySQL: LOCK IN SHARE MODE；Pgsql: FOR SHARE；
     * Sqlite no-op + E_NOTICE；Mssql 抛异常。同样须在事务内。
     *
     * @return $this
     */
    public function sharedLock()
    {
        return $this;
    }

    /**
     * print — 构建并输出 SQL（不执行），返回 Db::print() 结果。
     * 调用后 reset Db 句柄，Query 仍可继续用于真正的终端调用。
     *
     * @return array{sql:string,param:array}
     */
    public function print()
    {
        return ['sql' => '', 'param' => []];
    }

    /**
     * @return array
     */
    public function all()
    {
        return [];
    }

    /**
     * @return array|null
     */
    public function row()
    {
        return null;
    }

    /**
     * first — 等价 limit(1) + row()（不污染 op 列表）
     *
     * @return array|null
     */
    public function first()
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
     * count — 继承 where/join，忽略 order/limit/lock/fields。
     * 不得与 group() 组合（count over GROUP BY 会静默返回第一个分组的
     * 行数而非分组数）——检测到 group 时抛异常；分组统计请用
     * count() + all() 两步或 Db::cell() 手写聚合。
     *
     * @return int
     */
    public function count()
    {
        return 0;
    }

    /**
     * paginate — {count, list}；list 阶段继承 order 并强制 offset/limit，
     * count 阶段不带 order。仅保证单表语义；JOIN 场景请显式
     * count() + all() 两步并自行保证 FROM/WHERE 一致。
     * 与 group() 组合时抛异常（同 count() 的限制）。
     *
     * @param int $offset
     * @param int $limit
     * @return array{count:int,list:array}
     */
    public function paginate($offset, $limit)
    {
        return ['count' => 0, 'list' => []];
    }

    /**
     * update — 立即执行（调用即执行，与 Model::updateBy 对称），
     * 返回影响行数。要求至少一个**有效** where()/in() 条件，否则抛异常：
     * where([])（空数组）与 where('')（空串）会被静默跳过、不构成条件，
     * 同样拒绝执行——杜绝「动态拼条件为空时全表覆写」。
     * in('id', []) 是例外：它是安全空操作，返回 0 且不抛异常。
     *
     * @param array $data
     * @return int
     */
    public function update(array $data)
    {
        return 0;
    }

    /**
     * delete — 立即执行，返回影响行数。条件护栏同 update()：
     * 无条件 / where([]) / where('') 一律抛异常，不会生成无 WHERE 的
     * DELETE；in('id', []) 为安全空操作（返回 0）。
     *
     * @return int
     */
    public function delete()
    {
        return 0;
    }
}
