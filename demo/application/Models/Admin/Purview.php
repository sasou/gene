<?php
namespace Models\Admin;

/**
 * Purview Model
 * 
 * @author  sasou
 * @version  1.0
 * @date  2018-05-15
 */
class Purview extends \Gene\Model
{

    /**
     * lists
     *
     * @param  int $group_id  group_id
     * @return array
     */
    function lists($group_id)
    {
        return $this->db
                     ->select("sys_purview", "obj_id")
                     ->where("purview_type=1 and group_id=?", $group_id)
                     ->all();
    }
    
    /**
     * batchInsert
     *
     * @param  array $data  添加数据
     * @return int id
     */
    function batchInsert($data)
    {
        return $this->db
                    ->batchInsert("sys_purview", $data)
                    ->affectedRows();
    }
    
    /**
     * delAll
     *
     * @param  int $group_id group_id
     * @return int   count
     */
    function delAll($group_id)
    {
        return $this->db
                      ->delete("sys_purview")
                      ->where("group_id=?", $group_id)
                      ->affectedRows();
    }

}
