<?php
namespace Models\Admin;

/**
 * Group Model — Gene\Orm\Model ActiveRecord
 *
 * @author  sasou
 * @version  2.0
 */
class Group extends \Gene\Orm\Model
{
    protected static string $table = 'sys_group';
    protected static string $primaryKey = 'group_id';
    protected static array $fields = [
        'group_id', 'group_title', 'group_description', 'status',
    ];

    /**
     * lists — 顶级分组分页
     */
    function lists($start, $pagesize)
    {
        $where = ['group_pid' => 0];
        $count = static::query()->where($where)->count();
        $list = static::query()
            ->where($where)
            ->order('group_id asc')
            ->limit((int) $start, (int) $pagesize)
            ->all() ?: [];
        return ['count' => $count, 'list' => $list];
    }

    /**
     * row
     */
    function row($id)
    {
        return static::find($id);
    }

    /**
     * add
     */
    function add($data)
    {
        return static::create($data);
    }

    /**
     * edit
     */
    function edit($id, $data)
    {
        return static::updateBy($id, $data);
    }

    /**
     * status
     */
    function status($id)
    {
        return $this->db
            ->sql('update sys_group set status=abs(status-1)')
            ->where('group_id=?', $id)
            ->affectedRows();
    }

    /**
     * del — 仅删顶级节点
     */
    function del($id)
    {
        return $this->db
            ->delete('sys_group')
            ->where('group_pid=0 and group_id=?', $id)
            ->affectedRows();
    }

    /**
     * delAll
     */
    function delAll($id_arr)
    {
        return $this->db
            ->delete('sys_group')
            ->where('group_pid=0')
            ->in(' and group_id in(?)', $id_arr)
            ->affectedRows();
    }
}
