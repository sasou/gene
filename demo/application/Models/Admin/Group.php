<?php
namespace Models\Admin;

/**
 * Group Model
 * 
 * @author  sasou
 * @version  1.0
 */
class Group extends \Gene\Model
{

    /**
     * run
     *
     * @param  int  $start    分页
     * @param  int  $pageSize 每页数量
     * @return array
     */
    function lists($start, $pagesize)
    {
        $count = $this->db
                       ->count("sys_group")
                       ->where("group_pid=0")
                       ->cell();
        $list = $this->db
                     ->select("sys_group", "group_id,group_title,group_description,status")
                     ->where("group_pid=0")
                     ->order("group_id asc")
                     ->limit($start, $pagesize)
                     ->all();

        return ["count" => $count, "list" => $list];
    }

    /**
     * row
     *
     * @param  int   $id  id
     * @return array
     */
    function row($id)
    {
        return $this->db
                    ->select("sys_group")
                    ->where("group_id=?", $id)
                    ->limit(1)
                    ->row();
    }
    
    /**
     * add
     *
     * @param  array $data  添加数据
     * @return int id
     */
    function add($data)
    {
        return $this->db
                    ->insert("sys_group", $data)
                    ->lastId();
    }

    /**
     * edit
     *
     * @param  int   $id    更新id
     * @param  array $data  更新数据
     * @return int count
     */
    function edit($id, $data)
    {
        return $this->db
                    ->update("sys_group", $data)
                    ->where("group_id=?", $id)
                    ->affectedRows();
    }

    /**
     * status
     *
     * @param  int   $id  更新id
     * @return int count
     */
    function status($id)
    {
        return $this->db
                    ->sql("update sys_group set status=abs(status-1)")
                    ->where("group_id=?", $id)
                    ->affectedRows();
    }
    
    /**
     * del
     *
     * @param  int $id id
     * @return int count
     */
    function del($id)
    {
        return $this->db
                    ->delete("sys_group")
                    ->where("group_pid=0 and group_id=?", $id)
                    ->affectedRows();
    }

    /**
     * delAll
     *
     * @param  array $id_arr id数组
     * @return int   count
     */
    function delAll($id_arr)
    {
        return $this->db
                      ->delete("sys_group")
                      ->where("group_pid=0")
                      ->in(" and group_id in(?)", $id_arr)
                      ->affectedRows();
    }

}
