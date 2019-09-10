<?php
namespace Models\Doc;

/**
 * Mark Model
 * 
 * @author sasou<admin@php-gene.com>
 * @version 1.0
 */
class Mark extends \Gene\Model
{

    /**
     * run
     *
     * @param  array   $params   查询条件
     * @param  int     $start    开始
     * @param  int     $limit    数量
     * @return array
     */
    function lists($params, $start, $limit)
    {
        $count = $this->db
                      ->count("app_mark")
                      ->where($params)
                      ->cell();
        $list = $this->db
                     ->select("app_mark", "mark_id,mark_type,mark_title,user_id,status")
                     ->where($params)
                     ->order("mark_id desc")
                     ->limit($start, $limit)
                     ->all();

        return ["count" => $count, "list" => $list];
    }
    
    /**
     * listAll
     *
     * @param  int   $type  type
     * @return array
     */
    function listAll($type)
    {
        return $this->db
                     ->select("app_mark", "mark_id,mark_type,mark_title")
                     ->where("status = 1 and mark_type=?", $type)
                     ->order("`order` desc,mark_id asc")
                     ->limit(30)
                     ->all();
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
                    ->select("app_mark")
                    ->where("mark_id=?", $id)
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
                    ->insert("app_mark", $data)
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
                    ->update("app_mark", $data)
                    ->where("mark_id=?", $id)
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
                    ->sql("update app_mark set status=abs(status-1)")
                    ->where("mark_id=?", $id)
                    ->affectedRows();
    }
    
    /**
     * del
     *
     * @param  int $id id
     * @return int
     */
    function del($id)
    {
        return $this->db
                    ->delete("app_mark")
                    ->where("mark_id=?", $id)
                    ->affectedRows();
    }

    /**
     * delAll
     *
     * @param  array $id_arr id数组
     * @return int
     */
    function delAll($id_arr)
    {
        return $this->db
                      ->delete("app_mark")
                      ->in("mark_id in(?)", $id_arr)
                      ->affectedRows();
    }
    
}
