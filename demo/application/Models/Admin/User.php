<?php
namespace Models\Admin;

/**
 * User Model
 * 
 * @author  sasou
 * @version  1.0
 */
class User extends \Gene\Model
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
                      ->count("sys_user")
                      ->where($params)
                      ->cell();
        $list = $this->db
                     ->select("sys_user", "user_id,user_name,user_realname,user_icon,group_id,status")
                     ->where($params)
                     ->order("user_id desc")
                     ->limit($start, $limit)
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
                    ->select("sys_user")
                    ->where("user_id=?", $id)
                    ->limit(1)
                    ->row();
    }

	/**
     * getField
     * 
     * @param int    $id 
     * @param string $field 
     * @return string
     */
    public function getField($id, $field ='user_name')
    {
        $data = $this->row($id);
        return isset($data[$field]) ? $data[$field] : '';
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
                    ->insert("sys_user", $data)
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
                    ->update("sys_user", $data)
                    ->where("user_id=?", $id)
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
                    ->sql("update sys_user set status=abs(status-1)")
                    ->where("user_id=?", $id)
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
                    ->delete("sys_user")
                    ->where("user_id=?", $id)
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
                      ->delete("sys_user")
                      ->in("user_id in(?)", $id_arr)
                      ->affectedRows();
    }
    
    /**
     * countChird
     *
     * @param  mixed $id_arr id数组或者id
     * @return int   count
     */
    function countByGroupId($id_arr)
    {
        return $this->db
                       ->count("sys_user")
                       ->in("group_id in(?)", $id_arr)
                       ->cell();
    }
    
    /**
     * 检查登录
     * 
     * @param string $username 用户名
     * @return array
     */
    function getUserInfoByName($username)
    {
        return $this->db->sql("select 
                                    a.user_id,a.user_name,a.user_pass,a.user_salt,a.user_realname,a.user_icon,
                                    a.group_id,b.group_title,a.status 
                                from 
                                    sys_user a 
                                left join 
                                    sys_group b on b.group_id=a.group_id 
                                where 
                                    a.user_name=?", $username)->row();
    }

}
