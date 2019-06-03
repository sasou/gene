<?php
namespace Models\Admin;

/**
 * Log Model
 * 
 * @author  sasou
 * @version  1.0
 */
class Log extends \Gene\Model
{

    /**
     * lists
     *
     * @param  array   $params   查询条件
     * @param  int     $start    开始
     * @param  int     $limit    数量
     * @return array
     */
    function lists($params, $start, $limit)
    {
        $count = $this->db
                      ->count("sys_log")
                      ->where($params)
                      ->cell();
        $list = $this->db
                     ->select("sys_log", "log_id,log_title,log_data,log_url,log_ip,log_ip_area,user_id,status")
                     ->where($params)
                     ->order("log_id desc")
                     ->limit($start, $limit)
                     ->all();

        return ["count" => $count, "list" => $list];
    }
    
    /**
     * getUserLastLoginInfo
     *
     * @param  int   $user_id  user_id
     * @return array
     */
    function getUserLastLoginInfo($user_id)
    {
        $count = $this->db
                      ->count("sys_log")
                      ->where('user_id = ?', $user_id)
                      ->cell();
        $top = $this->db
                     ->select("sys_log", "log_ip,log_ip_area,addtime")
                     ->where('user_id = ?', $user_id)
                     ->order("log_id desc")
                     ->limit(1, 1)
                     ->row();

        return ["count" => $count, "top" => $top];
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
                    ->insert("sys_log", $data)
                    ->lastId();
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
                    ->sql("update sys_log set status=abs(status-1)")
                    ->where("log_id=?", $id)
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
                    ->delete("sys_log")
                    ->where("log_id=?", $id)
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
                      ->delete("sys_log")
                      ->in("log_id in(?)", $id_arr)
                      ->affectedRows();
    }

}
